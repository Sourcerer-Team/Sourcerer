#include "GitCredentialStorage.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <qcoreapplication.h>
#include <qtkeychain/keychain.h>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define THREAD_ID "GitCredentialStorage"

namespace GitRepo {

constexpr int AES_KEY_SIZE = 32; // 256 bits
constexpr int AES_IV_SIZE = 16;  // 128 bits
constexpr int PBKDF2_ITERATIONS = 100000;

static QString getLoggedInUser() {
  // Returns e.g. "C:/Users/JohnDoe"
  QString homePath = QDir::homePath();
  return QFileInfo(homePath).fileName(); // Returns "JohnDoe"
}

static bool saveMasterKeyToWindowsStore(const QString &appName,
                                        const QString &userName,
                                        const QString &masterKey) {
  // Create job on the stack (no need for manual memory cleanup or
  // setAutoDelete)
  QKeychain::WritePasswordJob job(appName);
  job.setKey(userName);       // Identifier (e.g., username)
  job.setTextData(masterKey); // Secret payload

  // Local event loop to block until finished signal is emitted
  QEventLoop loop;
  QObject::connect(&job, &QKeychain::WritePasswordJob::finished, &loop,
                   &QEventLoop::quit);

  job.start();
  loop.exec(); // Waits here until OS keyring operation finishes

  if (job.error()) {
    return false;
  }

  return true;
}

bool readMasterKeyFromWindowsStore(const QString &appName,
                                   const QString &userName,
                                   QString &outMasterKey) {
  QKeychain::ReadPasswordJob job(appName);
  job.setKey(userName);

  QEventLoop loop;
  QObject::connect(&job, &QKeychain::ReadPasswordJob::finished, &loop,
                   &QEventLoop::quit);
  job.start();
  loop.exec(); // Blocks until job emits finished signal

  if (job.error()) {
    return false;
  }

  outMasterKey = job.textData();
  return true;
}

GitCredentialStorage::GitCredentialStorage() {
  LOG_PERF(0, "Enter");
  LOG_INFO("GitCredentialStorage constructor called - initializing singleton");

  try {
    _masterKey = getEncryptionKey();
    LOG_INFO("Master key initialized, loading persisted credentials...");
    loadPersistedCredentials();
    LOG_INFO("GitCredentialStorage initialization complete");
  } catch (const std::exception &ex) {
    LOG_ERROR(Log::ErrorId::Security, "Exception: {}", ex.what());
  }

  LOG_PERF(0, "Exit");
}

GitCredentialStorage::~GitCredentialStorage() {
  LOG_PERF(0, "Enter");

  // Clear sensitive data from memory
  _masterKey.fill(0);
  for (auto &cred : _credentials) {
    cred.username.fill('\0');
    cred.password.fill('\0');
  }
  _credentials.clear();

  LOG_PERF(0, "Exit");
}

GitCredentialStorage &GitCredentialStorage::getInstance() {
  static GitCredentialStorage instance;
  return instance;
}

void GitCredentialStorage::storeCredentials(const QString &repoPath,
                                            const QString &username,
                                            const QString &password,
                                            bool remember) {
  LOG_PERF(0, "Enter");
  LOG_INFO("storeCredentials called: repoPath='{}', username='{}', remember={}",
           repoPath, username, remember);

  LOCKGUARD(_mutex);

  Credentials cred;
  cred.username = username;
  cred.password = password;
  cred.createdTime = QDateTime::currentMSecsSinceEpoch();
  cred.rememberCredentials = remember;

  _credentials[repoPath] = cred;
  LOG_INFO("Added credentials to in-memory map, map size now: {}",
           _credentials.size());

  if (remember) {
    LOG_INFO("Calling saveCredentialsToDisk...");
    saveCredentialsToDisk(repoPath, cred);
    LOG_INFO("saveCredentialsToDisk completed");
  } else {
    LOG_INFO("Not saving to disk (remember=false)");
  }

  LOG_PERF(0, "Exit");
}

bool GitCredentialStorage::getCredentials(const QString &repoPath,
                                          QString &username,
                                          QString &password) {
  LOG_PERF(0, "Enter");
  LOG_INFO("getCredentials called for repoPath: '{}'", repoPath);
  LOG_INFO("Current credentials map size: {}", _credentials.size());

  LOCKGUARD(_mutex);

  if (!_credentials.contains(repoPath)) {
    LOG_INFO("No credentials found in map for repository: {}", repoPath);

    // Debug: print all keys in the map
    LOG_INFO("Available repository paths in credentials map:");
    for (const auto &key : _credentials.keys()) {
      LOG_INFO("  - '{}'", key);
    }

    LOG_PERF(0, "Exit");
    return false;
  }

  const Credentials &cred = _credentials[repoPath];
  username = cred.username;
  password = cred.password;

  LOG_INFO("Retrieved credentials for repository: {}", repoPath);
  LOG_PERF(0, "Exit");
  return true;
}

bool GitCredentialStorage::hasCredentials(const QString &repoPath) const {
  LOCKGUARD(_mutex);
  return _credentials.contains(repoPath);
}

void GitCredentialStorage::removeCredentials(const QString &repoPath) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Removing credentials for repository: {}", repoPath);

