#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <cstring>

#include <svn_auth.h>
#include <svn_config.h>
#include <svn_dirent_uri.h>
#include <svn_error.h>
#include <svn_hash.h>
#include <svn_io.h>
#include <svn_opt.h>
#include <svn_path.h>
#include <svn_pools.h>
#include <svn_props.h>
#include <svn_repos.h>
#include <svn_string.h>
#include <svn_time.h>
#include <svn_utf.h>
#include <svn_wc.h>

#include <svnxx/init.hpp>

#ifdef SOURCERER_SVNREPO_TESTING
#include <sqlite3.h>
#endif

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitCredentialStorage.h>
#include <GitRepo.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QRegularExpression>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <chrono>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <thread>

#define THREAD_ID "SvnRepo"

using namespace std::chrono_literals;

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

// Checkout notifications between two progress events. Sending one event per
// file floods the event queue for large repositories.
constexpr size_t kCheckoutProgressStep = 100;

struct CheckoutProgressBaton {
  GitRepo::CloneProgress progress;
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
    if (notify->action != svn_wc_notify_update_completed &&
        progress->completedNotifications % kCheckoutProgressStep != 0) {
      break;
    }
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

struct SvnCredentialPayload {
  QString url;
  QString destinationDir;
  QString uuid;
  size_t requestId = 0;
  bool requested = false;
  // Stored credentials are tried once; the server refusing them leads to a
  // prompt (B21).
  bool usedStored = false;
  // The user answered; the answer is confirmed (and remembered) once the
  // server accepts it, or reported invalid when it asks again (B65).
  bool answered = false;
  GitRepo::GitCredentialsResponse answer;
};

thread_local SvnErrorContext gLastError;
thread_local const bool *gQuitFlag = nullptr;
thread_local QString gCurrentRepositoryPath;
// Credential prompts raised on this thread that have not been closed yet.
thread_local std::set<size_t> gPromptedCredentialIds;

std::mutex gInitMutex;
std::unique_ptr<apache::subversion::svnxx::init> gSvnInit;

// Configuration area every client context reads, empty for the user's own.
// Only changed by tests.
std::mutex gConfigDirectoryMutex;
QString gConfigDirectory;

static QString currentConfigDirectory() {
  std::lock_guard<std::mutex> lock(gConfigDirectoryMutex);
  return gConfigDirectory;
}

static void sendCredentialsSignal(EventManager::EventType type, size_t id) {
  EventManager::Event event(type);
  event.addData(id);
  EventManager::EventManager::getInstance().send(event);
}

static void finishCredentialPrompt(SvnCredentialPayload &payload,
                                   const bool success);

static bool acquireCredentials(SvnCredentialPayload &payload,
                               const QString &username,
                               GitRepo::GitCredentialsResponse &response) {
  const QString normalizedRepoPath =
      QDir::fromNativeSeparators(payload.destinationDir);

  LOG_INFO("acquireCredentials called with repoPath: '{}', url: '{}'",
           normalizedRepoPath, payload.url);

  GitRepo::GitCredentialStorage &credStorage =
      GitRepo::GitCredentialStorage::getInstance();

  // Stored credentials, once and while they haven't expired, as in the Git
  // backend (B66).
  QString storedUsername;
  QString storedPassword;
  const qlonglong expirationTime =
      ConfigManager::ConfigManager::getInstance().getLong(
          ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime);
  if (!payload.usedStored && !payload.answered &&
      credStorage.getCredentials(normalizedRepoPath, storedUsername,
                                 storedPassword) &&
      !credStorage.areCredentialsExpired(normalizedRepoPath, expirationTime)) {
    LOG_INFO("Found stored credentials for repository: {}", normalizedRepoPath);
    payload.usedStored = true;
    response.id = payload.requestId;
    response.username = storedUsername;
    response.password = storedPassword;
    response.rememberCredentials = true;
    return true;
  }
  payload.usedStored = true;

  LOG_INFO("No usable stored credentials for repository: {}",
           normalizedRepoPath);

  // Asked again after an answer: the server refused it.
  if (payload.answered && payload.requested) {
    sendCredentialsSignal(
        EventManager::EventType::GitRepoManagerGetCredentialsInvalid,
        payload.requestId);
  }

  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  const int token = eventManager.startWait();
  payload.requested = true;
  gPromptedCredentialIds.insert(payload.requestId);

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
  if (!success ||
      event.type() ==
          EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
    Private::setLastError(SvnError::Cancelled, SVN_ERR_CANCELLED,
                          QStringLiteral("Credential request was cancelled"));
    return false;
  }

  response = event.data<GitRepo::GitCredentialsResponse>();

  // Confirmed (closed, and remembered when asked to) by
  // confirmSaveCredentials once the server accepts it.
  payload.answered = true;
  payload.answer = response;
  return true;
}

// Called by Subversion with credentials the server accepted: the prompt
// that asked for them is closed, and the answer remembered if the user
// wanted it. Nothing is saved in Subversion's own auth area.
static svn_error_t *confirmFirstCredentials(void **credentials,
                                            void **iterBaton, void *,
                                            apr_hash_t *, const char *,
                                            apr_pool_t *) {
  *credentials = nullptr;
  *iterBaton = nullptr;
  return SVN_NO_ERROR;
}

static svn_error_t *confirmSaveCredentials(svn_boolean_t *saved, void *,
                                           void *providerBaton, apr_hash_t *,
                                           const char *, apr_pool_t *) {
  *saved = FALSE;
  auto *payload = static_cast<SvnCredentialPayload *>(providerBaton);
  if (!payload || !payload->answered || !payload->requested) {
    return SVN_NO_ERROR;
  }

  if (payload->answer.rememberCredentials) {
    GitRepo::GitCredentialStorage::getInstance().storeCredentials(
        QDir::fromNativeSeparators(payload->destinationDir),
        payload->answer.username, payload->answer.password, true);
  }
  finishCredentialPrompt(*payload, true);
  return SVN_NO_ERROR;
}

const svn_auth_provider_t kConfirmProvider = {
    SVN_AUTH_CRED_SIMPLE, confirmFirstCredentials, nullptr,
    confirmSaveCredentials};

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

// Why a certificate with these failures isn't trusted, for the error message.
static QString certificateFailures(const apr_uint32_t failures) {
  QStringList reasons;
  if (failures & SVN_AUTH_SSL_NOTYETVALID) {
    reasons << QStringLiteral("it is not valid yet");
  }
  if (failures & SVN_AUTH_SSL_EXPIRED) {
    reasons << QStringLiteral("it has expired");
  }
  if (failures & SVN_AUTH_SSL_CNMISMATCH) {
    reasons << QStringLiteral("it was issued for another host");
  }
  if (failures & SVN_AUTH_SSL_OTHER) {
    reasons << QStringLiteral("it could not be verified");
  }
  return reasons.join(QStringLiteral(", "));
}

// Server certificates follow the Git backend's policy (GitRepoCertificates):
// a chain the system trusts is accepted before this prompt is asked (the
// Windows store provider on Windows, OpenSSL's default CAs elsewhere). A
// certificate whose only failure is its unknown issuer is trusted on first
// use when it is self-signed, and pinned per host with the Git pins.
// Everything else is rejected, and nothing is stored in Subversion's own
// auth area.
static svn_error_t *sslServerTrustPromptCallback(
    svn_auth_cred_ssl_server_trust_t **cred_p, void *baton, const char *realm,
    apr_uint32_t failures, const svn_auth_ssl_server_cert_info_t *cert_info,
    svn_boolean_t may_save, apr_pool_t *pool) {
  Q_UNUSED(baton);
  Q_UNUSED(may_save);

  if (!cred_p) {
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, nullptr,
                            "Invalid SSL trust prompt arguments");
  }
  *cred_p = nullptr;

  // The realm is the server's address, "https://host:port".
  QString host = QUrl(QString::fromUtf8(realm ? realm : "")).host();
  if (host.isEmpty() && cert_info && cert_info->hostname) {
    host = QString::fromUtf8(cert_info->hostname);
  }

  // serf doesn't match IP addresses in subjectAltName, so a certificate for
  // a server addressed by IP fails as one for another host even when it
  // names the address (B67): the address is matched here.
  static const QRegularExpression ipAddress(
      QStringLiteral("^(\\d{1,3}(\\.\\d{1,3}){3}|\\[?[0-9a-fA-F:]*:[0-9a-fA-F:.]*\\]?)$"));
  const QByteArray der = QByteArray::fromBase64(QByteArray(
      cert_info && cert_info->ascii_cert ? cert_info->ascii_cert : ""));
  const apr_uint32_t checkedFailures =
      (failures & SVN_AUTH_SSL_CNMISMATCH) && ipAddress.match(host).hasMatch() &&
              GitRepo::certificateMatchesHost(der, host)
          ? failures & ~SVN_AUTH_SSL_CNMISMATCH
          : failures;

  QString rejection;
  bool trusted = failures == 0;
  if (!trusted && (checkedFailures & ~SVN_AUTH_SSL_UNKNOWNCA) != 0) {
    rejection = QStringLiteral("The TLS certificate of %1 is not trusted: %2.")
                    .arg(host, certificateFailures(failures));
  } else if (!trusted) {
    trusted = GitRepo::checkUnverifiedServerCertificate(der, host, &rejection);
  }

