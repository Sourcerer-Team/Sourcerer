// MIT License
// Copyright (c) 2026 Sourcerer

#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include "GitLfs.h"
#include "PatchFileUtils.h"

#include <Log.h>

#include <git2/sys/errors.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QVector>

#include <string>

#define THREAD_ID "GitRepo"

// Working copy: status, staging, commits, discarding and resetting.
namespace GitRepo {

using namespace Private;

static bool isLfsTrackedPath(git_repository *repo,
                             const QString &relativePath) {
  if (!repo || relativePath.isEmpty()) {
    return false;
  }

  const QByteArray pathUtf8 = QDir::fromNativeSeparators(relativePath).toUtf8();
  const char *value = nullptr;
  if (git_attr_get(&value, repo,
                   GIT_ATTR_CHECK_NO_SYSTEM | GIT_ATTR_CHECK_FILE_THEN_INDEX,
                   pathUtf8.constData(), "filter") != 0 ||
      !value) {
    return false;
  }

  return QString::fromUtf8(value).compare(QStringLiteral("lfs"),
                                          Qt::CaseInsensitive) == 0;
}

static bool stageLfsTrackedFile(git_repository *repo, git_index *index,
                                const QString &relativePath,
                                const QString &fullPath, QString *stagedOid) {
  QFile file(fullPath);
  if (!file.open(QIODevice::ReadOnly)) {
    git_error_set_str(GIT_ERROR_OS, "unable to read lfs source file");
    return false;
  }

  const QByteArray content = file.readAll();
  file.close();

  LOG_DEBUG("Staging LFS file '{}' from '{}' ({} bytes)",
            relativePath.toStdString(), fullPath.toStdString(), content.size());

  QString oid;
  if (!GitRepo::Lfs::storeObject(repo, content, oid)) {
    return false;
  }

  if (stagedOid) {
    *stagedOid = oid;
  }

  const QByteArray pointer = GitRepo::Lfs::makePointer(oid, content.size());
  git_index_entry entry = {};
  const QByteArray relativeUtf8 = relativePath.toUtf8();
  entry.path = relativeUtf8.constData();
  entry.mode = GIT_FILEMODE_BLOB;
  const int error = git_index_add_from_buffer(
      index, &entry, pointer.constData(), pointer.size());
  if (error) {
    setError(GitError::StageFilesFailedToAddPathToIndex, error);
    return false;
  }

  LOG_DEBUG("Staged LFS pointer for '{}' with oid '{}'",
            relativePath.toStdString(), oid.toStdString());
  return true;
}

bool hasConflicts(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *index_ptr = nullptr;
  int error = git_repository_index(&index_ptr, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get repository index when checking for conflicts: "
              "error={}",
              error);
    setError(GitError::HasConflictsFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool hasConflicts_result = git_index_has_conflicts(index_ptr) != 0;
  git_index_free(index_ptr);

  LOG_DEBUG("Repository has conflicts: {}", hasConflicts_result);
  LOG_PERF(0, "Exit");
  return hasConflicts_result;
}

bool checkoutCommit(git_repository *repo, const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Checking out commit: '{}'", commit.toStdString());

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid oid;
  git_commit *commit_ = nullptr;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to parse commit OID '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::CheckoutCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup commit '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::CheckoutCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Commit '{}' found, retrieving tree", commit.toStdString());

  git_tree *tree = nullptr;
  error = git_commit_tree(&tree, commit_);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get tree from commit: error={}",
              error);
    setError(GitError::CheckoutCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Checking out tree for commit '{}'", commit.toStdString());
  git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
  opts.checkout_strategy = GIT_CHECKOUT_SAFE;

  error = git_checkout_tree(repo, reinterpret_cast<const git_object *>(tree),
                            &opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to checkout tree: error={}", error);
    setError(GitError::CheckoutCommitFailedToCheckoutCommitTree, error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Setting HEAD to detached state at commit '{}'",
            commit.toStdString());
  error = git_repository_set_head_detached(repo, &oid);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set HEAD detached to commit '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::CheckoutCommitFailedToDetachHead, error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully checked out commit '{}'", commit.toStdString());
  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return true;
}

GitStatus getStatus(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting repository status");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  GitStatus status = {};

  const QString repoPath =
      QDir::fromNativeSeparators(git_repository_workdir(repo));
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_status_options opts;
  git_status_options_init(&opts, GIT_STATUS_OPTIONS_VERSION);
  opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
  opts.flags =
      GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
      GIT_STATUS_OPT_INCLUDE_IGNORED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
      GIT_STATUS_OPT_SORT_CASE_SENSITIVELY | GIT_STATUS_OPT_INCLUDE_UNREADABLE;

  git_status_list *statusList = nullptr;

  if (const int error = git_status_list_new(&statusList, repo, &opts)) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to create status list: error={}",
              error);
    setError(GitError::GetStatusFailedToCreateStatusList, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  size_t count = git_status_list_entrycount(statusList);
  LOG_DEBUG("Found {} status entries", count);

  if (quitFlag) {
    LOG_DEBUG("Repository status loading cancelled");
    git_status_list_free(statusList);
    LOG_PERF(0, "Exit");
    return {};
  }

  for (size_t i = 0; i < count; ++i) {
    if (quitFlag) {
      LOG_DEBUG("Repository status loading cancelled");
      break;
    }

    const git_status_entry *entry = git_status_byindex(statusList, i);

    Q_ASSERT(entry != nullptr);

    if (!entry) {
      continue;
    }

    const git_diff_delta *delta =
        entry->head_to_index ? entry->head_to_index : entry->index_to_workdir;
    const char *filePath = nullptr;
    if (delta) {
      filePath =
          delta->new_file.path ? delta->new_file.path : delta->old_file.path;
    }

    Q_ASSERT(filePath != nullptr);

    if (!filePath) {
      continue;
    }

    QString unixFilePath = QDir::fromNativeSeparators(filePath);
    const QString fullPath = repoPath + unixFilePath;

    if (QFileInfo(fullPath).isDir()) {
      if (QDir(fullPath)
              .entryList(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot)
              .isEmpty()) {
        // Skip empty directories
        continue;
      }
    }

    if (unixFilePath.endsWith('/')) {
      unixFilePath = unixFilePath.left(unixFilePath.length() - 1);
    }

    const unsigned int status_flags = entry->status;

    if ((status_flags & GIT_STATUS_WT_NEW) != 0) {
      status.newFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_MODIFIED) != 0) {
      status.modifiedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_DELETED) != 0) {
      status.deletedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_TYPECHANGE) != 0) {
      status.typeChangedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_RENAMED) != 0) {
      status.renamedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_UNREADABLE) != 0) {
      status.unreadableFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_NEW) != 0) {
      status.newStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_MODIFIED) != 0) {
      status.modifiedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_DELETED) != 0) {
      status.deletedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_TYPECHANGE) != 0) {
      status.typeChangedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_RENAMED) != 0) {
      status.renamedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_IGNORED) != 0) {
      status.ignoredFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_CONFLICTED) != 0) {
      status.conflictedFiles += unixFilePath;
    }
  }

  git_status_list_free(statusList);

  LOG_PERF(0, "Exit");

  return status;
}