  LOCKGUARD(_mutex);

  if (_credentials.contains(repoPath)) {
    // Clear sensitive data
    _credentials[repoPath].username.fill('\0');
    _credentials[repoPath].password.fill('\0');
    _credentials.remove(repoPath);
  }

  removeCredentialsFromDisk(repoPath);

  LOG_PERF(0, "Exit");
}

void GitCredentialStorage::clearMemoryCredentials() {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing all in-memory credentials");

  LOCKGUARD(_mutex);

  for (auto &cred : _credentials) {
    cred.username.fill('\0');
    cred.password.fill('\0');
  }
  _credentials.clear();

  // Reload persisted credentials
  loadPersistedCredentials();

  LOG_PERF(0, "Exit");
}

void GitCredentialStorage::clearAll() {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing all credentials including persisted");

  LOCKGUARD(_mutex);

  for (auto &cred : _credentials) {
    cred.username.fill('\0');
    cred.password.fill('\0');
  }
  _credentials.clear();

  // Remove credentials file
  QString credFile = getCredentialsFilePath();
  if (QFile::exists(credFile)) {
    QFile::remove(credFile);
  }

  LOG_PERF(0, "Exit");
}

bool GitCredentialStorage::areCredentialsExpired(
    const QString &repoPath, qint64 expirationTimeMs) const {
  LOCKGUARD(_mutex);

  if (!_credentials.contains(repoPath)) {
    LOG_INFO("areCredentialsExpired: No credentials found for '{}'", repoPath);
    return true;
  }

  const Credentials &cred = _credentials[repoPath];
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  qint64 expirationTime = cred.createdTime + expirationTimeMs;
  bool expired = expirationTime < now;

  LOG_INFO("areCredentialsExpired for '{}': created={}, now={}, expiration={}, "
           "expired={}",
           repoPath, cred.createdTime, now, expirationTime, expired);

  return expired;
}

QList<QString> GitCredentialStorage::getAllRepositoryPaths() const {
  LOCKGUARD(_mutex);
  return _credentials.keys();
}

bool GitCredentialStorage::getCredentialsInfo(const QString &repoPath,
                                              QString &username,
                                              qint64 &createdTime) const {
  LOCKGUARD(_mutex);

  if (!_credentials.contains(repoPath)) {
    return false;
  }

  const Credentials &cred = _credentials[repoPath];
  username = cred.username;
  createdTime = cred.createdTime;

  return true;
}

void GitCredentialStorage::loadPersistedCredentials() {
  LOG_PERF(0, "Enter");

  QString credFile = getCredentialsFilePath();
  LOG_INFO("Loading credentials from file: '{}'", credFile);

  if (!QFile::exists(credFile)) {
    LOG_INFO("No persisted credentials file found at: '{}'", credFile);
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_INFO("Credentials file exists, size: {} bytes", QFile(credFile).size());

  QFile file(credFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open credentials file: {}",
              file.errorString());
    LOG_PERF(0, "Exit");
    return;
  }

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);

  int version;
  stream >> version;

  LOG_INFO("Credentials file version: {}", version);

  if (version != 1) {
    LOG_ERROR(Log::ErrorId::Git, "Unsupported credentials file version: {}",
              version);
    file.close();
    LOG_PERF(0, "Exit");
    return;
  }

  qsizetype count;
  stream >> count;

  LOG_INFO("Loading {} persisted credential entries", count);

  for (int i = 0; i < count; ++i) {
    QString repoPath;
    QByteArray encryptedData;

    stream >> repoPath >> encryptedData;

    LOG_INFO("Attempting to load credentials for repoPath: '{}'", repoPath);

    try {
      QByteArray repoKey = deriveRepoKey(repoPath);
      QByteArray decryptedData = decrypt(encryptedData, repoKey);

      QDataStream credStream(decryptedData);
      credStream.setVersion(QDataStream::Qt_6_0);

      Credentials cred;
      credStream >> cred.username >> cred.password >> cred.createdTime;
      cred.rememberCredentials = true;

      _credentials[repoPath] = cred;

      LOG_INFO("Loaded credentials for repository: {}", repoPath);
    } catch (const std::exception &e) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to decrypt credentials for {}: {}",
                repoPath, e.what());
    }
  }

  file.close();
  LOG_PERF(0, "Exit");
}

