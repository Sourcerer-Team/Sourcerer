#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include "GitCredentialStorage.h"
#include "GitKeyStorage.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <LockGuard.h>
#include <Log.h>

#include <QByteArray>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#include <set>
#include <tuple>

#define THREAD_ID "GitRepo"

// Credentials: stored and prompted HTTPS credentials and SSH keys.
namespace GitRepo {

using namespace Private;

// How often the user is asked within one operation before it fails.
constexpr int MAX_CREDENTIAL_ATTEMPTS = 5;

constexpr qint64 DEFAULT_CREDENTIALS_EXPIRATION_TIME =
    30 * 60 * 1000; // 30 minutes

namespace Private {

Util::Mutex gGitCredentialsMutex;

QMap<QString, GitPlaintextCredentials> gGitCredentials;

void invalidateCredentials(const QString &url) {
  LOCKGUARD(gGitCredentialsMutex);

  const auto it = gGitCredentials.find(url);
  if (it != gGitCredentials.end())
    gGitCredentials.erase(it);
}

} // namespace Private

static thread_local std::set<size_t> gPromptedCredentialIds;

namespace Private {

void sendInvalidCredentialsSignal(const QString &url) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  EventManager::Event event(
      EventManager::EventType::GitRepoManagerGetCredentialsInvalid);
  event.addData(qHash(url));

  eventManager.send(event);
}

} // namespace Private

static QString repositoryRootPath(git_repository *repo);

static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &repoPath, const QString &url,
                   const QString &username, const int allowedTypes,
                   bool useStored = true);

static QString extractSshHost(const QString &url);

static bool promptForSshPassphrase(const QString &repoPath, const QString &url,
                                   const QString &username,
                                   QString &passphrase);

QString getBasicAuthHeader(const QString &repoPath, const QString &url) {
  LOG_PERF(0, "Enter");

  if (repoPath.isEmpty() || url.isEmpty()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();
  QString username;
  QString password;
  if (!credStorage.getCredentials(repoPath, username, password)) {
    const auto [success, credentials] = acquireCredentials(
        repoPath, url, QString(), GIT_CREDENTIAL_USERPASS_PLAINTEXT);
    if (!success) {
      LOG_PERF(0, "Exit");
      return {};
    }

    username = credentials.username;
    password = credentials.password;
  }

  const QByteArray auth =
      QString("%1:%2").arg(username, password).toUtf8().toBase64();
  LOG_PERF(0, "Exit");
  return QString::fromLatin1("Basic ") + QString::fromLatin1(auth);
}

QString getBasicAuthHeader(git_repository *repo, const QString &url) {
  if (!repo) {
    return {};
  }

  return getBasicAuthHeader(repositoryRootPath(repo), url);
}

static QString repoWorkdirPath(git_repository *repo) {
  if (!repo) {
    return {};
  }

  const char *workdir = git_repository_workdir(repo);
  if (workdir && *workdir) {
    return QDir::fromNativeSeparators(QString::fromUtf8(workdir));
  }

  const char *path = git_repository_path(repo);
  return path ? QDir::fromNativeSeparators(QString::fromUtf8(path)) : QString();
}

static QString normalizeRepoRoot(QString path) {
  if (path.endsWith("/.git/") || path.endsWith("\\.git\\")) {
    path.chop(6);
  } else if (path.endsWith("/.git") || path.endsWith("\\.git")) {
    path.chop(5);
  }

  return QDir::cleanPath(path);
}

static QString repositoryRootPath(git_repository *repo) {
  if (!repo) {
    return {};
  }

  if (git_repository_is_bare(repo)) {
    const char *path = git_repository_path(repo);
    return path ? QDir::cleanPath(QString::fromUtf8(path)) : QString();
  }

  return normalizeRepoRoot(repoWorkdirPath(repo));
}

static QString extractSshHost(const QString &url) {
  if (url.startsWith("ssh://", Qt::CaseInsensitive)) {
    return QUrl(url).host();
  }

  const QRegularExpression scpLike(
      QStringLiteral(R"(^(([^@]+)@)?([^:]+):/?(.+)$)"));
  const QRegularExpressionMatch scpMatch = scpLike.match(url);
  if (scpMatch.hasMatch()) {
    return scpMatch.captured(3);
  }

  return {};
}

static bool promptForSshPassphrase(const QString &repoPath, const QString &url,
                                   const QString &username,
                                   QString &passphrase) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();
  const int token = eventManager.startWait();

  GitCredentialsRequest request;
  request.id = qHash(url);
  request.url = url;
  request.username = username;
  request.allowedTypes = GIT_CREDENTIAL_SSH_KEY;
  request.repoPath = repoPath;
  request.needsPassphrase = true;

  const size_t promptId = request.id;
  gPromptedCredentialIds.insert(promptId);

  EventManager::Event credentialsEvent(
      EventManager::EventType::GitRepoManagerGetCredentials);
  credentialsEvent.addData(request);
  eventManager.send(credentialsEvent);

  const auto predicate = [&](EventManager::Event event) -> bool {
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
      const auto [id] = event.data<GitCredentialsErrorResponse>();
      return id == request.id;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();
      return response.id == request.id;
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
    return false;
  }

  const GitCredentialsResponse response = event.data<GitCredentialsResponse>();
  passphrase =
      !response.passphrase.isEmpty() ? response.passphrase : response.password;
  return true;
}

