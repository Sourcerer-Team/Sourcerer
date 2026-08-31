#include "SvnRepo.h"

#include <svn_auth.h>
#include <svn_config.h>
#include <svn_error.h>
#include <svn_hash.h>
#include <svn_opt.h>
#include <svn_pools.h>
#include <svn_props.h>
#include <svn_time.h>
#include <svn_wc.h>

#include <svnxx/init.hpp>

#include <EventManager.h>
#include <GitCredentialStorage.h>
#include <GitRepo.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>

#include <QByteArray>
#include <QDir>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <functional>
#include <memory>
#include <mutex>
#include <set>

#define THREAD_ID "SvnRepo"

namespace {

static void sendCloneProgressSignal(EventManager::EventType type,
                                    const QString &uuid,
                                    const GitRepo::CloneProgress &progress) {
  EventManager::Event event(type);
  event.addData(GitRepoManager::Structs::GitRepoResult{
      uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
      QVariant::fromValue(progress)});
  EventManager::EventManager::getInstance().send(event);
}

struct CheckoutProgressBaton {
  GitRepo::CloneProgress progress;
  size_t countedEntries = 0;
  size_t completedNotifications = 0;
};

static void emitCheckoutProgress(CheckoutProgressBaton &baton,
                                 const GitRepo::CloneProgressStageType stage) {
  baton.progress.started = true;
  baton.progress.finished = false;
  baton.progress.error = false;
  baton.progress.cloning = true;
  baton.progress.stage = stage;
  if (baton.progress.totalSteps > 0 &&
      baton.progress.completedSteps > baton.progress.totalSteps) {
    baton.progress.completedSteps = baton.progress.totalSteps;
  }
  baton.progress.lastStepPercentage =
      baton.progress.totalSteps > 0
          ? static_cast<int>((100.0 * baton.progress.completedSteps) /
                             baton.progress.totalSteps)
          : -1;
  sendCloneProgressSignal(
      EventManager::EventType::SvnRepoManagerCloneRepoProgress,
      baton.progress.uuid, baton.progress);
}

static svn_error_t *countCheckoutEntry(void *baton, const char *,
                                       const svn_dirent_t *, const svn_lock_t *,
                                       const char *, apr_pool_t *) {
  if (!baton) {
    return SVN_NO_ERROR;
  }

  auto *progress = static_cast<CheckoutProgressBaton *>(baton);
  ++progress->countedEntries;
  progress->progress.completedSteps = progress->countedEntries;
  progress->progress.totalSteps = progress->countedEntries + 1;
  progress->progress.lastStepPercentage =
      static_cast<int>((100.0 * progress->progress.completedSteps) /
                       progress->progress.totalSteps);
  emitCheckoutProgress(*progress, GitRepo::CloneProgressStageType::Fetch);
  return SVN_NO_ERROR;
}

static size_t countCheckoutEntries(svn_client_ctx_t *ctx, apr_pool_t *pool,
                                   const QByteArray &urlBytes,
                                   svn_opt_revision_t *pegRev,
                                   svn_opt_revision_t *rev,
                                   CheckoutProgressBaton &progressBaton) {
  svn_error_t *error =
      svn_client_list(urlBytes.constData(), pegRev, rev, TRUE, SVN_DIRENT_KIND,
                      FALSE, countCheckoutEntry, &progressBaton, ctx, pool);
  if (error) {
    svn_error_clear(error);
    return 0;
  }

  return progressBaton.countedEntries;
}

static void checkoutNotifyCallback(void *baton, const svn_wc_notify_t *notify,
                                   apr_pool_t *) {
  if (!baton || !notify) {
    return;
  }

  auto *progress = static_cast<CheckoutProgressBaton *>(baton);
  progress->progress.started = true;
  progress->progress.finished = false;
  progress->progress.error = false;
  progress->progress.cloning = true;
  progress->progress.stage = GitRepo::CloneProgressStageType::Checkout;

  switch (notify->action) {
  case svn_wc_notify_update_started:
    progress->progress.completedSteps = 0;
    sendCloneProgressSignal(
        EventManager::EventType::SvnRepoManagerCloneRepoProgress,
        progress->progress.uuid, progress->progress);
    break;
  case svn_wc_notify_update_add:
  case svn_wc_notify_update_update:
  case svn_wc_notify_update_delete:
  case svn_wc_notify_update_completed:
    ++progress->completedNotifications;
    progress->progress.completedSteps = progress->completedNotifications;
    if (progress->progress.totalSteps > 0 &&
        progress->progress.completedSteps > progress->progress.totalSteps) {
      progress->progress.completedSteps = progress->progress.totalSteps;
    }
    progress->progress.lastStepPercentage =
        progress->progress.totalSteps > 0
            ? static_cast<int>((100.0 * progress->progress.completedSteps) /
                               progress->progress.totalSteps)
            : -1;
    sendCloneProgressSignal(
        EventManager::EventType::SvnRepoManagerCloneRepoProgress,
        progress->progress.uuid, progress->progress);
    break;
  default:
    break;
  }
}

static void finishCloneProgress(const QString &uuid, const bool error,
                                const CheckoutProgressBaton &baton) {
  GitRepo::CloneProgress progress;
  progress.uuid = uuid;
  progress.completedSteps = baton.progress.totalSteps;
  progress.totalSteps = baton.progress.totalSteps;
  progress.lastStepPercentage = 100;
  progress.started = true;
  progress.finished = true;
  progress.error = error;
  progress.cloning = true;
  progress.stage = GitRepo::CloneProgressStageType::Checkout;

  std::this_thread::sleep_for(1000ms);

  sendCloneProgressSignal(
      EventManager::EventType::SvnRepoManagerCloneRepoProgress, progress.uuid,
      progress);
}

} // namespace