  if (!trusted) {
    LOG_WARN("{}", rejection);
    return svn_error_create(SVN_ERR_RA_SERF_SSL_CERT_UNTRUSTED, nullptr,
                            rejection.toUtf8().constData());
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
      apr_array_make(pool, 4, sizeof(svn_auth_provider_object_t *));

  // Operations that only need a user name, like commits to file://
  // repositories, use the name of the logged in user without asking.
  svn_auth_provider_object_t *provider = nullptr;
  svn_auth_get_username_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_get_simple_prompt_provider(&provider, simplePromptCallback, payload,
                                      2, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_get_username_prompt_provider(&provider, usernamePromptCallback,
                                        payload, 2, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  // Learns which answers the server accepted.
  auto *confirm = static_cast<svn_auth_provider_object_t *>(
      apr_pcalloc(pool, sizeof(svn_auth_provider_object_t)));
  confirm->vtable = &kConfirmProvider;
  confirm->provider_baton = payload;
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = confirm;

#ifdef Q_OS_WIN
  // Chains the Windows certificate store trusts never reach the prompt.
  svn_auth_get_windows_ssl_server_trust_provider(&provider, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;
#endif

  svn_auth_get_ssl_server_trust_prompt_provider(
      &provider, sslServerTrustPromptCallback, payload, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_baton_t *authBaton = nullptr;
  svn_auth_open(&authBaton, providers, pool);
  return authBaton;
}

// Cached credentials live in the configuration area too, so a test's
// configuration directory applies to them as well.
static void applyConfigDirectory(svn_auth_baton_t *authBaton,
                                 apr_pool_t *pool) {
  const QString configDir = currentConfigDirectory();
  if (!configDir.isEmpty()) {
    svn_auth_set_parameter(
        authBaton, SVN_AUTH_PARAM_CONFIG_DIR,
        svn_dirent_internal_style(configDir.toUtf8().constData(), pool));
  }
}

// Closes the credential prompt of payload, if one was shown.
static void finishCredentialPrompt(SvnCredentialPayload &payload,
                                   const bool success) {
  if (!payload.requested) {
    return;
  }

  sendCredentialsSignal(
      success ? EventManager::EventType::GitRepoManagerGetCredentialsFinished
              : EventManager::EventType::GitRepoManagerGetCredentialsInvalid,
      payload.requestId);
  gPromptedCredentialIds.erase(payload.requestId);
  payload.requested = false;
}

struct AprPoolDeleter {
  void operator()(apr_pool_t *pool) const noexcept {
    if (pool) {
      svn_pool_destroy(pool);
    }
  }
};

using AprPoolPtr = std::unique_ptr<apr_pool_t, AprPoolDeleter>;

static AprPoolPtr makePool() {
  return AprPoolPtr(svn_pool_create(nullptr));
}

static svn_error_t *cancelCallback(void *) {
  if (gQuitFlag && *gQuitFlag) {
    return svn_error_create(SVN_ERR_CANCELLED, nullptr, "Operation cancelled");
  }

  return SVN_NO_ERROR;
}

static svn_client_ctx_t *createClientContext(apr_pool_t *pool) {
  svn_client_ctx_t *ctx = nullptr;
  svn_error_t *error = svn_client_create_context2(&ctx, nullptr, pool);
  if (error) {
    Private::reportError(SvnError::CreateClientContextFailed, error);
    return nullptr;
  }

  const QString configDir = currentConfigDirectory();
  const char *configDirPath =
      configDir.isEmpty()
          ? nullptr
          : svn_dirent_internal_style(configDir.toUtf8().constData(), pool);
  error = svn_config_get_config(&(ctx->config), configDirPath, pool);
  if (error) {
    Private::reportError(SvnError::CreateClientContextFailed, error);
    return nullptr;
  }

  ctx->cancel_func = cancelCallback;
  ctx->cancel_baton = nullptr;

  return ctx;
}

static bool ensureInitialized() {
  std::lock_guard<std::mutex> lock(gInitMutex);
  if (gSvnInit) {
    return true;
  }

  try {
    gSvnInit = std::make_unique<apache::subversion::svnxx::init>();
    // Strings Subversion takes as "native", such as the paths in a patch
    // file, are UTF-8 here: APR passes file names to Windows as UTF-8 too.
    static apr_pool_t *utfPool = svn_pool_create(nullptr);
    svn_utf_initialize2(TRUE, utfPool);
    return true;
  } catch (const std::exception &ex) {
    Private::setLastError(SvnError::InitializationFailed, -1,
                          QString::fromUtf8(ex.what()));
    return false;
  }
}

static void setCurrentRepositoryPath(const QString &path) {
  gCurrentRepositoryPath = Private::normalizedPath(path);
}

static void clearCurrentRepositoryPath() {
  gCurrentRepositoryPath.clear();
}

struct StatusBaton {
  SvnStatus *status = nullptr;
  QString wcRoot;
  // Subversion does not report the content of unversioned directories.
  QStringList unversionedDirectories;
  // Folders staged with stageFiles.
  QSet<QString> stagedDirectories;
  // Roots of the externals defined in the working copy, with a trailing /.
  QStringList externals;
  // Folders reported for a change of their own: they stay in the lists when
  // files below them are reported too.
  QSet<QString> changedDirectories;
};

static void addStatusPath(SvnStatus *status, const QString &path,
                          const svn_client_status_t *entry, const bool staged) {
  // Only the files the user selected are staged. Everything else that has
  // changes is reported as unstaged, and only staged files are committed.
  switch (entry->node_status) {
  case svn_wc_status_added:
    if (!staged) {
      status->newFiles.append(path);
    } else if (entry->copied) {
      status->renamedStagedFiles.append(path);
    } else {
      status->newStagedFiles.append(path);
    }
    break;
  case svn_wc_status_deleted:
    (staged ? status->deletedStagedFiles : status->deletedFiles).append(path);
    break;
  case svn_wc_status_missing:
    status->deletedFiles.append(path);
    break;
  case svn_wc_status_replaced:
    (staged ? status->typeChangedStagedFiles : status->typeChangedFiles)
        .append(path);
    break;
  case svn_wc_status_modified:
  case svn_wc_status_merged:
    (staged ? status->modifiedStagedFiles : status->modifiedFiles).append(path);
    break;
  case svn_wc_status_obstructed:
    status->typeChangedFiles.append(path);
    break;
  case svn_wc_status_unversioned:
    status->newFiles.append(path);
    break;
  case svn_wc_status_ignored:
    // The default global-ignores hide *.rej, but the rejected hunks of a
    // patch are there for the user to look at.
    if (path.endsWith(QLatin1String(".svnpatch.rej"))) {
      status->newFiles.append(path);
    } else {
      status->ignoredFiles.append(path);
    }
    break;
  case svn_wc_status_conflicted:
    break;
  default:
    break;
  }

  if (entry->conflicted) {
    status->conflictedFiles.append(path);
  }
}

static svn_error_t *statusCallback(void *baton, const char *path,
                                   const svn_client_status_t *status,
                                   apr_pool_t *) {
  auto *payload = static_cast<StatusBaton *>(baton);
  if (!payload || !payload->status || !status || !path) {
    return SVN_NO_ERROR;
  }

  // An external is a working copy of its own, like a Git submodule: neither
  // it nor its changes are changes of this working copy.
  const QString absolutePath = QString::fromUtf8(status->local_abspath);
  const QString pathWithSlash = absolutePath + QLatin1Char('/');
  if (status->node_status == svn_wc_status_external ||
      std::any_of(payload->externals.cbegin(), payload->externals.cend(),
                  [&](const QString &root) {
                    return pathWithSlash.startsWith(root);
                  })) {
    return SVN_NO_ERROR;
  }

  // Directories only matter when they are added, deleted, unversioned or
  // ignored, or when their properties changed.
  const bool isDirectory = status->kind == svn_node_dir;
  const bool propertiesChanged =
      status->prop_status == svn_wc_status_modified ||
      status->prop_status == svn_wc_status_conflicted;
  if (isDirectory && status->node_status != svn_wc_status_added &&
      status->node_status != svn_wc_status_deleted &&
      status->node_status != svn_wc_status_replaced &&
      status->node_status != svn_wc_status_unversioned &&
      status->node_status != svn_wc_status_ignored &&
      status->node_status != svn_wc_status_missing && !propertiesChanged &&
      !status->conflicted) {
    return SVN_NO_ERROR;
  }

  QString relativePath =
      Private::relativeFilePath(payload->wcRoot, absolutePath);
  if (relativePath.isEmpty() || relativePath == QStringLiteral(".")) {
    // The root is a change of its own only when its properties changed.
    if (!propertiesChanged) {
      return SVN_NO_ERROR;
    }
    relativePath = QStringLiteral(".");
  }

  // The kind of unversioned nodes is unknown to Subversion, so it is read from
  // disk. Links and junctions are entries of their own and aren't entered.
  if (status->node_status == svn_wc_status_unversioned && !status->conflicted) {
    const QFileInfo info(absolutePath);
    const bool isWorkingCopy = QFileInfo::exists(
        absolutePath + QLatin1Char('/') +
        QString::fromUtf8(svn_wc_get_adm_dir(nullptr)));
    if (info.isDir() && !info.isSymLink() && !info.isJunction() &&
        !isWorkingCopy) {
      payload->unversionedDirectories.append(absolutePath);
      return SVN_NO_ERROR;
    }
  }

  // Changelists only hold files; staged folders are recorded separately.
  const bool staged =
      isDirectory
          ? payload->stagedDirectories.contains(absolutePath)
          : status->changelist && std::strcmp(status->changelist,
                                              Private::kStagingChangelist) == 0;
  if (isDirectory && (propertiesChanged || status->copied)) {
    payload->changedDirectories.insert(relativePath);
  }
  addStatusPath(payload->status, relativePath, status, staged);
  return SVN_NO_ERROR;
}

static void appendIgnorePatterns(apr_array_header_t *patterns,
                                 const svn_string_t *value, apr_pool_t *pool) {
  if (value) {
    svn_cstring_split_append(patterns, value->data, "\n\r", TRUE, pool);
  }
}

// Ignore patterns for the content of an unversioned directory below the
// versioned directory parentPath: the global-ignores of the configuration and
// the svn:global-ignores of parentPath and its parents. The svn:ignore of
// parentPath only applies to its direct children, which Subversion already
// filtered.
static svn_error_t *readIgnorePatterns(apr_array_header_t **patterns,
                                       const QString &parentPath,
                                       svn_client_ctx_t *ctx,
                                       apr_pool_t *pool) {
  SVN_ERR(svn_wc_get_default_ignores(patterns, ctx->config, pool));

  const QByteArray target = parentPath.toUtf8();
  const svn_opt_revision_t peg = Private::revisionUnspecified();
  const svn_opt_revision_t revision = Private::revisionWorking();
  apr_hash_t *properties = nullptr;
  apr_array_header_t *inherited = nullptr;
  SVN_ERR(svn_client_propget5(&properties, &inherited,
                              SVN_PROP_INHERITABLE_IGNORES, target.constData(),
                              &peg, &revision, nullptr, svn_depth_empty,
                              nullptr, ctx, pool, pool));

  for (apr_hash_index_t *index = apr_hash_first(pool, properties); index;
       index = apr_hash_next(index)) {
    appendIgnorePatterns(
        *patterns, static_cast<const svn_string_t *>(apr_hash_this_val(index)),
        pool);
  }

  for (int i = 0; inherited && i < inherited->nelts; ++i) {
    const auto *item =
        APR_ARRAY_IDX(inherited, i, const svn_prop_inherited_item_t *);
    appendIgnorePatterns(
        *patterns,
        static_cast<const svn_string_t *>(
            svn_hash_gets(item->prop_hash, SVN_PROP_INHERITABLE_IGNORES)),
        pool);
  }

  return SVN_NO_ERROR;
}

// Reports the files below an unversioned directory as new files, like Git
// reports the files of untracked directories. Nested working copies are
// reported as a whole, as Subversion does.
static void addUnversionedFiles(SvnStatus *status, const QString &wcRoot,
                                const QString &directory,
                                const apr_array_header_t *patterns,
                                const QStringList &externals,
                                apr_pool_t *pool) {
  const QFileInfoList entries = QDir(directory).entryInfoList(
      QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System,
      QDir::Name);
  for (const QFileInfo &entry : entries) {
    const QByteArray name = entry.fileName().toUtf8();
    if (svn_wc_is_adm_dir(name.constData(), pool) ||
        svn_wc_match_ignore_list(name.constData(), patterns, pool)) {
      continue;
    }

    // An external in a folder that isn't versioned is no change either.
    const QString path = entry.absoluteFilePath();
    if (externals.contains(path + QLatin1Char('/'))) {
      continue;
    }
    const bool isWorkingCopy = QFileInfo::exists(
        path + QLatin1Char('/') + QString::fromUtf8(svn_wc_get_adm_dir(pool)));
    if (entry.isDir() && !entry.isSymLink() && !entry.isJunction() &&
        !isWorkingCopy) {
      addUnversionedFiles(status, wcRoot, path, patterns, externals, pool);
    } else {
      status->newFiles.append(Private::relativeFilePath(wcRoot, path));
    }
  }
}

// Roots of the externals defined in the working copy at wcPath, each with a
// trailing slash. A definition that can't be read is skipped.
static QStringList externalRoots(const QByteArray &wcPath,
                                 svn_client_ctx_t *ctx, apr_pool_t *pool) {
  QStringList roots;
  apr_hash_t *properties = nullptr;
  const svn_opt_revision_t peg = Private::revisionUnspecified();
  const svn_opt_revision_t revision = Private::revisionWorking();
  svn_error_t *error = svn_client_propget5(
      &properties, nullptr, SVN_PROP_EXTERNALS, wcPath.constData(), &peg,
      &revision, nullptr, svn_depth_infinity, nullptr, ctx, pool, pool);
  if (error) {
    svn_error_clear(error);
    return roots;
  }

  for (apr_hash_index_t *index = apr_hash_first(pool, properties); index;
       index = apr_hash_next(index)) {
    const auto *definingDirectory =
        static_cast<const char *>(apr_hash_this_key(index));
    const auto *value =
        static_cast<const svn_string_t *>(apr_hash_this_val(index));
    apr_array_header_t *items = nullptr;
    error = svn_wc_parse_externals_description3(&items, definingDirectory,
                                                value->data, FALSE, pool);
    if (error) {
      svn_error_clear(error);
      continue;
    }
    for (int i = 0; i < items->nelts; ++i) {
      const auto *item = APR_ARRAY_IDX(items, i, const svn_wc_external_item2_t *);
      roots.append(QDir::cleanPath(QString::fromUtf8(definingDirectory) +
                                   QLatin1Char('/') +
                                   QString::fromUtf8(item->target_dir)) +
                   QLatin1Char('/'));
    }
  }
  return roots;
}

// Directories are only reported when nothing below them is, so they show up
// as folders of the files they hold. A folder with a change of its own stays.
static void removeParentDirectories(SvnStatus *status,
                                    const QSet<QString> &changedDirectories) {
  const std::initializer_list<QStringList *> lists = {
      &status->newFiles,           &status->modifiedFiles,
      &status->deletedFiles,       &status->typeChangedFiles,
      &status->newStagedFiles,     &status->modifiedStagedFiles,
      &status->deletedStagedFiles, &status->typeChangedStagedFiles,
      &status->renamedStagedFiles};

  QSet<QString> parents;
  for (const QStringList *list : lists) {
    for (const QString &path : *list) {
      for (int slash = path.lastIndexOf(QLatin1Char('/')); slash > 0;
           slash = path.lastIndexOf(QLatin1Char('/'), slash - 1)) {
        parents.insert(path.left(slash));
      }
    }
  }

  if (parents.isEmpty()) {
    return;
  }

  for (QStringList *list : lists) {
    list->erase(std::remove_if(list->begin(), list->end(),
                               [&](const QString &path) {
                                 return parents.contains(path) &&
                                        !changedDirectories.contains(path);
                               }),
                list->end());
  }
}

struct CommitLogBaton {
  QByteArray message;
};

static svn_error_t *commitLogCallback(const char **log_msg,
                                      const char **tmp_file,
                                      const apr_array_header_t *commit_items,
                                      void *baton, apr_pool_t *pool) {
  Q_UNUSED(commit_items);

  if (!log_msg || !baton) {
    return svn_error_create(SVN_ERR_INCORRECT_PARAMS, nullptr,
                            "Invalid commit log callback arguments");
  }

  const auto *payload = static_cast<const QByteArray *>(baton);
  *log_msg = apr_pstrdup(pool, payload->constData());
  if (tmp_file) {
    *tmp_file = nullptr;
  }
  return SVN_NO_ERROR;
}

static svn_error_t *infoCallback(void *baton, const char *,
                                 const svn_client_info2_t *info, apr_pool_t *) {
  if (!baton || !info) {
    return SVN_NO_ERROR;
  }

  auto *result = static_cast<Private::WorkingCopyInfo *>(baton);
  result->url = info->URL ? QString::fromUtf8(info->URL) : QString();
  result->reposRootUrl = info->repos_root_URL
                             ? QString::fromUtf8(info->repos_root_URL)
                             : QString();
  result->layoutRootUrl =
      Private::layoutRootUrlFor(result->url, result->reposRootUrl);
  result->reposUuid =
      info->repos_UUID ? QString::fromUtf8(info->repos_UUID) : QString();
  result->revision = info->rev;
  result->lastChangedRevision = info->last_changed_rev;
  result->lastChangedAuthor = info->last_changed_author
                                  ? QString::fromUtf8(info->last_changed_author)
                                  : QString();
  if (info->wc_info && info->wc_info->wcroot_abspath) {
    result->wcRoot = QString::fromUtf8(info->wc_info->wcroot_abspath);
  }

  return SVN_NO_ERROR;
}

struct ListDirectoriesBaton {
  QStringList *names = nullptr;
};

static svn_error_t *listDirectoriesCallback(void *baton, const char *path,
                                            const svn_dirent_t *dirent,
                                            const svn_lock_t *, const char *,
                                            const char *, const char *,
                                            apr_pool_t *) {
  auto *payload = static_cast<ListDirectoriesBaton *>(baton);
  if (!payload || !path || !dirent || dirent->kind != svn_node_dir || !*path) {
    return SVN_NO_ERROR;
  }

  payload->names->append(QString::fromUtf8(path));
  return SVN_NO_ERROR;
}

} // namespace

namespace Private {

void setLastError(SvnError error, int errorCode, const QString &errorMessage) {
  gLastError.error = error;
  gLastError.errorCode = errorCode;
  gLastError.errorMessage = errorMessage;
  ++gLastError.serial;
}

bool setUnsupported(const char *operation) {
  setLastError(SvnError::UnsupportedOperation, -1,
               QStringLiteral("%1 is not supported for Subversion "
                              "repositories.")
                   .arg(QString::fromUtf8(operation)));
  return false;
}

QString svnErrorMessage(const svn_error_t *error) {
  if (!error) {
    return QString();
  }

  // The outermost error explains what failed, the innermost one why.
  QStringList messages;
  for (const svn_error_t *current = error; current; current = current->child) {
    char buf[512];
    const char *message = svn_err_best_message(current, buf, sizeof(buf));
    const QString text = QString::fromUtf8(message ? message : "").trimmed();
    if (!text.isEmpty() && !messages.contains(text)) {
      messages.append(text);
    }
  }

  return messages.join(QStringLiteral("\n"));
}

bool reportError(SvnError kind, svn_error_t *error) {
  if (!error) {
    return true;
  }

  const SvnError reportedKind =
      svn_error_find_cause(error, SVN_ERR_CANCELLED) ? SvnError::Cancelled
                                                     : kind;
  setLastError(reportedKind, error->apr_err, svnErrorMessage(error));
  svn_error_clear(error);
  return false;
}

bool runWithClientContext(
    const std::function<bool(svn_client_ctx_t *, apr_pool_t *)> &fn,
    const QString &path) {
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

  const QString credentialsPath =
      path.isEmpty() ? gCurrentRepositoryPath : normalizedPath(path);

  SvnCredentialPayload payload;
  payload.url = credentialsPath;
  payload.destinationDir = credentialsPath;
  payload.uuid = credentialsPath;
  payload.requestId = qHash(credentialsPath);
  ctx->auth_baton = createAuthBaton(pool.get(), &payload);
  applyConfigDirectory(ctx->auth_baton, pool.get());

  const bool success = fn(ctx, pool.get());

  finishCredentialPrompt(payload, success);
  return success;
}

QuitScope::QuitScope(const bool *quitFlag) : _previous(gQuitFlag) {
  gQuitFlag = quitFlag;
}

QuitScope::~QuitScope() {
  gQuitFlag = _previous;
}

// libsvn's canonical form of a Windows path has an upper case drive letter;
// Debug builds assert on any other spelling ("c:/wc").
static QString withUpperCaseDrive(QString path) {
  if (path.size() >= 2 && path.at(1) == QLatin1Char(':') &&
      path.at(0).isLower()) {
    path[0] = path.at(0).toUpper();
  }
  return path;
}

QString normalizedPath(const QString &path) {
  if (path.isEmpty()) {
    return QString();
  }

  return withUpperCaseDrive(
      QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
}

QString absoluteFilePath(const QString &base, const QString &file) {
  if (QFileInfo(file).isAbsolute()) {
    return withUpperCaseDrive(
        QDir::cleanPath(QDir::fromNativeSeparators(file)));
  }

  return withUpperCaseDrive(
      QDir::cleanPath(QDir(base).absoluteFilePath(file)));
}

QString relativeFilePath(const QString &base, const QString &absolutePath) {
  return QDir::fromNativeSeparators(
      QDir(base).relativeFilePath(QDir::fromNativeSeparators(absolutePath)));
}

bool isMissingPathError(svn_error_t *error) {
  return error && (svn_error_find_cause(error, SVN_ERR_FS_NOT_FOUND) ||
                   svn_error_find_cause(error, SVN_ERR_FS_NOT_DIRECTORY) ||
                   svn_error_find_cause(error, SVN_ERR_RA_ILLEGAL_URL));
}

PathArray makePathArray(apr_pool_t *pool, const QStringList &paths) {
  PathArray result;
  result.storage.reserve(paths.size());
  result.array = apr_array_make(pool, static_cast<int>(paths.size()),
                                sizeof(const char *));
  for (const QString &path : paths) {
    result.storage.push_back(path.toUtf8());
    APR_ARRAY_PUSH(result.array, const char *) =
        result.storage.back().constData();
  }

  return result;
}

svn_opt_revision_t revisionUnspecified() {
  svn_opt_revision_t revision;
  revision.kind = svn_opt_revision_unspecified;
  return revision;
}

svn_opt_revision_t revisionHead() {
  svn_opt_revision_t revision;
  revision.kind = svn_opt_revision_head;
  return revision;
}

svn_opt_revision_t revisionBase() {
  svn_opt_revision_t revision;
  revision.kind = svn_opt_revision_base;
  return revision;
}

svn_opt_revision_t revisionWorking() {
  svn_opt_revision_t revision;
  revision.kind = svn_opt_revision_working;
  return revision;
}

svn_opt_revision_t revisionNumber(svn_revnum_t number) {
  svn_opt_revision_t revision;
  revision.kind = svn_opt_revision_number;
  revision.value.number = number;
  return revision;
}

bool parseRevision(const QString &commit, svn_revnum_t *revision) {
  QString text = commit.trimmed();
  if (text.startsWith('r', Qt::CaseInsensitive)) {
    text.remove(0, 1);
  }

  // Digits only ("+2" isn't a revision), and within svn_revnum_t, which is 32
  // bits on Windows.
  static const QRegularExpression digits(QStringLiteral("^[0-9]{1,19}$"));
  bool ok = digits.match(text).hasMatch();
  const qlonglong number = ok ? text.toLongLong(&ok) : -1;
  if (!ok || number < 0 ||
      number > (std::numeric_limits<svn_revnum_t>::max)()) {
    setLastError(
        SvnError::InvalidRevision, -1,
        QStringLiteral("'%1' is not a Subversion revision.").arg(commit));
    return false;
  }

  *revision = static_cast<svn_revnum_t>(number);
  return true;
}

svn_error_t *readInfo(WorkingCopyInfo *info, const QString &pathOrUrl,
                      const svn_opt_revision_t &revision, svn_client_ctx_t *ctx,
                      apr_pool_t *pool) {
  const QByteArray target = svn_path_is_url(pathOrUrl.toUtf8().constData())
                                ? pathOrUrl.toUtf8()
                                : normalizedPath(pathOrUrl).toUtf8();
  const svn_opt_revision_t peg = revisionUnspecified();
  return svn_client_info4(target.constData(), &peg, &revision, svn_depth_empty,
                          FALSE, TRUE, FALSE, nullptr, infoCallback, info, ctx,
                          pool);
}

bool readInfo(WorkingCopyInfo *info, const QString &path) {
  return runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        return reportError(
            SvnError::InfoFailed,
            readInfo(info, path, revisionUnspecified(), ctx, pool));
      },
      path);
}

QString trimmedUrl(QString url) {
  while (url.endsWith('/')) {
    url.chop(1);
  }
  return url;
}

QString canonicalUrl(const QString &urlOrPath) {
  const QString trimmed = urlOrPath.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }
  apr_pool_t *pool = svn_pool_create(nullptr);
  const QByteArray bytes = trimmed.toUtf8();
  QString result;
  if (svn_path_is_url(bytes.constData())) {
    // Lower case scheme and host, no trailing slash, file://localhost/ as
    // file:///, characters such as spaces escaped.
    QString url = QString::fromUtf8(svn_uri_canonicalize(
        svn_path_uri_autoescape(bytes.constData(), pool), pool));
    const QString localhost = QStringLiteral("file://localhost/");
    if (url.startsWith(localhost, Qt::CaseInsensitive)) {
      url = QStringLiteral("file:///") + url.mid(localhost.size());
    }
    // A drive letter in a file URL is upper case.
    if (url.startsWith(QLatin1String("file:///")) && url.size() > 9 &&
        url.at(9) == QLatin1Char(':')) {
      url[8] = url.at(8).toUpper();
    }
    result = url;
  } else {
    const char *fileUrl = nullptr;
    svn_error_t *error = svn_uri_get_file_url_from_dirent(
        &fileUrl,
        svn_dirent_canonicalize(
            normalizedPath(QDir::fromNativeSeparators(trimmed)).toUtf8()
                .constData(),
            pool),
        pool);
    if (error) {
      svn_error_clear(error);
    } else {
      result = QString::fromUtf8(fileUrl);
    }
  }
  svn_pool_destroy(pool);
  return result;
}

QString layoutRootUrlFor(const QString &url, const QString &reposRootUrl) {
  const QString root = trimmedUrl(reposRootUrl);
  const QString normalizedUrl = trimmedUrl(url);
  if (root.isEmpty() || !normalizedUrl.startsWith(root)) {
    return normalizedUrl;
  }

  const QStringList parts =
      normalizedUrl.mid(root.size()).split('/', Qt::SkipEmptyParts);
  for (qsizetype i = 0; i < parts.size(); ++i) {
    if (parts[i] == QStringLiteral("trunk") ||
        parts[i] == QStringLiteral("branches") ||
        parts[i] == QStringLiteral("tags")) {
      QString layoutRoot = root;
      for (qsizetype j = 0; j < i; ++j) {
        layoutRoot += QLatin1Char('/') + parts[j];
      }
      return layoutRoot;
    }
  }

  // No trunk/branches/tags in the path: the url itself holds the layout.
  return normalizedUrl;
}

// A path in libsvn's canonical URL form: percent-encoded except for the
// characters Subversion leaves as they are (svn_uri__char_validity). Any other
// spelling makes libsvn assert in Debug builds (B2h).
static QString encodedUrlPath(const QString &path) {
  return QString::fromLatin1(
      QUrl::toPercentEncoding(path, QByteArrayLiteral("!$&'()*+,;=:@/")));
}

QString urlOfRelativePath(const QString &rootUrl, const QString &relativePath) {
  return trimmedUrl(rootUrl) + QStringLiteral("/") +
         encodedUrlPath(relativePath);
}

QString branchUrl(const QString &reposRootUrl, const QString &name,
                  BranchType type) {
  const QString root = trimmedUrl(reposRootUrl);
  if (type == BranchType::Tag) {
    return root + QStringLiteral("/tags/") + encodedUrlPath(name);
  }

  if (name.isEmpty() || name == QStringLiteral("trunk")) {
    return root + QStringLiteral("/trunk");
  }

  return root + QStringLiteral("/branches/") + encodedUrlPath(name);
}

QString branchNameFromUrl(const QString &url, const QString &reposRootUrl,
                          BranchType type) {
  const QString normalizedUrl = trimmedUrl(url);
  const QString normalizedRoot = trimmedUrl(reposRootUrl);
  if (normalizedUrl.isEmpty()) {
    return {};
  }

  QString relativePath = normalizedUrl;
  if (!normalizedRoot.isEmpty() && normalizedUrl.startsWith(normalizedRoot)) {
    relativePath = normalizedUrl.mid(normalizedRoot.size());
  }

  while (relativePath.startsWith('/')) {
    relativePath.remove(0, 1);
  }

  const QStringList parts = relativePath.split('/', Qt::SkipEmptyParts);
  if (parts.isEmpty()) {
    // The layout root holds every branch; it isn't one.
    return {};
  }

  if (parts[0] == QStringLiteral("trunk")) {
    return type == BranchType::Tag ? QString() : QStringLiteral("trunk");
  }

  // Names are percent-encoded in URLs.
  const QString name = parts.size() >= 2
                           ? QUrl::fromPercentEncoding(parts[1].toUtf8())
                           : QString();
  if (parts[0] == QStringLiteral("branches") && parts.size() >= 2) {
    return type == BranchType::Tag ? QString() : name;
  }

  if (parts[0] == QStringLiteral("tags") && parts.size() >= 2) {
    return type == BranchType::Tag ? name : QString();
  }

  // Not in the trunk/branches/tags layout, e.g. a project directory that holds
  // the layout. It is not a branch, so the name stays empty.
  return {};
}

QString branchSubpath(const WorkingCopyInfo &info) {
  const QString root = trimmedUrl(info.layoutRootUrl);
  const QString url = trimmedUrl(info.url);
  if (!url.startsWith(root + QLatin1Char('/'))) {
    return {};
  }
  const QStringList parts = url.mid(root.size()).split('/', Qt::SkipEmptyParts);
  qsizetype first = -1;
  if (parts.value(0) == QLatin1String("trunk")) {
    first = 1;
  } else if (parts.value(0) == QLatin1String("branches") ||
             parts.value(0) == QLatin1String("tags")) {
    first = 2;
  }
  if (first < 0 || parts.size() <= first) {
    return {};
  }
  return QLatin1Char('/') + parts.mid(first).join(QLatin1Char('/'));
}

svn_error_t *readMaxRevision(svn_revnum_t *revision, const QString &wcPath,
                             bool committed, svn_client_ctx_t *ctx,
                             apr_pool_t *pool) {
  svn_wc_revision_status_t *status = nullptr;
  const QByteArray pathBytes = normalizedPath(wcPath).toUtf8();
  SVN_ERR(svn_wc_revision_status2(&status, ctx->wc_ctx, pathBytes.constData(),
                                  nullptr, committed, ctx->cancel_func,
                                  ctx->cancel_baton, pool, pool));
  *revision = status->max_rev;
  return SVN_NO_ERROR;
}

svn_error_t *listDirectories(QStringList *names, const QString &url,
                             svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const svn_opt_revision_t peg = revisionHead();
  const svn_opt_revision_t revision = revisionHead();

  ListDirectoriesBaton baton;
  baton.names = names;

  const QByteArray urlBytes = url.toUtf8();
  SVN_ERR(svn_client_list4(urlBytes.constData(), &peg, &revision, nullptr,
                           svn_depth_immediates, SVN_DIRENT_KIND, FALSE, FALSE,
                           listDirectoriesCallback, &baton, ctx, pool));

  names->removeDuplicates();
  names->sort(Qt::CaseInsensitive);
  return SVN_NO_ERROR;
}

QString makeLogMessage(const QString &summary, const QString &message,
                       bool cleanup, bool signoff, const QString &author,
                       const QString &email) {
  // The repository refuses a log message with a CR in it.
  const auto withLf = [](QString text) {
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
  };

  QString result = withLf(summary);
  if (!message.isEmpty()) {
    result += QStringLiteral("\n\n") + withLf(message);
  }

  if (cleanup) {
    static const QRegularExpression trailingWhitespace(QStringLiteral("\\s+$"));

    // Comment lines are removed from the message. The first line is the
    // summary, also when it starts with # (an issue number).
    const QStringList allLines = result.split('\n');
    QStringList lines;
    for (qsizetype i = 0; i < allLines.size(); ++i) {
      QString line = allLines[i];
      if (i == 0 || !line.startsWith('#')) {
        lines.append(line.remove(trailingWhitespace));
      }
    }
    result = lines.join('\n').trimmed();
  }

  if (signoff && !author.isEmpty()) {
    const QString signoffLine =
        email.isEmpty()
            ? QStringLiteral("Signed-off-by: %1").arg(author)
            : QStringLiteral("Signed-off-by: %1 <%2>").arg(author, email);
    if (!result.contains(signoffLine)) {
      result += QStringLiteral("\n\n") + signoffLine;
    }
  }

  return result;
}

LogMessageScope::LogMessageScope(svn_client_ctx_t *ctx, const QString &message)
    : _ctx(ctx), _message(message.toUtf8()) {
  _ctx->log_msg_func3 = commitLogCallback;
  _ctx->log_msg_baton3 = &_message;
}

LogMessageScope::~LogMessageScope() {
  _ctx->log_msg_func3 = nullptr;
  _ctx->log_msg_baton3 = nullptr;
}

static svn_error_t *deletedDirectoryCallback(void *baton, const char *,
                                             const svn_client_status_t *status,
                                             apr_pool_t *) {
  auto *directories = static_cast<QStringList *>(baton);
  if (directories && status && status->kind == svn_node_dir &&
      status->node_status == svn_wc_status_deleted) {
    const QString path = QString::fromUtf8(status->local_abspath);
    // The status walk visits a directory before its children, and removing
    // the changelists of a directory reaches everything below it.
    if (directories->isEmpty() ||
        !path.startsWith(directories->last() + QLatin1Char('/'))) {
      directories->append(path);
    }
  }
  return SVN_NO_ERROR;
}

// Committing the deletion of a directory removes the nodes below it from
// wc.db, but leaves their ACTUAL_NODE rows that only hold a changelist (a
// staged file). Such an actual-only row without a conflict makes every later
// read of the path, by a switch or an update, fail with "Corrupt data". The
// changelists below the directories that targets delete, down to depth, are
// removed first; the commit would clear them anyway.
static svn_error_t *removeChangelistsBelowDeletions(const QStringList &targets,
                                                    svn_depth_t depth,
                                                    svn_client_ctx_t *ctx,
                                                    apr_pool_t *pool) {
  QStringList directories;
  const svn_opt_revision_t revision = revisionWorking();
  for (const QString &target : targets) {
    const QByteArray pathBytes = normalizedPath(target).toUtf8();
    svn_revnum_t youngest = SVN_INVALID_REVNUM;
    SVN_ERR(svn_client_status6(&youngest, ctx, pathBytes.constData(), &revision,
                               depth, FALSE, FALSE, TRUE, FALSE, TRUE, FALSE,
                               nullptr, deletedDirectoryCallback, &directories,
                               pool));
  }
  if (directories.isEmpty()) {
    return SVN_NO_ERROR;
  }

  const PathArray paths = makePathArray(pool, directories);
  return svn_client_remove_from_changelists(paths.array, svn_depth_infinity,
                                            nullptr, ctx, pool);
}

svn_error_t *commitWorkingCopy(const QString &wcPath, const QString &message,
                               svn_client_ctx_t *ctx, apr_pool_t *pool) {
  SVN_ERR(removeChangelistsBelowDeletions({wcPath}, svn_depth_infinity, ctx,
                                          pool));
  const PathArray targets = makePathArray(pool, {normalizedPath(wcPath)});
  const LogMessageScope logMessage(ctx, message);
  return svn_client_commit6(targets.array, svn_depth_infinity, FALSE, FALSE,
                            TRUE, FALSE, FALSE, nullptr, nullptr, nullptr,
                            nullptr, ctx, pool);
}

svn_error_t *commitPaths(const QStringList &paths, const QString &message,
                         svn_client_ctx_t *ctx, apr_pool_t *pool) {
  SVN_ERR(removeChangelistsBelowDeletions(paths, svn_depth_empty, ctx, pool));
  const PathArray targets = makePathArray(pool, paths);
  const LogMessageScope logMessage(ctx, message);
  return svn_client_commit6(targets.array, svn_depth_empty, FALSE, FALSE, TRUE,
                            FALSE, FALSE, nullptr, nullptr, nullptr, nullptr,
                            ctx, pool);
}

// Root of the working copy that holds path: the nearest folder with an
// administrative directory, or path itself.
static QString workingCopyRootOf(const QString &path) {
  const QString admDir = QString::fromUtf8(svn_wc_get_adm_dir(nullptr));
  QDir dir(path);
  do {
    if (QFileInfo(dir.filePath(admDir)).isDir()) {
      return normalizedPath(dir.absolutePath());
    }
  } while (dir.cdUp());
  return path;
}

static QString stagedDirectoriesFile(const QString &wcRoot) {
  return wcRoot + QLatin1Char('/') +
         QString::fromUtf8(svn_wc_get_adm_dir(nullptr)) +
         QStringLiteral("/sourcerer-staged-directories");
}

QSet<QString> readStagedDirectories(const QString &path) {
  const QString root = workingCopyRootOf(normalizedPath(path));
  QFile file(stagedDirectoriesFile(root));
  QSet<QString> directories;
  if (!file.open(QIODevice::ReadOnly)) {
    return directories;
  }
  for (const QString &line :
       QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts)) {
    directories.insert(line == QLatin1String(".")
                           ? root
                           : absoluteFilePath(root, line.trimmed()));
  }
  return directories;
}

void writeStagedDirectories(const QString &path,
                            const QSet<QString> &directories) {
  const QString root = workingCopyRootOf(normalizedPath(path));
  const QString fileName = stagedDirectoriesFile(root);
  if (directories.isEmpty()) {
    QFile::remove(fileName);
    return;
  }

  QStringList lines;
  for (const QString &directory : directories) {
    const QString relativePath = relativeFilePath(root, directory);
    lines.append(relativePath.isEmpty() ? QStringLiteral(".") : relativePath);
  }
  lines.sort();
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) ||
      file.write((lines.join('\n') + '\n').toUtf8()) < 0) {
    LOG_WARN("Failed to write {}", fileName.toStdString());
  }
}