// Passphrases that opened a private key, by key path. Cleared by
// clearCachedCredentials.
static Util::Mutex gSshPassphrasesMutex;
static QMap<QString, QString> gSshPassphrases;

// Whether the private key needs a passphrase: a PEM key says so in its
// headers ("Proc-Type: 4,ENCRYPTED", "BEGIN ENCRYPTED PRIVATE KEY"), an
// OpenSSH key ("openssh-key-v1") names a cipher other than "none".
static bool isEncryptedPrivateKey(const QByteArray &content) {
  static const QByteArray begin = "-----BEGIN OPENSSH PRIVATE KEY-----";
  static const QByteArray end = "-----END OPENSSH PRIVATE KEY-----";
  const qsizetype from = content.indexOf(begin);
  const qsizetype to = content.indexOf(end);
  if (from < 0 || to < from) {
    return content.contains("ENCRYPTED");
  }

  QByteArray base64 = content.mid(from + begin.size(), to - from - begin.size());
  base64.replace('\r', "").replace('\n', "").replace(' ', "");
  const QByteArray data = QByteArray::fromBase64(base64);
  static const QByteArray magic("openssh-key-v1\0", 15);
  if (!data.startsWith(magic) || data.size() < magic.size() + 4) {
    return false;
  }
  const uchar *length =
      reinterpret_cast<const uchar *>(data.constData() + magic.size());
  const quint32 cipherLength = (quint32(length[0]) << 24) |
                               (quint32(length[1]) << 16) |
                               (quint32(length[2]) << 8) | quint32(length[3]);
  const QByteArray cipher = data.mid(magic.size() + 4, cipherLength);
  return !cipher.isEmpty() && cipher != "none";
}

// Whether the passphrase opens the private key. PEM keys are checked with
// OpenSSL, OpenSSH keys with ssh-keygen when it is available. Without a way
// to check, the passphrase is taken as it is.
static bool passphraseOpensKey(const QString &privateKeyPath,
                               const QByteArray &content,
                               const QString &passphrase) {
  if (content.contains("-----BEGIN OPENSSH PRIVATE KEY-----")) {
    const QString keygen =
        QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
    if (keygen.isEmpty()) {
      return true;
    }
    QProcess process;
    process.start(keygen, {QStringLiteral("-y"), QStringLiteral("-P"),
                           passphrase, QStringLiteral("-f"),
                           QDir::toNativeSeparators(privateKeyPath)});
    if (!process.waitForFinished(30000)) {
      process.kill();
      process.waitForFinished();
      return true;
    }
    if (process.exitStatus() == QProcess::NormalExit &&
        process.exitCode() == 0) {
      return true;
    }
    // Only a rejected passphrase counts as wrong, not other complaints
    // (such as the permissions of the key file).
    const QByteArray output =
        process.readAllStandardError() + process.readAllStandardOutput();
    return !output.contains("incorrect passphrase");
  }

  BIO *bio = BIO_new_mem_buf(content.constData(), static_cast<int>(content.size()));
  if (!bio) {
    return true;
  }
  QByteArray passphraseUtf8 = passphrase.toUtf8();
  EVP_PKEY *key =
      PEM_read_bio_PrivateKey(bio, nullptr, nullptr, passphraseUtf8.data());
  BIO_free(bio);
  ERR_clear_error();
  if (!key) {
    return false;
  }
  EVP_PKEY_free(key);
  return true;
}