namespace SvnRepo {
namespace {

static void setLastError(SvnError error, int errorCode,
                         const QString &errorMessage);

struct SvnCredentialPayload {
  QString url;
  QString destinationDir;
  QString uuid;
  size_t requestId = 0;
  bool requested = false;
};

static void sendCredentialsSignal(EventManager::EventType type, size_t id) {
  EventManager::Event event(type);
  event.addData(id);
  EventManager::EventManager::getInstance().send(event);
}

static bool acquireCredentials(SvnCredentialPayload &payload,
                               const QString &username,
                               GitRepo::GitCredentialsResponse &response) {
  const QString normalizedRepoPath =
      QDir::fromNativeSeparators(payload.destinationDir);

  LOG_INFO("acquireCredentials called with repoPath: '{}', url: '{}'",
           normalizedRepoPath, payload.url);

  GitRepo::GitCredentialStorage &credStorage =
      GitRepo::GitCredentialStorage::getInstance();

  QString storedUsername;
  QString storedPassword;
  if (credStorage.getCredentials(normalizedRepoPath, storedUsername,
                                 storedPassword)) {
    LOG_INFO("Found stored credentials for repository: {}", normalizedRepoPath);
    response.id = payload.requestId;
    response.username = storedUsername;
    response.password = storedPassword;
    response.rememberCredentials = true;
    return true;
  }

  LOG_INFO("No stored credentials found for repository: {}",
           normalizedRepoPath);

  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  const int token = eventManager.startWait();
  payload.requested = true;

  GitRepo::GitCredentialsRequest request;
  request.id = payload.requestId;
  request.url = payload.url;
  request.username = username;
  request.allowedTypes = 0;
  request.repoPath = normalizedRepoPath;
  request.needsPassphrase = false;

  EventManager::Event credentialsEvent(
      EventManager::EventType::GitRepoManagerGetCredentials);
  credentialsEvent.addData(request);
  eventManager.send(credentialsEvent);

  const auto predicate = [&](EventManager::Event event) -> bool {
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
      const auto [id] = event.data<GitRepo::GitCredentialsErrorResponse>();
      return id == request.id;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
      const auto submit = event.data<GitRepo::GitCredentialsResponse>();
      return submit.id == request.id;
    }
    default:
      return false;
    }
  };

  const std::set<EventManager::EventType> types{
      EventManager::EventType::GitRepoManagerGetCredentialsCancel,
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit};

  const auto &[success, event] =
      eventManager.waitOneOf(token, types, predicate);
  if (!success) {
    setLastError(SvnError::CheckoutFailed, SVN_ERR_CANCELLED,
                 QStringLiteral("Credential request was cancelled"));
    return false;
  }

  if (event.type() ==
      EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
    setLastError(SvnError::CheckoutFailed, SVN_ERR_CANCELLED,
                 QStringLiteral("Credential request was cancelled"));
    return false;
  }

  response = event.data<GitRepo::GitCredentialsResponse>();

  if (response.rememberCredentials) {
    credStorage.storeCredentials(normalizedRepoPath, response.username,
                                 response.password, true);
    LOG_INFO("Stored credentials for repository: {}", normalizedRepoPath);
  } else {
    LOG_INFO("User chose NOT to remember credentials");
  }

  return true;
}

static svn_error_t *simplePromptCallback(svn_auth_cred_simple_t **cred_p,
                                         void *baton, const char *realm,
                                         const char *username,
                                         svn_boolean_t may_save,
                                         apr_pool_t *pool) {
  Q_UNUSED(realm);
  Q_UNUSED(may_save);

  if (!cred_p || !baton) {
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, nullptr,
                            "Invalid credentials prompt arguments");
  }

  auto *payload = static_cast<SvnCredentialPayload *>(baton);
  GitRepo::GitCredentialsResponse response;
  if (!acquireCredentials(*payload,
                          username ? QString::fromUtf8(username) : QString(),
                          response)) {
    return svn_error_create(SVN_ERR_CANCELLED, nullptr,
                            "Credential request cancelled");
  }

  auto *creds = static_cast<svn_auth_cred_simple_t *>(
      apr_pcalloc(pool, sizeof(svn_auth_cred_simple_t)));
  const QByteArray usernameBytes = response.username.toUtf8();
  const QByteArray passwordBytes = response.password.toUtf8();
  creds->username = apr_pstrdup(pool, usernameBytes.constData());
  creds->password = apr_pstrdup(pool, passwordBytes.constData());
  creds->may_save = response.rememberCredentials;
  *cred_p = creds;
  return SVN_NO_ERROR;
}

