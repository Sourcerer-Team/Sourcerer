
#include "GitRepo.h"

#include "../../../component/GitRepoManager/include/GitRepoManagerStructs.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <LockGuard.h>
#include <Log.h>
#include <Uuid.h>

#include <git2.h>
#include <git2/branch.h>
#include <git2/checkout.h>
#include <git2/clone.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/graph.h>
#include <git2/index.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/revwalk.h>
#include <git2/signature.h>
#include <git2/submodule.h>
#include <git2/tag.h>
#include <git2/types.h>

#include <QDateTime>
#include <QDir>
#include <QMap>
#include <QString>

#include <set>

#define THREAD_ID "GitRepo"

#define ALL_BRANCHES "*"
#define HEAD "HEAD"

namespace GitRepo {

constexpr qint64 DEFAULT_CREDENTIALS_EXPIRATION_TIME =
    30 * 60 * 1000; // 30 minutes

constexpr int MAX_RETRIES = 3;

struct GitPlaintextCredentials {
  QString username;
  QString password;
  qint64 createdTime;
};

Util::Mutex gGitCredentialsMutex;
QMap<QString, GitPlaintextCredentials> gGitCredentials;

static int gLastError = -1;

static void git_strarray_destroy(git_strarray &arr) {
  for (int i = 0; i < arr.count; ++i) {
    free(arr.strings[i]);
  }

  free(arr.strings);

  arr.strings = nullptr;
  arr.count = 0;
}

static git_strarray qstringlist_to_git_strarray(const QStringList &list) {
  LOG_PERF(0, "Enter");

  git_strarray arr;
  arr.count = static_cast<size_t>(list.length());
  arr.strings = static_cast<char **>(malloc(arr.count * sizeof(char *)));

  for (size_t i = 0; i < arr.count; ++i) {
    arr.strings[i] = strdup(list[i].toStdString().c_str()); // allocates a copy
  }

  LOG_PERF(0, "Exit");

  return arr;
}

static void logGitError(const int error) {
  const git_error *gitError = git_error_last();
  if (!gitError || !gitError->message)
    return;

  gLastError = (gitError->klass & 0x00ff) << 16 | (error & 0xffff);

  LOG_ERROR(Log::ErrorId::Git,
            "Error is {}, error klass is {}, error message is '{}'", error,
            gitError->klass, gitError->message);
}

static void invalidateCredentials(const QString &url) {
  LOCKGUARD(gGitCredentialsMutex);

  const auto it = gGitCredentials.find(url);
  if (it != gGitCredentials.end())
    gGitCredentials.erase(it);
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

  error = git_commit_create_v(&commit_id, repo, HEAD, sig, sig, NULL,
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
                          defaultBranchName.toStdString().c_str(), false);
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
                                       name.toStdString().c_str());
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

int getLastError() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
  return gLastError;
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

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    const BranchType type) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (type == BranchType::Local) {
    git_tree *tree = nullptr;
    git_reference *localRef = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    int error = git_branch_lookup(&localRef, repo,
                                  branchOrTagName.toStdString().c_str(),
                                  GIT_BRANCH_LOCAL);
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Resolve the reference to a commit
    git_commit *targetCommit = nullptr;
    error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                               localRef, GIT_OBJECT_COMMIT);
    if (error) {
      logGitError(error);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 3. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      logGitError(error);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 4. Set checkout strategy
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 5. Checkout the tree
    error =
        git_checkout_tree(repo, reinterpret_cast<git_object *>(tree), &opts);
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_tree_free(tree);
    git_reference_free(localRef);
    git_commit_free(targetCommit);

  } else if (type == BranchType::Remote) {
    git_tree *tree = nullptr;
    git_reference *localRef = nullptr;
    git_reference *remoteRef = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    QString localName;
    if (const auto strList = branchOrTagName.split('/'); strList.length() > 0) {
      localName = strList.last();
    } else {
      localName = branchOrTagName;
    }

    const QString fullBranchName =
        QString("refs/remotes/%1").arg(branchOrTagName);

    // 1. Lookup the branch reference
    int error = git_reference_lookup(&remoteRef, repo,
                                     fullBranchName.toStdString().c_str());
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Resolve the reference to a commit
    git_commit *targetCommit = nullptr;
    error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                               remoteRef, GIT_OBJECT_COMMIT);
    if (error) {
      logGitError(error);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 3. Create local branch that tracks the remote branch
    error = git_branch_create(&localRef, repo, localName.toStdString().c_str(),
                              targetCommit, 0);
    if (error) {
      if (error == GIT_EEXISTS) {
        // 3.1 Branch already exists, try to lookup it
        error = git_branch_lookup(
            &localRef, repo, localName.toStdString().c_str(), GIT_BRANCH_LOCAL);
        if (error) {
          logGitError(error);
          git_commit_free(targetCommit);
          git_reference_free(remoteRef);
          LOG_PERF(0, "Exit");
          return false;
        }
      } else {
        logGitError(error);
        git_commit_free(targetCommit);
        git_reference_free(remoteRef);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    // 4. Set up tracking
    error = git_branch_set_upstream(localRef,
                                    branchOrTagName.toStdString().c_str());
    if (error) {
      logGitError(error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 5. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      logGitError(error);
      git_reference_free(localRef);
      git_reference_free(remoteRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Set checkout strategy
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 7. Checkout the tree
    error =
        git_checkout_tree(repo, reinterpret_cast<git_object *>(tree), &opts);
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_reference_free(remoteRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_reference_free(remoteRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_tree_free(tree);
    git_reference_free(localRef);
    git_reference_free(remoteRef);
    git_commit_free(targetCommit);
  } else if (type == BranchType::Tag) {
    git_object *tagTarget = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    // 1. Resolve the tag to its target commit/tree
    int error = git_revparse_single(&tagTarget, repo,
                                    branchOrTagName.toStdString().c_str());
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Set checkout options
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 3. Checkout the tree
    error = git_checkout_tree(repo, tagTarget, &opts);
    if (error) {
      logGitError(error);
      git_object_free(tagTarget);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 4. Detach HEAD to the tag's commit
    error = git_repository_set_head_detached(repo, git_object_id(tagTarget));
    if (error) {
      logGitError(error);
      git_object_free(tagTarget);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(tagTarget);
  } else {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Unsupported branch type {}",
              static_cast<int>(type));
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

void closeRepository(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return;
  }

  git_repository_free(repo);

  LOG_PERF(0, "Exit");
}

git_repository *openRepository(const QString &path) {
  LOG_PERF(0, "Enter");

  git_repository *repo = nullptr;

  if (const int error = git_repository_open(&repo, path.toStdString().c_str()))
    logGitError(error);

  LOG_PERF(0, "Exit");
  return repo;
}

static QStringList listCheckoutBranches(git_repository *repo,
                                        const BranchType type,
                                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  bool tagRequested = false;

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
  case BranchType::Local: {
    branchType = GIT_BRANCH_LOCAL;
    break;
  }
  case BranchType::Remote: {
    branchType = GIT_BRANCH_REMOTE;
    break;
  }
  case BranchType::Tag: {
    tagRequested = true;
    break;
  }
  case BranchType::All:
  default: {
    break;
  }
  }

  if (!tagRequested && branchType == GIT_BRANCH_ALL) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList activeBranches;

  if (!tagRequested) {
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

    git_reference *ref = nullptr;

    while (!quitFlag) {
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

      error = git_branch_is_checked_out(ref);
      if (error == 1) {
        activeBranches += branchName;
      } else if (error != 0) {
        git_reference_free(ref);
        logGitError(error);
        break;
      }

      git_reference_free(ref);
    };

    git_branch_iterator_free(it);
  }

  // Include HEAD if detached for local branches or if tag requested
  if ((type == BranchType::Local && activeBranches.isEmpty()) || tagRequested) {
    git_reference *headRef = nullptr;

    int error = git_repository_head(&headRef, repo);
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (git_reference_type(headRef) == GIT_REFERENCE_DIRECT) {
      // Detached HEAD — could be tag or raw commit
      if (tagRequested) {
        git_strarray tags = {nullptr};

        error = git_tag_list(&tags, repo);
        if (error) {
          logGitError(error);
          git_reference_free(headRef);
          LOG_PERF(0, "Exit");
          return {};
        }

        for (size_t i = 0; i < tags.count; ++i) {
          git_object *tagObj = nullptr;

          error = git_revparse_single(&tagObj, repo, tags.strings[i]);
          if (error) {
            logGitError(error);
            continue;
          }

          if (git_oid_equal(git_reference_target(headRef),
                            git_object_id(tagObj))) {
            // HEAD is detached at tag
            activeBranches += tags.strings[i];
            git_object_free(tagObj);
            break;
          }

          git_object_free(tagObj);
        }

        git_strarray_dispose(&tags);
      } else {
        // Raw commit
        if (const QString referenceName = git_reference_name(headRef);
            referenceName == HEAD) {
          activeBranches += HEAD;
        }
      }
    }
  }

  LOG_PERF(0, "Exit");

  return activeBranches;
}

static QStringList listBranches(git_repository *repo, const BranchType type,
                                const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
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

  if (branchType == GIT_BRANCH_ALL) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList activeBranches;

  if (branchType == GIT_BRANCH_LOCAL) {
    git_reference *headRef = nullptr;

    if (const int error = git_repository_head(&headRef, repo)) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (git_reference_type(headRef) == GIT_REFERENCE_DIRECT) {
      // Detached HEAD — could be tag or raw commit
      if (const QString referenceName = git_reference_name(headRef);
          referenceName == HEAD) {
        activeBranches += HEAD;
      }
    }

    git_reference_free(headRef);
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

  git_reference *ref = nullptr;

  while (!quitFlag) {
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

    activeBranches += branchName;

    git_reference_free(ref);
  };

  git_branch_iterator_free(it);

  LOG_PERF(0, "Exit");

  return activeBranches;
}

QStringList listLocalBranches(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Local, quitFlag);
}

QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Remote, quitFlag);
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

QString getActiveBranch(git_repository *repo, const BranchType type,
                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  const QStringList checkoutBranches =
      listCheckoutBranches(repo, type, quitFlag);

  LOG_PERF(0, "Exit");

  return !checkoutBranches.empty() ? checkoutBranches.first() : QString();
}

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch) {
  LOG_PERF(0, "Enter");

  git_repository_init_options opts{};

  int error = git_repository_init_options_init(
      &opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  opts.flags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;

  git_repository *repo = nullptr;

  const QString repoPath = workingDir + "/" + repoName;

  error = git_repository_init_ext(&repo, repoPath.toStdString().c_str(), &opts);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Internal error. Can't initialize git repository.");
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created = createInitialCommit(repo, defaultBranch);
  if (!created) {
    closeRepository(repo);
    LOG_ERROR(Log::ErrorId::Git, "Can't create initial commit.");
    LOG_PERF(0, "Exit");
    return false;
  }

  closeRepository(repo);

  LOG_PERF(0, "Exit");

  return true;
}

static int fetchProgressCallback(const git_indexer_progress *stats,
                                 void *payload) {
  if (!payload || !stats)
    return 0;

  const auto progress = static_cast<CloneProgress *>(payload);
  progress->completedSteps = stats->indexed_objects;
  progress->totalSteps = stats->total_objects;
  progress->started = true;
  progress->finished = false;
  progress->stage = CloneProgressStageType::Fetch;

  if (stats->total_objects > 0) {
    const int percentage =
        ((stats->indexed_objects * 1.0f) / stats->total_objects) * 100;
    if (percentage > progress->lastStepPercentage) {
      progress->lastStepPercentage = percentage;
      EventManager::Event progressEvent(
          EventManager::EventType::GitRepoManagerCloneRepoProgress);
      progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
          progress->uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
          QVariant::fromValue(*progress)});
      EventManager::EventManager::getInstance().send(progressEvent);
    }
  } else {
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        progress->uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(*progress)});
    EventManager::EventManager::getInstance().send(progressEvent);
  }

  return 0;
}

static void checkoutProgressCallback(const char *path, size_t completed_steps,
                                     const size_t total_steps, void *payload) {
  if (!payload)
    return;

  const auto progress = static_cast<CloneProgress *>(payload);
  progress->completedSteps = completed_steps;
  progress->totalSteps = total_steps;
  progress->started = true;
  progress->finished = false;
  progress->stage = CloneProgressStageType::Checkout;

  if (completed_steps == 0)
    progress->lastStepPercentage = 0;

  if (total_steps <= 0)
    return;

  const int percentage = ((completed_steps * 1.0f) / total_steps) * 100;
  if (percentage > progress->lastStepPercentage) {
    progress->lastStepPercentage = percentage;
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        progress->uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(*progress)});
    EventManager::EventManager::getInstance().send(progressEvent);
  }
}

