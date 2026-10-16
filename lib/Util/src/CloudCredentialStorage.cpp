#include "CloudCredentialStorage.h"

#include <Log.h>

#include <qcoreapplication.h>
#include <qtkeychain/keychain.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#define THREAD_ID "CloudCredentialtorage"

namespace Util {

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
    ConfigManager::ConfigManager &config =
        ConfigManager::ConfigManager::getInstance();

    config.setString(ConfigManager::ConfigKey::GitRepoCredentialsMasterKey,
                     masterKey);
    config.save();
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
    ConfigManager::ConfigManager &config =
        ConfigManager::ConfigManager::getInstance();

    outMasterKey =
        config.getString(ConfigManager::ConfigKey::GitRepoCredentialsMasterKey);

    return !outMasterKey.isEmpty();
  }

  outMasterKey = job.textData();
  return true;
}

CloudCredentialtorage::CloudCredentialtorage() {
  LOG_PERF(0, "Enter");
  LOG_INFO("CloudCredentialtorage constructor called - initializing singleton");

  try {
    _masterKey = getEncryptionKey();
    LOG_INFO("Master key initialized, loading persisted credentials...");
    loadPersistedCredentials();
    LOG_INFO("CloudCredentialtorage initialization complete");
  } catch (const std::exception &ex) {
    LOG_ERROR(Log::ErrorId::Security, "Exception: {}", ex.what());
  }

  LOG_PERF(0, "Exit");
}

CloudCredentialtorage::~CloudCredentialtorage() {
  LOG_PERF(0, "Enter");

  // Clear sensitive data from memory
  _masterKey.fill(0);
  _credentials.username.fill('\0');
  _credentials.password.fill('\0');
  _credentials.jwt.fill('\0');

  LOG_PERF(0, "Exit");
}

CloudCredentialtorage &CloudCredentialtorage::getInstance() {
  static CloudCredentialtorage instance;
  return instance;
}

void CloudCredentialtorage::storeCredentials(const QString &username,
                                             const QString &password,
                                             const QString &jwt,
                                             qint64 expirationTime,
                                             bool remember) {
  LOG_PERF(0, "Enter");
  LOG_INFO("storeCredentials called: username='{}', remember={}", username,
           remember);

  LOCKGUARD(_mutex);

  _credentials.username = username;
  _credentials.password = password;
  _credentials.jwt = jwt;
  _credentials.createdTime = QDateTime::currentMSecsSinceEpoch();
  _credentials.expirationTime = expirationTime;
  _credentials.rememberCredentials = remember;

  LOG_INFO("Added credentials to in-memory storage");

  if (remember) {
    LOG_INFO("Calling saveCredentialsToDisk...");
    saveCredentialsToDisk(_credentials);
    LOG_INFO("saveCredentialsToDisk completed");
  } else {
    LOG_INFO("Not saving to disk (remember=false)");
  }

  LOG_PERF(0, "Exit");
}

bool CloudCredentialtorage::getCredentials(QString &username, QString &password,
                                           QString &jwt) {
  LOG_PERF(0, "Enter");
  LOG_INFO("getCredentials called");

  LOCKGUARD(_mutex);

  if (_credentials.username.isEmpty()) {
    LOG_INFO("No credentials found in storage");
    LOG_PERF(0, "Exit");
    return false;
  }

  username = _credentials.username;
  password = _credentials.password;
  jwt = _credentials.jwt;

  LOG_INFO("Retrieved credentials, username='{}'", username);
  LOG_PERF(0, "Exit");
  return true;
}

bool CloudCredentialtorage::getJWT(QString &jwt) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  if (_credentials.jwt.isEmpty()) {
    LOG_INFO("No JWT found in storage");
    LOG_PERF(0, "Exit");
    return false;
  }

  jwt = _credentials.jwt;
  LOG_PERF(0, "Exit");
  return true;
}

bool CloudCredentialtorage::hasCredentials() const {
  LOCKGUARD(_mutex);
  return !_credentials.username.isEmpty();
}

bool CloudCredentialtorage::isJWTValid() const {
  LOCKGUARD(_mutex);

  if (_credentials.jwt.isEmpty()) {
    return false;
  }

  qint64 now = QDateTime::currentMSecsSinceEpoch();
  return _credentials.expirationTime > now;
}

void CloudCredentialtorage::removeCredentials() {
  LOG_PERF(0, "Enter");
  LOG_INFO("Removing credentials");

  LOCKGUARD(_mutex);

  // Clear sensitive data
  _credentials.username.fill('\0');
  _credentials.password.fill('\0');
  _credentials.jwt.fill('\0');
  _credentials.username.clear();
  _credentials.password.clear();
  _credentials.jwt.clear();
  _credentials.createdTime = 0;
  _credentials.expirationTime = 0;

  removeCredentialsFromDisk();

  LOG_PERF(0, "Exit");
}

