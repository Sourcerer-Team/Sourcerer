#include "GitRepo.h"
#include "GitCredentialStorage.h"
#include "GitKeyStorage.h"

#include "../../../component/GitRepoManager/include/GitRepoManagerStructs.h"

#include <CloudCredentialStorage.h>
#include <CloudRepo.h>
#include <ConfigManager.h>
#include <EventManager.h>
#include <JWTDecoder.h>
#include <LockGuard.h>
#include <Log.h>
#ifdef _WIN32
#include <applink.c>
#endif
#include <Uuid.h>

#include <git2.h>
#include <git2/blob.h>
#include <git2/branch.h>
#include <git2/cert.h>
#include <git2/checkout.h>
#include <git2/clone.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/graph.h>
#include <git2/index.h>
#include <git2/patch.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/revwalk.h>
#include <git2/signature.h>
#include <git2/submodule.h>
#include <git2/tag.h>
#include <git2/types.h>

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <cstring>
#include <set>
#include <thread>

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

struct GitCredentialPayload : public CloneProgress {
  QString repoPath;
};

Util::Mutex gGitCredentialsMutex;
QMap<QString, GitPlaintextCredentials> gGitCredentials;

Util::Mutex gErrorMutex;
QMap<std::thread::id, GitError> gErrorMap;

static void git_strarray_destroy(git_strarray &arr) {
  for (int i = 0; i < arr.count; ++i) {
    free(arr.strings[i]);
  }

  free(arr.strings);

  arr.strings = nullptr;
  arr.count = 0;
}

static git_strarray qstringlist_to_git_strarray(const QStringList &list) {
  git_strarray arr;
  arr.count = static_cast<size_t>(list.length());
  arr.strings = static_cast<char **>(malloc(arr.count * sizeof(char *)));

  for (size_t i = 0; i < arr.count; ++i) {
    arr.strings[i] = strdup(list[i].toStdString().c_str()); // allocates a copy
  }

  return arr;
}

static QString capitalizeFirst(const QString &str) {
  QString capitalized = str;
  if (!capitalized.isEmpty()) {
    capitalized[0] = capitalized[0].toUpper();
  }

  return capitalized;
}