static QString operationStateFile(const QString &wcRoot) {
  return wcRoot + QLatin1Char('/') +
         QString::fromUtf8(svn_wc_get_adm_dir(nullptr)) +
         QStringLiteral("/sourcerer/operation.json");
}

bool readOperationState(const QString &path, OperationState *state) {
  const QString root = workingCopyRootOf(normalizedPath(path));
  QFile file(operationStateFile(root));
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }
  const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
  if (object.value(QStringLiteral("version")).toInt() != 1) {
    return false;
  }

  const auto paths = [&](const char *key) {
    QStringList result;
    for (const QJsonValue &value :
         object.value(QLatin1String(key)).toArray()) {
      result.append(absoluteFilePath(root, value.toString()));
    }
    return result;
  };
  state->kind = object.value(QStringLiteral("kind")).toString();
  state->source = object.value(QStringLiteral("source")).toString();
  state->sourceUrl = object.value(QStringLiteral("sourceUrl")).toString();
  const QJsonArray revisions =
      object.value(QStringLiteral("revisions")).toArray();
  state->start = revisions.at(0).toInteger(-1);
  state->end = revisions.at(1).toInteger(-1);
  state->destination = object.value(QStringLiteral("destination")).toString();
  state->message = object.value(QStringLiteral("message")).toString();
  state->paths = paths("paths");
  state->added = paths("added");
  state->conflicted = paths("conflicted");
  return !state->kind.isEmpty();
}

