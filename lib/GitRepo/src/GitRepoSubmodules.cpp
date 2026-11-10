// MIT License
// Copyright (c) 2026 Sourcerer

#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSet>
#include <QVector>

#include <string>
#include <tuple>

#define THREAD_ID "GitRepo"

// Submodules.
namespace GitRepo {

using namespace Private;

struct ListSubmodulesPayload {
  QStringList submodules;
};

static int listSubmodulesCallback(git_submodule *sm, const char *name,
                                  void *payload) {
  if (!payload) {
    return 0;
  }

  auto *listPayload = static_cast<ListSubmodulesPayload *>(payload);

  // Leave out a submodule that is only in HEAD, e.g. one deleted but not
  // committed yet. One whose directory is missing is still listed.
  unsigned int location = 0;
  if (git_submodule_location(&location, sm) != 0 ||
      !(location & (GIT_SUBMODULE_STATUS_IN_INDEX |
                    GIT_SUBMODULE_STATUS_IN_CONFIG |
                    GIT_SUBMODULE_STATUS_IN_WD))) {
    return 0;
  }

  listPayload->submodules += name;

  return 0;
}

QStringList listSubmodules(git_repository *repo) {
  LOG_PERF(0, "Enter");

  ListSubmodulesPayload payload;

  const int error =
      git_submodule_foreach(repo, &listSubmodulesCallback, &payload);
  if (error) {
    setError(GitError::ListSubmodulesFailedToIterateSubmodules, error);
    return {};
  }

  LOG_PERF(0, "Exit");

  return payload.submodules;
}

QVector<SubmoduleInfo> listSubmodulesWithStatus(git_repository *repo,
                                                const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QVector<SubmoduleInfo> submodulesWithStatus;

  // Get all submodules first
  const QStringList submodules = listSubmodules(repo);

  for (const QString &submoduleName : submodules) {
    if (quitFlag) {
      break;
    }

    SubmoduleInfo info;
    info.name = submoduleName;
    info.statusInfo.status = SubmoduleStatusType::Unknown;

    // Try to get submodule status
    git_submodule *submodule = nullptr;
    if (git_submodule_lookup(&submodule, repo,
                             submoduleName.toStdString().c_str()) != 0) {
      info.statusInfo.status = SubmoduleStatusType::Error;
      submodulesWithStatus.push_back(info);
      continue;
    }

    // Get the expected commit (from parent repository's index)
    const git_oid *headId = git_submodule_head_id(submodule);

    // Get the actual commit (from submodule's working directory)
    const git_oid *workdirId = git_submodule_wd_id(submodule);

    if (workdirId) {
      const char *workdirIdStr = git_oid_tostr_s(workdirId);
      if (workdirIdStr) {
        info.commit = QString::fromUtf8(workdirIdStr);
      }
    }

    if (!headId || !workdirId) {
      info.statusInfo.status = SubmoduleStatusType::Error;
      git_submodule_free(submodule);
      submodulesWithStatus.push_back(info);
      continue;
    }

    if (git_oid_equal(headId, workdirId)) {
      info.statusInfo.status = SubmoduleStatusType::UpToDate;
    } else {
      info.statusInfo.status = SubmoduleStatusType::Outdated;
    }

    git_submodule_free(submodule);
    submodulesWithStatus.push_back(info);
  }

  LOG_PERF(0, "Exit");

  return submodulesWithStatus;
}

QString getSubmodulePath(git_repository *repo, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_submodule *submodule = nullptr;

  if (const int error = git_submodule_lookup(
          &submodule, repo, submoduleName.toStdString().c_str())) {
    setError(GitError::GetSubmodulePathFailedToLookupSubmodule, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  const char *path = git_submodule_path(submodule);
  if (!path) {
    git_submodule_free(submodule);
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString pathStr = path;

  git_submodule_free(submodule);

  LOG_PERF(0, "Exit");

  return pathStr;
}

// Parses a config section header such as `[submodule "a \"b\""]` into its
// section (lower case) and subsection. Returns false for any other line.
static bool parseConfigSectionHeader(const QByteArray &line, QByteArray &section,
                                     QByteArray &subsection,
                                     QByteArray &rest) {
  const QByteArray trimmed = line.trimmed();
  if (!trimmed.startsWith('[')) {
    return false;
  }

  qsizetype i = 1;
  while (i < trimmed.size() && trimmed[i] != ']' && trimmed[i] != '"' &&
         trimmed[i] != ' ' && trimmed[i] != '\t') {
    section += trimmed[i++];
  }
  section = section.toLower();

  while (i < trimmed.size() && (trimmed[i] == ' ' || trimmed[i] == '\t')) {
    ++i;
  }

  if (i < trimmed.size() && trimmed[i] == '"') {
    ++i;
    while (i < trimmed.size() && trimmed[i] != '"') {
      if (trimmed[i] == '\\' && i + 1 < trimmed.size()) {
        ++i;
      }
      subsection += trimmed[i++];
    }
    if (i >= trimmed.size()) {
      return false;
    }
    ++i;
  }

  if (i >= trimmed.size() || trimmed[i] != ']') {
    return false;
  }

  rest = trimmed.mid(i + 1).trimmed();
  return true;
}

// Removes the section `submodule.<name>` from the config file `path`: every
// variable through libgit2, then the headers left without variables.
static bool removeSubmoduleConfigSection(const QString &path,
                                         const QByteArray &name) {
  if (!QFileInfo::exists(path)) {
    return true;
  }

  git_config *config = nullptr;
  int error = git_config_open_ondisk(&config, path.toUtf8().constData());
  if (error) {
    setError(GitError::DeleteSubmoduleFailedToUpdateConfig, error);
    return false;
  }

  // Variable names are `submodule.<name>.<key>`; the name may contain dots,
  // the key doesn't.
  const QByteArray prefix = "submodule." + name + ".";
  QSet<QByteArray> keys;
  git_config_iterator *it = nullptr;
  error = git_config_iterator_new(&it, config);
  if (!error) {
    git_config_entry *entry = nullptr;
    while ((error = git_config_next(&entry, it)) == 0) {
      const QByteArray key(entry->name);
      if (key.startsWith(prefix) && !key.mid(prefix.size()).contains('.')) {
        keys.insert(key);
      }
    }
    git_config_iterator_free(it);
    if (error == GIT_ITEROVER) {
      error = 0;
    }
  }

  for (auto key = keys.cbegin(); !error && key != keys.cend(); ++key) {
    error = git_config_delete_multivar(config, key->constData(), ".*");
  }

  git_config_free(config);

  if (error) {
    setError(GitError::DeleteSubmoduleFailedToUpdateConfig, error);
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    setError(GitError::DeleteSubmoduleFailedToUpdateConfig, GIT_ERROR);
    return false;
  }
  const QList<QByteArray> lines = file.readAll().split('\n');
  file.close();

  // Drop the headers of the section that are followed by nothing but blank
  // lines and comments.
  QList<QByteArray> kept;
  bool changed = false;
  for (qsizetype i = 0; i < lines.size(); ++i) {
    QByteArray section;
    QByteArray subsection;
    QByteArray rest;
    if (parseConfigSectionHeader(lines[i], section, subsection, rest) &&
        section == "submodule" && subsection == name && rest.isEmpty()) {
      qsizetype next = i + 1;
      bool empty = true;
      for (; next < lines.size(); ++next) {
        const QByteArray trimmed = lines[next].trimmed();
        if (trimmed.startsWith('[')) {
          break;
        }
        if (!trimmed.isEmpty() && !trimmed.startsWith('#') &&
            !trimmed.startsWith(';')) {
          empty = false;
          break;
        }
      }

      if (empty) {
        // Keep the comments, drop the header and the blank lines.
        for (qsizetype j = i + 1; j < next; ++j) {
          if (!lines[j].trimmed().isEmpty()) {
            kept += lines[j];
          }
        }
        changed = true;
        i = next - 1;
        continue;
      }
    }

    kept += lines[i];
  }

  if (!changed) {
    return true;
  }

  // A file that ended with a newline still does.
  if (!lines.isEmpty() && lines.last().isEmpty() &&
      (kept.isEmpty() || !kept.last().isEmpty())) {
    kept += QByteArray();
  }

  QSaveFile out(path);
  if (!out.open(QIODevice::WriteOnly) || out.write(kept.join('\n')) < 0 ||
      !out.commit()) {
    setError(GitError::DeleteSubmoduleFailedToUpdateConfig, GIT_ERROR);
    return false;
  }

  return true;
}

bool deleteSubmodule(git_repository *repo, const QString &submoduleName,
                     const QString &submodulePath) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *workdir = git_repository_workdir(repo);
  const char *commonDir = git_repository_commondir(repo);
  if (!workdir || !commonDir) {
    setError(GitError::DeleteSubmoduleFailedToUpdateConfig, GIT_EBAREREPO);
    LOG_PERF(0, "Exit");
    return false;
  }

  // The name ends up in config section names and in .git/modules/<name>; an
  // empty or path-like name would reach other submodules or the whole
  // modules directory.
  const QString trimmedName = submoduleName.trimmed();
  if (trimmedName.isEmpty() || trimmedName != submoduleName ||
      QDir::isAbsolutePath(submoduleName) ||
      QDir::cleanPath(submoduleName).startsWith(QStringLiteral(".."))) {
    setOwnError(GitError::DeleteSubmoduleInvalidName, GIT_EINVALID);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QByteArray name = submoduleName.toUtf8();
  const QString workdirPath = QString::fromUtf8(workdir);
  const QDir workdirDir(workdirPath);

  // A path inside the working copy, relative to it; empty for the working
  // copy itself and for paths outside it.
  const auto insideWorkdir = [&](const QString &path) -> QString {
    if (path.trimmed().isEmpty()) {
      return {};
    }
    const QString relative = workdirDir.relativeFilePath(
        QDir::cleanPath(workdirDir.absoluteFilePath(path)));
    if (relative.isEmpty() || relative == QStringLiteral(".") ||
        relative.startsWith(QStringLiteral("..")) ||
        QDir::isAbsolutePath(relative)) {
      return {};
    }
    return relative;
  };

  // The index entry and the checkout are at the submodule's path, which can
  // differ from its name. Without metadata (e.g. after discarding
  // .gitmodules), derive it from the given directory.
  QString relativePath;
  bool known = false;
  git_submodule *submodule = nullptr;
  if (git_submodule_lookup(&submodule, repo, name.constData()) == 0) {
    relativePath = insideWorkdir(QString::fromUtf8(git_submodule_path(submodule)));
    known = true;
    git_submodule_free(submodule);
  } else {
    LOG_DEBUG(
        "Submodule '{}' not found in metadata, proceeding with manual cleanup",
        submoduleName.toStdString());
    relativePath = insideWorkdir(submodulePath);
    if (relativePath.isEmpty()) {
      relativePath = insideWorkdir(submoduleName);
    }
  }

  const QString gitModulesPath = workdirPath + ".gitmodules";
  if (QFileInfo::exists(gitModulesPath)) {
    if (!removeSubmoduleConfigSection(gitModulesPath, name)) {
      LOG_PERF(0, "Exit");
      return false;
    }

    if (const bool staged = stageFiles(repo, QStringList() << ".gitmodules");
        !staged) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (!removeSubmoduleConfigSection(
          QString::fromUtf8(commonDir) + QStringLiteral("config"), name)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    setError(GitError::DeleteSubmoduleFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Try to remove from index, but don't fail if it's not there
  // (user may have already discarded it)
  error = relativePath.isEmpty()
              ? GIT_ENOTFOUND
              : git_index_remove(index, relativePath.toUtf8().constData(), 0);
  if (error && error != GIT_ENOTFOUND) {
    setError(GitError::DeleteSubmoduleFailedToRemoveIndexEntry, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_index_write(index);
  git_index_free(index);
  if (error) {
    setError(GitError::DeleteSubmoduleFailedToWriteIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Only the submodule's own checkout is removed: a directory inside the
  // working copy that is the submodule's path, or (without metadata) that is
  // a git checkout. Never the working copy or anything outside it.
  if (!relativePath.isEmpty()) {
    const QString checkout = workdirDir.filePath(relativePath);
    const bool isCheckout =
        QFileInfo::exists(QDir(checkout).filePath(QStringLiteral(".git")));
    if (known || isCheckout) {
      QDir(checkout).removeRecursively();
    } else if (QFileInfo::exists(checkout)) {
      LOG_WARN("Not removing '{}', it is not a submodule checkout",
               checkout.toStdString());
    }
  }
  QDir(QString("%1modules/%2").arg(QString::fromUtf8(commonDir), submoduleName))
      .removeRecursively();

  LOG_PERF(0, "Exit");

  return true;
}

// Fetches `branchName` from `remoteName` into `submoduleRepo`, then checks out
// a local branch of the same name tracking the fetched remote branch (never
// leaving the submodule in a detached HEAD state). Submodules must always
// track a branch, whether it was chosen explicitly when the submodule was
// added, or resolved automatically as a sensible default.
static bool checkoutSubmoduleBranch(git_repository *submoduleRepo,
                                    const QString &repoPath,
                                    const QString &remoteName,
                                    const QString &branchName,
                                    const QString &uuid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(submoduleRepo != nullptr);

  if (!submoduleRepo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_remote *remote = nullptr;
  int error = git_remote_lookup(&remote, submoduleRepo,
                                remoteName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *remoteUrl = git_remote_url(remote);
  const QString urlStr = remoteUrl ? QString::fromUtf8(remoteUrl) : QString();
  LOG_DEBUG("Checking out submodule branch '{}' from {}/{} ({})",
            branchName.toStdString(), remoteName.toStdString(),
            branchName.toStdString(), urlStr.toStdString());

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials = &credentialsAcquireCallback;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  const QString refspec =
      QString("+refs/heads/%1:refs/remotes/%2/%1").arg(branchName, remoteName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  int retries = 0;
  const int maxRetries = MAX_RETRIES;

  do {
    error = git_remote_fetch(remote, &refspecs, &fetchOpts, nullptr);

    if (error) {
      setError(GitError::CheckoutSubmoduleBranchFailedToFetchRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(urlStr);
  finishFetchProgress(uuid, error != 0, false);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  if (error) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString remoteRefName =
      QString("refs/remotes/%1/%2").arg(remoteName, branchName);

  git_reference *remoteRef = nullptr;
  error = git_reference_lookup(&remoteRef, submoduleRepo,
                               remoteRefName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToLookupReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                             remoteRef, GIT_OBJECT_COMMIT);
  git_reference_free(remoteRef);
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToPeelReferenceToCommit,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Check out the branch's files before moving the branch and HEAD. The
  // checkout compares with the current HEAD, so checking out HEAD after
  // moving it would keep the files of the branch the clone checked out.
  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy =
      GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;
  // Submodule checkouts are routed through libgit2's filter pipeline, which
  // can hit an internal writer lifecycle assertion on some repositories.
  // Avoid filters here so submodule operations stay reliable.
  checkoutOpts.disable_filters = true;

  error = git_checkout_tree(submoduleRepo,
                            reinterpret_cast<git_object *>(targetCommit),
                            &checkoutOpts);
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToCheckoutHead, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  error = git_branch_lookup(&localRef, submoduleRepo,
                            branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error == GIT_ENOTFOUND) {
    error =
        git_branch_create(&localRef, submoduleRepo,
                          branchName.toStdString().c_str(), targetCommit, 0);
  }

  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToCreateLocalBranch, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *updatedLocalRef = nullptr;
  error = git_reference_set_target(&updatedLocalRef, localRef,
                                   git_commit_id(targetCommit),
                                   "Update submodule branch");
  git_reference_free(localRef);
  localRef = updatedLocalRef;
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToReadCommitId, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(localRef);
  git_commit_free(targetCommit);

  const QString headRefName = QString("refs/heads/%1").arg(branchName);
  error =
      git_repository_set_head(submoduleRepo, headRefName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToSetHeadToBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  setUpstream(submoduleRepo, remoteName, branchName, branchName);

  LOG_PERF(0, "Exit");

  return true;
}

static bool checkoutLocalSubmoduleBranch(git_repository *submoduleRepo,
                                         const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(submoduleRepo != nullptr);

  if (!submoduleRepo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  int error =
      git_branch_lookup(&localRef, submoduleRepo,
                        branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::CheckoutLocalSubmoduleBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString headRefName = QString("refs/heads/%1").arg(branchName);
  error =
      git_repository_set_head(submoduleRepo, headRefName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutLocalSubmoduleBranchFailedToSetHeadToBranch,
             error);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy =
      GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;
  // Submodule checkouts are routed through libgit2's filter pipeline, which
  // can hit an internal writer lifecycle assertion on some repositories.
  // Avoid filters here so submodule operations stay reliable.
  checkoutOpts.disable_filters = true;

  error = git_checkout_head(submoduleRepo, &checkoutOpts);
  git_reference_free(localRef);
  if (error) {
    setError(GitError::CheckoutLocalSubmoduleBranchFailedToCheckoutHead, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool updateSubmodule(git_repository *repo, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string name = submoduleName.toStdString();

  git_submodule *submodule = nullptr;
  git_repository *submoduleRepo = nullptr;

  auto cleanup = [&]() {
    if (submoduleRepo) {
      git_repository_free(submoduleRepo);
      submoduleRepo = nullptr;
    }

    if (submodule) {
      git_submodule_free(submodule);
      submodule = nullptr;
    }
  };

  auto fail = [&](const int error) {
    if (error) {
      setError(GitError::UpdateSubmoduleFailedToUpdateSubmodule, error);
    }

    cleanup();
    LOG_PERF(0, "Exit");
    return false;
  };

  int error = git_submodule_lookup(&submodule, repo, name.c_str());
  if (error) {
    return fail(error);
  }

  error = git_submodule_open(&submoduleRepo, submodule);
  if (error) {
    return fail(error);
  }

  // Stage the current local submodule HEAD into the parent repository index.
  // This uses the submodule's working tree state, not the remote branch tip.
  error = git_submodule_add_to_index(submodule, 1);
  if (error) {
    setError(GitError::UpdateSubmoduleFailedToAddSubmoduleToIndex, error);
    cleanup();
    LOG_PERF(0, "Exit");
    return false;
  }

  cleanup();

  LOG_PERF(0, "Exit");

  return true;
}

bool addSubmodule(git_repository *repo, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *repoPath = git_repository_path(repo);
  if (!repoPath) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool gitSchema = isSshOrGitUrl(url);
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  // Checked before anything is written.
  if (!commercial && gitSchema) {
    setError(GitError::AddSubmoduleGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git, "Adding submodules to git@ repositories is "
                                 "not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  // A submodule of the same name is kept as it is; the add is refused.
  git_submodule *existing = nullptr;
  if (git_submodule_lookup(&existing, repo, name.toStdString().c_str()) == 0) {
    git_submodule_free(existing);
    setError(GitError::AddSubmoduleAlreadyExists, GIT_EEXISTS);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_submodule *subModule = nullptr;

  int error =
      git_submodule_add_setup(&subModule, repo, url.toStdString().c_str(),
                              name.toStdString().c_str(), 1);
  if (error) {
    setError(GitError::AddSubmoduleFailedToSetupSubmodule, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository *subRepo = nullptr;

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy = GIT_CHECKOUT_SAFE;
  // Submodule clone also uses checkout filters, which can trigger an internal
  // libgit2 writer assertion on some repositories.
  checkoutOpts.disable_filters = true;
  checkoutOpts.progress_payload = &progressData;
  checkoutOpts.progress_cb = &checkoutProgressCallback;

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  git_submodule_update_options cloneOpts = GIT_SUBMODULE_UPDATE_OPTIONS_INIT;
  cloneOpts.fetch_opts = fetchOpts;
  cloneOpts.checkout_opts = checkoutOpts;

  error = git_submodule_clone(&subRepo, subModule, &cloneOpts);
  if (error) {
    setError(GitError::AddSubmoduleFailedToCloneSubmodule, error);
    finishCredentialsAcquiring(url);
    finishFetchProgress(uuid, true, false);
    if (subRepo) {
      git_repository_free(subRepo);
    }
    // Clean up the .gitmodules entry that was added by git_submodule_add_setup
    deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Send completion event to hide progress toast
  // Dismiss the credentials overlay first (same url key used for the widget);
  // finishFetchProgress sleeps and would otherwise keep the overlay open.
  finishCredentialsAcquiring(url);

  // This is needed even if no progress callbacks were fired (e.g., local path
  // submodules)
  finishFetchProgress(uuid, error != 0, false);

  // Submodules must always track a branch. Use the branch chosen in the add
  // submodule dialog; if none was provided, fall back to whatever branch the
  // clone above already checked out (the remote's default branch).
  QString trackedBranch = branch.trimmed();
  if (trackedBranch.isEmpty()) {
    git_reference *head = nullptr;
    if (!git_repository_head(&head, subRepo)) {
      const char *currentBranch = nullptr;
      if (!git_branch_name(&currentBranch, head) && currentBranch &&
          currentBranch[0]) {
        trackedBranch = QString::fromUtf8(currentBranch);
      }
      git_reference_free(head);
    }
  }

  if (trackedBranch.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Can't determine a branch to track for submodule '{}'",
              name.toStdString());
    git_repository_free(subRepo);
    deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  QString subRepoPath;
  if (const char *workdir = git_repository_workdir(subRepo)) {
    subRepoPath = QString::fromUtf8(workdir);
  }

  if (!checkoutSubmoduleBranch(subRepo, subRepoPath, "origin", trackedBranch,
                               uuid)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to check out tracked branch '{}' for submodule '{}'",
              trackedBranch.toStdString(), name.toStdString());
    git_repository_free(subRepo);
    deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Persist the tracked branch in .gitmodules so future updates keep
  // tracking the same branch.
  git_submodule_set_branch(repo, name.toStdString().c_str(),
                           trackedBranch.toStdString().c_str());

  error = git_submodule_add_finalize(subModule);
  if (error) {
    setError(GitError::AddSubmoduleFailedToFinalizeSubmoduleAdd, error);
    git_repository_free(subRepo);
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository_free(subRepo);
  git_submodule_free(subModule);

  LOG_PERF(0, "Exit");

  return true;
}

} // namespace GitRepo