GitSshAuthInfo getSshAuthInfo(const QString &repoPath, const QString &url,
                              const QString &usernameFromUrl) {
  LOG_PERF(0, "Enter");

  GitSshAuthInfo authInfo;
  authInfo.username =
      usernameFromUrl.isEmpty() ? QStringLiteral("git") : usernameFromUrl;

  const QString host = extractSshHost(url);
  if (host.isEmpty()) {
    LOG_PERF(0, "Exit");
    return authInfo;
  }

  GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
  GitKeyPair keyPair;
  if (!keyStorage.getKeyPairForHost(host, keyPair)) {
    // No key for the host: the first of ssh's default identities that
    // exists, id_ed25519 first. Without any, id_ed25519 (not valid then).
    const QString sshDir = GitKeyStorage::getSshDirectoryPath();
    const auto useKey = [&](const QString &name) {
      keyPair.keyName = name;
      keyPair.privateKeyPath = sshDir + '/' + name;
      keyPair.publicKeyPath = keyPair.privateKeyPath + QStringLiteral(".pub");
      return QFile::exists(keyPair.privateKeyPath) &&
             QFile::exists(keyPair.publicKeyPath);
    };
    if (!useKey(QStringLiteral("id_ed25519")) &&
        !useKey(QStringLiteral("id_ecdsa")) &&
        !useKey(QStringLiteral("id_rsa"))) {
      useKey(QStringLiteral("id_ed25519"));
    }
  }

  authInfo.privateKeyPath = keyPair.privateKeyPath;
  authInfo.publicKeyPath = keyPair.publicKeyPath;
  authInfo.passphrasePresent = false;
  authInfo.valid = QFile::exists(authInfo.privateKeyPath) &&
                   QFile::exists(authInfo.publicKeyPath);

  if (!authInfo.valid) {
    LOG_PERF(0, "Exit");
    return authInfo;
  }

  QFile keyFile(authInfo.privateKeyPath);
  if (keyFile.open(QIODevice::ReadOnly)) {
    const QByteArray keyContent = keyFile.readAll();
    keyFile.close();
    if (isEncryptedPrivateKey(keyContent)) {
      authInfo.passphrasePresent = true;

      // A passphrase that opened the key before is used again.
      {
        LOCKGUARD(gSshPassphrasesMutex);
        const auto it = gSshPassphrases.constFind(authInfo.privateKeyPath);
        if (it != gSshPassphrases.constEnd()) {
          authInfo.passphrase = *it;
          authInfo.valid = true;
          LOG_PERF(0, "Exit");
          return authInfo;
        }
      }

      // A wrong passphrase is caught here and asked again, before libssh2
      // fails the connection with it.
      for (int attempt = 0;; ++attempt) {
        QString passphrase;
        if (attempt >= MAX_CREDENTIAL_ATTEMPTS ||
            !promptForSshPassphrase(repoPath, url, authInfo.username,
                                    passphrase)) {
          authInfo.valid = false;
          LOG_PERF(0, "Exit");
          return authInfo;
        }
        if (passphraseOpensKey(authInfo.privateKeyPath, keyContent,
                               passphrase)) {
          authInfo.passphrase = passphrase;
          LOCKGUARD(gSshPassphrasesMutex);
          gSshPassphrases.insert(authInfo.privateKeyPath, passphrase);
          break;
        }
        sendInvalidCredentialsSignal(url);
      }
    }
  }

  authInfo.valid = true;
  LOG_PERF(0, "Exit");
  return authInfo;
}

void clearCachedCredentials(const QString &repoPath) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing cached credentials for repository: {}", repoPath);

  {
    LOCKGUARD(gSshPassphrasesMutex);
    gSshPassphrases.clear();
  }

  LOCKGUARD(gGitCredentialsMutex);

  // Clear all cached credentials to ensure fresh ones are loaded from storage
  // This is necessary when credentials are updated by the user
  gGitCredentials.clear();

  LOG_INFO("Cleared all cached credentials");
  LOG_PERF(0, "Exit");
}

namespace Private {

void finishCredentialsAcquiring(const QString &url) {
  // Only a prompt raised on this thread, and only once: the ones left are
  // closed by finishPendingCredentialPrompts.
  const size_t id = qHash(url);
  if (gPromptedCredentialIds.erase(id) == 0) {
    return;
  }

  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  EventManager::Event credentialsFinishedEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsFinished);
  credentialsFinishedEvent.addData(id);