bool writeOperationState(const QString &path, const OperationState &state) {
  const QString root = workingCopyRootOf(normalizedPath(path));
  const auto relative = [&](const QStringList &paths) {
    QJsonArray result;
    for (const QString &absolutePath : paths) {
      const QString relativePath = relativeFilePath(root, absolutePath);
      result.append(relativePath.isEmpty() ? QStringLiteral(".")
                                           : relativePath);
    }
    return result;
  };
  QJsonObject object;
  object.insert(QStringLiteral("version"), 1);
  object.insert(QStringLiteral("kind"), state.kind);
  object.insert(QStringLiteral("source"), state.source);
  object.insert(QStringLiteral("sourceUrl"), state.sourceUrl);
  object.insert(QStringLiteral("revisions"), QJsonArray{state.start, state.end});
  object.insert(QStringLiteral("destination"), state.destination);
  object.insert(QStringLiteral("message"), state.message);
  object.insert(QStringLiteral("paths"), relative(state.paths));
  object.insert(QStringLiteral("added"), relative(state.added));
  object.insert(QStringLiteral("conflicted"), relative(state.conflicted));

  const QString fileName = operationStateFile(root);
  QDir().mkpath(QFileInfo(fileName).path());
  QSaveFile file(fileName);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(QJsonDocument(object).toJson()) < 0 || !file.commit()) {
    setLastError(SvnError::Unknown, -1,
                 QStringLiteral("Failed to write %1.").arg(fileName));
    return false;
  }
  return true;
}