static svn_error_t *usernamePromptCallback(svn_auth_cred_username_t **cred_p,
                                           void *baton, const char *realm,
                                           svn_boolean_t may_save,
                                           apr_pool_t *pool) {
  Q_UNUSED(realm);
  Q_UNUSED(may_save);

  if (!cred_p || !baton) {
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, nullptr,
                            "Invalid credentials prompt arguments");
  }

  auto *payload = static_cast<SvnCredentialPayload *>(baton);
  GitRepo::GitCredentialsResponse response;
  if (!acquireCredentials(*payload, QString(), response)) {
    return svn_error_create(SVN_ERR_CANCELLED, nullptr,
                            "Credential request cancelled");
  }

  auto *creds = static_cast<svn_auth_cred_username_t *>(
      apr_pcalloc(pool, sizeof(svn_auth_cred_username_t)));
  const QByteArray usernameBytes = response.username.toUtf8();
  creds->username = apr_pstrdup(pool, usernameBytes.constData());
  creds->may_save = response.rememberCredentials;
  *cred_p = creds;
  return SVN_NO_ERROR;
}

static svn_error_t *sslServerTrustPromptCallback(
    svn_auth_cred_ssl_server_trust_t **cred_p, void *baton, const char *realm,
    apr_uint32_t failures, const svn_auth_ssl_server_cert_info_t *cert_info,
    svn_boolean_t may_save, apr_pool_t *pool) {
  Q_UNUSED(baton);
  Q_UNUSED(realm);
  Q_UNUSED(cert_info);
  Q_UNUSED(may_save);

  if (!cred_p) {
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, nullptr,
                            "Invalid SSL trust prompt arguments");
  }

  auto *creds = static_cast<svn_auth_cred_ssl_server_trust_t *>(
      apr_pcalloc(pool, sizeof(svn_auth_cred_ssl_server_trust_t)));
  creds->may_save = FALSE;
  creds->accepted_failures = failures;
  *cred_p = creds;
  return SVN_NO_ERROR;
}

