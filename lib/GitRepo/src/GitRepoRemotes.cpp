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
  if (urlStr.startsWith("git@") || urlStr.startsWith("git://")) {
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
      setError(GitError::PullFailedToAbortRebase, error);
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
  if (urlStr.startsWith("git@") || urlStr.startsWith("git://")) {
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

  for (const QString &remoteName : remotesToFetch) {
    if (!fetchRemoteInternal(repo, remoteName)) {
      continue;
    }
  }

  LOG_PERF(0, "Exit");
  return true;
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
  if (urlStr.startsWith("git@") || urlStr.startsWith("git://")) {
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

  // Try to set upstream using libgit2's function
  error = git_branch_set_upstream(
      localRef,
      QString("%1/%2").arg(remoteName, remoteBranchName).toStdString().c_str());

  // If it fails because the remote branch doesn't exist yet, set it manually in
  // config This allows users to set upstream on branches that will be created
  // on the next push
  if (error == GIT_ENOTFOUND || error == GIT_EINVALIDSPEC) {
    LOG_WARN("Remote branch '{}' does not exist, setting upstream directly in "
             "config",
             QString("%1/%2").arg(remoteName, remoteBranchName));

    // Get the git config file path
    const QString configPath =
        QString::fromUtf8(git_repository_path(repo)) + "config";

    // Read the existing config file
    QString configContent;
    QFile configFile(configPath);
    if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      configContent = QString::fromUtf8(configFile.readAll());
      configFile.close();
    }

    // Parse and update the config
    QStringList lines = configContent.split('\n');
    bool foundBranchSection = false;
    bool foundRemote = false;
    bool foundMerge = false;
    int sectionStart = -1;
    int sectionEnd = -1;

    // Find the branch section
    for (int i = 0; i < lines.size(); ++i) {
      const QString line = lines[i].trimmed();

      if (line == QString("[branch \"%1\"]").arg(localBranchName)) {
        foundBranchSection = true;
        sectionStart = i;

        // Find the end of this section (next [section] or end of file)
        for (int j = i + 1; j < lines.size(); ++j) {
          const QString nextLine = lines[j].trimmed();
          if (nextLine.startsWith('[')) {
            sectionEnd = j - 1;
            break;
          }
        }
        if (sectionEnd == -1) {
          sectionEnd = lines.size() - 1;
        }

        // Look for remote and merge entries in this section
        for (int j = sectionStart + 1; j <= sectionEnd; ++j) {
          const QString sectionLine = lines[j].trimmed();
          if (sectionLine.startsWith("remote =")) {
            lines[j] = QString("\tremote = %1").arg(remoteName);
            foundRemote = true;
          } else if (sectionLine.startsWith("merge =")) {
            lines[j] = QString("\tmerge = refs/heads/%1").arg(remoteBranchName);
            foundMerge = true;
          }
        }
        break;
      }
    }

    // Add missing entries to the section or create the section
    if (foundBranchSection) {
      // Insert missing entries at the end of the section (before sectionEnd)
      int insertPos = sectionEnd + 1;
      if (!foundRemote) {
        lines.insert(insertPos++, QString("\tremote = %1").arg(remoteName));
      }
      if (!foundMerge) {
        lines.insert(insertPos,
                     QString("\tmerge = refs/heads/%1").arg(remoteBranchName));
      }
    } else {
      // Create new section at the end of the file
      if (!configContent.isEmpty() && !configContent.endsWith('\n')) {
        lines.append(""); // Empty line before new section
      }
      lines.append(QString("[branch \"%1\"]").arg(localBranchName));
      lines.append(QString("\tremote = %1").arg(remoteName));
      lines.append(QString("\tmerge = refs/heads/%1").arg(remoteBranchName));
    }

    // Write back to config file
    configFile.setFileName(configPath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to write .git/config file when setting upstream");
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    configFile.write(lines.join('\n').toUtf8());
    configFile.close();

    LOG_INFO("Set upstream for branch '{}' to '{}/{}' via config file",
             localBranchName, remoteName, remoteBranchName);

    git_reference_free(localRef);
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
    // Fallback: git_branch_upstream returned GIT_ENOTFOUND, so read from git
    // config by iterating through all entries
    const QString configPath = QString::fromUtf8(git_repository_workdir(repo)) +
                               ".git" + QDir::separator() + "config";

    git_config *config = nullptr;
    error = git_config_open_ondisk(&config, configPath.toStdString().c_str());
    if (error) {
      setError(GitError::GetUpstreamFailedToOpenGitConfig, error);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return {};
    }

    const QString remoteConfigKey =
        QString("branch.%1.remote").arg(localBranchName);
    const QString mergeConfigKey =
        QString("branch.%1.merge").arg(localBranchName);

    // Iterate through config entries to find the keys we're looking for
    git_config_iterator *iterator = nullptr;
    error = git_config_iterator_new(&iterator, config);
    if (!error) {
      git_config_entry *entry = nullptr;
      while (!git_config_next(&entry, iterator)) {
        if (entry && entry->name && entry->value) {
          const QString entryName = QString::fromUtf8(entry->name);
          const QString entryValue = QString::fromUtf8(entry->value);

          if (entryName == remoteConfigKey) {
            remoteName = entryValue;
          } else if (entryName == mergeConfigKey) {
            QString mergeRef = entryValue;
            if (mergeRef.startsWith("refs/heads/")) {
              remoteBranchName =
                  mergeRef.mid(11); // Remove "refs/heads/" prefix
            } else {
              remoteBranchName = mergeRef;
            }
          }
        }
      }
      git_config_iterator_free(iterator);
    }

    git_config_free(config);
  }

  git_reference_free(localRef);

  LOG_PERF(0, "Exit");

  return std::make_tuple(!remoteName.isEmpty() && !remoteBranchName.isEmpty(),
                         remoteName, remoteBranchName);
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

    // Load additional targets from git config
    git_config *config = nullptr;
    error = git_repository_config(&config, repo);
    if (!error && config) {
      git_config_iterator *iterator = nullptr;
      error = git_config_iterator_new(&iterator, config);
      if (!error) {
        git_config_entry *entry = nullptr;
        const QString targetPrefix =
            QString("remote.%1.target.").arg(remoteName);

        // Map to collect targets: targetName -> (fetchUrl, pushUrl)
        QMap<QString, QPair<QString, QString>> targetMap;

        while (!git_config_next(&entry, iterator)) {
          if (entry && entry->name && entry->value) {
            const QString entryName = QString::fromUtf8(entry->name);
            const QString entryValue = QString::fromUtf8(entry->value);

            // Check if this entry is for a target of this remote
            if (entryName.startsWith(targetPrefix)) {
              // Extract target name and property (fetchUrl or pushUrl)
              const QString suffix = entryName.mid(targetPrefix.length());
              const QStringList parts = suffix.split('.');

              if (parts.size() >= 2) {
                const QString targetName = parts[0];
                const QString property = parts[1];

                if (!targetMap.contains(targetName)) {
                  targetMap[targetName] = {QString(), QString()};
                }

                if (property == "fetchurl") {
                  targetMap[targetName].first = entryValue;
                } else if (property == "pushurl") {
                  targetMap[targetName].second = entryValue;
                }
              }
            }
          }
        }

        // Add all collected targets
        for (auto it = targetMap.constBegin(); it != targetMap.constEnd();
             ++it) {
          const QString targetName = it.key();
          const QString targetFetchUrl = it.value().first;
          const QString targetPushUrl =
              it.value().second.isEmpty() ? targetFetchUrl : it.value().second;

          if (!targetFetchUrl.isEmpty() && targetName != "Default") {
            targets.append(
                RemoteTarget{targetName, targetFetchUrl, targetPushUrl});
          }
        }

        git_config_iterator_free(iterator);
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

  // Fall back to "origin" if upstream not found
  LOG_DEBUG("Using fallback default remote: 'origin'");
  LOG_PERF(0, "Exit");
  return "origin";
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

  // Set push URL
  error = git_remote_set_pushurl(repo, effectiveRemoteName.toUtf8().constData(),
                                 pushUrl.toUtf8().constData());
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

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::AddTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set fetch URL
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  error = git_config_set_string(config, fetchConfigKey.toUtf8().constData(),
                                fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::AddTargetFailedToSetFetchUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_set_string(config, pushConfigKey.toUtf8().constData(),
                                pushUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::AddTargetFailedToSetPushUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config_free(config);

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
    LOG_PERF(0, "Exit");
    return false;
  }

  // Cannot edit Default target via this function
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot edit 'Default' target - use updateRemote instead");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::EditTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set fetch URL
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  error = git_config_set_string(config, fetchConfigKey.toUtf8().constData(),
                                newFetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::EditTargetFailedToSetFetchUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_set_string(config, pushConfigKey.toUtf8().constData(),
                                newPushUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::EditTargetFailedToSetPushUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config_free(config);

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

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::DeleteTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Delete fetch URL
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  error = git_config_delete_entry(config, fetchConfigKey.toUtf8().constData());
  if (error && error != GIT_ENOTFOUND) {
    LOG_WARN("Failed to delete fetch URL config for target '{}' on remote "
             "'{}': error={}",
             targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::DeleteTargetFailedToDeleteFetchUrlConfig, error);
  }

  // Delete push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_delete_entry(config, pushConfigKey.toUtf8().constData());
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

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::SetDefaultTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get the new default target's fetch and push URLs
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);

  git_config_iterator *iter = nullptr;

  error = git_config_iterator_new(&iter, config);
  if (error) {
    setError(GitError::SetDefaultTargetFailedToCreateConfigIterator, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Find the actual stored keys (they might be lowercase due to git config
  // normalization)
  char *newFetchUrl = nullptr;
  char *newPushUrl = nullptr;
  git_config_entry *entry = nullptr;

  const QString targetPrefix =
      QString("remote.%1.target.%2.").arg(remoteName, targetName);

  while ((error = git_config_next(&entry, iter)) == 0) {
    if (entry && entry->name) {
      const QString entryName = QString::fromUtf8(entry->name);
      LOG_DEBUG("Config entry: {}", entryName.toStdString());

      if (entryName.startsWith(targetPrefix, Qt::CaseInsensitive)) {
        const QString suffix = entryName.mid(targetPrefix.length());
        if (suffix.compare("fetchUrl", Qt::CaseInsensitive) == 0) {
          newFetchUrl = strdup(entry->value);
          LOG_DEBUG("Found fetch URL: {}", newFetchUrl ? newFetchUrl : "null");
        } else if (suffix.compare("pushUrl", Qt::CaseInsensitive) == 0) {
          newPushUrl = strdup(entry->value);
          LOG_DEBUG("Found push URL: {}", newPushUrl ? newPushUrl : "null");
        }
      }
    }
  }

  git_config_iterator_free(iter);

  if (!newFetchUrl) {
    if (newPushUrl) {
      free(newPushUrl);
    }
    LOG_ERROR(
        Log::ErrorId::Git,
        "Failed to get fetch URL for target '{}' on remote '{}': key not found",
        targetName.toStdString(), remoteName.toStdString());
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // If push URL not found, use fetch URL as fallback
  if (!newPushUrl) {
    newPushUrl = newFetchUrl;
    LOG_DEBUG("Push URL not set, using fetch URL as fallback");
  }

  // Save current default target's URLs before switching
  char *currentFetchUrl = nullptr;
  char *currentPushUrl = nullptr;

  // Iterate config to find current fetch and push URLs
  git_config_iterator *defaultIter = nullptr;
  error = git_config_iterator_new(&defaultIter, config);
  if (error) {
    LOG_WARN("Failed to create config iterator for default URLs: error={}",
             error);
  } else {
    git_config_entry *defaultEntry = nullptr;
    const QString defaultPrefix = QString("remote.%1.").arg(remoteName);

    while ((error = git_config_next(&defaultEntry, defaultIter)) == 0) {
      if (defaultEntry && defaultEntry->name) {
        const QString entryName = QString::fromUtf8(defaultEntry->name);

        if (entryName.startsWith(defaultPrefix, Qt::CaseInsensitive)) {
          const QString suffix = entryName.mid(defaultPrefix.length());
          if (suffix.compare("url", Qt::CaseInsensitive) == 0) {
            currentFetchUrl = strdup(defaultEntry->value);
            LOG_DEBUG("Found current fetch URL: {}",
                      currentFetchUrl ? currentFetchUrl : "null");
          } else if (suffix.compare("pushurl", Qt::CaseInsensitive) == 0) {
            currentPushUrl = strdup(defaultEntry->value);
            LOG_DEBUG("Found current push URL: {}",
                      currentPushUrl ? currentPushUrl : "null");
          }
        }
      }
    }
    git_config_iterator_free(defaultIter);
  }

  if (!currentFetchUrl) {
    if (newFetchUrl) {
      free(newFetchUrl);
    }
    if (newPushUrl) {
      free(newPushUrl);
    }
    if (currentPushUrl) {
      free(currentPushUrl);
    }

    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get current fetch URL for remote '{}': key not found",
              remoteName.toStdString());
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set the old default target as a named target
  const QString oldFetchKey =
      QStringLiteral("remote.%1.target.Default.fetchUrl").arg(remoteName);
  error = git_config_set_string(config, oldFetchKey.toUtf8().constData(),
                                currentFetchUrl);
  if (error) {
    LOG_WARN("Failed to backup old default target for remote '{}': error={}",
             remoteName.toStdString(), error);
  }

  if (currentPushUrl) {
    const QString oldPushKey =
        QStringLiteral("remote.%1.target.Default.pushUrl").arg(remoteName);
    git_config_set_string(config, oldPushKey.toUtf8().constData(),
                          currentPushUrl);
  }

  // Set the new target as the default
  const QString defaultFetchKey =
      QStringLiteral("remote.%1.url").arg(remoteName);
  error = git_config_set_string(config, defaultFetchKey.toUtf8().constData(),
                                newFetchUrl);
  if (error) {
    if (newFetchUrl) {
      free(newFetchUrl);
    }
    if (newPushUrl) {
      free(newPushUrl);
    }
    if (currentFetchUrl) {
      free(currentFetchUrl);
    }
    if (currentPushUrl) {
      free(currentPushUrl);
    }

    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set default fetch URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::SetDefaultTargetFailedToSetDefaultFetchUrl, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString defaultPushKey =
      QStringLiteral("remote.%1.pushurl").arg(remoteName);
  error = git_config_set_string(config, defaultPushKey.toUtf8().constData(),
                                newPushUrl);
  if (error) {
    LOG_WARN("Failed to set default push URL for remote '{}': error={}",
             remoteName.toStdString(), error);
    // Continue even if push URL setting fails
  }

  if (newFetchUrl) {
    free(newFetchUrl);
  }
  if (newPushUrl) {
    free(newPushUrl);
  }
  if (currentFetchUrl) {
    free(currentFetchUrl);
  }
  if (currentPushUrl) {
    free(currentPushUrl);
  }

  git_config_free(config);

  LOG_DEBUG("Successfully set default target to '{}' for remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

} // namespace GitRepo
