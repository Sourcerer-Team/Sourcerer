
#include "GitRepo.h"

#include <Log.h>

#include <git2/branch.h>
#include <git2/refs.h>
#include <git2/submodule.h>
#include <git2/tag.h>
#include <git2/types.h>

#define THREAD_ID "GitRepo"

namespace GitRepo {

enum class BranchType : int { Unknown, Active, All, Local, Remote, Last };

void initForCurrentThread() {
  LOG_PERF(0, "Enter");

  const int numberOfCalls = git_libgit2_init();

  LOG_DEBUG("Number of initializations is {}", numberOfCalls);

  LOG_PERF(0, "Exit");
}

void shutdownForCurrentThread() {
  LOG_PERF(0, "Enter");

  const int numberofRemaining = git_libgit2_shutdown();

  LOG_DEBUG("Number of remaining initializations is {}", numberofRemaining);

  LOG_PERF(0, "Exit");
}

void closeRepository(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_PERF(0, "Exit");
}

git_repository *openRepository(const QString &path) {
  LOG_PERF(0, "Enter");

  git_repository *repo = nullptr;
  const int error = git_repository_open(&repo, path.toStdString().c_str());

  if (error) {
    const git_error *gitError = git_error_last();
    LOG_ERROR(Log::ErrorId::Git,
              "Error is {}, error klass is {}, error message is '{}'", error,
              gitError->klass, gitError->message);
  }

  LOG_PERF(0, "Exit");
  return repo;
}

static QStringList listBranches(git_repository *repo, BranchType type) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
  case BranchType::Active:
  case BranchType::Local: {
    branchType = GIT_BRANCH_LOCAL;
    break;
  }
  case BranchType::Remote: {
    branchType = GIT_BRANCH_REMOTE;
    break;
  }
  case BranchType::All:
  default: {
    break;
  }
  }

  git_branch_iterator *it = nullptr;

  int error = git_branch_iterator_new(&it, repo, branchType);
  if (error) {
    const git_error *gitError = git_error_last();
    LOG_ERROR(Log::ErrorId::Git, "Error: {}, klass: {}, message: '{}'.", error,
              gitError->klass, gitError->message);
    return {};
  }

  if (!it) {
    LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
    return {};
  }

  QStringList localBranches;
  git_reference *ref = nullptr;

  for (;;) {
    error = git_branch_next(&ref, &branchType, it);
    if (error == GIT_ITEROVER)
      break;

    if (error) {
      const git_error *gitError = git_error_last();
      LOG_ERROR(Log::ErrorId::Git, "Error: {}, klass: {}, message: '{}'.",
                error, gitError->klass, gitError->message);
      break;
    }

    if (!ref) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
      continue;
    }

    const char *branchName = nullptr;

    error = git_branch_name(&branchName, ref);
    if (error == GIT_EINVALID) {
      git_reference_free(ref);
      LOG_ERROR(Log::ErrorId::Git,
                "Reference is not a local or a remote branch.");
      continue;
    }

    if (error) {
      git_reference_free(ref);
      const git_error *gitError = git_error_last();
      LOG_ERROR(Log::ErrorId::Git, "Error: {}, klass: {}, message: '{}'.",
                error, gitError->klass, gitError->message);
      break;
    }

    if (!branchName) {
      git_reference_free(ref);
      LOG_ERROR(Log::ErrorId::Git, "Invalid branch name.");
      continue;
    }

    if (type == BranchType::Active) {
      error = git_branch_is_checked_out(ref);
      if (error == 1)
        localBranches += branchName;
      else if (error != 0) {
        git_reference_free(ref);
        const git_error *gitError = git_error_last();
        LOG_ERROR(Log::ErrorId::Git, "Error: {}, klass: {}, message: '{}'.",
                  error, gitError->klass, gitError->message);
        break;
      }
    } else
      localBranches += branchName;

    git_reference_free(ref);
  };

  git_branch_iterator_free(it);

  LOG_PERF(0, "Exit");

  return localBranches;
}

QStringList listLocalBranches(git_repository *repo) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Local);
}

QStringList listRemoteBranches(git_repository *repo) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Remote);
}

QStringList listTags(git_repository *repo) {
  LOG_PERF(0, "Enter");

  git_strarray tagNames = {0};
  const int error = git_tag_list(&tagNames, repo);
  if (error) {
    const git_error *gitError = git_error_last();
    LOG_ERROR(Log::ErrorId::Git, "Error: {}, klass: {}, message: '{}'.", error,
              gitError->klass, gitError->message);
    return {};
  }

  QStringList tags;

  for (int i = 0; i < tagNames.count; ++i)
    tags += tagNames.strings[i];

  git_strarray_dispose(&tagNames);

  LOG_PERF(0, "Exit");

  return tags;
}

static int listSubmodulesCallback(git_submodule *sm, const char *name,
                                  void *payload) {
  if (!payload)
    return -1;

  QStringList *submodules = static_cast<QStringList *>(payload);
  *submodules += name;

  return 0;
}

QStringList listSubmodules(git_repository *repo) {
  LOG_PERF(0, "Enter");

  QStringList submodules;

  const int error =
      git_submodule_foreach(repo, &listSubmodulesCallback, &submodules);
  if (error) {
    const git_error *gitError = git_error_last();
    LOG_ERROR(Log::ErrorId::Git, "Error: {}, klass: {}, message: '{}'.", error,
              gitError->klass, gitError->message);
    return {};
  }

  LOG_PERF(0, "Exit");

  return submodules;
}

QString getActiveBranch(git_repository *repo) {
  LOG_PERF(0, "Enter");

  const QStringList checkoutBranches = listBranches(repo, BranchType::Active);

  LOG_PERF(0, "Exit");

  return checkoutBranches.length() > 0 ? checkoutBranches.first() : QString();
}

} // namespace GitRepo