void removeOperationState(const QString &path) {
  QFile::remove(operationStateFile(workingCopyRootOf(normalizedPath(path))));
}

ConflictWatch::ConflictWatch(svn_client_ctx_t *ctx)
    : _ctx(ctx), _previousFunc(ctx->notify_func2),
      _previousBaton(ctx->notify_baton2) {
  _ctx->notify_func2 = &ConflictWatch::notify;
  _ctx->notify_baton2 = this;
}

ConflictWatch::~ConflictWatch() {
  _ctx->notify_func2 = _previousFunc;
  _ctx->notify_baton2 = _previousBaton;
}

void ConflictWatch::notify(void *baton, const svn_wc_notify_t *notify,
                           apr_pool_t *pool) {
  auto *watch = static_cast<ConflictWatch *>(baton);
  if (watch->_previousFunc) {
    watch->_previousFunc(watch->_previousBaton, notify, pool);
  }
  if (!notify || !notify->path) {
    return;
  }
  if (notify->action == svn_wc_notify_tree_conflict ||
      notify->content_state == svn_wc_notify_state_conflicted ||
      notify->prop_state == svn_wc_notify_state_conflicted) {
    const QString path = QDir::fromNativeSeparators(QString::fromUtf8(notify->path));
    if (!watch->_conflicted.contains(path)) {
      watch->_conflicted.append(path);
    }
  }
}