static svn_auth_baton_t *createAuthBaton(apr_pool_t *pool,
                                         SvnCredentialPayload *payload) {
  apr_array_header_t *providers =
      apr_array_make(pool, 3, sizeof(svn_auth_provider_object_t *));

  svn_auth_provider_object_t *provider = nullptr;
  svn_auth_get_simple_prompt_provider(&provider, simplePromptCallback, payload,
                                      2, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_get_username_prompt_provider(&provider, usernamePromptCallback,
                                        payload, 2, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_get_ssl_server_trust_prompt_provider(
      &provider, sslServerTrustPromptCallback, payload, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_baton_t *authBaton = nullptr;
  svn_auth_open(&authBaton, providers, pool);
  return authBaton;
}

template <typename T> T defaultValue() {
  return T{};
}

thread_local SvnErrorContext gLastError;
std::mutex gInitMutex;
std::unique_ptr<apache::subversion::svnxx::init> gSvnInit;

struct AprPoolDeleter {
  void operator()(apr_pool_t *pool) const noexcept {
    if (pool) {
      svn_pool_destroy(pool);
    }
  }
};

using AprPoolPtr = std::unique_ptr<apr_pool_t, AprPoolDeleter>;

static void setLastError(SvnError error, int errorCode,
                         const QString &errorMessage) {
  gLastError.error = error;
  gLastError.errorCode = errorCode;
  gLastError.errorMessage = errorMessage;
}

static QString svnErrorMessage(const svn_error_t *error) {
  if (!error) {
    return QString();
  }

  char buf[512];
  const char *message = svn_err_best_message(error, buf, sizeof(buf));
  return QString::fromUtf8(message ? message : "");
}

static QString pathFromLocalAbspath(const char *path) {
  return path ? QString::fromUtf8(path) : QString();
}

static AprPoolPtr makePool() {
  return AprPoolPtr(svn_pool_create(nullptr));
}

static svn_client_ctx_t *createClientContext(apr_pool_t *pool) {
  svn_client_ctx_t *ctx = nullptr;
  svn_error_t *error = svn_client_create_context2(&ctx, nullptr, pool);
  if (error) {
    setLastError(SvnError::CreateClientContextFailed, error->apr_err,
                 svnErrorMessage(error));
    svn_error_clear(error);
    return nullptr;
  }

  error = svn_config_get_config(&(ctx->config), nullptr, pool);
  if (error) {
    setLastError(SvnError::CreateClientContextFailed, error->apr_err,
                 svnErrorMessage(error));
    svn_error_clear(error);
    return nullptr;
  }

  return ctx;
}

struct PathArray {
  apr_array_header_t *array = nullptr;
  QVector<QByteArray> storage;
};

static void fillPathStatus(SvnStatus *status, const QString &path,
                           svn_wc_status_kind kind) {
  if (!status) {
    return;
  }

  switch (kind) {
  case svn_wc_status_added:
    status->newFiles.append(path);
    break;
  case svn_wc_status_deleted:
  case svn_wc_status_missing:
    status->deletedFiles.append(path);
    break;
  case svn_wc_status_conflicted:
    status->conflictedFiles.append(path);
    break;
  case svn_wc_status_unversioned:
    status->untrackedFiles.append(path);
    break;
  case svn_wc_status_ignored:
    status->ignoredFiles.append(path);
    break;
  case svn_wc_status_modified:
  case svn_wc_status_replaced:
  case svn_wc_status_merged:
  case svn_wc_status_obstructed:
    status->modifiedFiles.append(path);
    break;
  default:
    break;
  }
}

struct StatusBaton {
  SvnStatus *status = nullptr;
};

static svn_error_t *statusCallback(void *baton, const char *path,
                                   const svn_client_status_t *status,
                                   apr_pool_t *scratch_pool) {
  Q_UNUSED(scratch_pool);

  auto *payload = static_cast<StatusBaton *>(baton);
  if (!payload || !payload->status) {
    return SVN_NO_ERROR;
  }

  const QString localPath = pathFromLocalAbspath(path);
  fillPathStatus(payload->status, localPath,
                 status ? status->node_status : svn_wc_status_none);

  if (status && status->conflicted) {
    payload->status->conflictedFiles.append(localPath);
  }

  return SVN_NO_ERROR;
}

struct CommitLogBaton {
  QString message;
};

static svn_error_t *commitLogCallback(const char **log_msg,
                                      const char **tmp_file,
                                      const apr_array_header_t *commit_items,
                                      void *baton, apr_pool_t *pool) {
  Q_UNUSED(tmp_file);
  Q_UNUSED(commit_items);

  if (!log_msg || !baton) {
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, nullptr,
                            "Invalid commit log callback arguments");
  }

  const auto *payload = static_cast<CommitLogBaton *>(baton);
  const QByteArray utf8 = payload->message.toUtf8();
  *log_msg = apr_pstrdup(pool, utf8.constData());
  return SVN_NO_ERROR;
}

struct HistoryBaton {
  SvnHistory *history = nullptr;
};

static svn_error_t *historyCallback(void *baton, svn_log_entry_t *log_entry,
                                    apr_pool_t *pool) {
  if (!baton || !log_entry) {
    return SVN_NO_ERROR;
  }

  auto *payload = static_cast<HistoryBaton *>(baton);
  if (!payload->history) {
    return SVN_NO_ERROR;
  }

  SvnCommitInfo info;
  info.revision = static_cast<qint64>(log_entry->revision);

  if (log_entry->revprops) {
    if (const auto *author = static_cast<const svn_string_t *>(
            svn_hash_gets(log_entry->revprops, SVN_PROP_REVISION_AUTHOR))) {
      info.authorName =
          QString::fromUtf8(author->data, static_cast<int>(author->len));
    }

    if (const auto *message = static_cast<const svn_string_t *>(
            svn_hash_gets(log_entry->revprops, SVN_PROP_REVISION_LOG))) {
      info.message =
          QString::fromUtf8(message->data, static_cast<int>(message->len));
    }

    if (const auto *date = static_cast<const svn_string_t *>(
            svn_hash_gets(log_entry->revprops, SVN_PROP_REVISION_DATE))) {
      apr_time_t when = 0;
      svn_error_t *error = svn_time_from_cstring(&when, date->data, pool);
      if (!error) {
        info.timeStamp = static_cast<qint64>(when / 1000);
      } else {
        svn_error_clear(error);
      }
    }
  }

  const QString revision = QString("%1").arg(info.revision);
  payload->history->commits += revision;
  payload->history->commitInfos[revision] = info;
  return SVN_NO_ERROR;
}

static bool ensureInitialized() {
  std::lock_guard<std::mutex> lock(gInitMutex);
  if (gSvnInit) {
    return true;
  }

  try {
    gSvnInit = std::make_unique<apache::subversion::svnxx::init>();
    return true;
  } catch (const std::exception &ex) {
    setLastError(SvnError::InitializationFailed, -1,
                 QString::fromUtf8(ex.what()));
    return false;
  }
}

static bool runWithClientContext(
    const std::function<bool(svn_client_ctx_t *, apr_pool_t *)> &fn) {
  if (!ensureInitialized()) {
    return false;
  }

  auto pool = makePool();
  if (!pool) {
    setLastError(SvnError::InitializationFailed, -1,
                 QStringLiteral("Failed to create APR pool"));
    return false;
  }

  svn_client_ctx_t *ctx = createClientContext(pool.get());
  if (!ctx) {
    return false;
  }

  return fn(ctx, pool.get());
}

} // namespace

bool SvnStatus::hasChanges() const {
  return !newFiles.isEmpty() || !modifiedFiles.isEmpty() ||
         !deletedFiles.isEmpty() || !typeChangedFiles.isEmpty() ||
         !renamedFiles.isEmpty() || !unreadableFiles.isEmpty() ||
         !newStagedFiles.isEmpty() || !modifiedStagedFiles.isEmpty() ||
         !deletedStagedFiles.isEmpty() || !typeChangedStagedFiles.isEmpty() ||
         !renamedStagedFiles.isEmpty() /*|| !ignoredFiles.isEmpty()*/ ||
         !conflictedFiles.isEmpty() || !untrackedFiles.isEmpty();
}

bool SvnStatus::operator==(const SvnStatus &other) const {
  return path == other.path && newFiles == other.newFiles &&
         modifiedFiles == other.modifiedFiles &&
         deletedFiles == other.deletedFiles &&
         typeChangedFiles == other.typeChangedFiles &&
         renamedFiles == other.renamedFiles &&
         unreadableFiles == other.unreadableFiles &&
         newStagedFiles == other.newStagedFiles &&
         modifiedStagedFiles == other.modifiedStagedFiles &&
         deletedStagedFiles == other.deletedStagedFiles &&
         typeChangedStagedFiles == other.typeChangedStagedFiles &&
         renamedStagedFiles == other.renamedStagedFiles &&
         ignoredFiles == other.ignoredFiles &&
         conflictedFiles == other.conflictedFiles &&
         untrackedFiles == other.untrackedFiles;
}

SvnErrorContext getLastError() {
  return gLastError;
}

void initForCurrentThread() {
  ensureInitialized();
}

void shutdownForCurrentThread() {}

bool init() {
  return ensureInitialized();
}

bool checkoutRepository(const QString &url, const QString &destinationDir,
                        const QString &uuid) {
  return runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
    svn_revnum_t resultRev = SVN_INVALID_REVNUM;
    svn_opt_revision_t pegRev;
    pegRev.kind = svn_opt_revision_unspecified;
    svn_opt_revision_t rev;
    rev.kind = svn_opt_revision_head;

    SvnCredentialPayload credentialPayload;
    credentialPayload.url = url;
    credentialPayload.destinationDir = destinationDir;
    credentialPayload.uuid = uuid;
    credentialPayload.requestId = qHash(uuid);
    ctx->auth_baton = createAuthBaton(pool, &credentialPayload);

    CheckoutProgressBaton progressBaton;
    progressBaton.progress.uuid = uuid;
    progressBaton.progress.cloning = true;
    progressBaton.progress.stage = GitRepo::CloneProgressStageType::Checkout;

    const QByteArray urlBytes = url.toUtf8();
    const QByteArray destBytes =
        QFileInfo(destinationDir).absoluteFilePath().toUtf8();

    const size_t entryCount =
        countCheckoutEntries(ctx, pool, urlBytes, &pegRev, &rev, progressBaton);
    progressBaton.progress.totalSteps = entryCount > 0 ? entryCount : 1;
    progressBaton.progress.completedSteps = 0;
    progressBaton.progress.lastStepPercentage = 0;
    progressBaton.progress.stage = GitRepo::CloneProgressStageType::Checkout;
    emitCheckoutProgress(progressBaton,
                         GitRepo::CloneProgressStageType::Checkout);
    ctx->notify_func2 = checkoutNotifyCallback;
    ctx->notify_baton2 = &progressBaton;

    svn_error_t *error = svn_client_checkout3(
        &resultRev, urlBytes.constData(), destBytes.constData(), &pegRev, &rev,
        svn_depth_infinity, FALSE, FALSE, ctx, pool);
    finishCloneProgress(uuid, error != nullptr, progressBaton);
    if (credentialPayload.requested) {
      sendCredentialsSignal(
          error ? EventManager::EventType::GitRepoManagerGetCredentialsInvalid
                : EventManager::EventType::GitRepoManagerGetCredentialsFinished,
          credentialPayload.requestId);
    }
    if (error) {
      setLastError(SvnError::CheckoutFailed, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    Q_UNUSED(resultRev);
    return true;
  });
}

bool updateWorkingCopy(const QString &path) {
  return runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
    const QByteArray absPath = QFileInfo(path).absoluteFilePath().toUtf8();
    apr_array_header_t *paths = apr_array_make(pool, 1, sizeof(const char *));
    APR_ARRAY_PUSH(paths, const char *) = absPath.constData();

    svn_opt_revision_t rev;
    rev.kind = svn_opt_revision_head;

    apr_array_header_t *resultRevs = nullptr;
    svn_error_t *error =
        svn_client_update4(&resultRevs, paths, &rev, svn_depth_infinity, FALSE,
                           FALSE, FALSE, TRUE, FALSE, ctx, pool);
    if (error) {
      setLastError(SvnError::UpdateFailed, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    Q_UNUSED(resultRevs);
    return true;
  });
}

SvnStatus getStatus(const QString &path, bool recurse,
                    bool includeUnversioned) {
  SvnStatus result;
  result.path = QFileInfo(path).absoluteFilePath();

  const bool success =
      runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const QByteArray absPath = result.path.toUtf8();
        svn_opt_revision_t rev;
        rev.kind = svn_opt_revision_unspecified;

        StatusBaton baton;
        baton.status = &result;

        svn_revnum_t youngest = SVN_INVALID_REVNUM;
        svn_error_t *error =
            svn_client_status6(&youngest, ctx, absPath.constData(), &rev,
                               recurse ? svn_depth_infinity : svn_depth_empty,
                               includeUnversioned, FALSE, TRUE, FALSE, FALSE,
                               FALSE, nullptr, statusCallback, &baton, pool);
        if (error) {
          setLastError(SvnError::StatusFailed, error->apr_err,
                       svnErrorMessage(error));
          svn_error_clear(error);
          return false;
        }

        Q_UNUSED(youngest);
        return true;
      });

  if (!success) {
    return SvnStatus();
  }

  return result;
}

