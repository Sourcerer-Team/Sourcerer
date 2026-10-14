#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

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
#include <svn_wc.h>

#include <svnxx/init.hpp>

#include <EventManager.h>
#include <GitCredentialStorage.h>
#include <GitRepo.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>
#include <QtGlobal>

#include <chrono>
#include <functional>
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
};

thread_local SvnErrorContext gLastError;
thread_local const bool *gQuitFlag = nullptr;
thread_local QString gCurrentRepositoryPath;
// Credential prompts raised on this thread that have not been closed yet.
thread_local std::set<size_t> gPromptedCredentialIds;

std::mutex gInitMutex;
std::unique_ptr<apache::subversion::svnxx::init> gSvnInit;

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

  // The checkout runs long after authentication, so close the prompt now
  // instead of waiting for svn_client_checkout3 to return.
  finishCredentialPrompt(payload, true);

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

  svn_auth_get_ssl_server_trust_prompt_provider(
      &provider, sslServerTrustPromptCallback, payload, pool);
  APR_ARRAY_PUSH(providers, svn_auth_provider_object_t *) = provider;

  svn_auth_baton_t *authBaton = nullptr;
  svn_auth_open(&authBaton, providers, pool);
  return authBaton;
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

  error = svn_config_get_config(&(ctx->config), nullptr, pool);
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
};