bool ConflictWatch::reportConflicts(SvnError kind, const QString &wcPath,
                                    const QString &operation) const {
  if (_conflicted.isEmpty()) {
    return true;
  }
  QStringList paths;
  for (const QString &path : _conflicted) {
    const QString relativePath = relativeFilePath(wcPath, path);
    paths.append(relativePath.isEmpty() ? QStringLiteral(".") : relativePath);
  }
  paths.sort();
  setLastError(kind, -1,
               QStringLiteral("%1 left conflicts in: %2")
                   .arg(operation, paths.join(QStringLiteral(", "))));
  return false;
}

SvnStatus readStatus(const QString &wcPath, const bool *quitFlag) {
  SvnStatus result;
  result.path = normalizedPath(wcPath);

  const QuitScope quitScope(quitFlag);
  QSet<QString> changedDirectories;

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        StatusBaton baton;
        baton.status = &result;
        baton.wcRoot = result.path;
        baton.stagedDirectories = readStagedDirectories(result.path);

        const QByteArray absPath = result.path.toUtf8();
        baton.externals = externalRoots(absPath, ctx, pool);
        const svn_opt_revision_t revision = revisionWorking();
        svn_revnum_t youngest = SVN_INVALID_REVNUM;
        // Ignored files are reported too (no_ignore).
        if (!reportError(SvnError::StatusFailed,
                         svn_client_status6(
                             &youngest, ctx, absPath.constData(), &revision,
                             svn_depth_infinity, FALSE, FALSE, TRUE, TRUE,
                             TRUE, FALSE, nullptr, statusCallback, &baton,
                             pool))) {
          return false;
        }
        changedDirectories = baton.changedDirectories;

        for (const QString &directory : baton.unversionedDirectories) {
          apr_array_header_t *patterns = nullptr;
          if (!reportError(SvnError::StatusFailed,
                           readIgnorePatterns(&patterns,
                                              QFileInfo(directory).path(), ctx,
                                              pool))) {
            return false;
          }
          addUnversionedFiles(&result, result.path, directory, patterns,
                              baton.externals, pool);
        }
        return true;
      },
      wcPath);

  if (!success) {
    return SvnStatus();
  }

  removeParentDirectories(&result, changedDirectories);

  for (QStringList *list :
       {&result.newFiles, &result.modifiedFiles, &result.deletedFiles,
        &result.typeChangedFiles, &result.newStagedFiles,
        &result.modifiedStagedFiles, &result.deletedStagedFiles,
        &result.typeChangedStagedFiles, &result.renamedStagedFiles,
        &result.ignoredFiles, &result.conflictedFiles}) {
    list->removeDuplicates();
    list->sort();
  }

  return result;
}

