
#include "GitRepo.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <LockGuard.h>
#include <Log.h>
#include <Uuid.h>

#include <git2/branch.h>
#include <git2/checkout.h>
#include <git2/clone.h>
#include <git2/commit.h>
#include <git2/graph.h>
#include <git2/index.h>
#include <git2/refs.h>
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

namespace GitRepo {

constexpr qint64 DEFAULT_CREDENTIALS_EXPIRATION_TIME =
    30 * 60 * 1000; // 30 minutes

struct GitPlaintextCredentials {
  QString username;
  QString password;
  qint64 createdTime;
};

Util::Mutex gGitCredentialsMutex;
QMap<QString, GitPlaintextCredentials> gGitCredentials;

static void logGitError(int error) {
  const git_error *gitError = git_error_last();
  if (!gitError || !gitError->message)
    return;

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

  git_repository_free(repo);

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

static QStringList listCheckoutBranches(git_repository *repo,
                                        const BranchType type) {
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

  QStringList activeBranches;
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

  LOG_PERF(0, "Exit");

  return activeBranches;
}

static QStringList listBranches(git_repository *repo, const BranchType type) {
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

  QStringList activeBranches;
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

    activeBranches += branchName;

    git_reference_free(ref);
  };

  git_branch_iterator_free(it);

  LOG_PERF(0, "Exit");

  return activeBranches;
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

QString getActiveBranch(git_repository *repo, const BranchType type) {
  LOG_PERF(0, "Enter");

  const QStringList checkoutBranches = listCheckoutBranches(repo, type);

  LOG_PERF(0, "Exit");

  return checkoutBranches.length() > 0 ? checkoutBranches.first() : QString();
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

  CloneProgress *progress = static_cast<CloneProgress *>(payload);
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
      progressEvent.addData(*progress);
      EventManager::EventManager::getInstance().send(progressEvent);
    }
  } else {
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(*progress);
    EventManager::EventManager::getInstance().send(progressEvent);
  }

  return 0;
}

static void checkoutProgressCallback(const char *path, size_t completed_steps,
                                     size_t total_steps, void *payload) {
  if (!payload)
    return;

  CloneProgress *progress = static_cast<CloneProgress *>(payload);
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
    progressEvent.addData(*progress);
    EventManager::EventManager::getInstance().send(progressEvent);
  }
}

static void finishFetchProgress(const QString &uuid) {
  CloneProgress progress;
  progress.uuid = uuid;
  progress.completedSteps = 100;
  progress.totalSteps = 100;
  progress.started = true;
  progress.finished = true;

  EventManager::Event progressEvent(
      EventManager::EventType::GitRepoManagerCloneRepoProgress);
  progressEvent.addData(progress);
  EventManager::EventManager::getInstance().send(progressEvent);
}

static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &url, const QString &username,
                   int allowedTypes) {
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
    case EventManager::EventType::GitRepoManagerGetCredentialsError: {
      const GitCredentialsErrorResponse response =
          event.data<GitCredentialsErrorResponse>();
      success = response.uuid == request.uuid;
      break;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsResult: {
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
      EventManager::EventType::GitRepoManagerGetCredentialsError,
      EventManager::EventType::GitRepoManagerGetCredentialsResult};

  const auto &[success, event] =
      eventManager.waitOneOf(token, types, predicate);

  if (success) {
    if (event.type() ==
        EventManager::EventType::GitRepoManagerGetCredentialsError) {
      LOG_PERF(0, "Exit");
      return std::make_tuple(false, GitPlaintextCredentials{});
    } else if (event.type() ==
               EventManager::EventType::GitRepoManagerGetCredentialsResult) {
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

static int credentialsAquireCallback(git_credential **out, const char *url,
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

        LOG_INFO("Aquired credentials for '{}'", credentials.username);

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

      LOG_INFO("Aquired credentials for '{}'", credentials.username);

      gGitCredentialsMutex.lock();

      gGitCredentials[url] = credentials;

      gGitCredentialsMutex.unlock();
    } else {
      LOG_PERF(0, "Exit");
      return -1;
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
    return -1;
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
  fetch_opts.callbacks.credentials = &credentialsAquireCallback;
  fetch_opts.callbacks.transfer_progress = &fetchProgressCallback;
  fetch_opts.callbacks.payload = &progressData;
  clone_opts.checkout_opts = checkout_opts;
  clone_opts.fetch_opts = fetch_opts;

  const int error =
      git_clone(&repo, url.toStdString().c_str(),
                destinationDir.toStdString().c_str(), &clone_opts);

  finishFetchProgress(uuid);

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
                            const BranchType type) {
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

  if ((type == BranchType::All || branchName == "*")) {
    git_branch_iterator *it = nullptr;
    git_reference *ref = nullptr;
    git_branch_t branchType;

    if (branchName == "*") {
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

    while (git_branch_next(&ref, &branchType, it) == 0) {
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

    error = git_revwalk_push_ref(
        walker, QString("refs/heads/%1").arg(branchName).toStdString().c_str());
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

  while (git_revwalk_next(&oid, walker) == 0) {
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

    std::size_t parentCount = git_commit_parentcount(commit);

    for (std::size_t i = 0; i < parentCount; ++i) {
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

  history_.valid = true;

  LOG_PERF(0, "Exit");

  return history_;
}

QStringList getCommitBranches(git_repository *repo, const QString &commitOid) {
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
  while (git_branch_next(&ref, &type, iter) == 0) {
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

QStringList getCommitHeads(git_repository *repo, const QString &commitOid) {
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
  while (git_branch_next(&ref, &type, iter) == 0) {
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

bool getCommitIsHead(git_repository *repo, const QString &commitOid) {
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
  while (git_branch_next(&ref, &type, iter) == 0) {
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

} // namespace GitRepo