void CloudCredentialtorage::clearMemoryCredentials() {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing all in-memory credentials");

  LOCKGUARD(_mutex);

  _credentials.username.fill('\0');
  _credentials.password.fill('\0');
  _credentials.jwt.fill('\0');
  _credentials.username.clear();
  _credentials.password.clear();
  _credentials.jwt.clear();

  // Reload persisted credentials
  loadPersistedCredentials();

  LOG_PERF(0, "Exit");
}

void CloudCredentialtorage::clearAll() {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing all credentials including persisted");

  LOCKGUARD(_mutex);

  _credentials.username.fill('\0');
  _credentials.password.fill('\0');
  _credentials.jwt.fill('\0');
  _credentials.username.clear();
  _credentials.password.clear();
  _credentials.jwt.clear();

  // Remove credentials file
  QString credFile = getCredentialsFilePath();
  if (QFile::exists(credFile)) {
    QFile::remove(credFile);
    LOG_INFO("Deleted credentials file: {}", credFile);
  }

  LOG_PERF(0, "Exit");
}

bool CloudCredentialtorage::getCredentialsInfo(QString &username,
                                               qint64 &createdTime,
                                               qint64 &expirationTime) const {
  LOCKGUARD(_mutex);

  if (_credentials.username.isEmpty()) {
    return false;
  }

  username = _credentials.username;
  createdTime = _credentials.createdTime;
  expirationTime = _credentials.expirationTime;
  return true;
}

bool CloudCredentialtorage::areCredentialsExpired() const {
  LOCKGUARD(_mutex);

  if (_credentials.username.isEmpty()) {
    LOG_INFO("areCredentialsExpired: No credentials found");
    return true;
  }

  qint64 now = QDateTime::currentMSecsSinceEpoch();
  bool expired = _credentials.expirationTime < now;

  LOG_INFO(
      "areCredentialsExpired: created={}, now={}, expiration={}, expired={}",
      _credentials.createdTime, now, _credentials.expirationTime, expired);

  return expired;
}

void CloudCredentialtorage::loadPersistedCredentials() {
  LOG_PERF(0, "Enter");
  LOG_INFO("loadPersistedCredentials called");

  QString credFile = getCredentialsFilePath();
  LOG_INFO("Loading credentials from: {}", credFile);

  if (!QFile::exists(credFile)) {
    LOG_INFO("Credentials file does not exist");
    _credentialsLoaded = true;
    LOG_PERF(0, "Exit");
    return;
  }

  QFile file(credFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR(Log::ErrorId::Security,
              "Failed to open credentials file for reading");
    _credentialsLoaded = true;
    LOG_PERF(0, "Exit");
    return;
  }

  QByteArray encryptedData = file.readAll();
  file.close();

  if (encryptedData.isEmpty()) {
    LOG_INFO("Credentials file is empty");
    _credentialsLoaded = true;
    LOG_PERF(0, "Exit");
    return;
  }

  // Extract IV (first 16 bytes)
  if (encryptedData.size() < AES_IV_SIZE) {
    LOG_ERROR(Log::ErrorId::Security,
              "Encrypted data is too small to contain IV");
    _credentialsLoaded = true;
    LOG_PERF(0, "Exit");
    return;
  }

  const QByteArray decrypted = decrypt(encryptedData, _masterKey);

  if (decrypted.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to decrypt credentials");
    _credentialsLoaded = true;
    LOG_PERF(0, "Exit");
    return;
  }

  // Parse JSON
  const QJsonDocument doc = QJsonDocument::fromJson(decrypted);
  if (!doc.isObject()) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to parse credentials JSON");
    _credentialsLoaded = true;
    LOG_PERF(0, "Exit");
    return;
  }

  const QJsonObject obj = doc.object();
  _credentials.username = obj["username"].toString();
  _credentials.password = obj["password"].toString();
  _credentials.jwt = obj["jwt"].toString();
  _credentials.createdTime = obj["createdTime"].toVariant().toLongLong();
  _credentials.expirationTime = obj["expirationTime"].toVariant().toLongLong();
  _credentials.rememberCredentials = obj["rememberCredentials"].toBool(true);

  LOG_INFO("Loaded credentials for user: {}", _credentials.username);
  _credentialsLoaded = true;

  LOG_PERF(0, "Exit");
}

void CloudCredentialtorage::saveCredentialsToDisk(
    const CloudCredential &credentials) {
  LOG_PERF(0, "Enter");
  LOG_INFO("saveCredentialsToDisk called for user: {}", credentials.username);

  const QString credFile = getCredentialsFilePath();
  LOG_INFO("Saving credentials to: {}", credFile);

  // Create directory if it doesn't exist
  const QDir dir(QFileInfo(credFile).absolutePath());
  if (!dir.exists()) {
    dir.mkpath(".");
    LOG_INFO("Created credentials directory");
  }

  // Create JSON object
  QJsonObject obj;
  obj["username"] = credentials.username;
  obj["password"] = credentials.password;
  obj["jwt"] = credentials.jwt;
  obj["createdTime"] = static_cast<qint64>(credentials.createdTime);
  obj["expirationTime"] = static_cast<qint64>(credentials.expirationTime);
  obj["rememberCredentials"] = credentials.rememberCredentials;

  const QJsonDocument doc(obj);
  const QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

  // Encrypt and save
  const QByteArray encryptedData = encrypt(jsonData, _masterKey);

  QFile file(credFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(Log::ErrorId::Security,
              "Failed to open credentials file for writing");
    LOG_PERF(0, "Exit");
    return;
  }

  file.write(encryptedData);
  file.close();

  LOG_INFO("Credentials saved successfully");
  LOG_PERF(0, "Exit");
}