static void finishFetchProgress(const QString &uuid, const bool error) {
  CloneProgress progress;
  progress.uuid = uuid;
  progress.completedSteps = 100;
  progress.totalSteps = 100;
  progress.started = true;
  progress.finished = true;
  progress.error = error;

  EventManager::Event progressEvent(
      EventManager::EventType::GitRepoManagerCloneRepoProgress);
  progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
      uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
      QVariant::fromValue(progress)});
  EventManager::EventManager::getInstance().send(progressEvent);
}

static void finishCredentialsAquaring() {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  const EventManager::Event credentialsFinishedEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsFinished);

  eventManager.send(credentialsFinishedEvent);
}

static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &url, const QString &username,
                   const int allowedTypes) {
  LOG_PERF(0, "Enter");

  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  const int token = eventManager.startWait();

  GitCredentialsRequest request;
  request.uuid = Uuid::createUuid();
  request.url = url;
  request.username = username;
  request.allowedTypes = allowedTypes;

  EventManager::Event credentialsEvent(
      EventManager::EventType::GitRepoManagerGetCredentials);
  credentialsEvent.addData(request);

  eventManager.send(credentialsEvent);

  const auto predicate = [&](EventManager::Event event) -> bool {
    bool success = false;
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
      const auto [uuid] = event.data<GitCredentialsErrorResponse>();
      success = uuid == request.uuid;
      break;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();
      success = response.uuid == request.uuid;
      break;
    }
    default: {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Unknown event type.");
      break;
    }
    }

    return success;
  };

  const std::set<EventManager::EventType> types{
      EventManager::EventType::GitRepoManagerGetCredentialsCancel,
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit};

  const auto &[success, event] =
      eventManager.waitOneOf(token, types, predicate);

  if (success) {
    if (event.type() ==
        EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
      LOG_PERF(0, "Exit");
      return std::make_tuple(false, GitPlaintextCredentials{});
    } else if (event.type() ==
               EventManager::EventType::GitRepoManagerGetCredentialsSubmit) {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();
      LOG_PERF(0, "Exit");
      return std::make_tuple(
          true, GitPlaintextCredentials{response.username, response.password,
                                        QDateTime::currentMSecsSinceEpoch()});
    } else {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Unknown event type.");
    }
  }

  LOG_PERF(0, "Exit");
  return std::make_tuple(false, GitPlaintextCredentials{});
}