bool stageFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Staging {} files", files.size());

  int fileIndex = 0;
  for (const auto &file : files) {
    LOG_DEBUG("  File {}: '{}'", fileIndex++, file.toStdString());
  }

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository index: error={}",
              error);
    setError(GitError::StageFilesFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString repoPath =
      QDir::fromNativeSeparators(git_repository_workdir(repo));
  LOG_DEBUG("Repository working directory: '{}'", repoPath.toStdString());

  for (const auto &filePath : files) {
    const QString unixFilePath = QDir::fromNativeSeparators(filePath);
    const QString fullPath = QDir(repoPath).filePath(unixFilePath);
    const QString relativePath = QDir(repoPath).relativeFilePath(fullPath);
    LOG_DEBUG("Processing file: '{}' (relative: '{}')", fullPath.toStdString(),
              relativePath.toStdString());

    if (!QFileInfo::exists(fullPath)) {
      LOG_DEBUG("File does not exist, removing from index: '{}'",
                relativePath.toStdString());
      error =
          git_index_remove_bypath(index, relativePath.toStdString().c_str());
      if (error) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Failed to remove file from index '{}': error={}",
                  relativePath.toStdString(), error);
        setError(GitError::StageFilesFailedToRemovePathFromIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      continue;
    }

    // Only a directory can be a submodule. Probing a file for <file>/.git
    // fails for paths near MAX_PATH.
    if (!QFileInfo(fullPath).isDir()) {
      error = git_index_add_bypath(index, relativePath.toStdString().c_str());
      if (error) {
        setError(GitError::StageFilesFailedToAddPathToIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      continue;
    }

    git_submodule *submodule = nullptr;
    error = git_submodule_lookup(&submodule, repo,
                                 relativePath.toStdString().c_str());
    if (!error) {
      LOG_DEBUG("Path is a submodule, staging gitlink: '{}'",
                relativePath.toStdString());
      error = git_submodule_add_to_index(submodule, 0);
      git_submodule_free(submodule);
      if (error) {
        setError(GitError::StageFilesFailedToAddSubmoduleToIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else if (error == GIT_ENOTFOUND) {
      // A plain directory, stage its content recursively.
      giterr_clear();
      git_strarray pathspec =
          qstringlist_to_git_strarray(QStringList() << relativePath);
      error = git_index_add_all(index, &pathspec, GIT_INDEX_ADD_DEFAULT,
                                nullptr, nullptr);
      git_strarray_destroy(pathspec);
      if (error) {
        setError(GitError::StageFilesFailedToAddPathToIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else {
      setError(GitError::StageFilesFailedToAddMissingPathToIndex, error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_index_write(index);
  if (error) {
    setError(GitError::StageFilesFailedToWriteIndex, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  LOG_PERF(0, "Exit");

  return true;
}

bool unstageFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Nothing to unstage, git_reset_default() rejects an empty path list.
  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return true;
  }

  git_object *headCommit = nullptr;

  // Lookup HEAD commit
  int error = git_revparse_single(&headCommit, repo, "HEAD");
  if (error) {
    setError(GitError::UnstageFilesFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList unixFiles;

  for (const auto &filePath : files) {
    unixFiles += QDir::fromNativeSeparators(filePath);
  }

  git_strarray arr = qstringlist_to_git_strarray(unixFiles);

  // Reset index entries for selected paths to match HEAD
  error = git_reset_default(repo, headCommit, &arr);
  if (error) {
    setError(GitError::UnstageFilesFailedToResetPathsToHead, error);
    git_object_free(headCommit);
    git_strarray_destroy(arr);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(headCommit);
  git_strarray_destroy(arr);

  LOG_PERF(0, "Exit");

  return true;
}

bool commitStaged(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Creating commit: summary='{}', author='{}', email='{}'",
            summary.toStdString(), author.toStdString(), email.toStdString());
  LOG_DEBUG("Message length: {}, cleanup: {}, signoff: {}", message.length(),
            cleanup, signoff);

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Preparing commit: loading index and building tree");
  git_index *index = nullptr;
  git_oid treeId;
  git_oid commitId;
  git_tree *tree = nullptr;
  git_signature *sig = nullptr;
  git_commit *parent = nullptr;
  git_commit *parent2 = nullptr;
  bool isMerge = false;

  // Check if merge is in progress
  if (git_repository_state(repo) == GIT_REPOSITORY_STATE_MERGE) {
    isMerge = true;
    LOG_DEBUG("Merge in progress, will create merge commit with 2 parents");
  }

  // Load index
  int error = git_repository_index(&index, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository index: error={}",
              error);
    setError(GitError::CommitStagedFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write index to tree
  error = git_index_write_tree(&treeId, index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to write index to tree: error={}",
              error);
    setError(GitError::CommitStagedFailedToWriteIndexTree, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeId);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup tree: error={}", error);
    setError(GitError::CommitStagedFailedToLookupTree, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Tree prepared successfully");

  // Create signature
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CommitStagedFailedToCreateSignature, error);
    git_tree_free(tree);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Lookup HEAD commit (if it exists)
  error = git_repository_head_unborn(repo);
  if (!error) {
    git_reference *headRef = nullptr;
    error = git_repository_head(&headRef, repo);
    if (error) {
      setError(GitError::CommitStagedFailedToGetRepositoryHead, error);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_commit_lookup(&parent, repo, git_reference_target(headRef));
    if (error) {
      setError(GitError::CommitStagedFailedToGetReferenceTarget, error);
      git_reference_free(headRef);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(headRef);

    // If merge in progress, get the MERGE_HEAD as the second parent
    if (isMerge) {
      git_reference *mergeHeadRef = nullptr;
      error = git_reference_dwim(&mergeHeadRef, repo, "MERGE_HEAD");
      if (error) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Failed to read MERGE_HEAD during merge: error={}", error);
        setError(GitError::CommitStagedFailedToResolveMergeHeadReference,
                 error);
        git_commit_free(parent);
        git_signature_free(sig);
        git_tree_free(tree);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      error =
          git_commit_lookup(&parent2, repo, git_reference_target(mergeHeadRef));
      if (error) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Failed to lookup MERGE_HEAD commit: error={}", error);
        setError(GitError::CommitStagedFailedToGetSecondReferenceTarget, error);
        git_reference_free(mergeHeadRef);
        git_commit_free(parent);
        git_signature_free(sig);
        git_tree_free(tree);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_reference_free(mergeHeadRef);
      LOG_DEBUG("Successfully loaded MERGE_HEAD as second parent");
    }
  }

  const QString fullMessage =
      buildCommitMessage(summary, message, author, email, signoff);

  // Create commit with appropriate number of parents
  size_t parentCount = parent ? 1 : 0;
  if (isMerge && parent2) {
    parentCount = 2;
    LOG_DEBUG("Creating merge commit with 2 parents");
  }

  CommitSigningConfig signingConfig;
  QString signingError;
  bool signingConfigLoaded = true;
  if (signCommit) {
    signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::CommitStagedFailedToLoadSigningConfiguration, -1);

      if (parent) {
        git_commit_free(parent);
      }
      if (parent2) {
        git_commit_free(parent2);
      }

      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);

      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const bool doSigning = signCommit && signingConfig.enabled;
  if (doSigning) {
    CommitSigningPayload signingPayload;
    signingPayload.config = signingConfig;
    signingPayload.errorMessage = &signingError;

    git_commit_create_options commitOpts = GIT_COMMIT_CREATE_OPTIONS_INIT;
    commitOpts.allow_empty_commit = 1;
    commitOpts.author = sig;
    commitOpts.committer = sig;
    commitOpts.sign = commitSigningCallback;
    commitOpts.payload = &signingPayload;

    error = git_commit_create_from_stage(
        &commitId, repo, fullMessage.toStdString().c_str(), &commitOpts);
    if (error) {
      if (!signingError.isEmpty()) {
        LOG_ERROR(Log::ErrorId::Git, "Failed to sign commit: {}",
                  signingError.toStdString());
        setError(GitError::CommitStagedFailedToSignCommit, -1);
      } else {
        setError(GitError::CommitStagedFailedToCreateCommitFromStagedChanges,
                 error);
      }

      if (parent) {
        git_commit_free(parent);
      }
      if (parent2) {
        git_commit_free(parent2);
      }

      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);

      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    error = git_commit_create_v(&commitId, repo, "HEAD", sig, sig, nullptr,
                                fullMessage.toStdString().c_str(), tree,
                                parentCount, parent, parent2);
    if (error) {
      setError(GitError::CommitStagedFailedToCreateCommit, error);

      if (parent) {
        git_commit_free(parent);
      }
      if (parent2) {
        git_commit_free(parent2);
      }

      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);

      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (parent) {
    git_commit_free(parent);
  }
  if (parent2) {
    git_commit_free(parent2);
  }

  git_signature_free(sig);
  git_tree_free(tree);
  git_index_free(index);

  if (cleanup) {
    error = git_repository_state_cleanup(repo);
    if (error) {
      setError(GitError::CommitStagedFailedToCleanupRepositoryState, error);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool discardFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  for (const auto &nativeFile : files) {
    const QString file = QDir::fromNativeSeparators(nativeFile);

    git_object *headTree = nullptr;
    int error = git_revparse_single(&headTree, repo, "HEAD^{tree}");
    if (error) {
      setError(GitError::DiscardFilesFailedToResolveRevision, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // A file that is not in HEAD is only unstaged. Checking out HEAD over it
    // would delete it from disk.
    git_tree_entry *headEntry = nullptr;
    error = git_tree_entry_bypath(&headEntry,
                                  reinterpret_cast<git_tree *>(headTree),
                                  file.toStdString().c_str());
    if (error == GIT_ENOTFOUND) {
      giterr_clear();
      git_object_free(headTree);

      git_index *index = nullptr;
      error = git_repository_index(&index, repo);
      if (error) {
        setError(GitError::DiscardFilesFailedToGetRepositoryIndex, error);
        LOG_PERF(0, "Exit");
        return false;
      }

      error = git_index_remove_bypath(index, file.toStdString().c_str());
      if (!error) {
        error = git_index_write(index);
      }
      git_index_free(index);
      if (error) {
        setError(GitError::DiscardFilesFailedToRemoveDeletedIndexEntry, error);
        LOG_PERF(0, "Exit");
        return false;
      }

      continue;
    }
    git_tree_entry_free(headEntry);

    git_strarray arr = qstringlist_to_git_strarray(QStringList() << file);

    // 1. Reset working directory to HEAD
    git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
    checkoutOpts.checkout_strategy =
        GIT_CHECKOUT_FORCE | GIT_CHECKOUT_DISABLE_PATHSPEC_MATCH;
    checkoutOpts.paths = arr;

    error = git_checkout_tree(repo, headTree, &checkoutOpts);
    if (error) {
      setError(GitError::DiscardFilesFailedToCheckoutTree, error);
      git_strarray_destroy(arr);
      git_object_free(headTree);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_strarray_destroy(arr);
    git_object_free(headTree);

    // 2. Reset index entry to HEAD
    git_index *index = nullptr;
    error = git_repository_index(&index, repo);
    if (error) {
      setError(GitError::DiscardFilesFailedToGetRepositoryIndex, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object *headTree2 = nullptr;
    error = git_revparse_single(&headTree2, repo, "HEAD^{tree}");
    if (error) {
      setError(GitError::DiscardFilesFailedToResolveHeadRevision, error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_tree *tree = nullptr;
    error = git_tree_lookup(&tree, repo, git_object_id(headTree2));
    if (error) {
      setError(GitError::DiscardFilesFailedToGetHeadTreeObjectId, error);
      git_object_free(headTree2);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    const std::string path = file.toStdString();

    git_tree_entry *entry = nullptr;
    error = git_tree_entry_bypath(&entry, tree, path.c_str());
    if (error) {
      setError(GitError::DiscardFilesFailedToLookupTreeEntryByPath, error);
      git_tree_free(tree);
      git_object_free(headTree2);
      git_index_free(index);
      continue;
    }

    if (entry) {
      git_blob *blob = nullptr;
      error = git_blob_lookup(&blob, repo, git_tree_entry_id(entry));
      if (error) {
        setError(GitError::DiscardFilesFailedToReadTreeEntryId, error);
        git_tree_entry_free(entry);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      const QString repoPath = git_repository_workdir(repo);
      const QString fullPath =
          QDir::fromNativeSeparators(repoPath + QString::fromStdString(path));

      const QString directoryPath = QFileInfo(fullPath).path();

      QDir fileDir;
      fileDir.mkpath(directoryPath);

      // The content as a checkout writes it, through the filters (LFS,
      // line endings), not the raw blob.
      git_buf filtered = GIT_BUF_INIT;
      git_blob_filter_options filterOpts = GIT_BLOB_FILTER_OPTIONS_INIT;
      filterOpts.flags &= ~GIT_BLOB_FILTER_CHECK_FOR_BINARY;
      error = git_blob_filter(&filtered, blob, path.c_str(), &filterOpts);
      git_blob_free(blob);
      if (error) {
        setError(GitError::DiscardFilesFailedToCheckoutTree, error);
        git_tree_entry_free(entry);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      QFile file(fullPath);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR(Log::ErrorId::Git, "Can't open file \"{}\" for writing",
                  file.fileName());
        git_buf_dispose(&filtered);
        git_tree_entry_free(entry);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      file.write(filtered.ptr, static_cast<qint64>(filtered.size));
      file.close();

      git_buf_dispose(&filtered);

      // Capture entry data before freeing
      git_index_entry newEntry = {};
      newEntry.mode = git_tree_entry_filemode(entry);
      newEntry.id = *git_tree_entry_id(entry);
      newEntry.path = path.c_str();

      git_tree_entry_free(entry);

      error = git_index_remove(index, path.c_str(), 0); // Remove old entry
      if (error) {
        setError(GitError::DiscardFilesFailedToRemoveIndexEntry, error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      error = git_index_add(index, &newEntry); // Add fresh entry
      if (error) {
        setError(GitError::DiscardFilesFailedToAddIndexEntry, error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else {
      error = git_index_remove(index, path.c_str(), 0); // Remove if not in HEAD
      if (error) {
        setError(GitError::DiscardFilesFailedToRemoveDeletedIndexEntry, error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    git_tree_free(tree);
    git_object_free(headTree2);

    error = git_index_write(index);
    if (error) {
      setError(GitError::DiscardFilesFailedToWriteIndex, error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_index_free(index);
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool resetToCommit(git_repository *repo, const QString &commit,
                   ResetType type) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Resetting to commit '{}' with type {}", commit.toStdString(),
            static_cast<int>(type));

  git_object *target = nullptr;

  int error = git_revparse_single(&target, repo, commit.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to parse commit '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::ResetToCommitFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Target commit '{}' parsed successfully", commit.toStdString());

  switch (type) {
  case ResetType::Hard: {
    LOG_DEBUG("Performing HARD reset to commit '{}'", commit.toStdString());
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    error = git_reset(repo, target, GIT_RESET_HARD, &opts);
    break;
  }
  case ResetType::Mixed: {
    LOG_DEBUG("Performing MIXED reset to commit '{}'", commit.toStdString());
    error = git_reset(repo, target, GIT_RESET_MIXED, nullptr);
    break;
  }
  case ResetType::Soft: {
    LOG_DEBUG("Performing SOFT reset to commit '{}'", commit.toStdString());
    error = git_reset(repo, target, GIT_RESET_SOFT, nullptr);
    break;
  }
  default: {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Unsupported reset type: {}",
              static_cast<int>(type));
    error = GIT_EINVALIDSPEC;
    break;
  }
  }

  git_object_free(target);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Reset operation failed: error={}", error);
    setError(GitError::ResetToCommitFailedToResetRepository, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully reset to commit '{}'", commit.toStdString());
  LOG_PERF(0, "Exit");

  return true;
}

static QStringList createPatchTargetFiles(const QString &patchContent) {
  QStringList files;
  const QStringList lines =
      QString(patchContent).replace("\r\n", "\n").split('\n');
  for (const QString &line : lines) {
    if (line.startsWith("+++ ")) {
      const QString path = line.mid(4).trimmed();
      if (path != "/dev/null" && path.startsWith("b/")) {
        files.append(path.mid(2));
      }
    }
  }
  files.removeDuplicates();
  return files;
}

bool applyPatch(git_repository *repo, const QString &patchContent) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString workdirPath = QString::fromUtf8(git_repository_workdir(repo));
  // git_apply creates the files the patch adds itself and refuses to write
  // over existing ones, so only stub the files the patch modifies.
  QStringList targetFiles = patchTargetFiles(patchContent);
  for (const QString &newFile : patchNewFiles(patchContent)) {
    targetFiles.removeAll(newFile);
  }
  QStringList createdFiles;
  QString errorMessage;

  if (!ensureFilesExistForPatch(workdirPath, targetFiles, createdFiles,
                                &errorMessage)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "{}", errorMessage.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  git_diff *diff = nullptr;
  const QByteArray buffer = patchContent.toUtf8();

  int error = git_diff_from_buffer(&diff, buffer.constData(), buffer.size());
  if (error) {
    setError(GitError::ApplyPatchFailedToParsePatchBuffer, error);
    removeFiles(createdFiles);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_apply_options opts = GIT_APPLY_OPTIONS_INIT;
  error = git_apply(repo, diff, GIT_APPLY_LOCATION_WORKDIR, &opts);
  if (error) {
    setError(GitError::ApplyPatchFailedToApplyPatch, error);
    git_diff_free(diff);
    removeFiles(createdFiles);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_diff_free(diff);

  LOG_PERF(0, "Exit");

  return true;
}

QString listConflictsForAFile(git_repository *repo, const QString &filePath) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    setError(GitError::ListConflictsForAFileFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_index_conflict_iterator *iterator = nullptr;

  error = git_index_conflict_iterator_new(&iterator, index);
  if (error) {
    setError(GitError::ListConflictsForAFileFailedToCreateConflictIterator,
             error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return {};
  }

  QString fileContentWithMarkers;

  const std::string filePathStr = filePath.toStdString();

  const git_index_entry *ancestor = nullptr;
  const git_index_entry *ours = nullptr;
  const git_index_entry *theirs = nullptr;

  while (!git_index_conflict_next(&ancestor, &ours, &theirs, iterator)) {
    if (ours == nullptr || ours->path != filePathStr || theirs == nullptr ||
        theirs->path != filePathStr) {
      continue;
    }

    git_blob *blobAncestor = nullptr;
    git_blob *blobOurs = nullptr;
    git_blob *blobTheirs = nullptr;
    git_merge_file_input inputAncestor = GIT_MERGE_FILE_INPUT_INIT;
    git_merge_file_input inputOurs = GIT_MERGE_FILE_INPUT_INIT;
    git_merge_file_input inputTheirs = GIT_MERGE_FILE_INPUT_INIT;
    git_merge_file_options opts = GIT_MERGE_FILE_OPTIONS_INIT;
    git_merge_file_result result = {0};

    if (ancestor) {
      error = git_blob_lookup(&blobAncestor, repo, &ancestor->id);
      if (error) {
        continue;
      }

      inputAncestor.ptr =
          static_cast<const char *>(git_blob_rawcontent(blobAncestor));
      inputAncestor.size = git_blob_rawsize(blobAncestor);
      inputAncestor.path = "file";
      inputAncestor.mode = GIT_FILEMODE_BLOB;
    }

    error = git_blob_lookup(&blobOurs, repo, &ours->id);
    if (error) {
      git_blob_free(blobAncestor);
      continue;
    }

    inputOurs.ptr = static_cast<const char *>(git_blob_rawcontent(blobOurs));
    inputOurs.size = git_blob_rawsize(blobOurs);
    inputOurs.path = "file";
    inputOurs.mode = GIT_FILEMODE_BLOB;

    error = git_blob_lookup(&blobTheirs, repo, &theirs->id);
    if (error) {
      git_blob_free(blobOurs);
      git_blob_free(blobAncestor);
      continue;
    }

    inputTheirs.ptr =
        static_cast<const char *>(git_blob_rawcontent(blobTheirs));
    inputTheirs.size = git_blob_rawsize(blobTheirs);
    inputTheirs.path = "file";
    inputTheirs.mode = GIT_FILEMODE_BLOB;

    opts.flags =
        GIT_MERGE_FILE_STYLE_MERGE; // produces <<<<<<<, =======, >>>>>>>
    opts.favor = GIT_MERGE_FILE_FAVOR_NORMAL;

    // Perform merge
    error = git_merge_file(&result, &inputAncestor, &inputOurs, &inputTheirs,
                           &opts);
    if (error) {
      git_blob_free(blobTheirs);
      git_blob_free(blobOurs);
      git_blob_free(blobAncestor);
      continue;
    }

    fileContentWithMarkers +=
        QString::fromUtf8(result.ptr, static_cast<int>(result.len));

    git_merge_file_result_free(&result);
    git_blob_free(blobTheirs);
    git_blob_free(blobOurs);
    git_blob_free(blobAncestor);

    break;
  }

  git_index_conflict_iterator_free(iterator);
  git_index_free(index);

  LOG_PERF(0, "Exit");

  return fileContentWithMarkers;
}

bool getIsMergeInProgress(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const int state = git_repository_state(repo);

  LOG_PERF(0, "Exit");

  return state == GIT_REPOSITORY_STATE_MERGE ||
         state == GIT_REPOSITORY_STATE_REVERT ||
         state == GIT_REPOSITORY_STATE_CHERRYPICK;
  ;
}

bool abortMerge(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // A cherry-pick or revert that stopped with conflicts is aborted the same
  // way, see getIsMergeInProgress.
  if (!getIsMergeInProgress(repo)) {
    setOwnError(GitError::AbortMergeNothingToAbort, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = git_repository_state_cleanup(repo);
  if (error) {
    setError(GitError::AbortMergeFailedToCleanupRepositoryState, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object *head = NULL;

  error = git_revparse_single(&head, repo, "HEAD");
  if (error) {
    setError(GitError::AbortMergeFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reset(repo, head, GIT_RESET_HARD, NULL);
  if (error) {
    setError(GitError::AbortMergeFailedToResetRepository, error);
    git_object_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(head);

  LOG_PERF(0, "Exit");

  return true;
}

bool amend(git_repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *commitToAmend = nullptr;
  git_oid commitOid;
  git_oid newCommitOid;
  git_tree *tree = nullptr;
  const git_signature *authorSig = nullptr;
  git_signature *committerSig = nullptr;

  // 1. Get the HEAD commit (the one to amend)
  int error = git_reference_name_to_id(&commitOid, repo, "HEAD");
  if (error) {
    setError(GitError::AmendFailedToResolveReferenceNameToId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commitToAmend, repo, &commitOid);
  if (error) {
    setError(GitError::AmendFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // 2. Get the tree to commit (your staged changes)
  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    setError(GitError::AmendFailedToGetRepositoryIndex, error);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid treeOid;

  error = git_index_write_tree(&treeOid, index);
  if (error) {
    setError(GitError::AmendFailedToWriteIndexTree, error);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeOid);
  if (error) {
    setError(GitError::AmendFailedToLookupTree, error);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  // 3. Create committer signature (or use nullptr to keep original)
  error = git_signature_now(&committerSig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::AmendFailedToCreateSignature, error);
    git_tree_free(tree);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  authorSig = git_commit_author(commitToAmend);

  // 4. Amend the commit with the updated message and optional signoff
  const QString fullMessage =
      buildCommitMessage(summary, message, author, email, signoff);

  if (signCommit) {
    QVector<git_commit *> parents;
    const unsigned int parentCount = git_commit_parentcount(commitToAmend);
    parents.reserve(static_cast<int>(parentCount));

    for (unsigned int i = 0; i < parentCount; ++i) {
      git_commit *parent = nullptr;
      error = git_commit_parent(&parent, commitToAmend, i);
      if (error) {
        setError(GitError::AmendFailedToGetParentCommit, error);
        for (git_commit *loadedParent : parents) {
          git_commit_free(loadedParent);
        }
        git_signature_free(committerSig);
        git_tree_free(tree);
        git_index_free(index);
        git_commit_free(commitToAmend);
        LOG_PERF(0, "Exit");
        return false;
      }

      parents.push_back(parent);
    }

    CommitSigningConfig signingConfig;
    QString signingError;
    const bool signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::AmendFailedToLoadSigningConfiguration, -1);
      for (git_commit *loadedParent : parents) {
        git_commit_free(loadedParent);
      }
      git_signature_free(committerSig);
      git_tree_free(tree);
      git_index_free(index);
      git_commit_free(commitToAmend);
      LOG_PERF(0, "Exit");
      return false;
    }

    const bool doSigning = signingConfig.enabled;
    if (doSigning) {
      CommitSigningPayload signingPayload;
      signingPayload.config = signingConfig;
      signingPayload.errorMessage = &signingError;

      git_commit_create_ext_options commitOpts =
          GIT_COMMIT_CREATE_EXT_OPTIONS_INIT;
      commitOpts.sign = commitSigningCallback;
      commitOpts.payload = &signingPayload;

      error = git_commit_create_ext(
          &newCommitOid, repo, authorSig, committerSig,
          fullMessage.toStdString().c_str(), tree, parents.size(),
          parents.isEmpty() ? nullptr : parents.data(), &commitOpts);
      if (!error) {
        git_reference *headRef = nullptr;
        error = git_repository_head(&headRef, repo);
        if (!error) {
          git_reference *updatedRef = nullptr;
          error = git_reference_set_target(&updatedRef, headRef, &newCommitOid,
                                           "Amend commit");
          if (updatedRef) {
            git_reference_free(updatedRef);
          }
          git_reference_free(headRef);
        }
      }
    } else {
      error = git_commit_amend(
          &newCommitOid,
          commitToAmend, // Commit to amend
          "HEAD",        // Update HEAD reference
          authorSig,     // Keep original author (or provide new one)
          committerSig,  // New committer (or nullptr to keep original)
          nullptr,       // Use default encoding
          fullMessage.toStdString().c_str(), // Use updated message
          tree // New tree (or nullptr to keep original)
      );
    }

    for (git_commit *loadedParent : parents) {
      git_commit_free(loadedParent);
    }

    if (error) {
      if (doSigning && !signingError.isEmpty()) {
        LOG_ERROR(Log::ErrorId::Git, "Failed to sign commit: {}",
                  signingError.toStdString());
        setError(GitError::AmendFailedToSignCommit, -1);
      } else {
        setError(GitError::AmendFailedToCreateSignedAmendCommit, error);
      }

      git_signature_free(committerSig);
      git_tree_free(tree);
      git_index_free(index);
      git_commit_free(commitToAmend);
      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    error = git_commit_amend(
        &newCommitOid,
        commitToAmend, // Commit to amend
        "HEAD",        // Update HEAD reference
        authorSig,     // Keep original author (or provide new one)
        committerSig,  // New committer (or nullptr to keep original)
        nullptr,       // Use default encoding
        fullMessage.toStdString().c_str(), // Use updated message
        tree // New tree (or nullptr to keep original)
    );
    if (error) {
      setError(GitError::AmendFailedToAmendCommit, error);
      git_signature_free(committerSig);
      git_tree_free(tree);

      git_index_free(index);
      git_commit_free(commitToAmend);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  // Cleanup
  git_signature_free(committerSig);
  git_tree_free(tree);
  git_index_free(index);
  git_commit_free(commitToAmend);

  LOG_PERF(0, "Exit");

  return true;
}

} // namespace GitRepo