void CloudCredentialtorage::removeCredentialsFromDisk() {
  LOG_PERF(0, "Enter");

  QString credFile = getCredentialsFilePath();
  if (QFile::exists(credFile)) {
    if (QFile::remove(credFile)) {
      LOG_INFO("Removed credentials file: {}", credFile);
    } else {
      LOG_ERROR(Log::ErrorId::Security, "Failed to remove credentials file: {}",
                credFile);
    }
  }

  LOG_PERF(0, "Exit");
}

QByteArray CloudCredentialtorage::encrypt(const QByteArray &plaintext,
                                          const QByteArray &key) {
  unsigned char iv[AES_IV_SIZE];
  if (RAND_bytes(iv, AES_IV_SIZE) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to generate random IV");
    return QByteArray();
  }

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to create EVP context");
    return QByteArray();
  }

  QByteArray ciphertext;
  ciphertext.resize(plaintext.size() + EVP_MAX_BLOCK_LENGTH);
  auto *ciphertextData = reinterpret_cast<unsigned char *>(ciphertext.data());
  int len = 0;
  int ciphertext_len = 0;

  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                         reinterpret_cast<const unsigned char *>(key.data()),
                         iv) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "EVP_EncryptInit_ex failed");
    EVP_CIPHER_CTX_free(ctx);
    return QByteArray();
  }

  if (EVP_EncryptUpdate(
          ctx, ciphertextData, &len,
          reinterpret_cast<const unsigned char *>(plaintext.data()),
          plaintext.size()) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "EVP_EncryptUpdate failed");
    EVP_CIPHER_CTX_free(ctx);
    return QByteArray();
  }
  ciphertext_len = len;

  if (EVP_EncryptFinal_ex(ctx, ciphertextData + len, &len) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "EVP_EncryptFinal_ex failed");
    EVP_CIPHER_CTX_free(ctx);
    return QByteArray();
  }
  ciphertext_len += len;

  EVP_CIPHER_CTX_free(ctx);

  // Prepend IV to ciphertext
  QByteArray result(reinterpret_cast<const char *>(iv), AES_IV_SIZE);
  result.append(ciphertext.constData(), ciphertext_len);

  return result;
}

QByteArray CloudCredentialtorage::decrypt(const QByteArray &ciphertext,
                                          const QByteArray &key) {
  if (ciphertext.size() < AES_IV_SIZE) {
    LOG_ERROR(Log::ErrorId::Security, "Ciphertext too short");
    return QByteArray();
  }

  const unsigned char *iv =
      reinterpret_cast<const unsigned char *>(ciphertext.data());
  const unsigned char *data =
      reinterpret_cast<const unsigned char *>(ciphertext.data() + AES_IV_SIZE);
  int data_len = ciphertext.size() - AES_IV_SIZE;

  EVP_CIPHER_CTX *ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to create EVP context");
    return QByteArray();
  }

  QByteArray plaintext;
  plaintext.resize(data_len + EVP_MAX_BLOCK_LENGTH);
  auto *plaintextData = reinterpret_cast<unsigned char *>(plaintext.data());
  int len = 0;
  int plaintext_len = 0;

  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), nullptr,
                         reinterpret_cast<const unsigned char *>(key.data()),
                         iv) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "EVP_DecryptInit_ex failed");
    EVP_CIPHER_CTX_free(ctx);
    return QByteArray();
  }

  if (EVP_DecryptUpdate(ctx, plaintextData, &len, data, data_len) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "EVP_DecryptUpdate failed");
    EVP_CIPHER_CTX_free(ctx);
    return QByteArray();
  }
  plaintext_len = len;

  if (EVP_DecryptFinal_ex(ctx, plaintextData + len, &len) != 1) {
    LOG_ERROR(Log::ErrorId::Security, "EVP_DecryptFinal_ex failed");
    EVP_CIPHER_CTX_free(ctx);
    return QByteArray();
  }
  plaintext_len += len;

  EVP_CIPHER_CTX_free(ctx);

  return plaintext.left(plaintext_len);
}

QByteArray CloudCredentialtorage::getEncryptionKey() {
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

QString CloudCredentialtorage::getCredentialsFilePath() const {
  QString appDataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return appDataPath + "/cloud_credentials.dat";
}

} // namespace Util
