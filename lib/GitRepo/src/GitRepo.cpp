
#include "GitRepo.h"

#include <Log.h>

#include <git2/branch.h>
#include <git2/commit.h>
#include <git2/index.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/signature.h>
#include <git2/submodule.h>
#include <git2/tag.h>
#include <git2/types.h>

#define THREAD_ID "GitRepo"

namespace GitRepo {

enum class BranchType : int { Unknown, Active, All, Local, Remote, Last };

static void logGitError(int error) {
  const git_error *gitError = git_error_last();
  if (!gitError || !gitError->message)
    return;

  LOG_ERROR(Log::ErrorId::Git,
            "Error is {}, error klass is {}, error message is '{}'", error,
            gitError->klass, gitError->message);
}

static bool createInitialCommit(git_repository *repo,
                                const QString &defaultBranchName) {
  LOG_PERF(0, "Enter");

  git_signature *sig = nullptr;

  int error = git_signature_default(&sig, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid tree_id;

  error = git_index_write_tree(&tree_id, index);
  if (error) {
    git_signature_free(sig);
    git_index_free(index);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  git_tree *tree = nullptr;

  error = git_tree_lookup(&tree, repo, &tree_id);
  if (error) {
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid commit_id;

  error = git_commit_create_v(&commit_id, repo, "HEAD", sig, sig, NULL,
                              "Initial commit", tree, 0);
  if (error) {
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(tree);

  git_branch_iterator *it = nullptr;

  error = git_branch_iterator_new(&it, repo, GIT_BRANCH_LOCAL);
  if (error) {
    logGitError(error);
    return false;
  }

  if (!it) {
    LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
    return false;
  }

  bool foundMaster = false;
  git_reference *branchRef = nullptr;
  git_branch_t branchType = GIT_BRANCH_LOCAL;

  for (;;) {
    error = git_branch_next(&branchRef, &branchType, it);
    if (error == GIT_ITEROVER)
      break;

    if (error) {
      logGitError(error);
      break;
    }

    if (!branchRef) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
      continue;
    }

    const char *branchName = nullptr;

    error = git_branch_name(&branchName, branchRef);
    if (error == GIT_EINVALID) {
      git_reference_free(branchRef);
      LOG_ERROR(Log::ErrorId::Git,
                "Reference is not a local or a remote branch.");
      continue;
    }

    if (error) {
      git_reference_free(branchRef);
      logGitError(error);
      break;
    }

    if (strcmp(branchName, "master") == 0) {
      foundMaster = true;
      break;
    }
  }

  if (!foundMaster) {
    git_reference_free(branchRef);
    LOG_ERROR(Log::ErrorId::Git, "Coldn't find master branch.");
    return false;
  }

  git_reference *newRef = nullptr;

  error = git_branch_move(&newRef, branchRef,
                          defaultBranchName.toStdString().data(), false);
  if (error) {
    logGitError(error);
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

static git_commit *getLastCommit(git_repository *repo, const QString &name) {
  LOG_PERF(0, "Enter");

  git_commit *commit = nullptr;
  git_oid oid_parent_commit;

  int error = git_reference_name_to_id(&oid_parent_commit, repo,
                                       name.toStdString().data());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  error = git_commit_lookup(&commit, repo, &oid_parent_commit);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  LOG_PERF(0, "Exit");
  return commit;
}

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

  if (error)
    logGitError(error);

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
    logGitError(error);
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
      logGitError(error);
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
      logGitError(error);
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
        logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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

git_repository *initRepository(const QString &repoName,
                               const QString &workingDir,
                               const QString &defaultBranch) {
  LOG_PERF(0, "Enter");

  git_repository_init_options opts{};

  int error = git_repository_init_options_init(
      &opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  opts.flags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;

  git_repository *repo = nullptr;

  const QString repoPath = workingDir + "/" + repoName;

  error = git_repository_init_ext(&repo, repoPath.toStdString().data(), &opts);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Internal error. Can't initialize git repository.");
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  const bool created = createInitialCommit(repo, defaultBranch);
  if (!created) {
    LOG_ERROR(Log::ErrorId::Git, "Can't create initial commit.");
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  LOG_PERF(0, "Exit");

  return repo;
}

} // namespace GitRepo