QString readTextFile(const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    return QString();
  }

  return QString::fromUtf8(file.readAll());
}

svn_error_t *readFileAtRevision(QByteArray *content, const QString &pathOrUrl,
                                const svn_opt_revision_t &peg,
                                const svn_opt_revision_t &revision,
                                svn_client_ctx_t *ctx, apr_pool_t *pool,
                                bool expandKeywords) {
  svn_stringbuf_t *buffer = svn_stringbuf_create_empty(pool);
  svn_stream_t *stream = svn_stream_from_stringbuf(buffer, pool);

  const QByteArray target = svn_path_is_url(pathOrUrl.toUtf8().constData())
                                ? pathOrUrl.toUtf8()
                                : normalizedPath(pathOrUrl).toUtf8();
  SVN_ERR(svn_client_cat3(nullptr, stream, target.constData(), &peg, &revision,
                          expandKeywords, ctx, pool, pool));
  SVN_ERR(svn_stream_close(stream));

  *content = QByteArray(buffer->data, static_cast<int>(buffer->len));
  return SVN_NO_ERROR;
}

QVector<FilesDiffDelta> parseUnifiedDiff(const QString &diff) {
  static const QRegularExpression hunkHeader(
      QStringLiteral("^@@ -(\\d+)(?:,(\\d+))? \\+(\\d+)(?:,(\\d+))? @@"));

  // "--- path\t(revision 12)" and "+++ path\t(working copy)".
  const auto headerPath = [](const QString &line) {
    QString path = line.mid(4);
    const int tab = path.indexOf('\t');
    if (tab >= 0) {
      path = path.left(tab);
    }
    return path.trimmed();
  };

  QVector<FilesDiffDelta> deltas;
  FilesDiffDelta *delta = nullptr;
  FilesDiffHunk *hunk = nullptr;
  int oldLinesLeft = 0;
  int newLinesLeft = 0;

  const QStringList lines = QString(diff).remove('\r').split('\n');
  for (const QString &line : lines) {
    if (hunk && (oldLinesLeft > 0 || newLinesLeft > 0)) {
      const QChar origin = line.isEmpty() ? QChar(' ') : line.at(0);
      if (origin == ' ' || origin == '-' || origin == '+') {
        FilesDiffLine diffLine;
        diffLine.origin = origin;
        diffLine.content = line.mid(1);
        hunk->lines.append(diffLine);
        if (origin != '+') {
          --oldLinesLeft;
        }
        if (origin != '-') {
          --newLinesLeft;
        }
        continue;
      }
    }

    // "\ No newline at end of file" belongs to the line before it.
    if (hunk && line.startsWith(QStringLiteral("\\ "))) {
      hunk->lines.append(FilesDiffLine{QChar('\\'), line.mid(1)});
      continue;
    }

    if (line.startsWith(QStringLiteral("Index: "))) {
      deltas.append(FilesDiffDelta());
      delta = &deltas.last();
      delta->oldFilePath = line.mid(7).trimmed();
      delta->newFilePath = delta->oldFilePath;
      hunk = nullptr;
      continue;
    }

    if (line.startsWith(QStringLiteral("--- ")) && delta && !hunk) {
      delta->oldFilePath = headerPath(line);
      continue;
    }

    if (line.startsWith(QStringLiteral("+++ ")) && delta && !hunk) {
      delta->newFilePath = headerPath(line);
      continue;
    }

    const QRegularExpressionMatch match = hunkHeader.match(line);
    if (match.hasMatch() && delta) {
      delta->hunks.append(FilesDiffHunk());
      hunk = &delta->hunks.last();
      hunk->oldStart = match.captured(1).toInt();
      hunk->oldLines =
          match.captured(2).isEmpty() ? 1 : match.captured(2).toInt();
      hunk->newStart = match.captured(3).toInt();
      hunk->newLines =
          match.captured(4).isEmpty() ? 1 : match.captured(4).toInt();
      oldLinesLeft = hunk->oldLines;
      newLinesLeft = hunk->newLines;
      continue;
    }

    // "\ No newline at end of file" belongs to the previous line.
  }

  return deltas;
}

} // namespace Private

using namespace Private;

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

QString errorCodeText(int errorCode) {
  if (errorCode <= 0) {
    return {};
  }
  char buffer[512] = {};
  const char *text = svn_strerror(errorCode, buffer, sizeof(buffer));
  return text && *text ? QString::fromUtf8(text) : QString();
}

void initForCurrentThread() {
  ensureInitialized();
}

void shutdownForCurrentThread() {
  finishPendingCredentialPrompts();
}

bool init() {
  return ensureInitialized();
}

void finishPendingCredentialPrompts() {
  std::set<size_t> ids;
  ids.swap(gPromptedCredentialIds);

  for (const size_t id : ids) {
    sendCredentialsSignal(
        EventManager::EventType::GitRepoManagerGetCredentialsFinished, id);
  }
}

void clearCachedCredentials(const QString &repoPath) {
  // Credentials are read from GitCredentialStorage every time the server asks
  // for them, there is no cache to clear.
  LOG_INFO("No cached credentials to clear for repository: {}", repoPath);
}

QString getBasicAuthHeader(const QString &, const QString &) {
  // Only used for Git LFS transfers.
  return QString();
}

GitRepo::GitSshAuthInfo getSshAuthInfo(const QString &, const QString &,
                                       const QString &) {
  // svn+ssh uses the ssh client configured in the Subversion config.
  return GitRepo::GitSshAuthInfo();
}

QString openRepository(const QString &path) {
  LOG_PERF(0, "Enter");

  const QString repoPath = normalizedPath(path);
  LOG_DEBUG("Opening SVN repository at path: '{}'", repoPath.toStdString());

  if (repoPath.isEmpty() || !QFileInfo(repoPath).exists()) {
    setLastError(SvnError::ErrorWhileOpening, SVN_ERR_BAD_FILENAME,
                 QStringLiteral("Repository path does not exist."));
    LOG_PERF(0, "Exit");
    return QString();
  }

  // A folder inside a working copy opens the working copy, so paths are
  // relative to its root and nothing outside the folder is missed (B25).
  QString wcRoot;
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        int wcFormat = 0;
        const QByteArray pathBytes = repoPath.toUtf8();
        if (!reportError(SvnError::ErrorWhileOpening,
                         svn_wc_check_wc2(&wcFormat, ctx->wc_ctx,
                                          pathBytes.constData(), pool))) {
          return false;
        }
        if (wcFormat <= 0) {
          setLastError(SvnError::ErrorWhileOpening, SVN_ERR_WC_NOT_WORKING_COPY,
                       QStringLiteral("'%1' is not a Subversion working copy.")
                           .arg(repoPath));
          return false;
        }
        const char *root = nullptr;
        if (!reportError(SvnError::ErrorWhileOpening,
                         svn_client_get_wc_root(&root, pathBytes.constData(),
                                                ctx, pool, pool))) {
          return false;
        }
        wcRoot = normalizedPath(QString::fromUtf8(root));
        return true;
      },
      repoPath);

  if (!success) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  setCurrentRepositoryPath(wcRoot);

  LOG_PERF(0, "Exit");
  return wcRoot;
}