static void addStatusPath(SvnStatus *status, const QString &path,
                          const svn_client_status_t *entry) {
  switch (entry->node_status) {
  case svn_wc_status_added:
    if (entry->copied) {
      status->renamedStagedFiles.append(path);
    } else {
      status->newStagedFiles.append(path);
    }
    break;
  case svn_wc_status_deleted:
    status->deletedStagedFiles.append(path);
    break;
  case svn_wc_status_missing:
    status->deletedFiles.append(path);
    break;
  case svn_wc_status_replaced:
    status->typeChangedStagedFiles.append(path);
    break;
  case svn_wc_status_modified:
  case svn_wc_status_merged:
    status->modifiedStagedFiles.append(path);
    break;
  case svn_wc_status_obstructed:
    status->typeChangedFiles.append(path);
    break;
  case svn_wc_status_unversioned:
    status->newFiles.append(path);
    break;
  case svn_wc_status_ignored:
    status->ignoredFiles.append(path);
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

  // Directories only matter when they are added, deleted or unversioned.
  // Property changes of directories are not shown.
  if (status->kind == svn_node_dir &&
      status->node_status != svn_wc_status_added &&
      status->node_status != svn_wc_status_deleted &&
      status->node_status != svn_wc_status_replaced &&
      status->node_status != svn_wc_status_unversioned &&
      status->node_status != svn_wc_status_missing && !status->conflicted) {
    return SVN_NO_ERROR;
  }

  const QString relativePath = Private::relativeFilePath(
      payload->wcRoot, QString::fromUtf8(status->local_abspath));
  if (relativePath.isEmpty() || relativePath == QStringLiteral(".")) {
    return SVN_NO_ERROR;
  }

  addStatusPath(payload->status, relativePath, status);
  return SVN_NO_ERROR;
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
      error->apr_err == SVN_ERR_CANCELLED ? SvnError::Cancelled : kind;
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

QString normalizedPath(const QString &path) {
  if (path.isEmpty()) {
    return QString();
  }

  return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

QString absoluteFilePath(const QString &base, const QString &file) {
  if (QFileInfo(file).isAbsolute()) {
    return QDir::cleanPath(QDir::fromNativeSeparators(file));
  }

  return QDir::cleanPath(QDir(base).absoluteFilePath(file));
}

QString relativeFilePath(const QString &base, const QString &absolutePath) {
  return QDir::fromNativeSeparators(
      QDir(base).relativeFilePath(QDir::fromNativeSeparators(absolutePath)));
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

  bool ok = false;
  const qlonglong number = text.toLongLong(&ok);
  if (!ok || number < 0) {
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

QString urlOfRelativePath(const QString &rootUrl, const QString &relativePath) {
  const QByteArray encodedPath =
      QUrl::toPercentEncoding(relativePath, QByteArrayLiteral("/"));
  return trimmedUrl(rootUrl) + QStringLiteral("/") +
         QString::fromLatin1(encodedPath);
}

QString branchUrl(const QString &reposRootUrl, const QString &name,
                  BranchType type) {
  const QString root = trimmedUrl(reposRootUrl);
  if (type == BranchType::Tag) {
    return root + QStringLiteral("/tags/") + name;
  }

  if (name.isEmpty() || name == QStringLiteral("trunk")) {
    return root + QStringLiteral("/trunk");
  }

  return root + QStringLiteral("/branches/") + name;
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
    return type == BranchType::All ? QString() : QStringLiteral("trunk");
  }

  if (parts[0] == QStringLiteral("trunk")) {
    return type == BranchType::Tag ? QString() : QStringLiteral("trunk");
  }

  if (parts[0] == QStringLiteral("branches") && parts.size() >= 2) {
    return type == BranchType::Tag ? QString() : parts[1];
  }

  if (parts[0] == QStringLiteral("tags") && parts.size() >= 2) {
    return type == BranchType::Tag ? parts[1] : QString();
  }

  // Not in the trunk/branches/tags layout, e.g. a project directory that holds
  // the layout. It is not a branch, so the name stays empty.
  return {};
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
  QString result = summary;
  if (!message.isEmpty()) {
    result += QStringLiteral("\n\n") + message;
  }

  if (cleanup) {
    static const QRegularExpression trailingWhitespace(QStringLiteral("\\s+$"));

    QStringList lines;
    for (QString line : result.split('\n')) {
      if (!line.startsWith('#')) {
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

svn_error_t *commitWorkingCopy(const QString &wcPath, const QString &message,
                               svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const PathArray targets = makePathArray(pool, {normalizedPath(wcPath)});
  const LogMessageScope logMessage(ctx, message);
  return svn_client_commit6(targets.array, svn_depth_infinity, FALSE, FALSE,
                            TRUE, FALSE, FALSE, nullptr, nullptr, nullptr,
                            nullptr, ctx, pool);
}

SvnStatus readStatus(const QString &wcPath, const bool *quitFlag) {
  SvnStatus result;
  result.path = normalizedPath(wcPath);

  const QuitScope quitScope(quitFlag);

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        StatusBaton baton;
        baton.status = &result;
        baton.wcRoot = result.path;

        const QByteArray absPath = result.path.toUtf8();
        const svn_opt_revision_t revision = revisionWorking();
        svn_revnum_t youngest = SVN_INVALID_REVNUM;
        return reportError(SvnError::StatusFailed,
                           svn_client_status6(&youngest, ctx,
                                              absPath.constData(), &revision,
                                              svn_depth_infinity, FALSE, FALSE,
                                              TRUE, FALSE, TRUE, FALSE, nullptr,
                                              statusCallback, &baton, pool));
      },
      wcPath);

  if (!success) {
    return SvnStatus();
  }

  for (QStringList *list :
       {&result.newFiles, &result.modifiedFiles, &result.deletedFiles,
        &result.typeChangedFiles, &result.newStagedFiles,
        &result.modifiedStagedFiles, &result.deletedStagedFiles,
        &result.typeChangedStagedFiles, &result.renamedStagedFiles,
        &result.ignoredFiles, &result.conflictedFiles}) {
    list->removeDuplicates();
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
                                svn_client_ctx_t *ctx, apr_pool_t *pool) {
  svn_stringbuf_t *buffer = svn_stringbuf_create_empty(pool);
  svn_stream_t *stream = svn_stream_from_stringbuf(buffer, pool);

  const QByteArray target = svn_path_is_url(pathOrUrl.toUtf8().constData())
                                ? pathOrUrl.toUtf8()
                                : normalizedPath(pathOrUrl).toUtf8();
  SVN_ERR(svn_client_cat3(nullptr, stream, target.constData(), &peg, &revision,
                          FALSE, ctx, pool, pool));
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

  const bool success = runWithClientContext(
      [repoPath](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        int wcFormat = 0;
        const QByteArray pathBytes = repoPath.toUtf8();
        return reportError(SvnError::ErrorWhileOpening,
                           svn_wc_check_wc2(&wcFormat, ctx->wc_ctx,
                                            pathBytes.constData(), pool)) &&
               (wcFormat > 0 ||
                (setLastError(SvnError::ErrorWhileOpening,
                              SVN_ERR_WC_NOT_WORKING_COPY,
                              QStringLiteral("'%1' is not a Subversion "
                                             "working copy.")
                                  .arg(repoPath)),
                 false));
      },
      repoPath);

  if (!success) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  setCurrentRepositoryPath(repoPath);

  LOG_PERF(0, "Exit");
  return repoPath;
}

void closeRepository(const QString &) {
  finishPendingCredentialPrompts();
  clearCurrentRepositoryPath();
}

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  LOG_PERF(0, "Enter");

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

    CheckoutProgressBaton progressBaton;
    progressBaton.progress.uuid = uuid;
    progressBaton.progress.cloning = true;
    progressBaton.progress.stage = GitRepo::CloneProgressStageType::Checkout;

    const QByteArray urlBytes = url.toUtf8();
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

  LOG_PERF(0, "Exit");
  return success;
}

} // namespace SvnRepo

bool svn_init() {
  return SvnRepo::init();
}
