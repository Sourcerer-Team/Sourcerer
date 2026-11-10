// MIT License
// Copyright (c) 2026 Sourcerer

#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFileInfo>
#include <QVector>

#include <algorithm>
#include <string>

#define THREAD_ID "GitRepo"

// Linked worktrees.
namespace GitRepo {

using namespace Private;

static QString normalizeWorktreePath(const QString &path) {
  return QDir::cleanPath(QDir::fromNativeSeparators(path));
}

static bool isSameWorktreePath(const QString &lhs, const QString &rhs) {
  if (lhs.isEmpty() || rhs.isEmpty()) {
    return false;
  }

#ifdef _WIN32
  return lhs.compare(rhs, Qt::CaseInsensitive) == 0;
#else
  return lhs == rhs;
#endif
}

// Best-effort read of the branch and abbreviated HEAD commit of a worktree.
static void readWorktreeHeadInfo(git_repository *worktreeRepo, QString &branch,
                                 QString &head) {
  git_reference *headRef = nullptr;

  if (git_repository_head(&headRef, worktreeRepo) == 0) {
    if (git_reference_is_branch(headRef)) {
      if (const char *shorthand = git_reference_shorthand(headRef)) {
        branch = QString::fromUtf8(shorthand);
      }
    }

    if (const git_oid *oid = git_reference_target(headRef)) {
      char buffer[GIT_OID_HEXSZ + 1] = {0};
      git_oid_tostr(buffer, 8, oid);
      head = QString::fromLatin1(buffer);
    }

    git_reference_free(headRef);
    return;
  }

  // Unborn branch: HEAD is a symbolic reference to a branch without commits.
  git_reference *symbolicRef = nullptr;
  if (git_reference_lookup(&symbolicRef, worktreeRepo, "HEAD") == 0) {
    if (git_reference_type(symbolicRef) == GIT_REFERENCE_SYMBOLIC) {
      const char *target = git_reference_symbolic_target(symbolicRef);
      const QString prefix = QStringLiteral("refs/heads/");
      if (target) {
        const QString targetName = QString::fromUtf8(target);
        if (targetName.startsWith(prefix)) {
          branch = targetName.mid(prefix.length());
        }
      }
    }

    git_reference_free(symbolicRef);
  }
}

QVector<WorktreeInfo> listWorktrees(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_strarray names = {nullptr, 0};
  if (const int error = git_worktree_list(&names, repo)) {
    setError(GitError::ListWorktreesFailedToListWorktrees, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  const char *currentWorkdir = git_repository_workdir(repo);
  const QString currentPath =
      currentWorkdir ? normalizeWorktreePath(QString::fromUtf8(currentWorkdir))
                     : QString();

  QVector<WorktreeInfo> result;

  // The main worktree. When `repo` is itself a linked worktree the main one is
  // found through the common directory.
  git_repository *mainRepo = repo;
  bool ownsMainRepo = false;

  if (git_repository_is_worktree(repo)) {
    mainRepo = nullptr;

    const char *commonDir = git_repository_commondir(repo);
    if (commonDir && git_repository_open(&mainRepo, commonDir) == 0) {
      ownsMainRepo = true;
    } else {
      mainRepo = nullptr;
      LOG_WARN("Failed to open the main repository of the worktree");
    }
  }

  if (mainRepo && !git_repository_is_bare(mainRepo)) {
    if (const char *mainWorkdir = git_repository_workdir(mainRepo)) {
      WorktreeInfo info;
      info.path = normalizeWorktreePath(QString::fromUtf8(mainWorkdir));
      info.name = QFileInfo(info.path).fileName();
      info.isMain = true;
      info.isCurrent = isSameWorktreePath(info.path, currentPath);
      info.locked = false;
      info.valid = true;
      readWorktreeHeadInfo(mainRepo, info.branch, info.head);

      result.push_back(info);
    }
  }

  if (ownsMainRepo) {
    git_repository_free(mainRepo);
  }

  // Linked worktrees.
  QVector<WorktreeInfo> linked;

  for (size_t i = 0; i < names.count; ++i) {
    const char *name = names.strings[i];
    if (!name) {
      continue;
    }

    git_worktree *worktree = nullptr;
    if (const int error = git_worktree_lookup(&worktree, repo, name)) {
      LOG_WARN("{}: '{}' (error {})",
               gitErrorName(GitError::ListWorktreesFailedToOpenWorktree)
                   .toStdString(),
               name, error);
      continue;
    }

    WorktreeInfo info;
    info.name = QString::fromUtf8(name);
    if (const char *worktreePath = git_worktree_path(worktree)) {
      info.path = normalizeWorktreePath(QString::fromUtf8(worktreePath));
    }
    info.isMain = false;
    info.isCurrent = isSameWorktreePath(info.path, currentPath);
    info.locked = git_worktree_is_locked(nullptr, worktree) > 0;
    info.valid = git_worktree_validate(worktree) == 0;

    git_repository *worktreeRepo = nullptr;
    if (git_repository_open_from_worktree(&worktreeRepo, worktree) == 0) {
      readWorktreeHeadInfo(worktreeRepo, info.branch, info.head);
      git_repository_free(worktreeRepo);
    }

    git_worktree_free(worktree);

    linked.push_back(info);
  }

  git_strarray_dispose(&names);

  std::sort(linked.begin(), linked.end(),
            [](const WorktreeInfo &lhs, const WorktreeInfo &rhs) {
              return lhs.name.compare(rhs.name, Qt::CaseInsensitive) < 0;
            });

  result += linked;

  LOG_PERF(0, "Exit");

  return result;
}

bool addWorktree(git_repository *repo, const QString &name, const QString &path,
                 const QString &branch, const bool createBranch,
                 const QString &baseBranch) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (branch.isEmpty()) {
    setError(GitError::AddWorktreeBranchRequired, GIT_EINVALIDSPEC);
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string branchName = branch.toStdString();

  git_reference *branchRef = nullptr;

  if (createBranch) {
    git_commit *baseCommit = nullptr;
    int error = 0;

    if (baseBranch.isEmpty()) {
      git_reference *headRef = nullptr;
      error = git_repository_head(&headRef, repo);
      if (error) {
        setError(GitError::AddWorktreeFailedToResolveHead, error);
        LOG_PERF(0, "Exit");
        return false;
      }

      error =
          git_commit_lookup(&baseCommit, repo, git_reference_target(headRef));
      git_reference_free(headRef);
      if (error) {
        setError(GitError::AddWorktreeFailedToResolveHead, error);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else {
      git_reference *baseRef = nullptr;
      error = git_branch_lookup(
          &baseRef, repo, baseBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
      if (error) {
        setError(GitError::AddWorktreeFailedToLookupBranch, error);
        LOG_PERF(0, "Exit");
        return false;
      }

      error =
          git_commit_lookup(&baseCommit, repo, git_reference_target(baseRef));
      git_reference_free(baseRef);
      if (error) {
        setError(GitError::AddWorktreeFailedToLookupBranch, error);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    error =
        git_branch_create(&branchRef, repo, branchName.c_str(), baseCommit, 0);
    git_commit_free(baseCommit);
    if (error) {
      setError(GitError::AddWorktreeFailedToCreateBranch, error);
      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    const int error = git_branch_lookup(&branchRef, repo, branchName.c_str(),
                                        GIT_BRANCH_LOCAL);
    if (error) {
      setError(GitError::AddWorktreeFailedToLookupBranch, error);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  // Removes the branch created above so a failed add leaves no leftovers.
  const auto cleanupBranch = [&branchRef, createBranch]() {
    if (createBranch) {
      git_branch_delete(branchRef);
    }
    git_reference_free(branchRef);
    branchRef = nullptr;
  };

  const QString worktreePath = normalizeWorktreePath(path);
  const QString parentPath = QFileInfo(worktreePath).absolutePath();

  if (!QDir().mkpath(parentPath)) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to create directory '{}'",
              parentPath.toStdString());
    setError(GitError::AddWorktreeFailedToCreateDirectory, -1);
    cleanupBranch();
    LOG_PERF(0, "Exit");
    return false;
  }

  // libgit2 refuses to use a working directory that already exists, so reuse a
  // folder the user created (or picked) as long as it is completely empty.
  if (const QDir worktreeDir(worktreePath);
      worktreeDir.exists() &&
      worktreeDir.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot |
                          QDir::Hidden | QDir::System)) {
    QDir().rmdir(worktreePath);
  }

  git_worktree_add_options options = GIT_WORKTREE_ADD_OPTIONS_INIT;
  options.ref = branchRef;

  git_worktree *worktree = nullptr;
  const int error =
      git_worktree_add(&worktree, repo, name.toStdString().c_str(),
                       worktreePath.toStdString().c_str(), &options);
  if (error) {
    setError(GitError::AddWorktreeFailedToAddWorktree, error);
    cleanupBranch();
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree_free(worktree);
  git_reference_free(branchRef);

  LOG_PERF(0, "Exit");

  return true;
}

// Only linked worktrees have an administrative directory, so the main one
// can't be looked up by name. Its name is that of its directory.
static bool isMainWorktreeName(git_repository *repo, const QString &name) {
  const QVector<WorktreeInfo> worktrees = listWorktrees(repo);
  return std::any_of(worktrees.cbegin(), worktrees.cend(),
                     [&name](const WorktreeInfo &info) {
                       return info.isMain && info.name == name;
                     });
}

bool removeWorktree(git_repository *repo, const QString &name,
                    const bool force) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree *worktree = nullptr;
  const int lookupError =
      git_worktree_lookup(&worktree, repo, name.toStdString().c_str());
  if (lookupError) {
    // The main worktree can't be found by name, see isMainWorktreeName.
    if (lookupError == GIT_ENOTFOUND && isMainWorktreeName(repo, name)) {
      setError(GitError::RemoveWorktreeMainNotAllowed, lookupError);
    } else {
      setError(GitError::RemoveWorktreeFailedToLookupWorktree, lookupError);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  // Like `git worktree remove`, changes that would be lost need force.
  if (!force && git_worktree_validate(worktree) == 0) {
    git_repository *worktreeRepo = nullptr;
    if (git_repository_open_from_worktree(&worktreeRepo, worktree) == 0) {
      git_status_options statusOptions = GIT_STATUS_OPTIONS_INIT;
      statusOptions.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
      statusOptions.flags = GIT_STATUS_OPT_INCLUDE_UNTRACKED |
                            GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS;
      git_status_list *statusList = nullptr;
      const int statusError =
          git_status_list_new(&statusList, worktreeRepo, &statusOptions);
      const bool dirty =
          statusError == 0 && git_status_list_entrycount(statusList) > 0;
      git_status_list_free(statusList);
      git_repository_free(worktreeRepo);

      if (statusError || dirty) {
        setError(statusError ? GitError::RemoveWorktreeFailedToPrune
                             : GitError::RemoveWorktreeHasChanges,
                 statusError ? statusError : GIT_EUNCOMMITTED);
        git_worktree_free(worktree);
        LOG_PERF(0, "Exit");
        return false;
      }
    }
  }

  git_worktree_prune_options options = GIT_WORKTREE_PRUNE_OPTIONS_INIT;
  options.flags = GIT_WORKTREE_PRUNE_VALID | GIT_WORKTREE_PRUNE_WORKING_TREE;
  if (force) {
    options.flags |= GIT_WORKTREE_PRUNE_LOCKED;
  }

  if (const int error = git_worktree_prune(worktree, &options)) {
    setError(GitError::RemoveWorktreeFailedToPrune, error);
    git_worktree_free(worktree);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree_free(worktree);

  LOG_PERF(0, "Exit");

  return true;
}

bool lockWorktree(git_repository *repo, const QString &name,
                  const QString &reason) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree *worktree = nullptr;
  const int lookupError =
      git_worktree_lookup(&worktree, repo, name.toStdString().c_str());
  if (lookupError) {
    // Only linked worktrees have an administrative directory.
    if (lookupError == GIT_ENOTFOUND && isMainWorktreeName(repo, name)) {
      setError(GitError::LockWorktreeMainNotAllowed, lookupError);
    } else {
      setError(GitError::LockWorktreeFailedToLookupWorktree, lookupError);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string reasonUtf8 = reason.toStdString();
  const int error = git_worktree_lock(
      worktree, reasonUtf8.empty() ? nullptr : reasonUtf8.c_str());
  // GIT_ELOCKED: already locked, which is the state that was asked for.
  if (error && error != GIT_ELOCKED) {
    setError(GitError::LockWorktreeFailedToLock, error);
    git_worktree_free(worktree);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree_free(worktree);

  LOG_PERF(0, "Exit");

  return true;
}

bool unlockWorktree(git_repository *repo, const QString &name) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree *worktree = nullptr;
  const int lookupError =
      git_worktree_lookup(&worktree, repo, name.toStdString().c_str());
  if (lookupError) {
    if (lookupError == GIT_ENOTFOUND && isMainWorktreeName(repo, name)) {
      setError(GitError::UnlockWorktreeMainNotAllowed, lookupError);
    } else {
      setError(GitError::UnlockWorktreeFailedToLookupWorktree, lookupError);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  // Returns 1 if the worktree was not locked, 0 on success, < 0 on error.
  if (const int error = git_worktree_unlock(worktree); error < 0) {
    setError(GitError::UnlockWorktreeFailedToUnlock, error);
    git_worktree_free(worktree);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_worktree_free(worktree);

  LOG_PERF(0, "Exit");

  return true;
}

} // namespace GitRepo