void closeRepository(const QString &) {
  finishPendingCredentialPrompts();
  clearCurrentRepositoryPath();
}

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  LOG_PERF(0, "Enter");

  const QString checkoutUrl = canonicalUrl(url);
  if (checkoutUrl.isEmpty()) {
    setLastError(SvnError::CheckoutFailed, -1,
                 QStringLiteral("'%1' is not a repository URL.").arg(url));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext([&](svn_client_ctx_t *ctx,
                                                apr_pool_t *pool) {
    svn_revnum_t resultRev = SVN_INVALID_REVNUM;
    svn_opt_revision_t pegRev = revisionUnspecified();
    svn_opt_revision_t rev = revisionHead();

    SvnCredentialPayload credentialPayload;
    credentialPayload.url = url;
    credentialPayload.destinationDir = destinationDir;
    credentialPayload.uuid = uuid;
    credentialPayload.requestId = qHash(uuid);
    ctx->auth_baton = createAuthBaton(pool, &credentialPayload);
    applyConfigDirectory(ctx->auth_baton, pool);

    CheckoutProgressBaton progressBaton;
    progressBaton.progress.uuid = uuid;
    progressBaton.progress.cloning = true;
    progressBaton.progress.stage = GitRepo::CloneProgressStageType::Checkout;

    const QByteArray urlBytes = checkoutUrl.toUtf8();
    const QByteArray destBytes = normalizedPath(destinationDir).toUtf8();

    // The entries are not counted up front, listing a large repository takes
    // as long as checking it out. Without a total the progress is busy.
    progressBaton.progress.totalSteps = 0;
    progressBaton.progress.completedSteps = 0;
    progressBaton.progress.lastStepPercentage = -1;
    progressBaton.progress.stage = GitRepo::CloneProgressStageType::Checkout;
    emitCheckoutProgress(progressBaton,
                         GitRepo::CloneProgressStageType::Checkout);
    ctx->notify_func2 = checkoutNotifyCallback;
    ctx->notify_baton2 = &progressBaton;

    svn_error_t *error = svn_client_checkout3(
        &resultRev, urlBytes.constData(), destBytes.constData(), &pegRev, &rev,
        svn_depth_infinity, FALSE, FALSE, ctx, pool);
    finishCloneProgress(uuid, error != nullptr, progressBaton);
    finishCredentialPrompt(credentialPayload, error == nullptr);
    if (error) {
      return reportError(SvnError::CheckoutFailed, error);
    }

    setCurrentRepositoryPath(destinationDir);
    return true;
  });

  LOG_PERF(0, "Exit");
  return success;
}

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email) {
  LOG_PERF(0, "Enter");

  // Subversion working copies need a repository, so a local one is created
  // next to the working copy: <workingDir>/../<repoName>-svnrepo.
  Q_UNUSED(defaultBranch);

  const QString workingCopy = normalizedPath(workingDir);
  if (workingCopy.isEmpty() || repoName.trimmed().isEmpty()) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("Repository name and directory are required."));
    LOG_PERF(0, "Exit");
    return false;
  }
  // The repository is made next to the working copy, never elsewhere.
  if (repoName.contains(QLatin1Char('/')) ||
      repoName.contains(QLatin1Char('\\')) ||
      repoName.trimmed() == QLatin1String(".") ||
      repoName.trimmed() == QLatin1String("..")) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("'%1' isn't a repository name.").arg(repoName));
    LOG_PERF(0, "Exit");
    return false;
  }
  for (QDir dir(workingCopy);;) {
    if (QFileInfo::exists(dir.filePath(QStringLiteral(".git")))) {
      setLastError(SvnError::InvalidArgument, -1,
                   QStringLiteral("'%1' is inside the Git repository '%2'.")
                       .arg(workingCopy, dir.absolutePath()));
      LOG_PERF(0, "Exit");
      return false;
    }
    if (!dir.cdUp()) {
      break;
    }
  }

  const QString repositoryDir = normalizedPath(
      QFileInfo(workingCopy).absolutePath() + QStringLiteral("/") +
      repoName.trimmed() + QStringLiteral("-svnrepo"));
  if (QFileInfo::exists(repositoryDir)) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("'%1' already exists.").arg(repositoryDir));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        const QByteArray repositoryBytes = repositoryDir.toUtf8();
        svn_repos_t *repos = nullptr;
        if (!reportError(SvnError::InitializationFailed,
                         svn_repos_create(&repos, repositoryBytes.constData(),
                                          nullptr, nullptr, nullptr, nullptr,
                                          pool))) {
          return false;
        }

        const char *rootUrl = nullptr;
        if (!reportError(SvnError::InitializationFailed,
                         svn_uri_get_file_url_from_dirent(
                             &rootUrl, repositoryBytes.constData(), pool))) {
          return false;
        }

        const QString root = QString::fromUtf8(rootUrl);
        const PathArray directories =
            makePathArray(pool, {root + QStringLiteral("/trunk"),
                                 root + QStringLiteral("/branches"),
                                 root + QStringLiteral("/tags")});
        // The layout revision is the first one made in the user's name.
        if (!author.trimmed().isEmpty()) {
          svn_auth_set_parameter(
              ctx->auth_baton, SVN_AUTH_PARAM_DEFAULT_USERNAME,
              apr_pstrdup(pool, author.trimmed().toUtf8().constData()));
        }
        {
          const LogMessageScope logMessage(
              ctx, makeLogMessage(QStringLiteral("Initial layout"), QString(),
                                  true, false, author, email));
          if (!reportError(SvnError::CommitFailed,
                           svn_client_mkdir4(directories.array, FALSE, nullptr,
                                             nullptr, nullptr, ctx, pool))) {
            return false;
          }
        }

        const QByteArray trunkUrl = (root + QStringLiteral("/trunk")).toUtf8();
        const QByteArray workingCopyBytes = workingCopy.toUtf8();
        const svn_opt_revision_t peg = revisionUnspecified();
        const svn_opt_revision_t head = revisionHead();
        svn_revnum_t resultRev = SVN_INVALID_REVNUM;
        return reportError(
            SvnError::CheckoutFailed,
            svn_client_checkout3(&resultRev, trunkUrl.constData(),
                                 workingCopyBytes.constData(), &peg, &head,
                                 svn_depth_infinity, FALSE, TRUE, ctx, pool));
      },
      workingCopy);

  // A failed init leaves nothing behind, so it can be tried again.
  if (!success && QFileInfo::exists(repositoryDir)) {
    QDir(repositoryDir).removeRecursively();
  }

  LOG_PERF(0, "Exit");
  return success;
}

#ifdef SOURCERER_SVNREPO_TESTING
namespace Testing {

namespace {

std::mutex gMalfunctionMutex;
QStringList gMalfunctions;

svn_error_t *recordMalfunction(svn_boolean_t canReturn, const char *file,
                               int line, const char *expr) {
  {
    std::lock_guard<std::mutex> lock(gMalfunctionMutex);
    gMalfunctions.append(
        QStringLiteral("%1:%2: %3")
            .arg(QString::fromUtf8(file ? file : "?"))
            .arg(line)
            .arg(QString::fromUtf8(expr ? expr : "internal malfunction")));
  }
  // Returns SVN_ERR_ASSERTION_FAIL when the caller can handle an error,
  // aborts otherwise.
  return svn_error_raise_on_malfunction(canReturn, file, line, expr);
}

} // namespace

void setConfigDirectory(const QString &path) {
  std::lock_guard<std::mutex> lock(gConfigDirectoryMutex);
  gConfigDirectory = path.isEmpty() ? QString() : normalizedPath(path);
}

QString configDirectory() {
  return currentConfigDirectory();
}

bool sslServerTrustCheck(quint32 failures, const ServerCertificateInfo &info,
                         const QString &realm, quint32 *acceptedFailures) {
  if (acceptedFailures) {
    *acceptedFailures = 0;
  }
  if (!ensureInitialized()) {
    return false;
  }

  auto pool = makePool();
  const QByteArray hostname = info.hostname.toUtf8();
  const QByteArray fingerprint = info.fingerprint.toUtf8();
  const QByteArray validFrom = info.validFrom.toUtf8();
  const QByteArray validUntil = info.validUntil.toUtf8();
  const QByteArray issuer = info.issuerDistinguishedName.toUtf8();
  const QByteArray certificate = info.asciiCertificate.toUtf8();
  const QByteArray realmBytes = realm.toUtf8();

  svn_auth_ssl_server_cert_info_t certInfo = {};
  certInfo.hostname = hostname.constData();
  certInfo.fingerprint = fingerprint.constData();
  certInfo.valid_from = validFrom.constData();
  certInfo.valid_until = validUntil.constData();
  certInfo.issuer_dname = issuer.constData();
  certInfo.ascii_cert = certificate.constData();

  SvnCredentialPayload payload;
  svn_auth_cred_ssl_server_trust_t *creds = nullptr;
  svn_error_t *error = sslServerTrustPromptCallback(
      &creds, &payload, realmBytes.constData(), failures, &certInfo, FALSE,
      pool.get());
  if (error) {
    return reportError(SvnError::Unknown, error);
  }
  if (!creds) {
    return false;
  }
  if (acceptedFailures) {
    *acceptedFailures = creds->accepted_failures;
  }
  return true;
}

void installMalfunctionHandler() {
  ensureInitialized();
  svn_error_set_malfunction_handler(recordMalfunction);
}

QStringList takeMalfunctions() {
  std::lock_guard<std::mutex> lock(gMalfunctionMutex);
  QStringList malfunctions;
  malfunctions.swap(gMalfunctions);
  return malfunctions;
}

void resetLastError() {
  setLastError(SvnError::Unknown, -1, QString());
}

bool leaveStaleWorkingCopyLock(const QString &path) {
  // The row an interrupted operation leaves in wc.db: the root locked at
  // every depth.
  const QByteArray database =
      (normalizedPath(path) + QLatin1Char('/') +
       QString::fromUtf8(svn_wc_get_adm_dir(nullptr)) +
       QStringLiteral("/wc.db"))
          .toUtf8();
  sqlite3 *db = nullptr;
  bool success =
      sqlite3_open_v2(database.constData(), &db, SQLITE_OPEN_READWRITE,
                      nullptr) == SQLITE_OK;
  char *message = nullptr;
  success = success &&
            sqlite3_exec(db,
                         "INSERT INTO WC_LOCK (wc_id, local_dir_relpath, "
                         "locked_levels) SELECT id, '', -1 FROM WCROOT "
                         "LIMIT 1",
                         nullptr, nullptr, &message) == SQLITE_OK;
  if (!success) {
    setLastError(SvnError::Unknown, -1,
                 QString::fromUtf8(message ? message
                                           : sqlite3_errmsg(db)));
  }
  sqlite3_free(message);
  sqlite3_close(db);
  return success;
}

} // namespace Testing
#endif

} // namespace SvnRepo

bool svn_init() {
  return SvnRepo::init();
}