bool addFiles(const QString &path, const QStringList &files) {
  if (files.isEmpty()) {
    return true;
  }

  return runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
    const QString basePath = QFileInfo(path).absoluteFilePath();
    for (const QString &file : files) {
      const QString absPath = QDir(basePath).absoluteFilePath(file);
      const QByteArray absBytes = absPath.toUtf8();
      const bool recurse = QFileInfo(absPath).isDir();
      svn_error_t *error = svn_client_add5(
          absBytes.constData(), recurse ? svn_depth_infinity : svn_depth_empty,
          FALSE, FALSE, FALSE, TRUE, ctx, pool);
      if (error) {
        setLastError(SvnError::AddFailed, error->apr_err,
                     svnErrorMessage(error));
        svn_error_clear(error);
        return false;
      }
    }

    return true;
  });
}

bool deleteFiles(const QString &path, const QStringList &files) {
  if (files.isEmpty()) {
    return true;
  }

  return runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
    const QString basePath = QFileInfo(path).absoluteFilePath();
    PathArray array;
    array.storage.reserve(files.size());
    array.array = apr_array_make(pool, files.size(), sizeof(const char *));
    for (const QString &file : files) {
      array.storage.push_back(QDir(basePath).absoluteFilePath(file).toUtf8());
      APR_ARRAY_PUSH(array.array, const char *) =
          array.storage.back().constData();
    }

    svn_error_t *error = svn_client_delete4(array.array, FALSE, FALSE, nullptr,
                                            nullptr, nullptr, ctx, pool);
    if (error) {
      setLastError(SvnError::DeleteFailed, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    return true;
  });
}