void GitCredentialStorage::saveCredentialsToDisk(
    const QString &repoPath, const Credentials &credentials) {
  LOG_PERF(0, "Enter");

  QString credFile = getCredentialsFilePath();

  // Ensure directory exists
  QFileInfo fileInfo(credFile);
  QDir dir = fileInfo.absoluteDir();
  if (!dir.exists()) {
    dir.mkpath(".");
  }

  // Load all existing credentials
  QMap<QString, QByteArray> allEncryptedCreds;

  if (QFile::exists(credFile)) {
    QFile file(credFile);
    if (file.open(QIODevice::ReadOnly)) {
      QDataStream stream(&file);
      stream.setVersion(QDataStream::Qt_6_0);

      int version;
      stream >> version;

      if (version == 1) {
        qsizetype count;
        stream >> count;

        for (int i = 0; i < count; ++i) {
          QString path;
          QByteArray data;
          stream >> path >> data;
          allEncryptedCreds[path] = data;
        }
      }
      file.close();
    }
  }

  // Encrypt and add/update current credentials
  QByteArray credData;
  QDataStream credStream(&credData, QIODevice::WriteOnly);
  credStream.setVersion(QDataStream::Qt_6_0);
  credStream << credentials.username << credentials.password
             << credentials.createdTime;

  QByteArray repoKey = deriveRepoKey(repoPath);
  QByteArray encryptedData = encrypt(credData, repoKey);
  allEncryptedCreds[repoPath] = encryptedData;

  // Write all credentials back
  QFile file(credFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to open credentials file for writing: {}",
              file.errorString());
    LOG_PERF(0, "Exit");
    return;
  }

  // Set file permissions to user-only read/write
  file.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser);

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);

  stream << 1; // version
  stream << static_cast<qsizetype>(allEncryptedCreds.size());

  LOG_INFO("Writing {} credential entries to file", allEncryptedCreds.size());

  for (auto it = allEncryptedCreds.begin(); it != allEncryptedCreds.end();
       ++it) {
    stream << it.key() << it.value();
    LOG_INFO("Wrote entry for repository: '{}'", it.key());
  }

  file.close();

  LOG_INFO("Successfully saved credentials to disk for repository: {}",
           repoPath);
  LOG_PERF(0, "Exit");
}

void GitCredentialStorage::removeCredentialsFromDisk(const QString &repoPath) {
  LOG_PERF(0, "Enter");

  QString credFile = getCredentialsFilePath();
  if (!QFile::exists(credFile)) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Load all existing credentials
  QMap<QString, QByteArray> allEncryptedCreds;

  QFile file(credFile);
  if (file.open(QIODevice::ReadOnly)) {
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);

    int version;
    stream >> version;

    if (version == 1) {
      qsizetype count;
      stream >> count;

      for (int i = 0; i < count; ++i) {
        QString path;
        QByteArray data;
        stream >> path >> data;
        if (path != repoPath) {
          allEncryptedCreds[path] = data;
        }
      }
    }
    file.close();
  }

  // Write back without the removed entry
  if (allEncryptedCreds.isEmpty()) {
    QFile::remove(credFile);
  } else {
    if (file.open(QIODevice::WriteOnly)) {
      file.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser);

      QDataStream stream(&file);
      stream.setVersion(QDataStream::Qt_6_0);

      stream << 1; // version
      stream << allEncryptedCreds.size();

      for (auto it = allEncryptedCreds.begin(); it != allEncryptedCreds.end();
           ++it) {
        stream << it.key() << it.value();
      }

      file.close();
    }
  }

  LOG_INFO("Removed credentials from disk for repository: {}", repoPath);
  LOG_PERF(0, "Exit");
}