static void logGitError(const int error) {
  const git_error *gitError = git_error_last();
  if (!gitError || !gitError->message)
    return;

  {
    LOCKGUARD(gErrorMutex);
    gErrorMap[std::this_thread::get_id()] = {
        (gitError->klass & 0x00ff) << 16 | (error & 0xffff),
        capitalizeFirst(gitError->message)};
  }

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

static void sendInvalidCredentialsSignal(const QString &url) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  EventManager::Event event(
      EventManager::EventType::GitRepoManagerGetCredentialsInvalid);
  event.addData(qHash(url));

  eventManager.send(event);
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
    git_index_free(index);
    git_signature_free(sig);
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
    git_tree_free(tree);
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree_free(tree);
  git_signature_free(sig);

  git_branch_iterator *it = nullptr;

  error = git_branch_iterator_new(&it, repo, GIT_BRANCH_LOCAL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!it) {
    LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
    LOG_PERF(0, "Exit");
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
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *newRef = nullptr;

  error = git_branch_move(&newRef, branchRef,
                          defaultBranchName.toStdString().c_str(), false);
  if (error) {
    logGitError(error);
    git_reference_free(branchRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(newRef);
  git_reference_free(branchRef);

  LOG_PERF(0, "Exit");
  return true;
}

static bool createInitialCommitWithoutStaging(git_repository *repo) {
  LOG_PERF(0, "Enter");

  git_signature *sig = nullptr;
  int error = git_signature_default(&sig, repo);

  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create an empty tree builder
  git_treebuilder *builder = nullptr;
  error = git_treebuilder_new(&builder, repo, nullptr);

  if (error) {
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write the empty tree
  git_oid tree_id;
  error = git_treebuilder_write(&tree_id, builder);
  git_treebuilder_free(builder);

  if (error) {
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Lookup the empty tree
  git_tree *tree = nullptr;
  error = git_tree_lookup(&tree, repo, &tree_id);

  if (error) {
    git_signature_free(sig);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create the initial commit with empty tree
  git_oid commit_id;
  error = git_commit_create_v(&commit_id, repo, HEAD, sig, sig, nullptr,
                              "Initial commit", tree, 0);

  git_tree_free(tree);
  git_signature_free(sig);

  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
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

static bool isCommercialPlan() {
  LOG_PERF(0, "Enter");

  return true;

  Util::CloudCredentialtorage &credentialStorage =
      Util::CloudCredentialtorage::getInstance();

  QString username;
  qint64 creationTime = -1;
  qint64 expirationTime = -1;
  if (!credentialStorage.getCredentialsInfo(username, creationTime,
                                            expirationTime)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get creation time for checking commercial plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  QString jwt;
  if (!credentialStorage.getJWT(jwt)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get JWT for checking commercial plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify JWT signature using public key before processing claims
  const QString publicKeyPem = Util::JWTDecoder::getJWTPublicKey();
  if (publicKeyPem.isEmpty()) {
    LOG_WARN("JWT public key is not configured, skipping signature "
             "verification. This should be configured for security.");
  } else {
    if (!Util::JWTDecoder::verifySignature(jwt, publicKeyPem)) {
      LOG_ERROR(Log::ErrorId::Security,
                "JWT signature verification failed for commercial plan check.");
      LOG_PERF(0, "Exit");
      return false;
    }
    LOG_DEBUG("JWT signature verification successful");
  }

  const Util::JWTDecoder::Claims claims = Util::JWTDecoder::decodeClaims(jwt);
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const bool creationTimeInTheFuture = creationTime > now;
  const bool storageExpired = expirationTime < now;
  const bool jwtExpired = claims.exp < now;

  LOG_PERF(0, "Exit");

  return !creationTimeInTheFuture && !storageExpired && !jwtExpired &&
         (claims.plan == "Pro" || claims.plan == "Business");
}

GitError getLastError() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Retrieving last error for current thread");

  LOCKGUARD(gErrorMutex);

  const auto threadId = std::this_thread::get_id();
  if (gErrorMap.contains(threadId)) {
    const auto &error = gErrorMap[threadId];
    LOG_DEBUG("Thread error code: {}, message: '{}'", error.errorCode,
              error.errorMessage.toStdString());
  } else {
    LOG_DEBUG("No error found for current thread");
  }

  LOG_PERF(0, "Exit");
  return gErrorMap[std::this_thread::get_id()];
}

void initForCurrentThread() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Initializing libgit2 for current thread");

  const int numberOfCalls = git_libgit2_init();

  LOG_DEBUG("Number of initializations is {}", numberOfCalls);
  LOG_DEBUG("libgit2 initialized successfully, total initialization count: {}",
            numberOfCalls);

  // Get timeout values from ConfigManager
  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong connectTimeout = configManager.getLong(
      ConfigManager::ConfigKey::GitRepoServerConnectTimeout);
  const qlonglong serverTimeout =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoServerTimeout);

  LOG_DEBUG("Setting server connect timeout to {} ms", connectTimeout);
  LOG_DEBUG("Setting server timeout to {} ms", serverTimeout);

  // Set SSH connection timeout (applies to initial connection establishment)
  git_libgit2_opts(GIT_OPT_SET_SERVER_CONNECT_TIMEOUT,
                   static_cast<int>(connectTimeout));

  // Set SSH timeout (for libssh2 operations and data transfer)
  git_libgit2_opts(GIT_OPT_SET_SERVER_TIMEOUT, static_cast<int>(serverTimeout));

  LOG_PERF(0, "Exit");
}

void shutdownForCurrentThread() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Shutting down libgit2 for current thread");

  const int numberofRemaining = git_libgit2_shutdown();

  LOG_DEBUG("Number of remaining initializations is {}", numberofRemaining);
  LOG_DEBUG("libgit2 shutdown complete, remaining init count: {}",
            numberofRemaining);

  LOG_PERF(0, "Exit");
}

void clearCachedCredentials(const QString &repoPath) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing cached credentials for repository: {}", repoPath);

  LOCKGUARD(gGitCredentialsMutex);

  // Clear all cached credentials to ensure fresh ones are loaded from storage
  // This is necessary when credentials are updated by the user
  gGitCredentials.clear();

  LOG_INFO("Cleared all cached credentials");
  LOG_PERF(0, "Exit");
}

bool applyStash(git_repository *repo, int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Applying stash at index: {}", index);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_stash_apply_options options;
  git_stash_apply_options_init(&options, GIT_STASH_APPLY_OPTIONS_VERSION);

  // Optional: customize behavior
  options.flags = GIT_STASH_APPLY_REINSTATE_INDEX; // restores index state

  if (const int error = git_stash_apply(repo, index, &options)) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to apply stash at index {}: error={}",
              index, error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Check for conflicts after applying stash
  git_index *index_ptr = nullptr;
  int error = git_repository_index(&index_ptr, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get repository index after stash apply: error={}",
              error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return true; // Stash was applied, just couldn't check for conflicts
  }

  if (git_index_has_conflicts(index_ptr)) {
    LOG_DEBUG("Stash at index {} applied but has conflicts - manual resolution "
              "needed",
              index);
    git_index_free(index_ptr);
    LOG_PERF(0, "Exit");
    return true; // Return true because stash was applied, conflicts need
                 // resolution
  }

  git_index_free(index_ptr);

  LOG_DEBUG("Stash at index {} applied successfully", index);
  LOG_PERF(0, "Exit");
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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool hasConflicts_result = git_index_has_conflicts(index_ptr) != 0;
  git_index_free(index_ptr);

  LOG_DEBUG("Repository has conflicts: {}", hasConflicts_result);
  LOG_PERF(0, "Exit");
  return hasConflicts_result;
}

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    const BranchType type) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to checkout branch/tag: '{}', type: {}",
            branchOrTagName.toStdString(), static_cast<int>(type));

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when checking out branch '{}'",
              branchOrTagName.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  if (type == BranchType::Local) {
    LOG_DEBUG("Checking out local branch: '{}'", branchOrTagName.toStdString());
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
      git_commit_free(targetCommit);
      git_reference_free(localRef);
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
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      logGitError(error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_commit_free(targetCommit);
    git_reference_free(localRef);
    git_tree_free(tree);
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
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 5. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      logGitError(error);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
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
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
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
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_tree_free(tree);
    git_reference_free(localRef);
    git_commit_free(targetCommit);
    git_reference_free(remoteRef);
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
  LOG_DEBUG("Opening repository at path: '{}'", path.toStdString());

  git_repository *repo = nullptr;

  if (const int error =
          git_repository_open(&repo, path.toStdString().c_str())) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open repository at '{}': error={}",
              path.toStdString(), error);
    logGitError(error);
    return nullptr;
  }

  LOG_DEBUG("Repository opened successfully at '{}'", path.toStdString());

  // Check if repository has no commits (HEAD is unborn)
  if (repo && git_repository_head_unborn(repo) == 1) {
    LOG_DEBUG("Repository has unborn HEAD, creating initial commit");
    // Create empty initial commit without staging existing files
    createInitialCommitWithoutStaging(repo);
    LOG_DEBUG("Initial commit created for new repository");
  }

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

    git_reference_free(headRef);
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

QVector<BranchInfo> listRemoteBranchesWithStatus(git_repository *repo,
                                                 const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QVector<BranchInfo> branchesWithStatus;

  // First get all remote branches
  const QStringList remoteBranches = listRemoteBranches(repo, quitFlag);

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // For each remote branch, calculate its status
  for (const QString &remoteBranchName : remoteBranches) {
    if (quitFlag) {
      break;
    }

    BranchInfo branchInfo;
    branchInfo.name = remoteBranchName;

    // Get the remote reference
    const QString fullRemoteRefName =
        QString("refs/remotes/%1").arg(remoteBranchName);
    git_reference *remoteRef = nullptr;
    int error = git_reference_lookup(&remoteRef, repo,
                                     fullRemoteRefName.toStdString().c_str());
    if (error) {
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      git_reference_free(remoteRef);
      branchesWithStatus.append(branchInfo);
      continue;
    }

    const git_oid *remoteRefTargetOid = git_reference_target(remoteRef);
    if (!remoteRefTargetOid) {
      // Remote reference does not point to a valid OID
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      git_reference_free(remoteRef);
      branchesWithStatus.append(branchInfo);
      continue;
    }

    // Get the remote OID
    git_oid remoteOid;
    git_oid_cpy(&remoteOid, remoteRefTargetOid);
    git_reference_free(remoteRef);

    // Extract the local branch name from the remote branch name
    // Remote branches have format: "origin/branchname"
    const auto parts = remoteBranchName.split('/');
    QString localBranchName;
    if (parts.length() > 1) {
      localBranchName = parts.last();
    } else {
      localBranchName = remoteBranchName;
    }

    // Try to get the local branch that tracks this remote branch
    git_reference *localRef = nullptr;
    error = git_branch_lookup(&localRef, repo,
                              localBranchName.toStdString().c_str(),
                              GIT_BRANCH_LOCAL);

    if (error) {
      // No local tracking branch exists
      branchInfo.statusInfo.status = BranchStatusType::NoUpstream;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      continue;
    }

    const git_oid *localRefTargetOid = git_reference_target(localRef);
    if (!localRefTargetOid) {
      // Local reference does not point to a valid OID
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      git_reference_free(localRef);
      continue;
    }

    // Get the local OID
    git_oid localOid;
    git_oid_cpy(&localOid, localRefTargetOid);
    git_reference_free(localRef);

    // Calculate ahead/behind
    size_t ahead = 0, behind = 0;
    error =
        git_graph_ahead_behind(&ahead, &behind, repo, &localOid, &remoteOid);

    if (error) {
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      logGitError(error);
      continue;
    }

    // Determine the status type
    if (ahead == 0 && behind == 0) {
      branchInfo.statusInfo.status = BranchStatusType::UpToDate;
    } else if (ahead > 0 && behind == 0) {
      branchInfo.statusInfo.status = BranchStatusType::Ahead;
    } else if (ahead == 0 && behind > 0) {
      branchInfo.statusInfo.status = BranchStatusType::Behind;
    } else {
      branchInfo.statusInfo.status = BranchStatusType::Diverged;
    }

    branchInfo.statusInfo.aheadCount = ahead;
    branchInfo.statusInfo.behindCount = behind;

    branchesWithStatus.append(branchInfo);
  }

  LOG_PERF(0, "Exit");

  return branchesWithStatus;
}

QStringList listStashes(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QStringList stashes;

  const auto callback = [](size_t index, const char *message,
                           const git_oid *stash_id, void *payload) {
    if (!payload) {
      return -1;
    }

    const auto payloadStashes = static_cast<QStringList *>(payload);
    if (message) {
      *payloadStashes += QString::fromUtf8(message);
    } else {
      *payloadStashes += QString("stash@{%1}").arg(index);
    }

    return 0;
  };

  if (const int error = git_stash_foreach(repo, callback, &stashes)) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_PERF(0, "Exit");

  return stashes;
}

GitCommitStatus getStashStatus(git_repository *repo, int stashIndex) {
  LOG_PERF(0, "Enter");

  GitCommitStatus status;

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get stash commit object
  git_object *stash_obj = nullptr;
  git_commit *stash_commit = nullptr;
  git_tree *stash_tree = nullptr;
  git_tree *parent_tree = nullptr;
  git_commit *parent_commit = nullptr;
  git_tree *parent1_tree = nullptr;
  git_tree *parent2_tree = nullptr;
  git_commit *parent1_commit = nullptr;
  git_commit *parent2_commit = nullptr;
  git_diff *diff = nullptr;

  // Use git_revparse_single to get the stash commit
  QString stashRef = QString("stash@{%1}").arg(stashIndex);
  int error =
      git_revparse_single(&stash_obj, repo, stashRef.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get stash ref at index {}: error={}", stashIndex,
              error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Convert to commit
  git_object *peeled_obj = nullptr;
  error = git_object_peel(&peeled_obj, stash_obj, GIT_OBJ_COMMIT);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to peel stash object to commit at index {}: error={}",
              stashIndex, error);
    logGitError(error);
    git_object_free(stash_obj);
    LOG_PERF(0, "Exit");
    return {};
  }

  stash_commit = (git_commit *)peeled_obj;
  git_object_free(stash_obj);

  // Get stash tree
  error = git_commit_tree(&stash_tree, stash_commit);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get stash tree: error={}", error);
    logGitError(error);
    git_commit_free(stash_commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Check if stash tree is empty (common for stash commits which are merge
  // commits) If empty, try to use parent 1 (index state) or parent 2 (untracked
  // files)
  git_tree *actual_working_tree = stash_tree;

  if (git_tree_entrycount(stash_tree) == 0) {
    LOG_DEBUG("Stash tree is empty, trying parent 1 (index state)");

    // Try to get parent 1 commit (the index state in the stash)
    error = git_commit_parent(&parent1_commit, stash_commit, 1);
    if (error == 0) {
      error = git_commit_tree(&parent1_tree, parent1_commit);
      if (error == 0 && git_tree_entrycount(parent1_tree) > 0) {
        LOG_DEBUG("Using parent 1 tree (index state) with {} entries",
                  git_tree_entrycount(parent1_tree));
        actual_working_tree = parent1_tree;
      } else if (error == 0) {
        LOG_DEBUG("Parent 1 tree is also empty, trying parent 2");
        git_tree_free(parent1_tree);
        parent1_tree = nullptr;
        git_commit_free(parent1_commit);
        parent1_commit = nullptr;

        // Try parent 2 (untracked files)
        error = git_commit_parent(&parent2_commit, stash_commit, 2);
        if (error == 0) {
          error = git_commit_tree(&parent2_tree, parent2_commit);
          if (error == 0 && git_tree_entrycount(parent2_tree) > 0) {
            LOG_DEBUG("Using parent 2 tree (untracked files) with {} entries",
                      git_tree_entrycount(parent2_tree));
            actual_working_tree = parent2_tree;
          }
        }
      }
    }
  }

  // Get parent commit (the commit the stash was based on)
  error = git_commit_parent(&parent_commit, stash_commit, 0);
  if (error && error != GIT_ENOTFOUND) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get stash parent: error={}", error);
    logGitError(error);
    git_tree_free(stash_tree);
    if (parent1_tree) {
      git_tree_free(parent1_tree);
    }
    if (parent2_tree) {
      git_tree_free(parent2_tree);
    }
    if (parent1_commit) {
      git_commit_free(parent1_commit);
    }
    if (parent2_commit) {
      git_commit_free(parent2_commit);
    }
    git_commit_free(stash_commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (error == GIT_ENOTFOUND) {
    parent_tree = nullptr; // Initial stash
  } else {
    error = git_commit_tree(&parent_tree, parent_commit);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to get parent tree: error={}",
                error);
      logGitError(error);
      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      git_commit_free(stash_commit);
      git_commit_free(parent_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  // Generate diff
  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.flags |= GIT_DIFF_INCLUDE_TYPECHANGE;

  error = git_diff_tree_to_tree(&diff, repo, parent_tree, actual_working_tree,
                                &opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to create diff for stash: error={}",
              error);
    logGitError(error);

    git_tree_free(stash_tree);
    if (parent1_tree) {
      git_tree_free(parent1_tree);
    }
    if (parent2_tree) {
      git_tree_free(parent2_tree);
    }
    if (parent_tree) {
      git_tree_free(parent_tree);
    }
    git_commit_free(stash_commit);
    if (parent1_commit) {
      git_commit_free(parent1_commit);
    }
    if (parent2_commit) {
      git_commit_free(parent2_commit);
    }
    if (parent_commit) {
      git_commit_free(parent_commit);
    }
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get stash commit OID
  const git_oid *oid = git_commit_id(stash_commit);
  char oid_str[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oid_str, GIT_OID_HEXSZ + 1, oid);
  status.commit = QString::fromUtf8(oid_str);

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
        case GIT_DELTA_UNMODIFIED:
        case GIT_DELTA_IGNORED:
        case GIT_DELTA_UNTRACKED:
        case GIT_DELTA_CONFLICTED:
          break;
        }

        return 0;
      },
      nullptr, // binary_cb
      nullptr, // hunk_cb
      nullptr, // line_cb
      &status);

  // Cleanup
  git_diff_free(diff);
  git_tree_free(stash_tree);

  if (parent1_tree) {
    git_tree_free(parent1_tree);
  }
  if (parent2_tree) {
    git_tree_free(parent2_tree);
  }
  if (parent_tree) {
    git_tree_free(parent_tree);
  }

  if (parent1_commit) {
    git_commit_free(parent1_commit);
  }
  if (parent2_commit) {
    git_commit_free(parent2_commit);
  }
  if (parent_commit) {
    git_commit_free(parent_commit);
  }

  git_commit_free(stash_commit);

  LOG_DEBUG(
      "Stash status retrieved for index {}: {} new, {} modified, {} deleted",
      stashIndex, status.newFiles.size(), status.modifiedFiles.size(),
      status.deletedFiles.size());
  LOG_PERF(0, "Exit");
  return status;
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

struct ListSubmodulesPayload {
  const char *repoPath = nullptr;
  QStringList submodules;
};

static int listSubmodulesCallback(git_submodule *sm, const char *name,
                                  void *payload) {
  if (!payload) {
    return 0;
  }

  auto *listPayload = static_cast<ListSubmodulesPayload *>(payload);
  Q_ASSERT(listPayload->repoPath);
  if (!listPayload->repoPath) {
    return 0;
  }

  const char *path = git_submodule_path(sm);
  if (!path) {
    return 0;
  }

  const QString fullPath =
      QString("%1/../%2").arg(listPayload->repoPath).arg(path);

  if (const QDir dir(fullPath); !dir.exists()) {
    return 0;
  }

  listPayload->submodules += name;

  return 0;
}

QStringList listSubmodules(git_repository *repo) {
  LOG_PERF(0, "Enter");

  ListSubmodulesPayload payload;
  payload.repoPath = git_repository_path(repo);

  const int error =
      git_submodule_foreach(repo, &listSubmodulesCallback, &payload);
  if (error) {
    logGitError(error);
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

    if (!headId || !workdirId) {
      info.statusInfo.status = SubmoduleStatusType::Error;
      git_submodule_free(submodule);
      submodulesWithStatus.push_back(info);
      continue;
    }

    // Compare the two OIDs
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

bool createBranchFromCommit(git_repository *repo, const QString &commit,
                            const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid oid;
  git_commit *commit_ = nullptr;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;
  error = git_branch_create(&ref, repo, branchName.toStdString().c_str(),
                            commit_, 0);
  if (error) {
    logGitError(error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);
  git_commit_free(commit_);

  // Set upstream to default repository remote with same branch name
  const QString defaultRemote = getDefaultRemote(repo);
  if (!defaultRemote.isEmpty()) {
    LOG_DEBUG("Setting upstream for new branch '{}' to {}/{}",
              branchName.toStdString(), defaultRemote.toStdString(),
              branchName.toStdString());
    setUpstream(repo, defaultRemote, branchName, branchName);
  } else {
    LOG_DEBUG(
        "No default remote configured, skipping upstream setup for new branch "
        "'{}'",
        branchName.toStdString());
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteStash(git_repository *repo, int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to delete stash at index: {}", stashIndex);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when deleting stash at index {}",
              stashIndex);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (const int error = git_stash_drop(repo, stashIndex)) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteAllStashes(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to delete all stashes");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when deleting all stashes");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get the count of stashes first
  size_t stashCount = 0;
  if (const int error = git_stash_foreach(
          repo,
          [](size_t index, const char *name, const git_oid *oid,
             void *payload) -> int {
            (*static_cast<size_t *>(payload))++;
            return 0;
          },
          &stashCount)) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Found {} stashes to delete", stashCount);

  // Delete stashes from highest index to lowest to avoid index shifting issues
  for (int i = static_cast<int>(stashCount) - 1; i >= 0; --i) {
    if (const int error = git_stash_drop(repo, i)) {
      LOG_WARN("Failed to delete stash at index {}: error code {}", i, error);
      logGitError(error);
      // Continue trying to delete other stashes even if one fails
    } else {
      LOG_DEBUG("Deleted stash at index {}", i);
    }
  }

  LOG_PERF(0, "Exit");
  return true;
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
  if (!payload || !stats) {
    return 0;
  }

  std::tuple<CloneProgress, GitCredentialPayload> *callbackPayload =
      static_cast<std::tuple<CloneProgress, GitCredentialPayload> *>(payload);
  auto &[clonePayload, _] = *callbackPayload;

  clonePayload.completedSteps = stats->indexed_objects;
  clonePayload.totalSteps = stats->total_objects;
  clonePayload.started = true;
  clonePayload.finished = false;
  clonePayload.stage = CloneProgressStageType::Fetch;

  if (stats->total_objects > 0) {
    const int percentage =
        ((stats->indexed_objects * 1.0f) / stats->total_objects) * 100;
    if (percentage > clonePayload.lastStepPercentage) {
      clonePayload.lastStepPercentage = percentage;
      EventManager::Event progressEvent(
          EventManager::EventType::GitRepoManagerCloneRepoProgress);
      progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
          clonePayload.uuid,
          GitRepoManager::Structs::GitRepoCommandType::Unknown,
          QVariant::fromValue(clonePayload)});
      EventManager::EventManager::getInstance().send(progressEvent);
    }
  } else {
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        clonePayload.uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(clonePayload)});
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

static int certificateCheckCallback(git_cert *cert, int valid, const char *host,
                                    void *payload) {
  if (!cert || !host) {
    LOG_ERROR(Log::ErrorId::Network,
              "Certificate check called with null cert or host");
    return GIT_ECERTIFICATE;
  }

  const char *certTypeStr = nullptr;
  switch (cert->cert_type) {
  case GIT_CERT_X509:
    certTypeStr = "X.509";
    break;
  case GIT_CERT_HOSTKEY_LIBSSH2:
    certTypeStr = "SSH hostkey";
    break;
  case GIT_CERT_STRARRAY:
    certTypeStr = "certificate array";
    break;
  default:
    certTypeStr = "unknown type";
    break;
  }

  LOG_INFO("Certificate check for host: '{}', type: {}, valid: {}", host,
           certTypeStr, valid);

  // If the certificate is valid according to libgit2, accept it
  if (valid) {
    LOG_DEBUG("Certificate for host '{}' is valid", host);
    return 0; // Accept
  }

  // Certificate is not valid - log warning with details
  LOG_WARN("Certificate validation failed for host '{}' (type: {}). "
           "Accepting anyway to maintain connectivity. "
           "In production, consider enabling strict SSL verification.",
           host, certTypeStr);

  // For X.509 certificates, perform detailed validation
  if (cert->cert_type == GIT_CERT_X509) {
    git_cert_x509 *x509_cert = reinterpret_cast<git_cert_x509 *>(cert);
    if (x509_cert && x509_cert->data && x509_cert->len > 0) {
      LOG_DEBUG("X.509 certificate data present (size: {} bytes)",
                x509_cert->len);

      // Parse the certificate using OpenSSL
      const unsigned char *cert_data =
          reinterpret_cast<const unsigned char *>(x509_cert->data);
      X509 *x509 = d2i_X509(nullptr, &cert_data, x509_cert->len);

      if (x509) {
        // Get subject name
        char subject[256] = {0};
        X509_NAME_oneline(X509_get_subject_name(x509), subject,
                          sizeof(subject));

        // Get issuer name
        char issuer[256] = {0};
        X509_NAME_oneline(X509_get_issuer_name(x509), issuer, sizeof(issuer));

        LOG_INFO("Certificate subject: {}", subject);
        LOG_INFO("Certificate issuer: {}", issuer);

        // Check if it's self-signed
        bool isSelfSigned = (strcmp(subject, issuer) == 0);
        if (isSelfSigned) {
          LOG_WARN("Certificate for '{}' is self-signed", host);
        }

        // Perform certificate chain validation using OpenSSL
        X509_STORE *store = X509_STORE_new();
        if (store) {
          // Load system default CA certificates
          if (X509_STORE_set_default_paths(store) == 1) {
            LOG_DEBUG("Loaded system CA certificates");

            // Create verification context
            X509_STORE_CTX *ctx = X509_STORE_CTX_new();
            if (ctx) {
              // Initialize context with certificate and store
              if (X509_STORE_CTX_init(ctx, store, x509, nullptr) == 1) {
                // Set hostname for verification
                X509_VERIFY_PARAM *param = X509_STORE_CTX_get0_param(ctx);
                if (param) {
                  // Set hostname verification flags
#ifdef X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS
                  X509_VERIFY_PARAM_set_hostflags(
                      param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
#else
                  X509_VERIFY_PARAM_set_hostflags(param, 0);
#endif
                  X509_VERIFY_PARAM_set1_host(param, host, 0);
                }

                // Perform verification
                int verifyResult = X509_verify_cert(ctx);
                if (verifyResult == 1) {
                  LOG_INFO(
                      "Certificate chain validation successful for host '{}'",
                      host);
                } else {
                  int error = X509_STORE_CTX_get_error(ctx);
                  const char *errorStr = X509_verify_cert_error_string(error);
                  LOG_WARN(
                      "Certificate chain validation failed for host '{}': {} "
                      "(error code: {})",
                      host, errorStr, error);

                  // Log additional details about the failure
                  if (error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
                      error == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) {
                    LOG_WARN("Certificate chain contains self-signed "
                             "certificate");
                  } else if (error == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT ||
                             error ==
                                 X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
                    LOG_WARN("Unable to verify certificate issuer");
                  } else if (error == X509_V_ERR_CERT_HAS_EXPIRED ||
                             error == X509_V_ERR_CERT_NOT_YET_VALID) {
                    LOG_WARN("Certificate validity period issue");
                  }
                }
              }
              X509_STORE_CTX_free(ctx);
            } else {
              LOG_WARN("Failed to create X509_STORE_CTX");
            }
          } else {
            LOG_WARN("Failed to load system CA certificates");
          }
          X509_STORE_free(store);
        } else {
          LOG_WARN("Failed to create X509_STORE");
        }

        X509_free(x509);
      } else {
        LOG_WARN("Failed to parse X.509 certificate data");
      }
    }
  }

  // TODO: Add configuration option to enable strict SSL verification
  // For now, accept invalid certificates to maintain compatibility
  // with self-signed certificates and development environments

  // Return 0 to accept the certificate
  // Return GIT_ECERTIFICATE to reject it
  return 0; // Accept even if invalid
}

static void finishFetchProgress(const QString &uuid, const bool error,
                                const bool cloning) {
  CloneProgress progress;
  progress.uuid = uuid;
  progress.completedSteps = 100;
  progress.totalSteps = 100;
  progress.started = true;
  progress.finished = true;
  progress.error = error;
  progress.cloning = cloning;

  EventManager::Event progressEvent(
      EventManager::EventType::GitRepoManagerCloneRepoProgress);
  progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
      uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
      QVariant::fromValue(progress)});
  EventManager::EventManager::getInstance().send(progressEvent);
}

static void finishCredentialsAcquiring(const QString &url) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  EventManager::Event credentialsFinishedEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsFinished);
  credentialsFinishedEvent.addData(qHash(url));

  eventManager.send(credentialsFinishedEvent);
}

static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &repoPath, const QString &url,
                   const QString &username, const int allowedTypes) {
  LOG_PERF(0, "Enter");

  const QString normalizedRepoPath = QDir::fromNativeSeparators(repoPath);

  LOG_INFO("acquireCredentials called with repoPath: '{}', url: '{}'",
           normalizedRepoPath, url);

  // First, try to get stored credentials from encrypted storage
  GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();

  QString storedUsername, storedPassword;
  if (credStorage.getCredentials(normalizedRepoPath, storedUsername,
                                 storedPassword)) {
    LOG_INFO("Found stored credentials for repository: {}", normalizedRepoPath);

    // Check if credentials are expired
    const qlonglong expirationTimeout =
        ConfigManager::ConfigManager::getInstance().getLong(
            ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime);

    LOG_INFO("Expiration timeout: {} ms", expirationTimeout);

    if (!credStorage.areCredentialsExpired(normalizedRepoPath,
                                           expirationTimeout)) {
      LOG_INFO("Using stored credentials for repository: {}",
               normalizedRepoPath);
      LOG_PERF(0, "Exit");
      return std::make_tuple(
          true, GitPlaintextCredentials{storedUsername, storedPassword,
                                        QDateTime::currentMSecsSinceEpoch()});
    } else {
      LOG_INFO("Stored credentials expired for repository: {}",
               normalizedRepoPath);
    }
  } else {
    LOG_INFO("No stored credentials found for repository: {}",
             normalizedRepoPath);
  }

  // If no stored credentials or expired, request from user
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  const int token = eventManager.startWait();

  GitCredentialsRequest request;
  request.id = qHash(url);
  request.url = url;
  request.username = username;
  request.allowedTypes = allowedTypes;
  request.repoPath = normalizedRepoPath;

  EventManager::Event credentialsEvent(
      EventManager::EventType::GitRepoManagerGetCredentials);
  credentialsEvent.addData(request);

  eventManager.send(credentialsEvent);

  const auto predicate = [&](EventManager::Event event) -> bool {
    bool success = false;
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
      const auto [id] = event.data<GitCredentialsErrorResponse>();
      success = id == request.id;
      break;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();
      success = response.id == request.id;
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

      LOG_INFO("Received credentials response: username='{}', "
               "rememberCredentials={}",
               response.username, response.rememberCredentials);

      // Store credentials in encrypted storage if user wants to remember them
      if (response.rememberCredentials) {
        GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();
        credStorage.storeCredentials(normalizedRepoPath, response.username,
                                     response.password, true);
        LOG_INFO("Stored credentials for repository: {}", normalizedRepoPath);
      } else {
        LOG_INFO("User chose NOT to remember credentials");
      }

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

static bool validateSshKey(const QString &privateKeyPath,
                           const QString &publicKeyPath, bool passphrasePresent,
                           const QString &passphrase) {
  LOG_PERF(0, "Enter");

  // Check if files exist
  if (!QFile::exists(privateKeyPath)) {
    LOG_ERROR(Log::ErrorId::Git, "SSH private key not found: {}",
              privateKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!QFile::exists(publicKeyPath)) {
    LOG_ERROR(Log::ErrorId::Git, "SSH public key not found: {}", publicKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate private key using OpenSSL
  FILE *privateKeyFile = fopen(privateKeyPath.toStdString().c_str(), "r");
  if (!privateKeyFile) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open SSH private key: {}",
              privateKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY *privateKey = PEM_read_PrivateKey(
      privateKeyFile, nullptr, nullptr,
      !passphrasePresent ? nullptr : (void *)passphrase.toStdString().c_str());
  fclose(privateKeyFile);

  if (!privateKey) {
    unsigned long err = ERR_get_error();
    char errBuf[256];
    ERR_error_string_n(err, errBuf, sizeof(errBuf));
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to read SSH private key (OpenSSL error): {}", errBuf);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate that it's a valid key by checking if we can get its parameters
  const int keyType = EVP_PKEY_base_id(privateKey);
  if (keyType != EVP_PKEY_RSA && keyType != EVP_PKEY_DSA &&
      keyType != EVP_PKEY_EC && keyType != EVP_PKEY_ED25519) {
    LOG_ERROR(Log::ErrorId::Git, "SSH private key has unsupported type: {}",
              keyType);
    EVP_PKEY_free(privateKey);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate public key
  FILE *publicKeyFile = fopen(publicKeyPath.toStdString().c_str(), "r");
  if (!publicKeyFile) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open SSH public key: {}",
              publicKeyPath);
    EVP_PKEY_free(privateKey);
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY *publicKey =
      PEM_read_PUBKEY(publicKeyFile, nullptr, nullptr, nullptr);
  fclose(publicKeyFile);

  if (!publicKey) {
    // Public key might be in SSH format, not PEM - this is acceptable
    LOG_INFO("SSH public key is not in PEM format (likely SSH format): {}",
             publicKeyPath);
    EVP_PKEY_free(privateKey);
    LOG_PERF(0, "Exit");
    return true; // Accept SSH format public keys
  }

  // Verify that public and private keys match
  const int keyTypePublic = EVP_PKEY_base_id(publicKey);
  if (keyType != keyTypePublic) {
    LOG_ERROR(Log::ErrorId::Git,
              "SSH key pair type mismatch: private={}, public={}", keyType,
              keyTypePublic);
    EVP_PKEY_free(privateKey);
    EVP_PKEY_free(publicKey);
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY_free(privateKey);
  EVP_PKEY_free(publicKey);

  LOG_INFO("SSH key pair validated successfully: {}", privateKeyPath);
  LOG_PERF(0, "Exit");
  return true;
}

static int credentialsAcquireCallback(git_credential **out, const char *url,
                                      const char *username_from_url,
                                      const unsigned int allowed_types,
                                      void *payload) {
  LOG_PERF(0, "Enter");

  if (!out || !url) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  if (((allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) == 0) &&
      ((allowed_types & GIT_CREDENTIAL_SSH_KEY) == 0)) {
    LOG_PERF(0, "Exit");
    return 1;
  }

  // Extract repository path from payload
  QString repoPath;
  if (payload) {
    std::tuple<CloneProgress, GitCredentialPayload> *callbackPayload =
        static_cast<std::tuple<CloneProgress, GitCredentialPayload> *>(payload);
    const auto &[_, credentialsPayload] = *callbackPayload;
    repoPath = credentialsPayload.repoPath;
  }

  if (const QString urlString = QString::fromUtf8(url);
      urlString.startsWith("git@")) {
    // Extract host from git URL (e.g., "git@github.com:user/repo.git" ->
    // "github.com")
    QString host;
    const QRegularExpression hostRegex("^git@([^:]+):");
    const QRegularExpressionMatch match = hostRegex.match(urlString);
    if (match.hasMatch()) {
      host = match.captured(1);
      LOG_DEBUG("Extracted host '{}' from URL", host);
    }

    QString privateKeyPath;
    QString publicKeyPath;

    // Try to get key pair from GitKeyStorage for this specific host
    GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
    GitKeyPair keyPair;

    if (!host.isEmpty() && keyStorage.getKeyPairForHost(host, keyPair)) {
      privateKeyPath = keyPair.privateKeyPath;
      publicKeyPath = keyPair.publicKeyPath;
      LOG_INFO("Using SSH key '{}' for host '{}'", keyPair.keyName, host);
    } else {
      // Fall back to default SSH key
      const QString homePath =
          QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
      privateKeyPath = QString("%1/.ssh/id_ed25519").arg(homePath);
      publicKeyPath = QString("%1/.ssh/id_ed25519.pub").arg(homePath);
      LOG_INFO(
          "No specific SSH key configured for host '{}', using default: {}",
          host.isEmpty() ? "unknown" : host, privateKeyPath);
    }

    // Check if the private key is encrypted (requires passphrase)
    bool passphrasePresent = false;
    QString passphrase;
    QFile keyFile(privateKeyPath);
    if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString keyContent = QString::fromUtf8(keyFile.readAll());
      keyFile.close();

      // Check for encryption markers in the key file
      if (keyContent.contains("ENCRYPTED", Qt::CaseInsensitive)) {
        // Key is encrypted, need to ask for passphrase
        LOG_INFO("SSH key is encrypted, requesting passphrase from user");

        // Request passphrase from user using the event system
        EventManager::EventManager &eventManager =
            EventManager::EventManager::getInstance();

        const int token = eventManager.startWait();

        GitCredentialsRequest request;
        request.id = qHash(QString::fromUtf8(url));
        request.url = url;
        request.username = username_from_url ? username_from_url : "git";
        request.allowedTypes = allowed_types;
        request.repoPath = repoPath;
        request.needsPassphrase =
            true; // Signal that this is a passphrase request

        EventManager::Event credentialsEvent(
            EventManager::EventType::GitRepoManagerGetCredentials);
        credentialsEvent.addData(request);

        eventManager.send(credentialsEvent);

        const auto predicate = [&](EventManager::Event event) -> bool {
          bool success = false;
          switch (event.type()) {
          case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
            const auto [id] = event.data<GitCredentialsErrorResponse>();
            success = id == request.id;
            break;
          }
          case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
            const GitCredentialsResponse response =
                event.data<GitCredentialsResponse>();
            success = response.id == request.id;
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
            LOG_ERROR(Log::ErrorId::Git,
                      "User cancelled passphrase request for SSH key");
            LOG_PERF(0, "Exit");
            return GIT_EUSER;
          } else if (event.type() == EventManager::EventType::
                                         GitRepoManagerGetCredentialsSubmit) {
            const GitCredentialsResponse response =
                event.data<GitCredentialsResponse>();

            // Use the passphrase field if available, otherwise fall back to
            // password field
            passphrasePresent = response.needPassphrase;
            passphrase = !response.passphrase.isEmpty() ? response.passphrase
                                                        : response.password;
            LOG_INFO("Received passphrase for SSH key");
          }
        } else {
          LOG_ERROR(Log::ErrorId::Git, "Failed to get passphrase response");
          LOG_PERF(0, "Exit");
          return GIT_EUSER;
        }
      }
    }

    // Validate SSH keys using OpenSSL before attempting to use them
    if (!validateSshKey(privateKeyPath, publicKeyPath, passphrasePresent,
                        passphrase)) {
      LOG_ERROR(Log::ErrorId::Git,
                "SSH key validation failed for repository: {}", url);
      LOG_PERF(0, "Exit");
      return -1;
    }

    const int error = git_credential_ssh_key_new(
        out, username_from_url ? username_from_url : "git",
        publicKeyPath.toStdString().c_str(),
        privateKeyPath.toStdString().c_str(),
        passphrase.isEmpty() ? nullptr : passphrase.toStdString().c_str());

    LOG_PERF(0, "Exit");
    return error;
  }

  gGitCredentialsMutex.lock();

  if (const auto it = gGitCredentials.find(url); it != gGitCredentials.end()) {
    gGitCredentialsMutex.unlock();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qlonglong expirationTimeout =
        ConfigManager::ConfigManager::getInstance().getLong(
            ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime);
    if (it->createdTime + expirationTimeout < now) {
      const auto &[success, credentials] = acquireCredentials(
          repoPath, url, username_from_url ? username_from_url : "",
          allowed_types);
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
        repoPath, url, username_from_url ? username_from_url : "",
        allowed_types);
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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup commit '{}': error={}",
              commit.toStdString(), error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Commit '{}' found, retrieving tree", commit.toStdString());

  git_tree *tree = nullptr;
  error = git_commit_tree(&tree, commit_);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get tree from commit: error={}",
              error);
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting repository clone operation");
  LOG_DEBUG("  URL: '{}'", url.toStdString());
  LOG_DEBUG("  Destination directory: '{}'", destinationDir.toStdString());
  LOG_DEBUG("  UUID: '{}'", uuid.toStdString());

  const QDir workingDir;
  const bool workingDirExisted = workingDir.exists(destinationDir);
  LOG_DEBUG("Destination directory already existed: {}", workingDirExisted);

  if (!workingDir.mkpath(destinationDir)) {
    LOG_ERROR(Log::ErrorId::Git, "can't create destination dir '{}'",
              destinationDir.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Destination directory created/validated successfully");

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cloning git@ repositories is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository *repo = nullptr;
  git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
  git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = true;

  GitCredentialPayload credPayload;
  credPayload.repoPath = destinationDir;
  credPayload.uuid = uuid;
  credPayload.cloning = true;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  checkout_opts.progress_payload = &progressData;
  checkout_opts.progress_cb = &checkoutProgressCallback;
  fetch_opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetch_opts.callbacks.transfer_progress = &fetchProgressCallback;
  fetch_opts.callbacks.certificate_check = &certificateCheckCallback;
  fetch_opts.callbacks.payload = &callbackPayload;
  clone_opts.checkout_opts = checkout_opts;
  clone_opts.fetch_opts = fetch_opts;

  LOG_DEBUG("Initialized clone options, starting git_clone operation");
  const int error =
      git_clone(&repo, url.toStdString().c_str(),
                destinationDir.toStdString().c_str(), &clone_opts);

  LOG_DEBUG("git_clone returned with error code: {}", error);

  finishFetchProgress(uuid, error != 0, true);

  if (commercial) {
    finishCredentialsAcquiring(url);
  }

  if (error || !repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Clone failed: error code={}, repo pointer null={}", error,
              !repo);
    invalidateCredentials(url);

    if (!workingDirExisted) {
      LOG_DEBUG("Cleaning up destination directory as clone failed");
      QDir dirToRemove(destinationDir);
      if (!dirToRemove.removeRecursively()) {
        LOG_WARN(
            "Failed to clean up destination directory after clone failure");
      }
    }

    logGitError(error);
    LOG_ERROR(Log::ErrorId::Git, "Repository clone failed for URL: {}",
              url.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Repository cloned successfully");
  git_repository_free(repo);
  LOG_INFO("Clone operation completed successfully for URL: {}",
           url.toStdString());

  LOG_PERF(0, "Exit");
  return true;
}

GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            const BranchType type, const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting branch history: branchName='{}', type={}",
            branchName.toStdString(), static_cast<int>(type));

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

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
        git_reference_free(ref);
        git_revwalk_free(walker);
        git_branch_iterator_free(it);
        LOG_PERF(0, "Exit");
        return {};
      }

      if (!currentBranchName) {
        git_reference_free(ref);
        git_branch_iterator_free(it);
        git_revwalk_free(walker);
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
        git_reference_free(ref);
        git_branch_iterator_free(it);
        git_revwalk_free(walker);
        LOG_ERROR(Log::ErrorId::Git, "Unknown branch type!");
        LOG_PERF(0, "Exit");
        return {};
      }

      error =
          git_revwalk_push_ref(walker, fullBranchName.toStdString().c_str());
      if (error) {
        logGitError(error);
        git_reference_free(ref);
        git_branch_iterator_free(it);
        git_revwalk_free(walker);
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
        git_commit_free(commit);
        git_revwalk_free(walker);
        LOG_PERF(0, "Exit");
        return {};
      }

      str = git_oid_tostr_s(parentOid);
      if (!str) {
        LOG_ERROR(
            Log::ErrorId::Git,
            "Can't convert parent oid at index {} for oid \"{}\" to string.", i,
            oidStr);
        git_commit_free(commit);
        git_revwalk_free(walker);
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
      git_commit_free(commit);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (const char *summary = git_commit_summary(commit))
      history_.commitInfos[oidStr].summary = summary;
    else {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit summary for oid \"{}\"",
                oidStr);
      git_commit_free(commit);
      git_revwalk_free(walker);
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
      git_commit_free(commit);
      git_revwalk_free(walker);
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

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  if (commitOid.isEmpty()) {
    LOG_WARN("Commit \"{}\" is empty", commitOid);
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_LOCAL);
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

      branchOid = git_object_id(branchTip);

      git_object_free(branchTip);
      git_reference_free(symbolicRef);
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

QStringList getCommitTags(git_repository *repo, const QString &commitOid,
                          const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList tags;
  git_strarray array = {nullptr, 0};

  const int error = git_tag_list_match(&array, "", repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  const QByteArray commitOidUtf8 = commitOid.toUtf8();

  for (size_t i = 0; i < array.count && !quitFlag; ++i) {
    const QString tagName = array.strings[i];
    git_object *object = nullptr;

    const QString spec = QStringLiteral("refs/tags/") + tagName;

    if (git_revparse_single(&object, repo, spec.toStdString().c_str()) != 0) {
      continue;
    }

    git_object *commitObject = nullptr;
    if (git_object_peel(&commitObject, object, GIT_OBJECT_COMMIT) == 0) {
      const git_oid *commitOidValue = git_object_id(commitObject);
      if (commitOidValue &&
          git_oid_streq(commitOidValue, commitOidUtf8.constData())) {
        tags.append(tagName);
      }
      git_object_free(commitObject);
    }

    git_object_free(object);
  }

  git_strarray_dispose(&array);

  LOG_PERF(0, "Exit");
  return tags;
}

QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
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

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  size_t count = git_status_list_entrycount(statusList);
  LOG_DEBUG("Found {} status entries", count);

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

GitCommitStatus getCommitStatus(git_repository *repo,
                                const QString &commitOid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

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
      git_tree_free(tree);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parent_tree, parent);
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_commit_free(parent);
      git_commit_free(commit);
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

    git_tree_free(tree);

    if (parent_tree) {
      git_tree_free(parent_tree);
    }

    if (parent) {
      git_commit_free(parent);
    }

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

    git_diff_free(diff);

    if (parent_tree) {
      git_tree_free(parent_tree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit);
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

  // Cleanup in reverse order of allocation
  git_diff_free(diff);

  if (parent_tree) {
    git_tree_free(parent_tree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit);

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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString repoPath = git_repository_workdir(repo);
  LOG_DEBUG("Repository working directory: '{}'", repoPath.toStdString());

  for (const auto &filePath : files) {
    const QString unixFilePath = QDir::fromNativeSeparators(filePath);
    const QString fullPath = repoPath + unixFilePath;
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

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

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
                  const QString &email, bool cleanup) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Creating commit: summary='{}', author='{}', email='{}'",
            summary.toStdString(), author.toStdString(), email.toStdString());
  LOG_DEBUG("Message length: {}, cleanup: {}", message.length(), cleanup);

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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write index to tree
  error = git_index_write_tree(&treeId, index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to write index to tree: error={}",
              error);
    logGitError(error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeId);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup tree: error={}", error);
    logGitError(error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Tree prepared successfully");

  // Create signature
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    logGitError(error);
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
      logGitError(error);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_commit_lookup(&parent, repo, git_reference_target(headRef));
    if (error) {
      logGitError(error);
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
        logGitError(error);
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
        logGitError(error);
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

  const QString fullMessage = summary + "\n\n" + message;

  // Create commit with appropriate number of parents
  size_t parentCount = parent ? 1 : 0;
  if (isMerge && parent2) {
    parentCount = 2;
    LOG_DEBUG("Creating merge commit with 2 parents");
  }

  error = git_commit_create_v(&commitId, repo, "HEAD", sig, sig, nullptr,
                              fullMessage.toStdString().c_str(), tree,
                              parentCount, parent, parent2);
  if (error) {
    logGitError(error);

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
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");

  return true;
}

static QString createFullPatchFromContent(const QString &filePath,
                                          const QString &fileContent,
                                          bool newFile,
                                          const QString &indexLine) {
  LOG_PERF(0, "Enter");

  const QString convertedContent = QString(fileContent).replace("\r\n", "\n");
  QStringList lines = convertedContent.split('\n');
  if (!convertedContent.isEmpty() && convertedContent.endsWith('\n')) {
    lines.removeLast();
  }
  const int lineCount = lines.size();

  QString patch;
  QTextStream stream(&patch);

  if (!indexLine.isEmpty()) {
    stream << indexLine << "\n";
  }

  if (newFile) {
    stream << "--- /dev/null\n";
  } else {
    stream << "--- a/" << filePath << "\n";
  }

  stream << "+++ b/" << filePath << "\n";
  stream << "@@ -1,0 +1," << lineCount << " @@\n";

  for (const QString &line : lines) {
    stream << "+" << line << "\n";
  }

  LOG_PERF(0, "Exit");

  return patch;
}

static QString createIndexLineFromContent(const QString &oldFileContent,
                                          const QString &newFileContent) {

    if (oldFileContent.isEmpty() && newFileContent.isEmpty()) {
        return {};
    }

    if (oldFileContent.isEmpty()) {
        const QString normalizedNewContent =
                QString(newFileContent).replace("\r\n", "\n");

        git_oid newOid;

        const QByteArray newBytes = normalizedNewContent.toUtf8();

        git_odb_hash(&newOid, newBytes.constData(),
                     static_cast<size_t>(newBytes.size()), GIT_OBJECT_BLOB);

        char newOidStr[GIT_OID_HEXSZ + 1] = {0};

        git_oid_tostr(newOidStr, sizeof(newOidStr), &newOid);

        return QString("index 0000000..%2")
                .arg(QString::fromUtf8(newOidStr).left(7));
  }

  if (newFileContent.isEmpty()) {
      const QString normalizedOldContent =
              QString(oldFileContent).replace("\r\n", "\n");

      git_oid oldOid;

      const QByteArray oldBytes = normalizedOldContent.toUtf8();

      git_odb_hash(&oldOid, oldBytes.constData(),
                   static_cast<size_t>(oldBytes.size()), GIT_OBJECT_BLOB);

      char oldOidStr[GIT_OID_HEXSZ + 1] = {0};

      git_oid_tostr(oldOidStr, sizeof(oldOidStr), &oldOid);

      return QString("index %1..0000000")
              .arg(QString::fromUtf8(oldOidStr).left(7));
  }

  const QString normalizedOldContent =
          QString(oldFileContent).replace("\r\n", "\n");
  const QString normalizedNewContent =
          QString(newFileContent).replace("\r\n", "\n");

  git_oid oldOid;
  git_oid newOid;

  const QByteArray oldBytes = normalizedOldContent.toUtf8();
  const QByteArray newBytes = normalizedNewContent.toUtf8();

  git_odb_hash(&oldOid, oldBytes.constData(),
               static_cast<size_t>(oldBytes.size()), GIT_OBJECT_BLOB);
  git_odb_hash(&newOid, newBytes.constData(),
               static_cast<size_t>(newBytes.size()), GIT_OBJECT_BLOB);

  char oldOidStr[GIT_OID_HEXSZ + 1] = {0};
  char newOidStr[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oldOidStr, sizeof(oldOidStr), &oldOid);
  git_oid_tostr(newOidStr, sizeof(newOidStr), &newOid);

  return QString("index %1..%2")
          .arg(QString::fromUtf8(oldOidStr).left(7),
               QString::fromUtf8(newOidStr).left(7));
}

static FilesDiffDelta
createFullPatchDeltasFromContent(const QString &filePath,
                                 const QString &fileContent) {
  LOG_PERF(0, "Enter");

  const QString convertedContent = QString(fileContent).replace("\r\n", "\n");
  QStringList lines = convertedContent.split('\n');
  if (!convertedContent.isEmpty() && convertedContent.endsWith('\n')) {
    lines.removeLast();
  }

  FilesDiffDelta delta;
  delta.oldFilePath = filePath;
  delta.newFilePath = filePath;

  FilesDiffHunk hunk;
  hunk.oldStart = 1;
  hunk.newStart = 1;
  hunk.oldLines = 0;
  hunk.newLines = lines.size();

  for (const QString &line : lines) {
    FilesDiffLine diffLine;
    diffLine.origin = '+';
    diffLine.content = line;
    hunk.lines.append(diffLine);
  }

  delta.hunks.append(hunk);

  LOG_PERF(0, "Exit");

  return delta;
}

FilesDiff diffFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

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
    git_commit_free(commit);
    git_object_free(headCommit);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit_free(commit);
  git_object_free(headCommit);

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

  QString diffOutput;

  QVector<FilesDiffDelta> filesDeltas;

  size_t num_deltas = git_diff_num_deltas(diff);
  for (size_t i = 0; i < num_deltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(diff, i);
    diffOutput += QString("diff --git a/%1 b/%2\n")
                      .arg(delta->old_file.path, delta->new_file.path);

    FilesDiffDelta filesDiffDelta;
    filesDiffDelta.oldFilePath = delta->old_file.path;
    filesDiffDelta.newFilePath = delta->new_file.path;

    git_patch *patch;
    error = git_patch_from_diff(&patch, diff, i);
    if (error) {
      logGitError(error);
      continue;
    }

    const size_t num_hunks = git_patch_num_hunks(patch);
    for (size_t h = 0; h < num_hunks; ++h) {
      const git_diff_hunk *hunk;
      error = git_patch_get_hunk(&hunk, NULL, patch, h);
      if (error) {
        logGitError(error);
        continue;
      }

      diffOutput += QString("@@ -%1,%2 +%3,%4 @@\n")
                        .arg(hunk->old_start)
                        .arg(hunk->old_lines)
                        .arg(hunk->new_start)
                        .arg(hunk->new_lines);

      FilesDiffHunk filesDiffHunk;
      filesDiffHunk.oldStart = hunk->old_start;
      filesDiffHunk.oldLines = hunk->old_lines;
      filesDiffHunk.newStart = hunk->new_start;
      filesDiffHunk.newLines = hunk->new_lines;

      size_t num_lines = git_patch_num_lines_in_hunk(patch, h);
      for (size_t l = 0; l < num_lines; ++l) {
        const git_diff_line *line;
        error = git_patch_get_line_in_hunk(&line, patch, h, l);
        if (error) {
          logGitError(error);
          continue;
        }

        const char *start = line->content;
        const char *end = start + line->content_len;
        const char *newline =
            static_cast<const char *>(memchr(start, '\n', end - start));
        if (!newline) {
          newline = end;
        }

        const QString lineContent = QString::fromUtf8(start, newline - start);

        diffOutput += QString("%1%2\n").arg(line->origin, lineContent);

        FilesDiffLine filesDiffLine;
        filesDiffLine.origin = line->origin;
        filesDiffLine.content = lineContent;

        filesDiffHunk.lines += filesDiffLine;
      }

      filesDiffDelta.hunks += filesDiffHunk;
    }

    git_patch_free(patch);

    filesDeltas += filesDiffDelta;
  }

  git_diff_free(diff);

  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  headCommit = nullptr;

  error = git_reference_peel(&headCommit, headRef, GIT_OBJECT_COMMIT);
  if (error) {
    logGitError(error);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error =
      git_commit_tree(&tree, reinterpret_cast<const git_commit *>(headCommit));
  if (error) {
    logGitError(error);
    git_reference_free(headRef);
    git_commit_free(reinterpret_cast<git_commit *>(headCommit));
    LOG_PERF(0, "Exit");
    return {};
  }

  const ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong maxBlobSize =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;

  for (const auto &file : files) {
    const QString repoPath = git_repository_workdir(repo);
    const QString filePath = QDir::fromNativeSeparators(file);
    const QString fullPath = repoPath + filePath;

    QFile fileObj(fullPath);
    if (fileObj.open(QIODevice::ReadOnly)) {
      const QByteArray content = fileObj.readAll();
      filesContent.insert(file, QString::fromUtf8(content));
      fileObj.close();
    } else {
      filesContent.insert(file, QString());
    }

    git_tree_entry *entry = nullptr;

    error = git_tree_entry_bypath(&entry, tree, filePath.toStdString().c_str());
    if (error) {
      logGitError(error);
      repoContent.insert(file, QString());
      continue;
    }

    const git_oid *fullOid = git_tree_entry_id(entry);
    if (!fullOid) {
      logGitError(error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    git_blob *blob = nullptr;
    error = git_blob_lookup_prefix(&blob, repo, fullOid, 7);
    if (error) {
      logGitError(error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    const char *blobContent =
        static_cast<const char *>(git_blob_rawcontent(blob));
    const size_t blobSize = git_blob_rawsize(blob);
    if (blobSize > static_cast<size_t>(maxBlobSize)) {
      // Blob too large to include in repo content
      repoContent.insert(
          file,
          QString("[[FILE SIZE EXCEEDS LIMIT OF %1 BYTES]]").arg(maxBlobSize));
      git_blob_free(blob);
      git_tree_entry_free(entry);
      continue;
    }

    repoContent.insert(file, QString::fromUtf8(blobContent, blobSize));

    git_blob_free(blob);
    git_tree_entry_free(entry);
  }

  git_tree_free(tree);
  git_commit_free(reinterpret_cast<git_commit *>(headCommit));
  git_reference_free(headRef);

  for (const auto &file : files) {
    const bool newFile = repoContent.value(file).isEmpty();
    const QString indexLine =
            createIndexLineFromContent(repoContent.value(file), filesContent[file]);
    const QString patch = createFullPatchFromContent(file, filesContent[file],
                                                     newFile, indexLine);

    diffOutput += QString("diff --git a/%1 b/%2\n").arg(file, file);

    if (newFile) {
        diffOutput += "new file mode 100644\n";
    }

    diffOutput += patch;

    const FilesDiffDelta patchDeltas =
        createFullPatchDeltasFromContent(file, filesContent[file]);
    filesDeltas += patchDeltas;
  }

  FilesDiff filesDiff;
  filesDiff.diff = diffOutput;
  filesDiff.filesContent = filesContent;
  filesDiff.repoContent = repoContent;
  filesDiff.deltas = filesDeltas;
  filesDiff.valid = true;

  LOG_PERF(0, "Exit");

  return filesDiff;
}

bool discardFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

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

    git_strarray_destroy(arr);
    git_object_free(headTree);

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
      git_object_free(headTree2);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    const std::string path = file.toStdString();

    git_tree_entry *entry = nullptr;
    error = git_tree_entry_bypath(&entry, tree, path.c_str());
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_object_free(headTree2);
      git_index_free(index);
      continue;
    }

    if (entry) {
      git_blob *blob = nullptr;
      error = git_blob_lookup(&blob, repo, git_tree_entry_id(entry));
      if (error) {
        logGitError(error);
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

      QFile file(fullPath);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR(Log::ErrorId::Git, "Can't open file \"{}\" for writing",
                  file.fileName());
        git_blob_free(blob);
        git_tree_entry_free(entry);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      file.write(static_cast<const char *>(git_blob_rawcontent(blob)),
                 git_blob_rawsize(blob));
      file.close();

      git_blob_free(blob);

      // Capture entry data before freeing
      git_index_entry newEntry = {};
      newEntry.mode = git_tree_entry_filemode(entry);
      newEntry.id = *git_tree_entry_id(entry);
      newEntry.path = path.c_str();

      git_tree_entry_free(entry);

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

bool pull(git_repository *repo, const QString &activeBranchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting pull operation for active branch: '{}'",
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
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchNameC = nullptr;
    error = git_branch_name(&branchNameC, head);
    if (error != 0 || !branchNameC) {
      logGitError(error);
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
    logGitError(GIT_ENOTFOUND);
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
    logGitError(error);
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
      logGitError(error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(fetchUuid, error != 0, false);

  if (commercial) {
    finishCredentialsAcquiring(urlStr);
  }

  if (error) {
    logGitError(error);
    git_strarray_destroy(refspecs);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  // Merge the upstream branch into the active branch
  git_reference *head = nullptr;
  error = git_repository_head(&head, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Merging upstream branch {}/{} into current branch '{}'",
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
    logGitError(error);
    git_reference_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get annotated commit from upstream
  git_annotated_commit *upstreamCommit = nullptr;
  error = git_annotated_commit_from_ref(&upstreamCommit, repo, upstream);
  if (error) {
    logGitError(error);
    git_reference_free(upstream);
    git_reference_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Analyze merge
  git_merge_analysis_t analysis;
  git_merge_preference_t preference;

  error = git_merge_analysis(
      &analysis, &preference, repo,
      const_cast<const git_annotated_commit **>(&upstreamCommit), 1);
  if (error) {
    logGitError(error);
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
      logGitError(error);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_checkout_tree(repo, (git_object *)targetCommit, &checkout_opts);
    if (error) {
      logGitError(error);
      git_commit_free(targetCommit);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference *newHead = nullptr;
    error = git_reference_set_target(&newHead, head, targetOid, "Fast-forward");
    if (error) {
      logGitError(error);
      git_commit_free(targetCommit);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(newHead);
    git_commit_free(targetCommit);
  } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
    // Normal merge
    git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
    checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_merge(
        repo, const_cast<const git_annotated_commit **>(&upstreamCommit), 1,
        &merge_opts, &checkout_opts);
    if (error) {
      logGitError(error);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_annotated_commit_free(upstreamCommit);
  git_reference_free(upstream);
  git_reference_free(head);

  LOG_PERF(0, "Exit");
  return true;
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

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // Collect unique remotes by iterating through local branches and their
  // upstream remotes
  QSet<QString> remotesToFetch;

  git_branch_iterator *it = nullptr;
  int error = git_branch_iterator_new(&it, repo, GIT_BRANCH_LOCAL);
  if (error) {
    LOG_DEBUG("Failed to create branch iterator: error={}", error);
    logGitError(error);
    // Not an error - repo might have no branches
    LOG_PERF(0, "Exit");
    return true;
  }

  git_reference *ref = nullptr;
  git_branch_t branchType;

  while (git_branch_next(&ref, &branchType, it) == 0) {
    if (branchType != GIT_BRANCH_LOCAL) {
      git_reference_free(ref);
      continue;
    }

    const char *branchNameC = git_reference_shorthand(ref);
    if (!branchNameC) {
      git_reference_free(ref);
      continue;
    }

    QString branchName = QString::fromUtf8(branchNameC);
    LOG_DEBUG("Checking branch '{}'", branchName.toStdString());

    // Get upstream for this branch
    auto [upstreamSuccess, remoteName, remoteBranchName] =
        getUpstream(repo, branchName);

    if (upstreamSuccess && !remoteName.isEmpty()) {
      remotesToFetch.insert(remoteName);
      LOG_DEBUG("Adding remote '{}' to fetch list", remoteName.toStdString());
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(it);

  if (remotesToFetch.isEmpty()) {
    LOG_DEBUG("No remotes with configured upstreams found");
    LOG_PERF(0, "Exit");
    return true; // No remotes to fetch is not an error
  }

  LOG_DEBUG("Found {} unique remotes to fetch", remotesToFetch.size());

  const bool commercial = isCommercialPlan();

  // Fetch from each collected remote
  for (const QString &remoteName : remotesToFetch) {
    git_remote *remote = nullptr;
    error = git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
                remoteName.toStdString(), error);
      logGitError(error);
      continue;
    }

    const char *url = git_remote_url(remote);
    LOG_DEBUG("Fetching from remote '{}' at URL: '{}'",
              remoteName.toStdString(), url);

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
          LOG_DEBUG(
              "SSH key configured for host '{}', setting max retries to 1",
              host.toStdString());
        }
      }
    }

    if (!commercial && gitSchema) {
      logGitError(GIT_EAUTH);
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

    std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(
        progressData, credPayload);

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
        logGitError(error);
        gGitCredentialsMutex.lock();
        gGitCredentials.remove(urlStr);
        gGitCredentialsMutex.unlock();
        sendInvalidCredentialsSignal(urlStr);
      }

      ++retries;
    } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

    finishFetchProgress(fetchUuid, error != 0, false);

    if (commercial) {
      finishCredentialsAcquiring(urlStr);
    }

    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to fetch from remote '{}': error={}",
                remoteName.toStdString(), error);
      logGitError(error);
      git_strarray_destroy(refspecs);
      git_remote_free(remote);
      continue; // Continue with next remote even if one fails
    }

    git_strarray_destroy(refspecs);
    git_remote_free(remote);
  }

  LOG_PERF(0, "Exit");
  return true; // Success if we completed the operation
}

bool push(git_repository *repo, const QString &activeBranchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push operation for active branch: '{}'",
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
      logGitError(error);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchNameC = nullptr;
    error = git_branch_name(&branchNameC, head);
    if (error != 0 || !branchNameC) {
      logGitError(error);
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
    logGitError(GIT_ENOTFOUND);
    LOG_DEBUG("Branch '{}' has no upstream configured",
              targetBranchName.toStdString());
    LOG_PERF(0, "Exit");
    return false; // Nothing to push, but not an error
  }

  LOG_DEBUG("Branch '{}' will be pushed to {}/{}",
            targetBranchName.toStdString(), remoteName.toStdString(),
            remoteBranchName.toStdString());

  // Push to the upstream remote
  const bool commercial = isCommercialPlan();
  int error = 0;

  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    logGitError(error);
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
    LOG_ERROR(Log::ErrorId::Git,
              "Pushing to git@ repositories is not available on the free "
              "plan.");
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

  git_push_options pushOpts;
  git_push_options_init(&pushOpts, GIT_PUSH_OPTIONS_VERSION);
  pushOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  pushOpts.callbacks.transfer_progress = &fetchProgressCallback;
  pushOpts.callbacks.certificate_check = &certificateCheckCallback;
  pushOpts.callbacks.payload = &callbackPayload;

  // Build refspec for pushing the branch to its upstream
  const QString refspec = QString("refs/heads/%1:refs/heads/%2")
                              .arg(targetBranchName, remoteBranchName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);
  LOG_DEBUG("Using push refspec: '{}'", refspec.toStdString());

  do {
    error = git_remote_push(remote, &refspecs, &pushOpts);

    if (error) {
      logGitError(error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(pushUuid, error != 0, false);

  if (commercial) {
    finishCredentialsAcquiring(urlStr);
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_PERF(0, "Exit");

  return error == 0;
}

bool pushTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push tag operation for tag: '{}'", tagName.toStdString());

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

  // Verify tag exists
  git_reference *tagRef = nullptr;
  const QString tagRefName = QString("refs/tags/%1").arg(tagName);
  int error =
      git_reference_lookup(&tagRef, repo, tagRefName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Tag '{}' not found: error={}",
              tagName.toStdString(), error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  git_reference_free(tagRef);
  LOG_DEBUG("Tag '{}' found", tagName.toStdString());

  git_remote *remote = nullptr;
  const QString defaultRemote = getDefaultRemote(repo);

  error = git_remote_lookup(&remote, repo, defaultRemote.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Pushing tags to git@ repositories is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Push tag with refspec: refs/tags/tagName:refs/tags/tagName
  const QString refspec = QString("refs/tags/%1:refs/tags/%1").arg(tagName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    error = git_remote_push(remote, &refspecs, &opts);
    if (error) {
      logGitError(error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Push tag operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool pushAllTags(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push all tags operation");

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

  // Get list of all tags
  const QStringList allTags = listTags(repo);
  if (allTags.isEmpty()) {
    LOG_INFO("No tags to push");
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_DEBUG("Found {} tags to push", allTags.size());

  git_remote *remote = nullptr;
  const QString defaultRemote = getDefaultRemote(repo);

  int error =
      git_remote_lookup(&remote, repo, defaultRemote.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Pushing tags to git@ repositories is not available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Create refspecs for all tags: refs/tags/tagName:refs/tags/tagName
  QStringList refspecList;
  for (const auto &tag : allTags) {
    refspecList += QString("refs/tags/%1:refs/tags/%1").arg(tag);
  }
  git_strarray refspecs = qstringlist_to_git_strarray(refspecList);

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    error = git_remote_push(remote, &refspecs, &opts);
    if (error) {
      logGitError(error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Push all tags operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool stash(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid stashId;
  git_signature *signature = nullptr;

  // Get default signature (author info)
  int error = git_signature_default(&signature, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_stash_save(
      &stashId,  // Optional: receives the stash commit ID
      repo,      // Your git_repository pointer
      signature, // Signature of the stasher
      nullptr,   // Optional message
      GIT_STASH_DEFAULT | GIT_STASH_INCLUDE_UNTRACKED // Flags (can be
      // GIT_STASH_INCLUDE_UNTRACKED, etc.)
  );
  if (error) {
    logGitError(error);
    git_signature_free(signature);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(signature);

  LOG_PERF(0, "Exit");

  return true;
}

bool setUpstream(git_repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  int error = git_branch_lookup(
      &localRef, repo, localBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    logGitError(error);
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
    logGitError(error);
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
    LOG_PERF(0, "Exit");
    return {};
  }

  git_reference *localRef = nullptr;
  int error = git_branch_lookup(
      &localRef, repo, localBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_reference *upstreamRef = nullptr;
  error = git_branch_upstream(&upstreamRef, localRef);

  QString remoteName;
  QString remoteBranchName;

  if (error && error != GIT_ENOTFOUND) {
    logGitError(error);
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
      logGitError(error);
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

bool resetToCommit(git_repository *repo, const QString &commit,
                   ResetType type) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Resetting to commit '{}' with type {}", commit.toStdString(),
            static_cast<int>(type));

  git_object *target = nullptr;

  int error = git_revparse_single(&target, repo, commit.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to parse commit '{}': error={}",
              commit.toStdString(), error);
    logGitError(error);
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
    break;
  }
  }

  git_object_free(target);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Reset operation failed: error={}", error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully reset to commit '{}'", commit.toStdString());
  LOG_PERF(0, "Exit");

  return true;
}

bool revertCommit(git_repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message) {
  LOG_PERF(0, "Enter");

  git_commit *targetCommit = nullptr;

  int error = git_revparse_single((git_object **)&targetCommit, repo,
                                  commit.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *headCommit = nullptr;
  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    logGitError(error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&headCommit, repo, git_reference_target(headRef));
  if (error) {
    logGitError(error);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *revertIndex = nullptr;
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;

  error = git_revert_commit(&revertIndex, repo, targetCommit, headCommit,
                            0, // mainline = 0 for non-merge commits
                            &mergeOpts);
  if (error) {
    logGitError(error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid treeOid;

  error = git_index_write_tree_to(&treeOid, revertIndex, repo);
  if (error) {
    logGitError(error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree *revertTree = nullptr;

  error = git_tree_lookup(&revertTree, repo, &treeOid);
  if (error) {
    logGitError(error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_tree_free(revertTree);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid newCommitOid;

  error = git_commit_create_v(&newCommitOid, repo, "HEAD", sig, sig, nullptr,
                              message.toStdString().c_str(), revertTree, 1,
                              headCommit);
  if (error) {
    logGitError(error);
    git_signature_free(sig);
    git_tree_free(revertTree);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(revertTree);
  git_index_free(revertIndex);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

static QStringList createPatchTargetFiles(const QString &patchContent) {
  QStringList files;
  const QStringList lines = QString(patchContent).replace("\r\n", "\n").split('\n');
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
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString workdirPath = QString::fromUtf8(git_repository_workdir(repo));
  const QStringList targetFiles = createPatchTargetFiles(patchContent);
  QStringList createdFiles;

  for (const QString &filePath : targetFiles) {
    const QString fullPath = QDir(workdirPath).filePath(filePath);
    QFileInfo fileInfo(fullPath);
    if (fileInfo.exists()) {
      continue;
    }

    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Can't create parent directory for patch file \"{}\".",
                fullPath.toStdString());
      for (const QString &createdFile : createdFiles) {
        QFile::remove(createdFile);
      }
      LOG_PERF(0, "Exit");
      return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Can't create empty file \"{}\" for patch application.",
                fullPath.toStdString());
      for (const QString &createdFile : createdFiles) {
        QFile::remove(createdFile);
      }
      LOG_PERF(0, "Exit");
      return false;
    }

    file.close();
    createdFiles.append(fullPath);
  }

  git_diff *diff = nullptr;
  const QByteArray buffer = patchContent.toUtf8();

  int error = git_diff_from_buffer(&diff, buffer.constData(), buffer.size());
  if (error) {
    logGitError(error);
    for (const QString &createdFile : createdFiles) {
      QFile::remove(createdFile);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  git_apply_options opts = GIT_APPLY_OPTIONS_INIT;
  error = git_apply(repo, diff, GIT_APPLY_LOCATION_WORKDIR, &opts);
  if (error) {
    logGitError(error);
    git_diff_free(diff);
    for (const QString &createdFile : createdFiles) {
      QFile::remove(createdFile);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  git_diff_free(diff);

  LOG_PERF(0, "Exit");

  return true;
}

bool cherryPickCommit(git_repository *repo, const QString &commit,
                      const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  git_commit *targetCommit = nullptr;

  int error = git_revparse_single((git_object **)&targetCommit, repo,
                                  commit.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *headCommit = nullptr;
  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    logGitError(error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&headCommit, repo, git_reference_target(headRef));
  if (error) {
    logGitError(error);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *cherryPickIndex = nullptr;
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;

  error = git_cherrypick_commit(&cherryPickIndex, repo, targetCommit,
                                headCommit, 0, &mergeOpts);
  if (error) {
    logGitError(error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid treeOid;

  error = git_index_write_tree_to(&treeOid, cherryPickIndex, repo);
  if (error) {
    logGitError(error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree *cherryPickTree = nullptr;

  error = git_tree_lookup(&cherryPickTree, repo, &treeOid);
  if (error) {
    logGitError(error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_tree_free(cherryPickTree);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid newCommitOid;
  const char *commitMessage = git_commit_message(targetCommit);

  error = git_commit_create_v(&newCommitOid, repo, "HEAD", sig, sig, nullptr,
                              commitMessage, cherryPickTree, 1, headCommit);
  if (error) {
    logGitError(error);
    git_signature_free(sig);
    git_tree_free(cherryPickTree);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(cherryPickTree);
  git_index_free(cherryPickIndex);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

bool renameBranch(git_repository *repo, const QString &oldName,
                  const QString &newName) {
  LOG_PERF(0, "Enter");

  git_reference *ref = nullptr;

  int error = git_branch_lookup(&ref, repo, oldName.toStdString().c_str(),
                                GIT_BRANCH_LOCAL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_move(&ref, ref, newName.toStdString().c_str(), 0);
  if (error) {
    logGitError(error);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteBranch(git_repository *repo, const QString &branchName) {
  LOG_PERF(0, "Enter");

  git_reference *ref = nullptr;

  int error = git_branch_lookup(&ref, repo, branchName.toStdString().c_str(),
                                GIT_BRANCH_LOCAL);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_delete(ref);
  if (error) {
    logGitError(error);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteRemoteBranch(git_repository *repo, const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting delete remote branch operation for branch: '{}'",
            branchName.toStdString());

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

  git_remote *remote = nullptr;

  auto [ok, remoteName, _] = getUpstream(repo, branchName);
  if (!ok || remoteName.isEmpty()) {
    remoteName = "origin";
  }

  int error =
      git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    LOG_ERROR(Log::ErrorId::Git,
              "Deleting remote branches from git@ repositories is not "
              "available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Delete branch with refspec: :refs/heads/branchName (empty source = delete)
  // Strip remote prefix (e.g., "origin/branch" -> "branch")
  QString cleanBranchName = branchName;
  if (branchName.startsWith("origin/")) {
    cleanBranchName =
        branchName.mid(7); // Remove "origin/" prefix (7 characters)
  }

  const QString refspec = QString(":refs/heads/%1").arg(cleanBranchName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  LOG_DEBUG("Using refspec for delete: '{}' (original branch name: '{}')",
            refspec.toStdString(), branchName.toStdString());

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    LOG_DEBUG("Attempting deleteRemoteBranch push (attempt {}/{})", retries + 1,
              maxRetries);
    error = git_remote_push(remote, &refspecs, &opts);
    LOG_DEBUG("deleteRemoteBranch git_remote_push returned error code: {}",
              error);

    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "deleteRemoteBranch: git_remote_push failed with error: {}",
                error);
      logGitError(error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    } else {
      LOG_DEBUG("deleteRemoteBranch: git_remote_push succeeded");
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Delete remote branch operation completed with error code: {}, "
            "returning {}",
            error, (error == 0 ? "success" : "failure"));
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool createTagFromCommit(git_repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message) {
  LOG_PERF(0, "Enter");

  git_oid targetOid;

  int error = git_oid_fromstr(&targetOid, commit.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  error = git_commit_lookup(&targetCommit, repo, &targetOid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid tagOid;

  error = git_tag_create(&tagOid, repo, tagName.toStdString().c_str(),
                         reinterpret_cast<git_object *>(targetCommit), sig,
                         message.toStdString().c_str(), 0);
  if (error) {
    logGitError(error);
    git_signature_free(sig);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

QString createPatchFromCommit(git_repository *repo, const QString &commit) {
  LOG_PERF(0, "Enter");

  git_oid oid;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    logGitError(error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *parent = nullptr;
  git_tree *parentTree = nullptr;

  if (const int parentCount = git_commit_parentcount(commit_);
      parentCount > 0) {
    error = git_commit_parent(&parent, commit_, 0);
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parentTree, parent);
    if (error) {
      logGitError(error);
      git_commit_free(parent);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  git_diff *diff = nullptr;

  error = git_diff_tree_to_tree(&diff, repo, parentTree, tree, nullptr);
  if (error) {
    logGitError(error);

    if (parentTree) {
      git_tree_free(parentTree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  QString patch;
  const size_t count = git_diff_num_deltas(diff);
  for (size_t i = 0; i < count; ++i) {
    git_patch *patch_ = nullptr;

    error = git_patch_from_diff(&patch_, diff, i);
    if (error) {
      logGitError(error);
      continue;
    }

    git_buf buf = {nullptr};

    error = git_patch_to_buf(&buf, patch_);
    if (error) {
      logGitError(error);
      git_patch_free(patch_);
      continue;
    }

    if (buf.ptr && buf.size > 0) {
      patch += QString::fromUtf8(buf.ptr, buf.size);
    }

    git_buf_dispose(&buf);
    git_patch_free(patch_);
  }

  git_diff_free(diff);

  if (parentTree) {
    git_tree_free(parentTree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return patch;
}

bool renameTag(git_repository *repo, const QString &oldName,
               const QString &newName) {
  LOG_PERF(0, "Enter");

  git_reference *tagRef = nullptr;
  git_object *target = nullptr;
  git_oid newOid;

  int error = git_reference_lookup(
      &tagRef, repo,
      QString("refs/tags/%1").arg(oldName).toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reference_peel(&target, tagRef, GIT_OBJECT_TAG);
  if (error < 0) {
    // Might be a lightweight tag — try commit or tree
    error = git_reference_peel(&target, tagRef, GIT_OBJECT_ANY);
    if (error) {
      logGitError(error);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  // If it's an annotated tag, get message and tagger
  if (git_object_type(target) == GIT_OBJECT_TAG) {
    git_tag *tag = nullptr;
    tag = reinterpret_cast<git_tag *>(target);
    const git_signature *tagger = git_tag_tagger(tag);
    if (!tagger) {
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *message = git_tag_message(tag);

    git_object *tagTarget = nullptr;

    error = git_tag_target(&tagTarget, tag);
    if (error) {
      logGitError(error);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_tag_create(&newOid, repo, newName.toStdString().c_str(),
                           tagTarget, tagger, message, 0);
    if (error) {
      logGitError(error);
      git_object_free(tagTarget);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(tagTarget);
  } else {
    // Lightweight tag
    error = git_tag_create_lightweight(
        &newOid, repo, newName.toStdString().c_str(), target, 0);
    if (error) {
      logGitError(error);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_tag_delete(repo, oldName.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_object_free(target);
    git_reference_free(tagRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(target);
  git_reference_free(tagRef);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");

  if (const int error = git_tag_delete(repo, tagName.toStdString().c_str())) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteRemoteTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting delete remote tag operation for tag: '{}'",
            tagName.toStdString());

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

  git_remote *remote = nullptr;
  const QString defaultRemoteName = getDefaultRemote(repo);

  int error =
      git_remote_lookup(&remote, repo, defaultRemoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    LOG_ERROR(Log::ErrorId::Git, "Deleting remote tags from git@ repositories "
                                 "is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Delete tag with refspec: :refs/tags/tagName (empty source = delete)
  // Note: For tags, we typically don't have a remote prefix, but clean it just
  // in case
  QString cleanTagName = tagName;
  if (const auto parts = tagName.split('/'); parts.length() > 1) {
    cleanTagName = parts.last();
  }

  const QString refspec = QString(":refs/tags/%1").arg(cleanTagName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  LOG_DEBUG("Using refspec for delete: '{}' (original tag name: '{}')",
            refspec.toStdString(), tagName.toStdString());

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    LOG_DEBUG("Attempting deleteRemoteTag push (attempt {}/{})", retries + 1,
              maxRetries);
    error = git_remote_push(remote, &refspecs, &opts);
    LOG_DEBUG("deleteRemoteTag git_remote_push returned: {}", error);

    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "deleteRemoteTag push failed: {}", error);
      logGitError(error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Delete remote tag operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

QString getSubmodulePath(git_repository *repo, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  git_submodule *submodule = nullptr;

  if (const int error = git_submodule_lookup(
          &submodule, repo, submoduleName.toStdString().c_str())) {
    logGitError(error);
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

bool deleteSubmodule(git_repository *repo, const QString &submoduleName,
                     const QString &submodulePath) {
  LOG_PERF(0, "Enter");

  const std::string name = submoduleName.toStdString();
  const std::string path = submodulePath.toStdString();

  git_submodule *submodule = nullptr;

  int error = git_submodule_lookup(&submodule, repo, name.c_str());
  if (error) {
    // Submodule lookup failed (e.g., after discarding .gitmodules)
    // Continue with manual cleanup instead of failing
    LOG_DEBUG(
        "Submodule '{}' not found in metadata, proceeding with manual cleanup",
        name);
    submodule = nullptr;
  }

  const char *repositoryPath = git_repository_path(repo);
  if (!repositoryPath) {
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string gitModulesPath =
      QString("%1/../.gitmodules").arg(repositoryPath).toStdString();
  const std::string gitModulesTmpPath =
      QString("%1/../.gitmodules.tmp").arg(repositoryPath).toStdString();

  FILE *src = fopen(gitModulesPath.c_str(), "r");
  FILE *dst = fopen(gitModulesTmpPath.c_str(), "w");

  if (src && dst) {
    char line[1024];
    int skip = 0;
    while (fgets(line, sizeof(line), src)) {
      if (strstr(line, name.c_str())) {
        skip = 1;
        continue;
      }

      if (skip && line[0] == '[') {
        skip = 0;
      }

      if (!skip) {
        fputs(line, dst);
      }
    }

    fclose(src);
    src = nullptr;
    fclose(dst);
    dst = nullptr;

    remove(gitModulesPath.c_str());
    if (rename(gitModulesTmpPath.c_str(), gitModulesPath.c_str()) != 0) {
      // If rename failed, try to clean up the temp file
      remove(gitModulesTmpPath.c_str());
      git_submodule_free(submodule);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (const bool staged = stageFiles(repo, QStringList() << ".gitmodules");
        !staged) {
      git_submodule_free(submodule);
      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    if (src) {
      fclose(src);
    }
    if (dst) {
      fclose(dst);
    }
    // Clean up temp file if it was created
    remove(gitModulesTmpPath.c_str());
  }

  // Remove submodule section from .git/config file
  const std::string gitDirPath = repositoryPath;
  const std::string gitConfigFile =
      gitDirPath +
      (gitDirPath.back() == '/' || gitDirPath.back() == '\\' ? "" : "/") +
      "config";

  src = fopen(gitConfigFile.c_str(), "r");
  dst = fopen((gitConfigFile + ".tmp").c_str(), "w");

  if (src && dst) {
    char line[1024];
    int skipSection = 0;
    while (fgets(line, sizeof(line), src)) {
      // Check if this is the submodule section header
      if (strstr(line, "[submodule") && strstr(line, name.c_str())) {
        skipSection = 1;
        continue;
      }

      // Check if we're leaving the submodule section (new section starts)
      if (skipSection && line[0] == '[') {
        skipSection = 0;
      }

      // Only write lines that are not part of the submodule section
      if (!skipSection) {
        fputs(line, dst);
      }
    }

    fclose(src);
    src = nullptr;
    fclose(dst);
    dst = nullptr;

    remove(gitConfigFile.c_str());
    if (rename((gitConfigFile + ".tmp").c_str(), gitConfigFile.c_str()) != 0) {
      // If rename failed, try to clean up the temp file
      remove((gitConfigFile + ".tmp").c_str());
    }
  } else {
    if (src) {
      fclose(src);
    }
    if (dst) {
      fclose(dst);
    }
    // Clean up temp file if it was created
    remove((gitConfigFile + ".tmp").c_str());
  }

  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    logGitError(error);
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  // Try to remove from index, but don't fail if it's not there
  // (user may have already discarded it)
  error = git_index_remove(index, name.c_str(), 0);
  if (error && error != GIT_ENOTFOUND) {
    logGitError(error);
    git_index_free(index);
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_index_write(index);
  if (error) {
    logGitError(error);
    git_index_free(index);
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);
  if (submodule) {
    git_submodule_free(submodule);
  }

  QDir(submodulePath).removeRecursively();
  QDir(QString("%1/modules/%2").arg(repositoryPath).arg(name))
      .removeRecursively();

  LOG_PERF(0, "Exit");

  return true;
}

bool updateSubmodule(git_repository *repo, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const std::string name = submoduleName.toStdString();

  git_submodule *submodule = nullptr;

  int error = git_submodule_lookup(&submodule, repo, name.c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_submodule_init(submodule, 0);
  if (error) {
    logGitError(error);
    git_submodule_free(submodule);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Configure fetch and checkout options with certificate checking
  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy = GIT_CHECKOUT_SAFE;

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials = &credentialsAcquireCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;

  git_submodule_update_options updateOpts = GIT_SUBMODULE_UPDATE_OPTIONS_INIT;
  updateOpts.fetch_opts = fetchOpts;
  updateOpts.checkout_opts = checkoutOpts;

  error = git_submodule_update(submodule, 1, &updateOpts);
  if (error) {
    logGitError(error);
    git_submodule_free(submodule);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_submodule_free(submodule);

  LOG_PERF(0, "Exit");

  return true;
}

bool addSubmodule(git_repository *repo, const QString &url, const QString &name,
                  const QString &uuid) {
  LOG_PERF(0, "Enter");

  const char *repoPath = git_repository_path(repo);
  if (!repoPath) {
    LOG_PERF(0, "Exit");
    return false;
  }

  deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));

  git_submodule *subModule = nullptr;

  int error =
      git_submodule_add_setup(&subModule, repo, url.toStdString().c_str(),
                              name.toStdString().c_str(), 1);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    LOG_ERROR(Log::ErrorId::Git, "Adding submodules to git@ repositories is "
                                 "not available on the free plan.");
    git_submodule_free(subModule);
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
    logGitError(error);
    finishFetchProgress(uuid, true, false);
    finishCredentialsAcquiring(url);
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
  // This is needed even if no progress callbacks were fired (e.g., local path
  // submodules)
  finishFetchProgress(uuid, error != 0, false);

  // Dismiss credentials overlay using the URL (same key used for storing the
  // widget)
  finishCredentialsAcquiring(url);

  error = git_submodule_add_finalize(subModule);
  if (error) {
    logGitError(error);
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

FilesDiff diffFilesFromCommit(git_repository *repo, const QStringList &files,
                              const QString &commit) {
  LOG_PERF(0, "Enter");

  git_oid oid;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    logGitError(error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *parent = nullptr;
  git_tree *parentTree = nullptr;

  if (git_commit_parentcount(commit_) > 0) {
    error = git_commit_parent(&parent, commit_, 0);
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parentTree, parent);
    if (error) {
      logGitError(error);
      git_commit_free(parent);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  git_diff *diff = nullptr;

  git_strarray arr = qstringlist_to_git_strarray(files);

  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.pathspec = arr;

  error = git_diff_tree_to_tree(&diff, repo, parentTree, tree, &opts);
  if (error) {
    logGitError(error);

    git_strarray_destroy(arr);

    if (parentTree) {
      git_tree_free(parentTree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit_);

    LOG_PERF(0, "Exit");
    return {};
  }

  QString diffOutput;

  QVector<FilesDiffDelta> filesDeltas;

  size_t num_deltas = git_diff_num_deltas(diff);
  for (size_t i = 0; i < num_deltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(diff, i);
    diffOutput += QString("diff --git a/%1 b/%2\n")
                      .arg(delta->old_file.path, delta->new_file.path);

    FilesDiffDelta filesDiffDelta;
    filesDiffDelta.oldFilePath = delta->old_file.path;
    filesDiffDelta.newFilePath = delta->new_file.path;

    git_patch *patch;
    error = git_patch_from_diff(&patch, diff, i);
    if (error) {
      logGitError(error);
      continue;
    }

    const size_t num_hunks = git_patch_num_hunks(patch);
    for (size_t h = 0; h < num_hunks; ++h) {
      const git_diff_hunk *hunk;
      error = git_patch_get_hunk(&hunk, NULL, patch, h);
      if (error) {
        logGitError(error);
        continue;
      }

      diffOutput += QString("@@ -%1,%2 +%3,%4 @@\n")
                        .arg(hunk->old_start)
                        .arg(hunk->old_lines)
                        .arg(hunk->new_start)
                        .arg(hunk->new_lines);

      FilesDiffHunk filesDiffHunk;
      filesDiffHunk.oldStart = hunk->old_start;
      filesDiffHunk.oldLines = hunk->old_lines;
      filesDiffHunk.newStart = hunk->new_start;
      filesDiffHunk.newLines = hunk->new_lines;

      size_t num_lines = git_patch_num_lines_in_hunk(patch, h);
      for (size_t l = 0; l < num_lines; ++l) {
        const git_diff_line *line;
        error = git_patch_get_line_in_hunk(&line, patch, h, l);
        if (error) {
          logGitError(error);
          continue;
        }

        const char *start = line->content;
        const char *end = start + line->content_len;
        const char *newline =
            static_cast<const char *>(memchr(start, '\n', end - start));
        if (!newline) {
          newline = end;
        }

        const QString lineContent = QString::fromUtf8(start, newline - start);

        diffOutput += QString("%1%2\n").arg(line->origin, lineContent);

        FilesDiffLine filesDiffLine;
        filesDiffLine.origin = line->origin;
        filesDiffLine.content = lineContent;

        filesDiffHunk.lines += filesDiffLine;
      }

      filesDiffDelta.hunks += filesDiffHunk;
    }

    git_patch_free(patch);

    filesDeltas += filesDiffDelta;
  }

  git_diff_free(diff);
  git_strarray_destroy(arr);

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong maxBlobSize =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;

  for (const auto &file : files) {
    const QString repoPath = git_repository_workdir(repo);
    const QString filePath = QDir::fromNativeSeparators(file);
    const QString fullPath = repoPath + filePath;

    QFile fileObj(fullPath);
    if (fileObj.open(QIODevice::ReadOnly)) {
      const QByteArray content = fileObj.readAll();
      filesContent.insert(file, QString::fromUtf8(content));
      fileObj.close();
    } else {
      filesContent.insert(file, QString());
    }

    git_tree_entry *entry = nullptr;

    if (!parentTree) {
      repoContent.insert(file, QString());
      continue;
    }

    error = git_tree_entry_bypath(&entry, parentTree,
                                  filePath.toStdString().c_str());
    if (error) {
      logGitError(error);
      repoContent.insert(file, QString());
      continue;
    }

    const git_oid *fullOid = git_tree_entry_id(entry);
    if (!fullOid) {
      logGitError(error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    git_blob *blob = nullptr;
    error = git_blob_lookup(&blob, repo, fullOid);
    if (error) {
      logGitError(error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    const char *blobContent =
        static_cast<const char *>(git_blob_rawcontent(blob));
    const size_t blobSize = git_blob_rawsize(blob);

    if (blobSize > static_cast<size_t>(maxBlobSize)) {
      repoContent.insert(
          file,
          QString("[[FILE SIZE EXCEEDS LIMIT OF %1 BYTES]]").arg(maxBlobSize));
      git_blob_free(blob);
      git_tree_entry_free(entry);
      continue;
    }

    repoContent.insert(file, QString::fromUtf8(blobContent, blobSize));

    git_blob_free(blob);
    git_tree_entry_free(entry);
  }

  if (parentTree) {
    git_tree_free(parentTree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit_);

  FilesDiff filesDiff;
  filesDiff.diff = diffOutput;
  filesDiff.filesContent = filesContent;
  filesDiff.repoContent = repoContent;
  filesDiff.deltas = filesDeltas;
  filesDiff.valid = true;

  LOG_PERF(0, "Exit");

  return filesDiff;
}

qint64 getFileSizeFromCommit(git_repository *repo, const QString &filePath,
                             const QString &commit) {
  LOG_PERF(0, "Enter");

  git_oid oid;
  int error;

  // If commit is empty, use HEAD
  if (commit.isEmpty()) {
    git_reference *head = nullptr;
    error = git_repository_head(&head, repo);
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return -1;
    }

    error = git_reference_name_to_id(&oid, repo, git_reference_name(head));
    git_reference_free(head);
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return -1;
    }
  } else {
    error = git_oid_fromstr(&oid, commit.toStdString().c_str());
    if (error) {
      logGitError(error);
      LOG_PERF(0, "Exit");
      return -1;
    }
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    logGitError(error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const QString normalizedPath = QDir::fromNativeSeparators(filePath);
  git_tree_entry *entry = nullptr;

  error =
      git_tree_entry_bypath(&entry, tree, normalizedPath.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const git_oid *blobOid = git_tree_entry_id(entry);
  if (!blobOid) {
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_blob *blob = nullptr;
  error = git_blob_lookup(&blob, repo, blobOid);
  if (error) {
    logGitError(error);
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const qint64 blobSize = static_cast<qint64>(git_blob_rawsize(blob));

  git_blob_free(blob);
  git_tree_entry_free(entry);
  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return blobSize;
}

bool merge(git_repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting merge operation: source='{}', destination='{}', "
            "author='{}', email='{}'",
            sourceBranch.toStdString(), destinationBranch.toStdString(),
            author.toStdString(), email.toStdString());

  git_reference *srcRef = nullptr;
  git_reference *dstRef = nullptr;

  // Lookup branches
  int error = git_branch_lookup(
      &srcRef, repo, sourceBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to lookup source branch '{}': error={}",
              sourceBranch.toStdString(), error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Source branch '{}' lookup successful", sourceBranch.toStdString());

  error = git_branch_lookup(
      &dstRef, repo, destinationBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to lookup destination branch '{}': error={}",
              destinationBranch.toStdString(), error);
    logGitError(error);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Destination branch '{}' lookup successful",
            destinationBranch.toStdString());

  git_object *dstObj = nullptr;

  error = git_reference_peel(&dstObj, dstRef, GIT_OBJECT_COMMIT);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to peel destination reference: error={}", error);
    logGitError(error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Checking out destination branch");
  error = git_checkout_tree(repo, dstObj, nullptr);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to checkout tree for destination branch: error={}",
              error);
    logGitError(error);
    git_object_free(dstObj);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(dstObj);

  error = git_repository_set_head(repo, git_reference_name(dstRef));
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to set repository HEAD: error={}",
              error);
    logGitError(error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Repository HEAD set, preparing to merge");
  // Get annotated commits
  git_annotated_commit *srcCommit = nullptr;

  error = git_annotated_commit_from_ref(&srcCommit, repo, srcRef);
  if (error) {
    logGitError(error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *dstCommit = nullptr;

  error = git_annotated_commit_from_ref(&dstCommit, repo, dstRef);
  if (error) {
    logGitError(error);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_merge_analysis_t analysis;
  git_merge_preference_t preference;

  const git_annotated_commit *commits[1] = {srcCommit};

  // Merge analysis
  error = git_merge_analysis(&analysis, &preference, repo, commits, 1);
  if (error) {
    logGitError(error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
    LOG_DEBUG("Destination branch is already up to date.");
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return true;
  } else if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
    LOG_DEBUG("Performing fast-forward merge...");

    git_oid targetOid;
    git_commit *srcCommitObj = nullptr;
    error = git_commit_lookup(&srcCommitObj, repo,
                              git_annotated_commit_id(srcCommit));
    if (error) {
      logGitError(error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference *updatedRef = nullptr;

    // Update destination branch to point to source commit
    error = git_reference_set_target(
        &updatedRef, dstRef, git_commit_id(srcCommitObj), "Fast-forward merge");
    if (error) {
      logGitError(error);
      git_commit_free(srcCommitObj);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit *newCommit = nullptr;

    error = git_commit_lookup(&newCommit, repo, git_commit_id(srcCommitObj));
    if (error) {
      logGitError(error);
      git_reference_free(updatedRef);
      git_commit_free(srcCommitObj);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(updatedRef);
    git_commit_free(srcCommitObj);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);

    git_checkout_options ffOpts = GIT_CHECKOUT_OPTIONS_INIT;
    ffOpts.checkout_strategy =
        GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;

    error = git_reset(repo, reinterpret_cast<git_object *>(newCommit),
                      GIT_RESET_HARD, &ffOpts);
    if (error) {
      logGitError(error);
      git_commit_free(newCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(newCommit);

    LOG_PERF(0, "Exit");
    return true;
  } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
    LOG_DEBUG("Performing normal merge...");

    git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;

    error = git_merge(repo, commits, 1, &mergeOpts, &checkoutOpts);
    if (error) {
      logGitError(error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_index *index = NULL;
    error = git_repository_index(&index, repo);
    if (error) {
      logGitError(error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (git_index_has_conflicts(index)) {
      LOG_DEBUG("Merge has conflicts. Resolve them before committing.");
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Write tree
    git_oid treeOid;
    git_tree *tree = nullptr;

    error = git_index_write_tree(&treeOid, index);
    if (error) {
      logGitError(error);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_tree_lookup(&tree, repo, &treeOid);
    if (error) {
      logGitError(error);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Create commit
    git_signature *sig = nullptr;

    error = git_signature_now(&sig, author.toStdString().c_str(),
                              email.toStdString().c_str());
    if (error) {
      logGitError(error);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_oid commitOid;
    git_commit *dstCommitObj = nullptr;
    git_commit *srcCommitObj = nullptr;

    error =
        git_commit_lookup(&dstCommitObj, repo, git_reference_target(dstRef));
    if (error) {
      logGitError(error);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error =
        git_commit_lookup(&srcCommitObj, repo, git_reference_target(srcRef));
    if (error) {
      logGitError(error);
      git_commit_free(dstCommitObj);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    const git_commit *parents[2] = {dstCommitObj, srcCommitObj};

    const QString comment = QString("Merge branch '%1' into '%2'")
                                .arg(sourceBranch, destinationBranch);

    error = git_commit_create(&commitOid, repo, "HEAD", sig, sig, NULL,
                              comment.toStdString().c_str(), tree, 2, parents);
    if (error) {
      logGitError(error);
      git_commit_free(srcCommitObj);
      git_commit_free(dstCommitObj);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(srcCommitObj);
    git_commit_free(dstCommitObj);
    git_signature_free(sig);
    git_tree_free(tree);
    git_index_free(index);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_PERF(0, "Exit");

  return true;
}

QString listConflictsForAFile(git_repository *repo, const QString &filePath) {
  LOG_PERF(0, "Enter");

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_index_conflict_iterator *iterator = nullptr;

  error = git_index_conflict_iterator_new(&iterator, index);
  if (error) {
    logGitError(error);
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
    if (ours->path == nullptr || ours->path != filePathStr ||
        theirs == nullptr || theirs->path != filePathStr) {
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

    fileContentWithMarkers += result.ptr;

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

  const int state = git_repository_state(repo);

  LOG_PERF(0, "Exit");

  return state == GIT_REPOSITORY_STATE_MERGE;
}

bool abortMerge(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (git_repository_state(repo) != GIT_REPOSITORY_STATE_MERGE) {
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = git_repository_state_cleanup(repo);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object *head = NULL;

  error = git_revparse_single(&head, repo, "HEAD");
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reset(repo, head, GIT_RESET_HARD, NULL);
  if (error) {
    logGitError(error);
    git_object_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(head);

  LOG_PERF(0, "Exit");

  return true;
}

QVector<RemoteInfo> listRemotes(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Listing remotes");

  Q_ASSERT(repo != nullptr);

  QVector<RemoteInfo> remotes;

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return remotes;
  }

  git_strarray remote_list;
  int error = git_remote_list(&remote_list, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to list remotes: error={}", error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return remotes;
  }

  // Get default remote
  const QString defaultRemote = getDefaultRemote(repo);

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

  git_branch_t branch_type = GIT_BRANCH_LOCAL;
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
    logGitError(error);
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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_config_set_string(config, "sourcerer.defaultremote",
                                remoteName.toUtf8().constData());
  git_config_free(config);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set default remote in config: error={}", error);
    logGitError(error);
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
    logGitError(error);
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
      logGitError(error);
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
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully removed remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool updateRemote(git_repository *repo, const QString &remoteName,
                  const QString &fetchUrl, const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Updating remote '{}' with fetch URL: '{}'",
            remoteName.toStdString(), fetchUrl.toStdString());

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

  // Set fetch URL
  int error = git_remote_set_url(repo, remoteName.toUtf8().constData(),
                                 fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL
  error = git_remote_set_pushurl(repo, remoteName.toUtf8().constData(),
                                 pushUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
  }

  // Delete push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_delete_entry(config, pushConfigKey.toUtf8().constData());
  if (error && error != GIT_ENOTFOUND) {
    LOG_WARN("Failed to delete push URL config for target '{}' on remote "
             "'{}': error={}",
             targetName.toStdString(), remoteName.toStdString(), error);
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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
    logGitError(error);
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

bool amend(git_repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  git_commit *commitToAmend = nullptr;
  git_oid commitOid;
  git_oid newCommitOid;
  git_tree *tree = nullptr;
  const git_signature *authorSig = nullptr;
  git_signature *committerSig = nullptr;

  // 1. Get the HEAD commit (the one to amend)
  int error = git_reference_name_to_id(&commitOid, repo, "HEAD");
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commitToAmend, repo, &commitOid);
  if (error) {
    logGitError(error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // 2. Get the tree to commit (your staged changes)
  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    logGitError(error);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid treeOid;

  error = git_index_write_tree(&treeOid, index);
  if (error) {
    logGitError(error);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeOid);
  if (error) {
    logGitError(error);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  // 3. Create committer signature (or use nullptr to keep original)
  error = git_signature_now(&committerSig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    logGitError(error);
    git_tree_free(tree);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  authorSig = git_commit_author(commitToAmend);

  // 4. Amend the commit
  error = git_commit_amend(
      &newCommitOid,
      commitToAmend, // Commit to amend
      "HEAD",        // Update HEAD reference
      authorSig,     // Keep original author (or provide new one)
      committerSig,  // New committer (or nullptr to keep original)
      nullptr,       // Use default encoding
      summary.toStdString()
          .c_str(), // Keep original message (or provide new one)
      tree          // New tree (or nullptr to keep original)
  );
  if (error) {
    logGitError(error);
    git_signature_free(committerSig);
    git_tree_free(tree);

    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
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