bool revertFiles(const QString &path, const QStringList &files) {
  if (files.isEmpty()) {
    return true;
  }

  return runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
    const QString basePath = QFileInfo(path).absoluteFilePath();
    PathArray array;
    array.storage.reserve(files.size());
    array.array = apr_array_make(pool, files.size(), sizeof(const char *));
    for (const QString &file : files) {
      array.storage.push_back(QDir(basePath).absoluteFilePath(file).toUtf8());
      APR_ARRAY_PUSH(array.array, const char *) =
          array.storage.back().constData();
    }

    svn_error_t *error =
        svn_client_revert4(array.array, svn_depth_infinity, nullptr, FALSE,
                           FALSE, FALSE, ctx, pool);
    if (error) {
      setLastError(SvnError::RevertFailed, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    return true;
  });
}

bool commit(const QString &path, const QString &message) {
  return runWithClientContext([&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
    const QByteArray absPath = QFileInfo(path).absoluteFilePath().toUtf8();
    apr_array_header_t *targets = apr_array_make(pool, 1, sizeof(const char *));
    APR_ARRAY_PUSH(targets, const char *) = absPath.constData();

    CommitLogBaton baton;
    baton.message = message;
    ctx->log_msg_func3 = commitLogCallback;
    ctx->log_msg_baton3 = &baton;

    svn_commit_info_t *commitInfo = nullptr;
    svn_error_t *error =
        svn_client_commit4(&commitInfo, targets, svn_depth_infinity, FALSE,
                           FALSE, nullptr, nullptr, ctx, pool);
    if (error) {
      setLastError(SvnError::CommitFailed, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    Q_UNUSED(commitInfo);
    return true;
  });
}

SvnHistory getHistory(const QString &path, int limit) {
  SvnHistory result;
  if (limit <= 0) {
    return result;
  }

  const bool success = runWithClientContext([&](svn_client_ctx_t *ctx,
                                                apr_pool_t *pool) {
    const QByteArray absPath = QFileInfo(path).absoluteFilePath().toUtf8();
    apr_array_header_t *targets = apr_array_make(pool, 1, sizeof(const char *));
    APR_ARRAY_PUSH(targets, const char *) = absPath.constData();

    svn_opt_revision_t pegRev;
    pegRev.kind = svn_opt_revision_unspecified;

    svn_opt_revision_t startRev;
    startRev.kind = svn_opt_revision_number;
    startRev.value.number = 0;

    svn_opt_revision_t endRev;
    endRev.kind = svn_opt_revision_head;

    svn_opt_revision_range_t *range = static_cast<svn_opt_revision_range_t *>(
        apr_pcalloc(pool, sizeof(*range)));
    range->start = startRev;
    range->end = endRev;

    apr_array_header_t *ranges =
        apr_array_make(pool, 1, sizeof(const svn_opt_revision_range_t *));
    APR_ARRAY_PUSH(ranges, const svn_opt_revision_range_t *) = range;

    apr_array_header_t *revprops =
        apr_array_make(pool, 3, sizeof(const char *));
    APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_AUTHOR;
    APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_DATE;
    APR_ARRAY_PUSH(revprops, const char *) = SVN_PROP_REVISION_LOG;

    HistoryBaton baton;
    baton.history = &result;

    svn_error_t *error =
        svn_client_log5(targets, &pegRev, ranges, limit, FALSE, FALSE, FALSE,
                        revprops, historyCallback, &baton, ctx, pool);
    if (error) {
      setLastError(SvnError::HistoryFailed, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    result.valid = true;
    return true;
  });

  if (!success) {
    return SvnHistory();
  }

  return result;
}

bool abortMerge(const QString &) {
  return false;
}

bool addSubmodule(const QString &, const QString &, const QString &,
                  const QString &, const QString &) {
  return false;
}

bool amend(const QString &, const QString &, const QString &, const QString &,
           const QString &, bool, bool) {
  return false;
}

bool applyPatch(const QString &, const QString &) {
  return false;
}

bool applyStash(const QString &, int) {
  return false;
}

QString listConflictsForStashFile(const QString &, int, const QString &) {
  return {};
}

bool checkoutBranch(const QString &, const QString &, BranchType) {
  return false;
}

bool checkoutCommit(const QString &, const QString &) {
  return false;
}

QString openRepository(const QString &path) {
  LOG_PERF(0, "Enter");

  const QString normalizedPath =
      QDir::cleanPath(QFileInfo(path).absoluteFilePath());
  LOG_DEBUG("Opening SVN repository at path: '{}'", normalizedPath.toStdString());

  if (normalizedPath.isEmpty() || !QFileInfo(normalizedPath).exists()) {
    setLastError(SvnError::ErrorWhileOpening, SVN_ERR_BAD_FILENAME,
                 QStringLiteral("Repository path does not exist."));
    LOG_PERF(0, "Exit");
    return QString();
  }

  const bool success = runWithClientContext([normalizedPath](svn_client_ctx_t *ctx,
                                                             apr_pool_t *pool) {
    int wcFormat = 0;
    const QByteArray pathBytes = normalizedPath.toUtf8();
    svn_error_t *error = svn_wc_check_wc2(&wcFormat, ctx->wc_ctx,
                                          pathBytes.constData(), pool);
    if (error) {
      setLastError(SvnError::ErrorWhileOpening, error->apr_err,
                   svnErrorMessage(error));
      svn_error_clear(error);
      return false;
    }

    Q_UNUSED(wcFormat);
    return true;
  });

  if (!success) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  LOG_PERF(0, "Exit");
  return normalizedPath;
}

void closeRepository(const QString &) {}

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  return checkoutRepository(url, destinationDir, uuid);
}

bool commitStaged(const QString &, const QString &, const QString &,
                  const QString &, const QString &, bool, bool, bool) {
  return false;
}

bool createBranchFromCommit(const QString &, const QString &, const QString &) {
  return false;
}

QString createPatchFromCommit(const QString &, const QString &) {
  return QString();
}

bool createTagFromCommit(const QString &, const QString &, const QString &,
                         const QString &, const QString &, const QString &) {
  return false;
}

bool addTarget(const QString &, const QString &, const QString &,
               const QString &, const QString &) {
  return false;
}

bool editTarget(const QString &, const QString &, const QString &,
                const QString &, const QString &) {
  return false;
}

bool deleteTarget(const QString &, const QString &, const QString &) {
  return false;
}

bool setDefaultTarget(const QString &, const QString &, const QString &) {
  return false;
}

bool deleteBranch(const QString &, const QString &) {
  return false;
}

bool deleteRemoteBranch(const QString &, const QString &) {
  return false;
}

bool deleteStash(const QString &, int) {
  return false;
}

bool deleteAllStashes(const QString &) {
  return false;
}

bool deleteSubmodule(const QString &, const QString &, const QString &) {
  return false;
}

bool deleteTag(const QString &, const QString &) {
  return false;
}

bool deleteRemoteTag(const QString &, const QString &) {
  return false;
}

FilesDiff diffFiles(const QString &, const QStringList &) {
  return {};
}

FilesDiff diffFilesFromCommit(const QString &, const QStringList &,
                              const QString &) {
  return {};
}

bool discardFiles(const QString &, const QStringList &) {
  return false;
}

bool fetchAll(const QString &) {
  return false;
}

bool fetchRemote(const QString &, const QString &) {
  return false;
}

bool rebaseBranches(const QString &, const QString &, const QString &,
                    const QString &, const QString &) {
  return false;
}

qint64 getFileSizeFromCommit(const QString &, const QString &,
                             const QString &) {
  return -1;
}

QString getActiveBranch(const QString &, BranchType, bool) {
  return {};
}

QStringList getCommitBranches(const QString &, const QString &, bool) {
  return {};
}

QStringList getCommitTags(const QString &, const QString &, bool) {
  return {};
}

QStringList getCommitHeads(const QString &, const QString &, bool) {
  return {};
}

bool getCommitIsHead(const QString &, const QString &, bool) {
  return false;
}

CommitStatus getCommitStatus(const QString &, const QString &) {
  return {};
}

CommitStatus getStashStatus(const QString &, int) {
  return {};
}

bool getIsMergeInProgress(const QString &) {
  return false;
}

SvnHistory getBranchHistory(const QString &, const QString &, BranchType,
                            bool) {
  return {};
}

QString getSubmodulePath(const QString &, const QString &) {
  return {};
}

std::tuple<bool, QString, QString> getUpstream(const QString &,
                                               const QString &) {
  return {false, QString(), QString()};
}

bool initRepository(const QString &, const QString &, const QString &) {
  return false;
}

QString listConflictsForAFile(const QString &, const QString &) {
  return {};
}

QStringList listLocalBranches(const QString &, const bool &) {
  return {};
}

QStringList listRemoteBranches(const QString &, const bool &) {
  return {};
}

QVector<BranchInfo> listRemoteBranchesWithStatus(const QString &,
                                                 const bool &) {
  return {};
}

QStringList listStashes(const QString &, const bool &) {
  return {};
}

QStringList listSubmodules(const QString &) {
  return {};
}

QVector<SubmoduleInfo> listSubmodulesWithStatus(const QString &, const bool &) {
  return {};
}

QStringList listTags(const QString &) {
  return {};
}

bool merge(const QString &, const QString &, const QString &, const QString &,
           const QString &) {
  return false;
}

bool pull(const QString &, const QString &, bool) {
  return false;
}

bool push(const QString &, const QString &, bool) {
  return false;
}

bool pushTag(const QString &, const QString &) {
  return false;
}

bool pushAllTags(const QString &) {
  return false;
}

bool removeRemote(const QString &, const QString &) {
  return false;
}

bool renameBranch(const QString &, const QString &, const QString &) {
  return false;
}

bool renameTag(const QString &, const QString &, const QString &) {
  return false;
}

bool resetToCommit(const QString &, const QString &, ResetType) {
  return false;
}

bool revertCommit(const QString &, const QString &, const QString &,
                  const QString &, const QString &) {
  return false;
}

bool cherryPickCommit(const QString &, const QString &, const QString &,
                      const QString &) {
  return false;
}

bool setUpstream(const QString &, const QString &, const QString &,
                 const QString &) {
  return false;
}

bool stageFiles(const QString &, const QStringList &) {
  return false;
}

bool stash(const QString &) {
  return false;
}

SvnStatus getStatus(const QString &, const bool &) {
  return {};
}

bool updateSubmodule(const QString &, const QString &) {
  return false;
}

bool unstageFiles(const QString &, const QStringList &) {
  return false;
}

QVector<RemoteInfo> listRemotes(const QString &) {
  return {};
}

bool addRemote(const QString &, const QString &, const QString &,
               const QString &) {
  return false;
}

bool updateRemote(const QString &, const QString &, const QString &,
                  const QString &, const QString &) {
  return false;
}

bool setDefaultRemote(const QString &, const QString &) {
  return false;
}

QString getDefaultRemote(const QString &) {
  return {};
}

} // namespace SvnRepo

bool svn_init() {
  return SvnRepo::init();
}