  eventManager.send(credentialsFinishedEvent);
}

} // namespace Private

void finishPendingCredentialPrompts() {
  // Credential prompts are keyed by a hash of the URL they were raised for.
  // That URL is not always the remote URL the calling operation knows about
  // (e.g. Git LFS endpoints), and prompts can also be raised outside of the
  // fetch/push window (e.g. LFS smudge during checkout). Close every prompt
  // raised on this thread so that no overlay is left behind.
  std::set<size_t> ids;
  ids.swap(gPromptedCredentialIds);

  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  for (const size_t id : ids) {
    EventManager::Event event(
        EventManager::EventType::GitRepoManagerGetCredentialsFinished);
    event.addData(id);
    eventManager.send(event);
  }
}

static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &repoPath, const QString &url,
                   const QString &username, const int allowedTypes,
                   const bool useStored) {
  LOG_PERF(0, "Enter");

  const QString normalizedRepoPath = QDir::fromNativeSeparators(repoPath);

  LOG_INFO("acquireCredentials called with repoPath: '{}', url: '{}'",
           normalizedRepoPath, url);

  // First, try to get stored credentials from encrypted storage
  GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();

  QString storedUsername, storedPassword;
  if (useStored && credStorage.getCredentials(normalizedRepoPath, storedUsername,
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

  gPromptedCredentialIds.insert(request.id);

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

namespace Private {

int credentialsAcquireCallback(git_credential **out, const char *url,
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
  GitCredentialPayload *credentialsPayload = nullptr;
  if (payload) {
    std::tuple<CloneProgress, GitCredentialPayload> *callbackPayload =
        static_cast<std::tuple<CloneProgress, GitCredentialPayload> *>(payload);
    credentialsPayload = &std::get<1>(*callbackPayload);
    repoPath = credentialsPayload->repoPath;
  }

  if (const QString urlString = QString::fromUtf8(url);
      urlString.startsWith("git@") ||
      urlString.startsWith("ssh://", Qt::CaseInsensitive)) {
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
    const GitSshAuthInfo sshAuth =
        getSshAuthInfo(repoPath, QString::fromUtf8(url),
                       username_from_url ? username_from_url : "git");
    if (!sshAuth.valid) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to acquire SSH auth info for: {}",
                QString::fromUtf8(url));
      LOG_PERF(0, "Exit");
      return GIT_EUSER;
    }

    const int error = git_credential_ssh_key_new(
        out, sshAuth.username.toStdString().c_str(),
        sshAuth.publicKeyPath.toStdString().c_str(),
        sshAuth.privateKeyPath.toStdString().c_str(),
        sshAuth.passphrase.isEmpty()
            ? nullptr
            : sshAuth.passphrase.toStdString().c_str());

    LOG_PERF(0, "Exit");
    return error;
  }

  // libgit2 asks again within one operation only when the server rejected
  // the credentials it was given. Forget them and ask the user, instead of
  // handing the same ones back until libgit2 gives up.
  const int attempt =
      credentialsPayload ? credentialsPayload->credentialAttempts++ : 0;
  if (attempt > 0) {
    invalidateCredentials(url);
    sendInvalidCredentialsSignal(url);
    if (attempt >= MAX_CREDENTIAL_ATTEMPTS) {
      LOG_ERROR(Log::ErrorId::Git, "Credentials for '{}' were rejected {} times",
                QString::fromUtf8(url), attempt);
      LOG_PERF(0, "Exit");
      return GIT_EUSER;
    }

    const auto &[success, credentials] = acquireCredentials(
        repoPath, url, username_from_url ? username_from_url : "",
        allowed_types, /*useStored=*/false);
    if (!success) {
      LOG_PERF(0, "Exit");
      return GIT_EUSER;
    }

    gGitCredentialsMutex.lock();
    gGitCredentials[url] = credentials;
    gGitCredentialsMutex.unlock();

    const int error = git_credential_userpass_plaintext_new(
        out, credentials.username.toStdString().c_str(),
        credentials.password.toStdString().c_str());
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
    setError(
        GitError::CredentialsAcquireCallbackFailedToCreatePlaintextCredentials,
        error);
    LOG_PERF(0, "Exit");
    return error;
  }

  LOG_PERF(0, "Exit");

  return 0;
}

} // namespace Private

} // namespace GitRepo