static int credentialsAcquireCallback(git_credential **out, const char *url,
                                      const char *username_from_url,
                                      unsigned int allowed_types,
                                      void *payload) {
  LOG_PERF(0, "Enter");

  if ((allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) == 0) {
    LOG_PERF(0, "Exit");
    return 1;
  }

  gGitCredentialsMutex.lock();

  const auto it = gGitCredentials.find(url);
  if (it != gGitCredentials.end()) {

    gGitCredentialsMutex.unlock();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qlonglong expirationTimeout =
        ConfigManager::ConfigManager::getInstance().getLong(
            ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime);
    if (it->createdTime + expirationTimeout < now) {

      const auto &[success, credentials] = acquireCredentials(
          url, username_from_url ? username_from_url : "", allowed_types);
      if (success) {

        LOG_INFO("Acquired credentials for '{}'", credentials.username);

        gGitCredentialsMutex.lock();

        gGitCredentials[url] = credentials;

        gGitCredentialsMutex.unlock();
      } else {
        LOG_PERF(0, "Exit");
        return -1;
      }
    }
  } else {

    gGitCredentialsMutex.unlock();

    const auto &[success, credentials] = acquireCredentials(
        url, username_from_url ? username_from_url : "", allowed_types);
    if (success) {

      LOG_INFO("Acquired credentials for '{}'", credentials.username);

      gGitCredentialsMutex.lock();

      gGitCredentials[url] = credentials;

      gGitCredentialsMutex.unlock();
    } else {
      LOG_PERF(0, "Exit");
      return GIT_EUSER;
    }
  }

  gGitCredentialsMutex.lock();

  const auto &credentials = gGitCredentials[url];

  gGitCredentialsMutex.unlock();

  const int error = git_credential_userpass_plaintext_new(
      out, credentials.username.toStdString().c_str(),
      credentials.password.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return error;
  }

  LOG_PERF(0, "Exit");

  return 0;
}

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  LOG_PERF(0, "Enter");

  const QDir workingDir;
  const bool workingDirExisted = workingDir.exists(destinationDir);
  if (!workingDir.mkpath(destinationDir)) {
    LOG_ERROR(Log::ErrorId::Git, "can't create destination dir '{}'",
              destinationDir);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository *repo = nullptr;
  git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
  git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;

  CloneProgress progressData;
  progressData.uuid = uuid;

  checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  checkout_opts.progress_payload = &progressData;
  checkout_opts.progress_cb = &checkoutProgressCallback;
  fetch_opts.callbacks.credentials = &credentialsAcquireCallback;
  fetch_opts.callbacks.transfer_progress = &fetchProgressCallback;
  fetch_opts.callbacks.payload = &progressData;
  clone_opts.checkout_opts = checkout_opts;
  clone_opts.fetch_opts = fetch_opts;

  const int error =
      git_clone(&repo, url.toStdString().c_str(),
                destinationDir.toStdString().c_str(), &clone_opts);

  finishFetchProgress(uuid, error != 0);
  finishCredentialsAquaring();

  if (error || !repo) {
    invalidateCredentials(url);

    if (!workingDirExisted) {
      QDir dirToRemove(destinationDir);
      dirToRemove.removeRecursively();
    }

    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository_free(repo);

  LOG_PERF(0, "Exit");

  return true;
}

GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            const BranchType type, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  GitHistory history_;

  git_revwalk *walker = nullptr;

  int error = git_revwalk_new(&walker, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
  if (error) {
    logGitError(error);
    git_revwalk_free(walker);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (type == BranchType::All || branchName == ALL_BRANCHES) {
    git_branch_iterator *it = nullptr;
    git_reference *ref = nullptr;
    git_branch_t branchType;

    if (branchName == ALL_BRANCHES) {
      if (type == BranchType::Local) {
        branchType = GIT_BRANCH_LOCAL;
      } else if (type == BranchType::Remote) {
        branchType = GIT_BRANCH_REMOTE;
      } else {
        git_revwalk_free(walker);
        LOG_PERF(0, "Exit");
        return {};
      }
    } else {
      branchType = GIT_BRANCH_ALL;
    }

    error = git_branch_iterator_new(&it, repo, branchType);
    if (error) {
      logGitError(error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    while (git_branch_next(&ref, &branchType, it) == 0 && !quitFlag) {
      const char *currentBranchName = nullptr;

      error = git_branch_name(&currentBranchName, ref);
      if (error) {
        logGitError(error);
        git_revwalk_free(walker);
        git_branch_iterator_free(it);
        LOG_PERF(0, "Exit");
        return {};
      }

      if (!currentBranchName) {
        git_revwalk_free(walker);
        git_branch_iterator_free(it);
        LOG_ERROR(Log::ErrorId::Git, "No branch name found!");
        LOG_PERF(0, "Exit");
        return {};
      }

      QString fullBranchName;

      if (branchType == GIT_BRANCH_LOCAL)
        fullBranchName = QString("refs/heads/%1").arg(currentBranchName);
      else if (branchType == GIT_BRANCH_REMOTE)
        fullBranchName = QString("refs/remotes/%1").arg(currentBranchName);
      else {
        git_revwalk_free(walker);
        git_branch_iterator_free(it);
        LOG_ERROR(Log::ErrorId::Git, "Unknown branch type!");
        LOG_PERF(0, "Exit");
        return {};
      }

      error =
          git_revwalk_push_ref(walker, fullBranchName.toStdString().c_str());
      if (error) {
        logGitError(error);
        git_revwalk_free(walker);
        git_branch_iterator_free(it);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_reference_free(ref);
    }

    git_branch_iterator_free(it);
  } else if (type == BranchType::Local) {
    if (branchName.isEmpty()) {
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (branchName == HEAD) {
      error = git_revwalk_push_head(walker);
    } else {
      error = git_revwalk_push_ref(
          walker,
          QString("refs/heads/%1").arg(branchName).toStdString().c_str());
    }
    if (error) {
      logGitError(error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else if (type == BranchType::Remote) {
    if (branchName.isEmpty()) {
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_revwalk_push_ref(
        walker,
        QString("refs/remotes/%1").arg(branchName).toStdString().c_str());
    if (error) {
      logGitError(error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else if (type == BranchType::Tag) {
    if (branchName.isEmpty()) {
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_revwalk_push_ref(
        walker, QString("refs/tags/%1").arg(branchName).toStdString().c_str());
    if (error) {
      logGitError(error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else {
    git_revwalk_free(walker);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_oid oid;

  QSet<QString> processed;

  while (git_revwalk_next(&oid, walker) == 0 && !quitFlag) {
    const char *str = git_oid_tostr_s(&oid);
    if (!str) {
      LOG_ERROR(Log::ErrorId::Git, "Can't convert oid to string.");
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    const QString oidStr = str;

    git_commit *commit = nullptr;

    error = git_commit_lookup(&commit, repo, &oid);
    if (error) {
      logGitError(error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (!commit) {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit with oid \"{}\"",
                oidStr);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (processed.contains(oidStr)) {
      continue;
    }

    processed.insert(oidStr);

    history_.commits.push_back(oidStr);

    const std::size_t parentCount = git_commit_parentcount(commit);

    for (std::size_t i = 0; i < parentCount && !quitFlag; ++i) {
      const git_oid *parentOid = git_commit_parent_id(commit, i);
      if (!parentOid) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Can't lookup parent oid at index {} for oid \"{}\"", i,
                  oidStr);
        git_revwalk_free(walker);
        git_commit_free(commit);
        LOG_PERF(0, "Exit");
        return {};
      }

      str = git_oid_tostr_s(parentOid);
      if (!str) {
        LOG_ERROR(
            Log::ErrorId::Git,
            "Can't convert parent oid at index {} for oid \"{}\" to string.", i,
            oidStr);
        git_revwalk_free(walker);
        git_commit_free(commit);
        LOG_PERF(0, "Exit");
        return {};
      }

      const QString parentOidStr = str;

      if (oidStr == parentOidStr) {
        LOG_ERROR(Log::ErrorId::Git, "The commit \"{}\"is parent of itself",
                  parentOidStr)
        continue;
      }

      history_.commitInfos[oidStr].parents.push_back(parentOidStr);
      history_.commitInfos[parentOidStr].children.push_back(oidStr);
    }

    history_.commitInfos[oidStr].oid = oid;

    if (const char *message = git_commit_message(commit))
      history_.commitInfos[oidStr].message = message;
    else {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit message for oid \"{}\"",
                oidStr);
      git_revwalk_free(walker);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (const char *summary = git_commit_summary(commit))
      history_.commitInfos[oidStr].summary = summary;
    else {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit summary for oid \"{}\"",
                oidStr);
      git_revwalk_free(walker);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (const git_signature *signature = git_commit_author(commit)) {
      if (signature->name)
        history_.commitInfos[oidStr].authorName = signature->name;

      if (signature->email)
        history_.commitInfos[oidStr].authorEmail = signature->email;

      history_.commitInfos[oidStr].timeStamp =
          signature->when.time * 1000 + signature->when.offset * 60 * 1000;
    } else {
      LOG_ERROR(Log::ErrorId::Git,
                "Can't lookup commit signature for oid \"{}\"", oidStr);
      git_revwalk_free(walker);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }

    history_.commitInfos[oidStr].branches += branchName;

    git_commit_free(commit);
  }

  git_revwalk_free(walker);

  if (!quitFlag) {
    std::sort(history_.commits.begin(), history_.commits.end(),
              [&](const QString &a, const QString &b) {
                auto it = history_.commitInfos.find(a);
                if (it == history_.commitInfos.end()) {
                  return false;
                }

                const GitCommitInfo &commitInfoA = it.value();

                it = history_.commitInfos.find(b);
                if (it == history_.commitInfos.end()) {
                  return false;
                }

                const GitCommitInfo &commitInfoB = it.value();

                return commitInfoA.timeStamp < commitInfoB.timeStamp;
              });
  }

  history_.valid = true;

  LOG_PERF(0, "Exit");

  return quitFlag ? GitHistory{} : history_;
}

QStringList getCommitBranches(git_repository *repo, const QString &commitOid,
                              const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (commitOid.isEmpty()) {
    LOG_WARN("Commit \"{}\" is empty", commitOid);
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    const char *branchName = nullptr;

    error = git_branch_name(&branchName, ref);
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (!branchName) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_commit *branchCommit = nullptr;
    const git_oid *branchOid = nullptr;

    if (git_reference_type(ref) == GIT_REFERENCE_SYMBOLIC) {
      git_reference *symbolicRef = nullptr;
      error = git_reference_resolve(&symbolicRef, ref);
      if (error) {
        logGitError(error);
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_object *branchTip = nullptr;

      error = git_reference_peel(&branchTip, symbolicRef, GIT_OBJECT_COMMIT);
      if (error) {
        logGitError(error);
        git_reference_free(symbolicRef);
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_reference_free(symbolicRef);

      branchOid = git_object_id(branchTip);
      git_object_free(branchTip);
    } else {
      branchOid = git_reference_target(ref);
    }

    if (!branchOid) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    // Check if branch contains the commit
    if (const int contains = git_graph_descendant_of(repo, branchOid, &oid);
        contains == 1)
      branches += branchName;

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return branches;
}

QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    git_object *branchTip = nullptr;
    error = git_reference_peel(&branchTip, ref, GIT_OBJECT_COMMIT);
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    const char *branchName = nullptr;
    error = git_branch_name(&branchName, ref);
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_object_free(branchTip);

    // Check if this commit is the HEAD of the branch
    const git_oid *branchHeadOid = git_reference_target(ref);
    if (branchHeadOid) {
      const bool head = git_oid_equal(branchHeadOid, &oid) == 0;
      if (head)
        branches += branchName;
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return branches;
}

bool getCommitIsHead(git_repository *repo, const QString &commitOid,
                     const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    git_object *branchTip = nullptr;
    error = git_reference_peel(&branchTip, ref, GIT_OBJECT_COMMIT);
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchName = nullptr;
    error = git_branch_name(&branchName, ref);
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      logGitError(error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(branchTip);

    // Check if this commit is the HEAD of the branch
    const git_oid *branchHeadOid = git_reference_target(ref);
    if (branchHeadOid) {
      const bool head = git_oid_equal(branchHeadOid, &oid) == 0;
      if (head) {
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return true;
      }
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return false;
}

GitStatus getStatus(git_repository *repo) {
  LOG_PERF(0, "Enter");

  GitStatus status;

  const QString repoPath =
      QDir::fromNativeSeparators(git_repository_workdir(repo));

  git_status_options opts;
  git_status_options_init(&opts, GIT_STATUS_OPTIONS_VERSION);
  opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
  opts.flags =
      GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
      GIT_STATUS_OPT_INCLUDE_IGNORED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
      GIT_STATUS_OPT_SORT_CASE_SENSITIVELY | GIT_STATUS_OPT_INCLUDE_UNREADABLE;

  git_status_list *statusList = nullptr;

  int error = git_status_list_new(&statusList, repo, &opts);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  size_t count = git_status_list_entrycount(statusList);

  for (size_t i = 0; i < count; ++i) {
    const git_status_entry *entry = git_status_byindex(statusList, i);

    Q_ASSERT(entry != nullptr);

    if (!entry) {
      continue;
    }

    const char *filePath =
        entry->head_to_index      ? entry->head_to_index->new_file.path
        : entry->index_to_workdir ? entry->index_to_workdir->new_file.path
                                  : nullptr;

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
      status.modifiedStagedFiles += unixFilePath;
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

GitCommitStatus getCommitStatus(git_repository *repo,
                                const QString &commitOid) {
  LOG_PERF(0, "Enter");

  GitCommitStatus status;

  git_commit *commit = nullptr;
  git_commit *parent = nullptr;
  git_tree *tree = nullptr;
  git_tree *parent_tree = nullptr;
  git_diff *diff = nullptr;

  // Lookup commit by SHA
  git_oid oid;
  git_oid_fromstr(&oid, commitOid.toStdString().c_str());

  int error = git_commit_lookup(&commit, repo, &oid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get commit tree
  error = git_commit_tree(&tree, commit);
  if (error) {
    logGitError(error);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  const int parentCount = git_commit_parentcount(commit);

  // Get parent tree (if exists)
  if (parentCount > 0) {
    error = git_commit_parent(&parent, commit, 0);
    if (error) {
      logGitError(error);
      git_commit_free(commit);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parent_tree, parent);
    if (error) {
      logGitError(error);
      git_commit_free(commit);
      git_commit_free(parent);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else {
    parent_tree = nullptr; // Initial commit
  }

  // Generate diff
  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.flags |= GIT_DIFF_INCLUDE_UNTRACKED | GIT_DIFF_INCLUDE_TYPECHANGE |
                GIT_DIFF_RECURSE_UNTRACKED_DIRS;

  error = git_diff_tree_to_tree(&diff, repo, parent_tree, tree, &opts);
  if (error) {
    logGitError(error);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Detect renames
  git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
  find_opts.flags = GIT_DIFF_FIND_RENAMES;

  error = git_diff_find_similar(diff, &find_opts);
  if (error) {
    logGitError(error);
    git_commit_free(commit);
    git_tree_free(tree);
    LOG_PERF(0, "Exit");
    return {};
  }

  status.commit = commitOid;

  // Walk the diff
  git_diff_foreach(
      diff,
      [](const git_diff_delta *delta, float, void *payload) {
        auto *status = static_cast<GitCommitStatus *>(payload);

        switch (delta->status) {
        case GIT_DELTA_ADDED:
          status->newFiles += delta->new_file.path;
          break;
        case GIT_DELTA_MODIFIED:
          status->modifiedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_DELETED:
          status->deletedFiles += delta->old_file.path;
          break;
        case GIT_DELTA_RENAMED:
          status->renamedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_TYPECHANGE:
          status->typeChangedFiles += delta->new_file.path;
          break;
        default:
          break;
        }

        return 0;
      },
      nullptr, nullptr, nullptr, &status);

  LOG_PERF(0, "Exit");

  return status;
}

bool stageFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString repoPath = git_repository_workdir(repo);
  for (const auto &filePath : files) {
    const QString unixFilePath = QDir::fromNativeSeparators(filePath);
    const QString fullPath = repoPath + unixFilePath;
    const QString relativePath = QDir(repoPath).relativeFilePath(fullPath);
    if (!QFileInfo::exists(fullPath)) {
      error =
          git_index_remove_bypath(index, relativePath.toStdString().c_str());
      if (error) {
        logGitError(error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      continue;
    }

    error = git_index_add_bypath(index, relativePath.toStdString().c_str());
    if (error) {
      logGitError(error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_index_write(index);
  if (error) {
    logGitError(error);
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

  git_object *headCommit = nullptr;

  // Lookup HEAD commit
  int error = git_revparse_single(&headCommit, repo, "HEAD");
  if (error) {
    logGitError(error);
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
    logGitError(error);
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
                  const QString &email) {
  LOG_PERF(0, "Enter");

  git_index *index = nullptr;
  git_oid treeId;
  git_oid commitId;
  git_tree *tree = nullptr;
  git_signature *sig = nullptr;
  git_commit *parent = nullptr;

  // Load index
  int error = git_repository_index(&index, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write index to tree
  error = git_index_write_tree(&treeId, index);
  if (error) {
    logGitError(error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeId);
  if (error) {
    logGitError(error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create signature
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_index_free(index);
    git_tree_free(tree);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Lookup HEAD commit (if it exists)
  error = git_repository_head_unborn(repo);
  if (!error) {
    git_reference *headRef = nullptr;
    error = git_repository_head(&headRef, repo);
    if (error) {
      logGitError(error);
      git_signature_free(sig);
      git_index_free(index);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_commit_lookup(&parent, repo, git_reference_target(headRef));
    if (error) {
      logGitError(error);
      git_signature_free(sig);
      git_index_free(index);
      git_tree_free(tree);
      git_reference_free(headRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(headRef);
  }

  const QString fullMessage = summary + "\n\n" + message;

  // Create commit
  error = git_commit_create_v(&commitId, repo, "HEAD", sig, sig, nullptr,
                              fullMessage.toStdString().c_str(), tree,
                              parent ? 1 : 0, parent);
  if (error) {
    logGitError(error);
    git_signature_free(sig);
    git_index_free(index);
    git_tree_free(tree);

    if (parent) {
      git_commit_free(parent);
    }

    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_index_free(index);
  git_tree_free(tree);

  if (parent) {
    git_commit_free(parent);
  }

  LOG_PERF(0, "Exit");

  return true;
}

QString diffFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  git_object *headCommit = nullptr;
  git_commit *commit = nullptr;
  git_tree *headTree = nullptr;

  int error = git_revparse_single(&headCommit, repo, "HEAD");
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_commit_lookup(&commit, repo, git_object_id(headCommit));
  if (error) {
    logGitError(error);
    git_object_free(headCommit);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_commit_tree(&headTree, commit);
  if (error) {
    logGitError(error);
    git_object_free(headCommit);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_object_free(headCommit);
  git_commit_free(commit);

  git_strarray arr = qstringlist_to_git_strarray(files);

  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.pathspec = arr;

  git_diff *diff = nullptr;
  error = git_diff_tree_to_workdir(&diff, repo, headTree, &opts);
  if (error) {
    logGitError(error);
    git_tree_free(headTree);
    git_strarray_destroy(arr);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree_free(headTree);
  git_strarray_destroy(arr);

  const auto callback = [](const git_diff_delta *, const git_diff_hunk *hunk,
                           const git_diff_line *line, void *payload) -> int {
    QString *output = static_cast<QString *>(payload);

    Q_ASSERT(output != nullptr);

    if (!output) {
      return 0;
    }

    if (line->origin == GIT_DIFF_LINE_FILE_HDR) {
      // Skip file header lines
      return 0;
    }

    output->append(QString::fromUtf8(line->content, line->content_len));

    return 0;
  };

  QString diffOutput;

  error = git_diff_print(diff, GIT_DIFF_FORMAT_PATCH, callback, &diffOutput);
  if (error) {
    logGitError(error);
    git_diff_free(diff);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_diff_free(diff);

  LOG_PERF(0, "Exit");

  return diffOutput;
}

bool discardFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  for (const auto &file : files) {
    git_object *headTree = nullptr;
    int error = git_revparse_single(&headTree, repo, "HEAD^{tree}");
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_strarray arr = qstringlist_to_git_strarray(QStringList() << file);

    // 1. Reset working directory to HEAD
    git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
    checkoutOpts.checkout_strategy =
        GIT_CHECKOUT_FORCE | GIT_CHECKOUT_DISABLE_PATHSPEC_MATCH;
    checkoutOpts.paths = arr;

    error = git_checkout_tree(repo, headTree, &checkoutOpts);
    if (error) {
      logGitError(error);
      git_strarray_destroy(arr);
      git_object_free(headTree);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(headTree);
    git_strarray_destroy(arr);

    // 2. Reset index entry to HEAD
    git_index *index = nullptr;
    error = git_repository_index(&index, repo);
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object *headTree2 = nullptr;
    error = git_revparse_single(&headTree2, repo, "HEAD^{tree}");
    if (error) {
      logGitError(error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_tree *tree = nullptr;
    error = git_tree_lookup(&tree, repo, git_object_id(headTree2));
    if (error) {
      logGitError(error);
      git_index_free(index);
      git_object_free(headTree2);
      LOG_PERF(0, "Exit");
      return false;
    }

    const std::string path = file.toStdString();

    git_tree_entry *entry = nullptr;
    error = git_tree_entry_bypath(&entry, tree, path.c_str());
    if (error) {
      logGitError(error);
      git_index_free(index);
      git_object_free(headTree2);
      continue;
    }

    if (entry) {
      git_blob *blob = nullptr;
      error = git_blob_lookup(&blob, repo, git_tree_entry_id(entry));
      if (error) {
        logGitError(error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        git_tree_entry_free(entry);
        LOG_PERF(0, "Exit");
        return false;
      }

      const QString repoPath = git_repository_workdir(repo);
      const QString fullPath =
          QDir::fromNativeSeparators(repoPath + QString::fromStdString(path));

      const QString directoryPath = QFileInfo(fullPath).path();

      QDir fileDir;
      fileDir.mkpath(directoryPath);

      QFile file(fullPath);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR(Log::ErrorId::Git, "Can't open file \"{}\" for writing",
                  file.fileName());
        git_blob_free(blob);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        git_tree_entry_free(entry);
        LOG_PERF(0, "Exit");
        return false;
      }

      file.write(static_cast<const char *>(git_blob_rawcontent(blob)),
                 git_blob_rawsize(blob));
      file.close();

      git_blob_free(blob);

      git_index_entry newEntry = {};
      newEntry.mode = git_tree_entry_filemode(entry);
      newEntry.id = *git_tree_entry_id(entry);
      newEntry.path = path.c_str();

      error = git_index_remove(index, path.c_str(), 0); // Remove old entry
      if (error) {
        logGitError(error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      error = git_index_add(index, &newEntry); // Add fresh entry
      if (error) {
        logGitError(error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else {
      error = git_index_remove(index, path.c_str(), 0); // Remove if not in HEAD
      if (error) {
        logGitError(error);
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
      logGitError(error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_index_free(index);
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool pull(git_repository *repo) {
  LOG_PERF(0, "Enter");

  // Step 1: Get current branch
  git_reference *head = nullptr;

  int error = git_repository_head(&head, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (const char *localBranchName = git_reference_shorthand(head);
      !localBranchName) {
    git_reference_free(head);
    LOG_ERROR(Log::ErrorId::Git, "Can't get local branch name");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Step 2: Get upstream tracking branch
  git_reference *upstream = nullptr;

  error = git_branch_upstream(&upstream, head);
  if (error != 0) {
    git_reference_free(head);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Step 3: Fetch from remote
  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, "origin");
  if (error != 0) {
    git_reference_free(upstream);
    git_reference_free(head);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  int retries = 0;

  do {
    git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
    fetchOpts.callbacks.credentials = &credentialsAcquireCallback;
    fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;

    error = git_remote_fetch(remote, nullptr, &fetchOpts, nullptr);

    if (error) {
      const char *url = git_remote_url(remote);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < MAX_RETRIES);

  finishCredentialsAquaring();

  if (error) {
    git_remote_free(remote);
    git_reference_free(upstream);
    git_reference_free(head);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Step 4: Get annotated commit from upstream
  git_annotated_commit *upstreamCommit = nullptr;

  error = git_annotated_commit_from_ref(&upstreamCommit, repo, upstream);
  if (error) {
    git_remote_free(remote);
    git_reference_free(upstream);
    git_reference_free(head);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Step 5: Analyze merge
  git_merge_analysis_t analysis;
  git_merge_preference_t preference;

  error = git_merge_analysis(
      &analysis, &preference, repo,
      const_cast<const git_annotated_commit **>(&upstreamCommit), 1);
  if (error) {
    git_annotated_commit_free(upstreamCommit);
    git_remote_free(remote);
    git_reference_free(upstream);
    git_reference_free(head);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
    // Fast-forward
    const git_oid *targetOid = git_annotated_commit_id(upstreamCommit);
    if (!targetOid) {
      git_annotated_commit_free(upstreamCommit);
      git_remote_free(remote);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_ERROR(Log::ErrorId::Git, "Can't get target OID for fast-forward");
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit *targetCommit = nullptr;

    error = git_commit_lookup(&targetCommit, repo, targetOid);
    if (error) {
      git_annotated_commit_free(upstreamCommit);
      git_remote_free(remote);
      git_reference_free(upstream);
      git_reference_free(head);
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_checkout_tree(repo, (git_object *)targetCommit, &checkout_opts);
    if (error) {
      git_commit_free(targetCommit);
      git_annotated_commit_free(upstreamCommit);
      git_remote_free(remote);
      git_reference_free(upstream);
      git_reference_free(head);
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference *newHead = nullptr;

    error = git_reference_set_target(&newHead, head, targetOid, "Fast-forward");
    if (error) {
      git_commit_free(targetCommit);
      git_annotated_commit_free(upstreamCommit);
      git_remote_free(remote);
      git_reference_free(upstream);
      git_reference_free(head);
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(targetCommit);
    git_reference_free(newHead);
  } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
    // Normal merge
    git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_merge(
        repo, const_cast<const git_annotated_commit **>(&upstreamCommit), 1,
        &merge_opts, &checkout_opts);
    if (error) {
      git_annotated_commit_free(upstreamCommit);
      git_remote_free(remote);
      git_reference_free(upstream);
      git_reference_free(head);
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // You must commit manually after resolving conflicts
    // Or use git_commit_create() if no conflicts
  }

  // Cleanup
  git_annotated_commit_free(upstreamCommit);
  git_reference_free(upstream);
  git_reference_free(head);
  git_remote_free(remote);

  LOG_PERF(0, "Exit");

  return true;
}

} // namespace GitRepo
