#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include "GitKeyStorage.h"
#include "GitLfs.h"

#include <Log.h>
#include <Uuid.h>

#include <git2/sys/errors.h>

#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QVector>

#include <cstring>
#include <tuple>

#define THREAD_ID "GitRepo"

// Remotes, targets, upstreams and synchronization (fetch, pull, push).
namespace GitRepo {

using namespace Private;

namespace {

// Targets (more URL pairs per remote) are kept as
// sourcerer-target.<remote>:<target>.fetchUrl and .pushUrl. Remote names can't
// contain ':'. Older versions used remote.<remote>.target.<target>.*, which git
// reads as a remote called "<remote>.target.<target>"; those are migrated.
constexpr auto TARGET_SECTION = "sourcerer-target.";
constexpr auto LEGACY_TARGET_INFIX = ".target.";

struct TargetEntry {
  QString name; // Full configuration key as stored.
  QString remote;
  QString target;
  QString property; // "fetchurl" or "pushurl", lower case.
  QString value;
};

QString targetKey(const QString &remote, const QString &target,
                  const char *property) {
  return QStringLiteral("%1%2:%3.%4")
      .arg(QLatin1String(TARGET_SECTION), remote, target,
           QLatin1String(property));
}

// Splits "<prefix><remote>:<target>.<property>" (or the legacy form) into its
// parts. Returns false for other keys.
bool parseTargetEntry(const QString &name, const QString &value,
                      TargetEntry *entry) {
  const int lastDot = name.lastIndexOf('.');
  if (lastDot < 0) {
    return false;
  }
  entry->name = name;
  entry->value = value;
  entry->property = name.mid(lastDot + 1).toLower();
  if (entry->property != QStringLiteral("fetchurl") &&
      entry->property != QStringLiteral("pushurl")) {
    return false;
  }

  const QString section = QLatin1String(TARGET_SECTION);
  if (name.startsWith(section, Qt::CaseInsensitive)) {
    const QString subsection = name.mid(section.length(),
                                        lastDot - section.length());
    const int colon = subsection.indexOf(':');
    if (colon <= 0) {
      return false;
    }
    entry->remote = subsection.left(colon);
    entry->target = subsection.mid(colon + 1);
    return !entry->target.isEmpty();
  }

  const QString legacyPrefix = QStringLiteral("remote.");
  if (name.startsWith(legacyPrefix, Qt::CaseInsensitive)) {
    const QString rest = name.mid(legacyPrefix.length(),
                                  lastDot - legacyPrefix.length());
    const int infix = rest.indexOf(QLatin1String(LEGACY_TARGET_INFIX));
    if (infix <= 0) {
      return false;
    }
    entry->remote = rest.left(infix);
    entry->target = rest.mid(infix + int(strlen(LEGACY_TARGET_INFIX)));
    return !entry->target.isEmpty();
  }

  return false;
}

// Every target entry of the configuration, legacy ones included.
QVector<TargetEntry> readTargetEntries(git_config *config) {
  QVector<TargetEntry> entries;
  git_config_iterator *iterator = nullptr;
  if (git_config_iterator_new(&iterator, config) != 0) {
    return entries;
  }

  git_config_entry *entry = nullptr;
  while (git_config_next(&entry, iterator) == 0) {
    if (!entry || !entry->name || !entry->value) {
      continue;
    }
    TargetEntry target;
    if (parseTargetEntry(QString::fromUtf8(entry->name),
                         QString::fromUtf8(entry->value), &target)) {
      entries += target;
    }
  }
  git_config_iterator_free(iterator);
  return entries;
}

bool isLegacyTargetEntry(const TargetEntry &entry) {
  return !entry.name.startsWith(QLatin1String(TARGET_SECTION),
                                Qt::CaseInsensitive);
}

// Moves legacy target entries to the sourcerer-target section.
void migrateLegacyTargets(git_config *config) {
  const QVector<TargetEntry> entries = readTargetEntries(config);
  for (const TargetEntry &entry : entries) {
    if (!isLegacyTargetEntry(entry)) {
      continue;
    }
    const QByteArray newKey =
        targetKey(entry.remote, entry.target,
                  entry.property == QStringLiteral("fetchurl") ? "fetchUrl"
                                                                : "pushUrl")
            .toUtf8();
    git_config_entry *existing = nullptr;
    if (git_config_get_entry(&existing, config, newKey.constData()) == 0) {
      git_config_entry_free(existing);
    } else {
      git_config_set_string(config, newKey.constData(),
                            entry.value.toUtf8().constData());
    }
    git_config_delete_entry(config, entry.name.toUtf8().constData());
  }
}

void migrateLegacyTargets(git_repository *repo) {
  git_config *config = nullptr;
  if (git_repository_config(&config, repo) == 0) {
    migrateLegacyTargets(config);
    git_config_free(config);
  }
}

// Reads one target's URLs. Returns false if it has no fetch URL.
bool readTarget(git_config *config, const QString &remote,
                const QString &target, QString *fetchUrl, QString *pushUrl) {
  *fetchUrl = QString();
  *pushUrl = QString();
  for (const TargetEntry &entry : readTargetEntries(config)) {
    if (isLegacyTargetEntry(entry) || entry.remote != remote ||
        entry.target != target) {
      continue;
    }
    if (entry.property == QStringLiteral("fetchurl")) {
      *fetchUrl = entry.value;
    } else {
      *pushUrl = entry.value;
    }
  }
  return !fetchUrl->isEmpty();
}

// Writes a target. An empty push URL removes it, pushes then use the fetch URL.
int writeTarget(git_config *config, const QString &remote,
                const QString &target, const QString &fetchUrl,
                const QString &pushUrl, bool *fetchFailed) {
  *fetchFailed = true;
  int error = git_config_set_string(
      config, targetKey(remote, target, "fetchUrl").toUtf8().constData(),
      fetchUrl.toUtf8().constData());
  if (error) {
    return error;
  }
  *fetchFailed = false;

  const QByteArray pushKey = targetKey(remote, target, "pushUrl").toUtf8();
  if (pushUrl.isEmpty()) {
    error = git_config_delete_entry(config, pushKey.constData());
    return error == GIT_ENOTFOUND ? 0 : error;
  }
  return git_config_set_string(config, pushKey.constData(),
                               pushUrl.toUtf8().constData());
}

// Removes every entry of a remote's targets, or moves them to `newRemote`.
void moveTargets(git_config *config, const QString &remote,
                 const QString &newRemote) {
  for (const TargetEntry &entry : readTargetEntries(config)) {
    if (isLegacyTargetEntry(entry) || entry.remote != remote) {
      continue;
    }
    if (!newRemote.isEmpty()) {
      git_config_set_string(
          config,
          targetKey(newRemote, entry.target,
                    entry.property == QStringLiteral("fetchurl") ? "fetchUrl"
                                                                  : "pushUrl")
              .toUtf8()
              .constData(),
          entry.value.toUtf8().constData());
    }
    git_config_delete_entry(config, entry.name.toUtf8().constData());
  }
}

bool remoteExists(git_repository *repo, const QString &remoteName) {
  git_remote *remote = nullptr;
  if (git_remote_lookup(&remote, repo, remoteName.toUtf8().constData()) != 0) {
    return false;
  }
  git_remote_free(remote);
  return true;
}

} // namespace

bool pull(git_repository *repo, const QString &activeBranchName, bool rebase) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting {} operation for active branch: '{}'",
            rebase ? "pull with rebase" : "pull",
            activeBranchName.isEmpty() ? "(none)"
                                       : activeBranchName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // If no active branch provided, get current branch from HEAD
  QString targetBranchName = activeBranchName;
  if (targetBranchName.isEmpty()) {
    git_reference *head = nullptr;
    int error = git_repository_head(&head, repo);
    if (error) {
      setError(GitError::PullFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchNameC = nullptr;
    error = git_branch_name(&branchNameC, head);
    if (error != 0 || !branchNameC) {
      setError(GitError::PullFailedToReadBranchName, error);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    targetBranchName = QString::fromUtf8(branchNameC);
    git_reference_free(head);
  }

  LOG_DEBUG("Target branch for pull: '{}'", targetBranchName.toStdString());

  // Get upstream for the active branch
  auto [upstreamSuccess, remoteName, remoteBranchName] =
      getUpstream(repo, targetBranchName);
  if (!upstreamSuccess || remoteName.isEmpty()) {
    setError(GitError::PullNoUpstreamConfigured, GIT_ENOTFOUND);
    LOG_DEBUG("Branch '{}' has no upstream configured",
              targetBranchName.toStdString());
    LOG_PERF(0, "Exit");
    return false; // Nothing to pull, but not an error
  }

  LOG_DEBUG("Branch '{}' will be pulled from {}/{}",
            targetBranchName.toStdString(), remoteName.toStdString(),
            remoteBranchName.toStdString());

  // Fetch from the upstream remote
  const bool commercial = isCommercialPlan();
  int error = 0;

  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::PullFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *url = git_remote_url(remote);
  LOG_DEBUG("Fetching from remote '{}' at URL: '{}'", remoteName.toStdString(),
            url);

  bool gitSchema = false;
  int retries = 0;
  const QString urlStr = QString::fromUtf8(url);
  if (isSshOrGitUrl(urlStr)) {
    gitSchema = true;
    LOG_DEBUG("Using git schema for authentication");
  }

  int maxRetries = MAX_RETRIES;
  if (gitSchema) {
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(urlStr);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1;
        LOG_DEBUG("SSH key configured for host '{}', setting max retries to 1",
                  host.toStdString());
      }
    }
  }

  if (!commercial && gitSchema) {
    setError(GitError::PullGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Pulling from git@ repositories is not available on the free "
              "plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString fetchUuid = Uuid::createUuid();

  CloneProgress progressData;
  progressData.uuid = fetchUuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.uuid = fetchUuid;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  // Fetch all heads and tags
  git_strarray refspecs = qstringlist_to_git_strarray(
      QStringList()
      << QString("+refs/heads/*:refs/remotes/%1/*").arg(remoteName)
      << "+refs/tags/*:refs/tags/*");

  // Fetch URL
  const char *fetchUrl = git_remote_url(remote);

  if (fetchUrl) {
    LOG_INFO("Fetch URL: {}", fetchUrl);
  }

  // Push URL (may be NULL if not set separately)
  const char *pushUrl = git_remote_pushurl(remote);

  if (pushUrl) {
    LOG_INFO("Push URL: {}", pushUrl);
  }

  do {
    error = git_remote_fetch(remote, &refspecs, &fetchOpts, nullptr);

    if (error) {
      setError(GitError::PullFailedToFetchRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(urlStr);

  finishFetchProgress(fetchUuid, error != 0, false);

  if (error) {
    setError(GitError::PullFailedToFetchRemoteWithCredentials, error);
    git_strarray_destroy(refspecs);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  // A branch that isn't checked out can only be fast-forwarded, merging or
  // rebasing it needs its files.
  {
    git_reference *localRef = nullptr;
    error = git_branch_lookup(&localRef, repo,
                              targetBranchName.toUtf8().constData(),
                              GIT_BRANCH_LOCAL);
    if (error) {
      setError(GitError::PullFailedToLookupReference, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (git_branch_is_head(localRef) != 1) {
      const QString upstreamRefName =
          QString("refs/remotes/%1/%2").arg(remoteName, remoteBranchName);
      git_oid upstreamOid{};
      error = git_reference_name_to_id(&upstreamOid, repo,
                                       upstreamRefName.toUtf8().constData());
      if (error) {
        setError(GitError::PullFailedToLookupReference, error);
        git_reference_free(localRef);
        LOG_PERF(0, "Exit");
        return false;
      }

      const git_oid *localOid = git_reference_target(localRef);
      bool success = true;
      if (localOid && !git_oid_equal(localOid, &upstreamOid)) {
        if (git_graph_descendant_of(repo, &upstreamOid, localOid) == 1) {
          git_reference *updated = nullptr;
          error = git_reference_set_target(&updated, localRef, &upstreamOid,
                                           "pull: Fast-forward");
          git_reference_free(updated);
          if (error) {
            setError(GitError::PullFailedToSetReferenceTarget, error);
            success = false;
          }
        } else {
          setError(GitError::PullBranchNotCheckedOutCannotFastForward,
                   GIT_ENONFASTFORWARD);
          success = false;
        }
      }

      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return success;
    }

    git_reference_free(localRef);
  }

  // Merge/rebase the upstream branch into the active branch
  git_reference *head = nullptr;
  error = git_repository_head(&head, repo);
  if (error) {
    setError(GitError::PullFailedToGetRepositoryHeadAfterFetch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Resolving upstream branch {}/{} for current branch '{}'",
            remoteName.toStdString(), remoteBranchName.toStdString(),
            targetBranchName.toStdString());

  // Get upstream tracking reference
  git_reference *upstream = nullptr;
  const QString upstreamRefName =
      QString("refs/remotes/%1/%2").arg(remoteName, remoteBranchName);
  error = git_reference_lookup(&upstream, repo,
                               upstreamRefName.toStdString().c_str());
  if (error != 0) {
    LOG_DEBUG("Failed to lookup upstream reference '{}'",
              upstreamRefName.toStdString());
    setError(GitError::PullFailedToLookupReference, error);
    git_reference_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get annotated commit from upstream
  git_annotated_commit *upstreamCommit = nullptr;
  error = git_annotated_commit_from_ref(&upstreamCommit, repo, upstream);
  if (error) {
    setError(GitError::PullFailedToCreateAnnotatedCommitFromReference, error);
    git_reference_free(upstream);
    git_reference_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool result = false;

  if (rebase) {
    git_rebase_options rebaseOpts = GIT_REBASE_OPTIONS_INIT;
    rebaseOpts.checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;

    git_signature *signature = nullptr;
    error = git_signature_default(&signature, repo);
    if (error) {
      setError(GitError::PullFailedToGetDefaultSignature, error);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_rebase *rebaseSession = nullptr;
    error = git_rebase_init(&rebaseSession, repo, nullptr, upstreamCommit,
                            nullptr, &rebaseOpts);
    if (error) {
      setError(GitError::PullFailedToStartRebase, error);
      git_signature_free(signature);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    bool rebaseSuccess = true;
    git_rebase_operation *operation = nullptr;
    git_oid commitId{};

    while ((error = git_rebase_next(&operation, rebaseSession)) == 0) {
      error = git_rebase_commit(&commitId, rebaseSession, nullptr, signature,
                                nullptr, nullptr);
      if (error == 0) {
        continue;
      }

      if (error == GIT_EUNMERGED) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Rebase stopped because conflicts must be resolved manually");
        setError(GitError::PullRebaseFailedWithConflicts, error);
      } else {
        setError(GitError::PullFailedToCommitRebaseStep, error);
      }
      git_rebase_abort(rebaseSession);
      rebaseSuccess = false;
      break;
    }

    if (rebaseSuccess && error == GIT_ITEROVER) {
      error = git_rebase_finish(rebaseSession, signature);
      if (error) {
        setError(GitError::PullFailedToFinishRebase, error);
        git_rebase_abort(rebaseSession);
        rebaseSuccess = false;
      }
    } else if (rebaseSuccess) {
      // git_rebase_next failed, e.g. local changes are in the way.
      setError(GitError::PullFailedToCommitRebaseStep, error);
      git_rebase_abort(rebaseSession);
      rebaseSuccess = false;
    }

    git_rebase_free(rebaseSession);
    git_signature_free(signature);
    result = rebaseSuccess;
  } else {
    // Analyze merge
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;

    error = git_merge_analysis(
        &analysis, &preference, repo,
        const_cast<const git_annotated_commit **>(&upstreamCommit), 1);
    if (error) {
      setError(GitError::PullFailedToCreateAnnotatedCommit, error);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
      // Fast-forward
      const git_oid *targetOid = git_annotated_commit_id(upstreamCommit);
      if (!targetOid) {
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_ERROR(Log::ErrorId::Git, "Can't get target OID for fast-forward");
        LOG_PERF(0, "Exit");
        return false;
      }

      git_commit *targetCommit = nullptr;
      error = git_commit_lookup(&targetCommit, repo, targetOid);
      if (error) {
        setError(GitError::PullFailedToLookupCommit, error);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
      checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

      error =
          git_checkout_tree(repo, (git_object *)targetCommit, &checkout_opts);
      if (error) {
        setError(GitError::PullFailedToPeelFetchedTargetCommit, error);
        git_commit_free(targetCommit);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_reference *newHead = nullptr;
      error =
          git_reference_set_target(&newHead, head, targetOid, "Fast-forward");
      if (error) {
        setError(GitError::PullFailedToSetReferenceTarget, error);
        git_commit_free(targetCommit);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_reference_free(newHead);
      git_commit_free(targetCommit);
      result = true;
    } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
      // Normal merge
      git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
      git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
      checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

      error = git_merge(
          repo, const_cast<const git_annotated_commit **>(&upstreamCommit), 1,
          &merge_opts, &checkout_opts);
      if (error) {
        setError(GitError::PullFailedToCreateMergeAnnotatedCommit, error);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      result = true;
    } else {
      result = true;
    }
  }

  git_annotated_commit_free(upstreamCommit);
  git_reference_free(upstream);
  git_reference_free(head);

  LOG_PERF(0, "Exit");
  return result;
}

static bool fetchRemoteInternal(git_repository *repo,
                                const QString &remoteName) {
  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    return false;
  }

  if (remoteName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name is empty");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_remote *remote = nullptr;
  int error =
      git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::FetchRemoteInternalFailedToLookupRemote, error);
    return false;
  }

  const char *url = git_remote_url(remote);
  LOG_DEBUG("Fetching from remote '{}' at URL: '{}'", remoteName.toStdString(),
            url);

  bool gitSchema = false;
  const QString urlStr = QString::fromUtf8(url);
  if (isSshOrGitUrl(urlStr)) {
    gitSchema = true;
    LOG_DEBUG("Using git schema for authentication");
  }

  int maxRetries = MAX_RETRIES;
  if (gitSchema) {
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(urlStr);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1;
        LOG_DEBUG("SSH key configured for host '{}', setting max retries to 1",
                  host.toStdString());
      }
    }
  }

  const bool commercial = isCommercialPlan();
  if (!commercial && gitSchema) {
    setError(GitError::FetchRemoteGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Fetching from git@ repositories is not available on the free "
              "plan.");
    git_remote_free(remote);
    return false;
  }

  const QString fetchUuid = Uuid::createUuid();

  CloneProgress progressData;
  progressData.uuid = fetchUuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.uuid = fetchUuid;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  // Fetch all heads and tags
  git_strarray refspecs = qstringlist_to_git_strarray(
      QStringList()
      << QString("+refs/heads/*:refs/remotes/%1/*").arg(remoteName)
      << "+refs/tags/*:refs/tags/*");

  int retries = 0;
  error = 0;
  do {
    error = git_remote_fetch(remote, &refspecs, &fetchOpts, nullptr);

    if (error) {
      setError(GitError::FetchRemoteInternalFailedToFetchRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(urlStr);

  finishFetchProgress(fetchUuid, error != 0, false);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to fetch from remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::FetchRemoteInternalFailedToFetchRemoteWithCredentials,
             error);
    git_strarray_destroy(refspecs);
    git_remote_free(remote);
    return false;
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);
  return true;
}

bool fetchRemote(git_repository *repo, const QString &remoteName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting fetch operation for remote: '{}'",
            remoteName.toStdString());

  const bool success = fetchRemoteInternal(repo, remoteName);

  LOG_PERF(0, "Exit");
  return success;
}

bool fetchAll(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting fetch all operation");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Fetch every configured remote, not just remotes referenced by local
  // branch upstreams. This keeps Fetch All aligned with the UI expectation
  // that all remotes are refreshed.
  const auto remotes = listRemotes(repo);
  if (remotes.isEmpty()) {
    LOG_DEBUG("No remotes found in repository");
    LOG_PERF(0, "Exit");
    return true;
  }

  QSet<QString> remotesToFetch;
  for (const auto &remote : remotes) {
    if (remote.name.isEmpty()) {
      continue;
    }

    remotesToFetch.insert(remote.name);
    LOG_DEBUG("Adding remote '{}' to fetch list", remote.name.toStdString());
  }

  if (remotesToFetch.isEmpty()) {
    LOG_DEBUG("No fetchable remotes found");
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_DEBUG("Found {} unique remotes to fetch", remotesToFetch.size());

  // A remote that fails doesn't stop the others, but fails the whole fetch
  // with its error.
  bool success = true;
  for (const QString &remoteName : remotesToFetch) {
    if (!fetchRemoteInternal(repo, remoteName)) {
      success = false;
    }
  }

  LOG_PERF(0, "Exit");
  return success;
}

bool push(git_repository *repo, const QString &activeBranchName, bool force) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting {}push operation for active branch: '{}'",
            force ? "force " : "",
            activeBranchName.isEmpty() ? "(none)"
                                       : activeBranchName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // If no active branch provided, get current branch from HEAD
  QString targetBranchName = activeBranchName;
  if (targetBranchName.isEmpty()) {
    git_reference *head = nullptr;
    int error = git_repository_head(&head, repo);
    if (error) {
      setError(GitError::PushFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchNameC = nullptr;
    error = git_branch_name(&branchNameC, head);
    if (error != 0 || !branchNameC) {
      setError(GitError::PushFailedToReadBranchName, error);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    targetBranchName = QString::fromUtf8(branchNameC);
    git_reference_free(head);
  }

  LOG_DEBUG("Target branch for push: '{}'", targetBranchName.toStdString());

  // Get upstream for the active branch
  auto [upstreamSuccess, remoteName, remoteBranchName] =
      getUpstream(repo, targetBranchName);
  if (!upstreamSuccess || remoteName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Branch '{}' has no remote configured. Specify a remote for this "
              "branch before pushing.",
              targetBranchName.toStdString());
    setError(GitError::PushNoUpstreamConfigured, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Branch '{}' will be pushed to {}/{}",
            targetBranchName.toStdString(), remoteName.toStdString(),
            remoteBranchName.toStdString());

  const QString localRefName =
      QStringLiteral("refs/heads/%1").arg(targetBranchName);
  const QString upstreamRefName =
      QStringLiteral("refs/remotes/%1/%2").arg(remoteName, remoteBranchName);
  git_oid localRefOid = {};
  git_oid upstreamRefOid = {};
  char localOidStr[GIT_OID_HEXSZ + 1] = {0};
  char upstreamOidStr[GIT_OID_HEXSZ + 1] = {0};

  if (git_reference_name_to_id(&localRefOid, repo,
                               localRefName.toStdString().c_str()) == 0) {
    git_oid_tostr(localOidStr, GIT_OID_HEXSZ + 1, &localRefOid);
  }
  if (git_reference_name_to_id(&upstreamRefOid, repo,
                               upstreamRefName.toStdString().c_str()) == 0) {
    git_oid_tostr(upstreamOidStr, GIT_OID_HEXSZ + 1, &upstreamRefOid);
  }

  LOG_DEBUG("Push ref state: local='{}' ({}) upstream='{}' ({})",
            localRefName.toStdString(),
            localOidStr[0] ? localOidStr : "unresolved",
            upstreamRefName.toStdString(),
            upstreamOidStr[0] ? upstreamOidStr : "unresolved");

  // Push to the upstream remote
  const bool commercial = isCommercialPlan();
  int error = 0;

  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::PushFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *url = git_remote_url(remote);
  LOG_DEBUG("Pushing to remote '{}' at URL: '{}'", remoteName.toStdString(),
            url);

  bool gitSchema = false;
  int retries = 0;
  const QString urlStr = QString::fromUtf8(url);
  if (isSshOrGitUrl(urlStr)) {
    gitSchema = true;
    LOG_DEBUG("Using git schema for authentication");
  }

  int maxRetries = MAX_RETRIES;
  if (gitSchema) {
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(urlStr);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1;
        LOG_DEBUG("SSH key configured for host '{}', setting max retries to 1",
                  host.toStdString());
      }
    }
  }

  if (!commercial && gitSchema) {
    setError(GitError::PushGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Pushing to git@ repositories is not available on the free "
              "plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!GitRepo::Lfs::uploadLocalObjects(
          repo, repoPath, remoteName, urlStr,
          QStringLiteral("refs/heads/%1").arg(targetBranchName))) {
    setError(GitError::PushFailedToPushRemote, GIT_EUSER);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString pushUuid = Uuid::createUuid();

  CloneProgress progressData;
  progressData.uuid = pushUuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.uuid = pushUuid;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  gPushReferenceStatus = {};

  git_push_options pushOpts;
  git_push_options_init(&pushOpts, GIT_PUSH_OPTIONS_VERSION);
  pushOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  pushOpts.callbacks.transfer_progress = &fetchProgressCallback;
  pushOpts.callbacks.sideband_progress = &pushSidebandProgressCallback;
  pushOpts.callbacks.certificate_check = &certificateCheckCallback;
  pushOpts.callbacks.push_update_reference = &pushUpdateReferenceCallback;
  pushOpts.callbacks.payload = &callbackPayload;

  // Build refspec for pushing the branch to its upstream
  const QString refspec = QString("%1refs/heads/%2:refs/heads/%3")
                              .arg(force ? QStringLiteral("+") : QString(),
                                   targetBranchName, remoteBranchName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);
  LOG_DEBUG("Using push refspec: '{}'", refspec.toStdString());
  LOG_DEBUG("Push source commit SHA: '{}'", localOidStr);

  do {
    gPushReferenceStatus = {};
    error = git_remote_push(remote, &refspecs, &pushOpts);

    if (error == 0 && gPushReferenceStatus.rejected) {
      LOG_ERROR(Log::ErrorId::Git, "Push rejected by remote for ref '{}': {}",
                gPushReferenceStatus.refname.toStdString(),
                gPushReferenceStatus.status.toStdString());
      error = GIT_EUSER;
      git_error_set_str(GIT_ERROR_NET,
                        gPushReferenceStatus.status.toUtf8().constData());
    }

    if (error) {
      setError(GitError::PushFailedToPushRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(urlStr);

  finishFetchProgress(pushUuid, error != 0, false);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_PERF(0, "Exit");

  return error == 0;
}

bool setUpstream(git_repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  int error = git_branch_lookup(
      &localRef, repo, localBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::SetUpstreamFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Like `git branch --set-upstream-to`, the remote has to exist.
  if (!remoteExists(repo, remoteName)) {
    setError(GitError::SetUpstreamRemoteNotFound, GIT_ENOTFOUND);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Try to set upstream using libgit2's function
  error = git_branch_set_upstream(
      localRef,
      QString("%1/%2").arg(remoteName, remoteBranchName).toStdString().c_str());

  // If it fails because the remote branch doesn't exist yet, the branch
  // configuration is written directly. This allows users to set upstream on
  // branches that will be created on the next push. The repository
  // configuration is used so that git's syntax, includes and linked worktrees
  // (which share the configuration of the main repository) are handled.
  if (error == GIT_ENOTFOUND || error == GIT_EINVALIDSPEC) {
    LOG_WARN("Remote branch '{}' does not exist, setting upstream directly in "
             "config",
             QString("%1/%2").arg(remoteName, remoteBranchName));

    git_config *config = nullptr;
    error = git_repository_config(&config, repo);
    if (!error) {
      const QString section =
          QStringLiteral("branch.%1.").arg(localBranchName);
      error = git_config_set_string(
          config, (section + QStringLiteral("remote")).toUtf8().constData(),
          remoteName.toUtf8().constData());
      if (!error) {
        error = git_config_set_string(
            config, (section + QStringLiteral("merge")).toUtf8().constData(),
            QStringLiteral("refs/heads/%1")
                .arg(remoteBranchName)
                .toUtf8()
                .constData());
      }
      git_config_free(config);
    }

    git_reference_free(localRef);
    if (error) {
      setError(GitError::SetUpstreamFailedToSetBranchUpstream, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    LOG_INFO("Set upstream for branch '{}' to '{}/{}' via config",
             localBranchName, remoteName, remoteBranchName);
    LOG_PERF(0, "Exit");
    return true;
  }

  if (error) {
    setError(GitError::SetUpstreamFailedToSetBranchUpstream, error);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(localRef);

  LOG_PERF(0, "Exit");

  return true;
}

std::tuple<bool, QString, QString> getUpstream(git_repository *repo,
                                               const QString &localBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_reference *localRef = nullptr;
  int error = git_branch_lookup(
      &localRef, repo, localBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::GetUpstreamFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_reference *upstreamRef = nullptr;
  error = git_branch_upstream(&upstreamRef, localRef);

  QString remoteName;
  QString remoteBranchName;

  if (error && error != GIT_ENOTFOUND) {
    setError(GitError::GetUpstreamFailedToGetBranchUpstream, error);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (error != GIT_ENOTFOUND) {
    // Successfully got upstream reference from libgit2
    const QString refName = git_reference_name(upstreamRef);
    LOG_DEBUG("Upstream reference: '{}'", refName.toStdString());

    // Parse "refs/remotes/{remoteName}/{remoteBranchName}"
    // remoteBranchName can contain slashes, so we need to split carefully
    const QString prefix = "refs/remotes/";
    if (refName.startsWith(prefix)) {
      const QString withoutPrefix = refName.mid(prefix.length());
      const int firstSlash = withoutPrefix.indexOf('/');
      if (firstSlash != -1) {
        remoteName = withoutPrefix.left(firstSlash);
        remoteBranchName = withoutPrefix.mid(firstSlash + 1);
        LOG_DEBUG("Parsed - Remote: '{}', Branch: '{}'",
                  remoteName.toStdString(), remoteBranchName.toStdString());
      }
    }
    git_reference_free(upstreamRef);
  } else {
    // Fallback: the remote branch doesn't exist (yet), read the branch
    // configuration. The repository configuration follows includes and is
    // shared by linked worktrees.
    git_config *config = nullptr;
    error = git_repository_config(&config, repo);
    if (error) {
      setError(GitError::GetUpstreamFailedToOpenGitConfig, error);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return {};
    }

    const QString section = QStringLiteral("branch.%1.").arg(localBranchName);
    git_buf value = GIT_BUF_INIT;
    if (git_config_get_string_buf(
            &value, config,
            (section + QStringLiteral("remote")).toUtf8().constData()) == 0) {
      remoteName = QString::fromUtf8(value.ptr);
    }
    git_buf_dispose(&value);
    if (git_config_get_string_buf(
            &value, config,
            (section + QStringLiteral("merge")).toUtf8().constData()) == 0) {
      const QString mergeRef = QString::fromUtf8(value.ptr);
      remoteBranchName = mergeRef.startsWith(QStringLiteral("refs/heads/"))
                             ? mergeRef.mid(11)
                             : mergeRef;
    }
    git_buf_dispose(&value);

    git_config_free(config);
  }

  git_reference_free(localRef);

  const bool configured = !remoteName.isEmpty() && !remoteBranchName.isEmpty();
  if (!configured) {
    setOwnError(GitError::GetUpstreamNotConfigured, GIT_ENOTFOUND);
  }

  LOG_PERF(0, "Exit");

  return std::make_tuple(configured, remoteName, remoteBranchName);
}

QVector<RemoteInfo> listRemotes(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Listing remotes");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  // Before listing: legacy targets look like remotes to libgit2 as well.
  migrateLegacyTargets(repo);

  git_strarray remote_list;
  int error = git_remote_list(&remote_list, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to list remotes: error={}", error);
    setError(GitError::ListRemotesFailedToListRemotes, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get default remote
  const QString defaultRemote = getDefaultRemote(repo);

  QVector<RemoteInfo> remotes;
  QSet<QString> processedRemotes;

  for (size_t i = 0; i < remote_list.count; ++i) {
    const QString remoteName = QString::fromUtf8(remote_list.strings[i]);

    const QStringList splitRemoteName = remoteName.split('.');

    if (splitRemoteName.isEmpty()) {
      LOG_DEBUG("Skipping empty remote name");
      continue;
    }

    if (splitRemoteName.length() >= 2 &&
        processedRemotes.contains(splitRemoteName[0]) &&
        splitRemoteName[1] == "target") {
      LOG_DEBUG("Skipping duplicate remote '{}'", remoteName.toStdString());
      continue;
    }

    processedRemotes.insert(splitRemoteName[0]);

    git_remote *remote = nullptr;
    error = git_remote_lookup(&remote, repo, remote_list.strings[i]);
    if (error) {
      LOG_DEBUG("Failed to lookup remote '{}': error={}",
                remoteName.toStdString(), error);
      continue;
    }

    const QString fetchUrl = QString::fromUtf8(git_remote_url(remote));
    const QString pushUrl = git_remote_pushurl(remote) != nullptr
                                ? QString::fromUtf8(git_remote_pushurl(remote))
                                : fetchUrl;

    // Create default target for backward compatibility
    QVector<RemoteTarget> targets;
    targets.append(RemoteTarget{QStringLiteral("Default"), fetchUrl, pushUrl});

    // Load additional targets from git config, sorted by name.
    git_config *config = nullptr;
    error = git_repository_config(&config, repo);
    if (!error && config) {
      QMap<QString, QPair<QString, QString>> targetMap;
      for (const TargetEntry &entry : readTargetEntries(config)) {
        if (isLegacyTargetEntry(entry) || entry.remote != remoteName) {
          continue;
        }
        if (entry.property == QStringLiteral("fetchurl")) {
          targetMap[entry.target].first = entry.value;
        } else {
          targetMap[entry.target].second = entry.value;
        }
      }

      for (auto it = targetMap.constBegin(); it != targetMap.constEnd();
           ++it) {
        const QString targetFetchUrl = it.value().first;
        const QString targetPushUrl =
            it.value().second.isEmpty() ? targetFetchUrl : it.value().second;
        if (!targetFetchUrl.isEmpty() && it.key() != "Default") {
          targets.append(RemoteTarget{it.key(), targetFetchUrl, targetPushUrl});
        }
      }
      git_config_free(config);
    }

    remotes.append(RemoteInfo{remoteName, fetchUrl, pushUrl,
                              remoteName == defaultRemote, targets});

    git_remote_free(remote);
  }

  git_strarray_dispose(&remote_list);

  LOG_DEBUG("Listed {} remotes", remotes.size());
  LOG_PERF(0, "Exit");

  return remotes;
}

QString getDefaultRemote(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting default remote");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return "";
  }

  // The remote chosen with setDefaultRemote, while it still exists.
  git_config *config = nullptr;
  if (git_repository_config(&config, repo) == 0) {
    git_buf configured = GIT_BUF_INIT;
    if (git_config_get_string_buf(&configured, config,
                                  "sourcerer.defaultremote") == 0 &&
        configured.ptr && configured.size > 0) {
      const QString remoteName = QString::fromUtf8(configured.ptr);
      git_remote *remote = nullptr;
      if (git_remote_lookup(&remote, repo, configured.ptr) == 0) {
        git_remote_free(remote);
        git_buf_dispose(&configured);
        git_config_free(config);
        LOG_DEBUG("Found configured default remote: '{}'",
                  remoteName.toStdString());
        LOG_PERF(0, "Exit");
        return remoteName;
      }
      LOG_DEBUG("Configured default remote '{}' no longer exists",
                remoteName.toStdString());
    }
    git_buf_dispose(&configured);
    git_config_free(config);
  }

  git_reference *ref = nullptr;

  // Try to get the upstream branch of the current branch
  git_reference *current_ref = nullptr;
  int error = git_repository_head(&current_ref, repo);

  if (error == 0 && current_ref != nullptr) {
    const char *shorthand = nullptr;
    error = git_branch_upstream(&ref, current_ref);

    if (error == 0 && ref != nullptr) {
      const char *upstream_name = git_reference_shorthand(ref);
      if (upstream_name != nullptr) {
        const QString upstream = QString::fromUtf8(upstream_name);
        const QStringList parts = upstream.split('/');
        if (parts.size() >= 2) {
          const QString remoteName = parts.first();
          LOG_DEBUG("Found upstream remote: '{}'", remoteName.toStdString());
          git_reference_free(ref);
          git_reference_free(current_ref);
          LOG_PERF(0, "Exit");
          return remoteName;
        }
      }
      git_reference_free(ref);
    }

    git_reference_free(current_ref);
  }

  // Fall back to "origin", or else the first remote by name, as long as it
  // exists. Without remotes there is no default.
  if (remoteExists(repo, QStringLiteral("origin"))) {
    LOG_DEBUG("Using fallback default remote: 'origin'");
    LOG_PERF(0, "Exit");
    return "origin";
  }

  QString firstRemote;
  git_strarray remoteNames = {nullptr, 0};
  if (git_remote_list(&remoteNames, repo) == 0) {
    for (size_t i = 0; i < remoteNames.count; ++i) {
      const QString name = QString::fromUtf8(remoteNames.strings[i]);
      if (name.contains(QLatin1String(LEGACY_TARGET_INFIX))) {
        continue;
      }
      if (firstRemote.isEmpty() || name < firstRemote) {
        firstRemote = name;
      }
    }
    git_strarray_dispose(&remoteNames);
  }

  LOG_DEBUG("Using fallback default remote: '{}'", firstRemote.toStdString());
  LOG_PERF(0, "Exit");
  return firstRemote;
}

bool setDefaultRemote(git_repository *repo, const QString &remoteName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Setting default remote to: '{}'", remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify the remote exists
  git_remote *remote = nullptr;
  int error = git_remote_lookup(&remote, repo, remoteName.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Remote '{}' not found: error={}",
              remoteName.toStdString(), error);
    setError(GitError::SetDefaultRemoteFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_remote_free(remote);

  // Set by updating configuration - set the push.default option or store in a
  // custom config We'll use a simple approach: store the default remote name in
  // a custom config variable
  git_config *config = nullptr;
  error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::SetDefaultRemoteFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_config_set_string(config, "sourcerer.defaultremote",
                                remoteName.toUtf8().constData());
  git_config_free(config);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set default remote in config: error={}", error);
    setError(GitError::SetDefaultRemoteFailedToWriteDefaultRemoteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully set default remote to: '{}'",
            remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool addRemote(git_repository *repo, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding remote '{}' with fetch URL: '{}'", remoteName.toStdString(),
            fetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name and fetch URL cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create the remote
  git_remote *remote = nullptr;
  int error = git_remote_create(&remote, repo, remoteName.toUtf8().constData(),
                                fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to create remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::AddRemoteFailedToCreateRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL if different from fetch URL
  if (!pushUrl.isEmpty() && pushUrl != fetchUrl) {
    error = git_remote_set_pushurl(repo, remoteName.toUtf8().constData(),
                                   pushUrl.toUtf8().constData());
    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to set push URL for remote '{}': error={}",
                remoteName.toStdString(), error);
      setError(GitError::AddRemoteFailedToSetPushUrl, error);
      git_remote_free(remote);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_remote_free(remote);

  LOG_DEBUG("Successfully added remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool removeRemote(git_repository *repo, const QString &remoteName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Removing remote: '{}'", remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = git_remote_delete(repo, remoteName.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to delete remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::RemoveRemoteFailedToDeleteRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *config = nullptr;
  if (git_repository_config(&config, repo) == 0) {
    moveTargets(config, remoteName, QString());
    git_config_free(config);
  }

  LOG_DEBUG("Successfully removed remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool updateRemote(git_repository *repo, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Updating remote '{}' -> '{}' with fetch URL: '{}'",
            remoteName.toStdString(), newRemoteName.toStdString(),
            fetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || newRemoteName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote names and fetch URL cannot be empty");
    setOwnError(GitError::UpdateRemoteInvalidArguments, GIT_EINVALID);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!remoteExists(repo, remoteName)) {
    setOwnError(GitError::UpdateRemoteNotFound, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  QString effectiveRemoteName = remoteName;
  if (newRemoteName != remoteName) {
    git_strarray problems = {0};
    const int renameError =
        git_remote_rename(&problems, repo, remoteName.toUtf8().constData(),
                          newRemoteName.toUtf8().constData());
    if (renameError) {
      LOG_ERROR(
          Log::ErrorId::Git, "Failed to rename remote '{}' to '{}': error={}",
          remoteName.toStdString(), newRemoteName.toStdString(), renameError);
      setError(GitError::UpdateRemoteFailedToRenameRemote, renameError);
      LOG_PERF(0, "Exit");
      return false;
    }
    effectiveRemoteName = newRemoteName;

    // The targets and the chosen default follow the rename.
    git_config *config = nullptr;
    if (git_repository_config(&config, repo) == 0) {
      migrateLegacyTargets(config);
      moveTargets(config, remoteName, newRemoteName);

      git_buf defaultRemote = GIT_BUF_INIT;
      if (git_config_get_string_buf(&defaultRemote, config,
                                    "sourcerer.defaultremote") == 0 &&
          QString::fromUtf8(defaultRemote.ptr) == remoteName) {
        git_config_set_string(config, "sourcerer.defaultremote",
                              newRemoteName.toUtf8().constData());
      }
      git_buf_dispose(&defaultRemote);
      git_config_free(config);
    }
  }

  // Set fetch URL
  int error = git_remote_set_url(repo, effectiveRemoteName.toUtf8().constData(),
                                 fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::UpdateRemoteFailedToSetFetchUrl, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL. libgit2 refuses an empty one, a null one removes it so that
  // pushes use the fetch URL.
  const QByteArray pushUrlUtf8 = pushUrl.toUtf8();
  error = git_remote_set_pushurl(
      repo, effectiveRemoteName.toUtf8().constData(),
      pushUrl.isEmpty() ? nullptr : pushUrlUtf8.constData());
  // Removing a push URL that isn't set is fine.
  if (error == GIT_ENOTFOUND && pushUrl.isEmpty()) {
    error = 0;
  }
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::UpdateRemoteFailedToSetPushUrl, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully updated remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool addTarget(git_repository *repo, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding target '{}' to remote '{}' with fetch URL: '{}'",
            targetName.toStdString(), remoteName.toStdString(),
            fetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Remote name, target name, and fetch URL cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Prevent overwriting Default target
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Cannot add target with name 'Default' - use updateRemote instead");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!remoteExists(repo, remoteName)) {
    LOG_ERROR(Log::ErrorId::Git, "Remote '{}' not found",
              remoteName.toStdString());
    setError(GitError::AddTargetRemoteNotFound, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::AddTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  migrateLegacyTargets(config);

  QString existingFetchUrl;
  QString existingPushUrl;
  if (readTarget(config, remoteName, targetName, &existingFetchUrl,
                 &existingPushUrl)) {
    LOG_ERROR(Log::ErrorId::Git, "Target '{}' exists on remote '{}'",
              targetName.toStdString(), remoteName.toStdString());
    setError(GitError::AddTargetAlreadyExists, GIT_EEXISTS);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool fetchFailed = false;
  error = writeTarget(config, remoteName, targetName, fetchUrl, pushUrl,
                      &fetchFailed);
  git_config_free(config);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to write target '{}' on remote '{}': error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(fetchFailed ? GitError::AddTargetFailedToSetFetchUrlConfig
                         : GitError::AddTargetFailedToSetPushUrlConfig,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully added target '{}' to remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool editTarget(git_repository *repo, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Editing target '{}' on remote '{}' with new fetch URL: '{}'",
            targetName.toStdString(), remoteName.toStdString(),
            newFetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty() || newFetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Remote name, target name, and fetch URL cannot be empty");
    setOwnError(GitError::EditTargetInvalidArguments, GIT_EINVALID);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Cannot edit Default target via this function
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot edit 'Default' target - use updateRemote instead");
    setOwnError(GitError::EditTargetInvalidArguments, GIT_EINVALID);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!remoteExists(repo, remoteName)) {
    LOG_ERROR(Log::ErrorId::Git, "Remote '{}' not found",
              remoteName.toStdString());
    setError(GitError::AddTargetRemoteNotFound, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::EditTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  migrateLegacyTargets(config);

  QString oldFetchUrl;
  QString oldPushUrl;
  if (!readTarget(config, remoteName, targetName, &oldFetchUrl, &oldPushUrl)) {
    LOG_ERROR(Log::ErrorId::Git, "Target '{}' not found on remote '{}'",
              targetName.toStdString(), remoteName.toStdString());
    setError(GitError::EditTargetNotFound, GIT_ENOTFOUND);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool fetchFailed = false;
  error = writeTarget(config, remoteName, targetName, newFetchUrl, newPushUrl,
                      &fetchFailed);
  git_config_free(config);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to write target '{}' on remote '{}': error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(fetchFailed ? GitError::EditTargetFailedToSetFetchUrlConfig
                         : GitError::EditTargetFailedToSetPushUrlConfig,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully edited target '{}' on remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool deleteTarget(git_repository *repo, const QString &remoteName,
                  const QString &targetName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Deleting target '{}' from remote '{}'", targetName.toStdString(),
            remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name and target name cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Cannot delete Default target via this function
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot delete 'Default' target - use removeRemote instead");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::DeleteTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  migrateLegacyTargets(config);

  QString oldFetchUrl;
  QString oldPushUrl;
  if (!readTarget(config, remoteName, targetName, &oldFetchUrl, &oldPushUrl)) {
    LOG_ERROR(Log::ErrorId::Git, "Target '{}' not found on remote '{}'",
              targetName.toStdString(), remoteName.toStdString());
    setError(GitError::DeleteTargetNotFound, GIT_ENOTFOUND);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_config_delete_entry(
      config, targetKey(remoteName, targetName, "fetchUrl").toUtf8().constData());
  if (error && error != GIT_ENOTFOUND) {
    LOG_WARN("Failed to delete fetch URL config for target '{}' on remote "
             "'{}': error={}",
             targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::DeleteTargetFailedToDeleteFetchUrlConfig, error);
  }

  error = git_config_delete_entry(
      config, targetKey(remoteName, targetName, "pushUrl").toUtf8().constData());
  if (error && error != GIT_ENOTFOUND) {
    LOG_WARN("Failed to delete push URL config for target '{}' on remote "
             "'{}': error={}",
             targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::DeleteTargetFailedToDeletePushUrlConfig, error);
  }

  git_config_free(config);

  LOG_DEBUG("Successfully deleted target '{}' from remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool setDefaultTarget(git_repository *repo, const QString &remoteName,
                      const QString &targetName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Setting default target '{}' for remote '{}'",
            targetName.toStdString(), remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name and target name cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (targetName == QStringLiteral("Default")) {
    LOG_DEBUG("Target '{}' is already the default", targetName.toStdString());
    LOG_PERF(0, "Exit");
    return true;
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::SetDefaultTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  migrateLegacyTargets(config);

  QString newFetchUrl;
  QString newPushUrl;
  if (!readTarget(config, remoteName, targetName, &newFetchUrl, &newPushUrl)) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Failed to get fetch URL for target '{}' on remote '{}': key not found",
        targetName.toStdString(), remoteName.toStdString());
    setError(GitError::EditTargetNotFound, GIT_ENOTFOUND);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, remoteName.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Remote '{}' not found: error={}",
              remoteName.toStdString(), error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }
  const QString currentFetchUrl = QString::fromUtf8(git_remote_url(remote));
  const QString currentPushUrl = git_remote_pushurl(remote)
                                     ? QString::fromUtf8(git_remote_pushurl(remote))
                                     : QString();
  git_remote_free(remote);

  // The target and the remote swap their URLs, so the previous default stays
  // available as this target.
  const QByteArray remoteNameUtf8 = remoteName.toUtf8();
  const QByteArray newPushUrlUtf8 = newPushUrl.toUtf8();
  error = git_remote_set_url(repo, remoteNameUtf8.constData(),
                             newFetchUrl.toUtf8().constData());
  if (!error) {
    error = git_remote_set_pushurl(
        repo, remoteNameUtf8.constData(),
        newPushUrl.isEmpty() ? nullptr : newPushUrlUtf8.constData());
    if (error == GIT_ENOTFOUND && newPushUrl.isEmpty()) {
      error = 0;
    }
  }
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set default fetch URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::SetDefaultTargetFailedToSetDefaultFetchUrl, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool fetchFailed = false;
  error = writeTarget(config, remoteName, targetName, currentFetchUrl,
                      currentPushUrl, &fetchFailed);
  if (error) {
    LOG_WARN("Failed to keep the previous default of remote '{}' as target "
             "'{}': error={}",
             remoteName.toStdString(), targetName.toStdString(), error);
  }

  git_config_free(config);

  LOG_DEBUG("Successfully set default target to '{}' for remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

} // namespace GitRepo