QByteArray GitCredentialStorage::encrypt(const QByteArray &plaintext,
                                         const QByteArray &key) {
  // Generate random IV
  unsigned char iv[AES_IV_SIZE];
  if (RAND_bytes(iv, AES_IV_SIZE) != 1) {
    throw std::runtime_error("Failed to generate IV");
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    throw std::runtime_error("Failed to create cipher context");
  }

  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                         reinterpret_cast<const unsigned char *>(key.data()),
                         iv) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("Failed to initialize encryption");
  }

  int maxOutLen = plaintext.size() + AES_BLOCK_SIZE;
  QByteArray ciphertext;
  ciphertext.resize(maxOutLen);

  int outLen = 0;
  if (EVP_EncryptUpdate(
          ctx, reinterpret_cast<unsigned char *>(ciphertext.data()), &outLen,
          reinterpret_cast<const unsigned char *>(plaintext.data()),
          plaintext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("Failed to encrypt data");
  }

  int finalLen = 0;
  if (EVP_EncryptFinal_ex(
          ctx, reinterpret_cast<unsigned char *>(ciphertext.data()) + outLen,
          &finalLen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("Failed to finalize encryption");
  }

  EVP_CIPHER_CTX_free(ctx);

  ciphertext.resize(outLen + finalLen);

  // Prepend IV to ciphertext
  QByteArray result(reinterpret_cast<const char *>(iv), AES_IV_SIZE);
  result.append(ciphertext);

  return result;
}

QByteArray GitCredentialStorage::decrypt(const QByteArray &ciphertext,
                                         const QByteArray &key) {
  if (ciphertext.size() < AES_IV_SIZE) {
    throw std::runtime_error("Invalid ciphertext: too short");
  }

  // Extract IV from the beginning
  QByteArray iv = ciphertext.left(AES_IV_SIZE);
  QByteArray actualCiphertext = ciphertext.mid(AES_IV_SIZE);

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    throw std::runtime_error("Failed to create cipher context");
  }

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                         reinterpret_cast<const unsigned char *>(key.data()),
                         reinterpret_cast<const unsigned char *>(iv.data())) !=
      1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("Failed to initialize decryption");
  }

  QByteArray plaintext;
  plaintext.resize(actualCiphertext.size() + AES_BLOCK_SIZE);

  int outLen = 0;
  if (EVP_DecryptUpdate(
          ctx, reinterpret_cast<unsigned char *>(plaintext.data()), &outLen,
          reinterpret_cast<const unsigned char *>(actualCiphertext.data()),
          actualCiphertext.size()) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("Failed to decrypt data");
  }

  int finalLen = 0;
  if (EVP_DecryptFinal_ex(
          ctx, reinterpret_cast<unsigned char *>(plaintext.data()) + outLen,
          &finalLen) != 1) {
    EVP_CIPHER_CTX_free(ctx);
    throw std::runtime_error("Failed to finalize decryption");
  }

  EVP_CIPHER_CTX_free(ctx);

  plaintext.resize(outLen + finalLen);
  return plaintext;
}

QByteArray GitCredentialStorage::getEncryptionKey() {
  LOG_PERF(0, "Enter");

  const QString userName = getLoggedInUser();
  const QString appName = qAppName();

  QString masterKey;
  bool success = readMasterKeyFromWindowsStore(appName, userName, masterKey);

  QByteArray key;
  if (!success || masterKey.isEmpty()) {
    // Generate new key
    key.resize(AES_KEY_SIZE);
    if (RAND_bytes(reinterpret_cast<unsigned char *>(key.data()),
                   AES_KEY_SIZE) != 1) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to generate master key");
      throw std::runtime_error("Failed to generate master key");
    }

    // Store it
    masterKey = key.toHex();

    success = saveMasterKeyToWindowsStore(appName, userName, masterKey);
    if (!success) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to save master key");
      throw std::runtime_error("Failed to save master key");
    }

    LOG_INFO("Generated new master encryption key");
  } else {
    key = QByteArray::fromHex(masterKey.toUtf8());
  }

  LOG_PERF(0, "Exit");
  return key;
}

QString GitCredentialStorage::getCredentialsFilePath() const {
  QString dataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return QDir(dataPath).filePath("git_credentials.dat");
}

QByteArray GitCredentialStorage::deriveRepoKey(const QString &repoPath) {
  // Use PBKDF2 to derive a repository-specific key from the master key
  QByteArray salt =
      QCryptographicHash::hash(repoPath.toUtf8(), QCryptographicHash::Sha256);

  QByteArray derivedKey;
  derivedKey.resize(AES_KEY_SIZE);

  if (PKCS5_PBKDF2_HMAC(
          _masterKey.data(), _masterKey.size(),
          reinterpret_cast<const unsigned char *>(salt.data()), salt.size(),
          PBKDF2_ITERATIONS, EVP_sha256(), AES_KEY_SIZE,
          reinterpret_cast<unsigned char *>(derivedKey.data())) != 1) {
    throw std::runtime_error("Failed to derive repository key");
  }

  return derivedKey;
}

} // namespace GitRepo
