// MIT License
// Copyright (c) 2026 Sourcerer

#include "TokensStorage.h"

#include <Log.h>

#include <qcoreapplication.h>
#include <qtkeychain/keychain.h>

#include <QCryptographicHash>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QRandomGenerator>
#include <QStandardPaths>

#include <openssl/aes.h>
#include <openssl/evp.h>
#include <openssl/rand.h>

#include <memory>
#include <stdexcept>

#define THREAD_ID "IssueTrackers::TokensStorage"

namespace IssueTrackers {
namespace {

constexpr int AES_KEY_SIZE = 32;
constexpr int AES_IV_SIZE = 16;
constexpr int PBKDF2_ITERATIONS = 100000;
constexpr int STORAGE_VERSION = 3;

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

QString safeKeyPart(const QString &value) {
  return value.trimmed().toLower();
}

} // namespace

TokensStorage::TokensStorage() {
  LOG_PERF(0, "Enter");

  try {
    _masterKey = getEncryptionKey();
    LOG_INFO("Master key loaded succesfully!");
    loadPersistedTokens();
    LOG_INFO("Persisted toekens loaded succesfully!");
  } catch (const std::exception &ex) {
    LOG_ERROR(Log::ErrorId::Security, "Exception: {}", ex.what());
  }

  LOG_PERF(0, "Exit");
}

TokensStorage::~TokensStorage() {
  LOG_PERF(0, "Enter");

  _masterKey.fill(0);
  for (auto &token : _tokens) {
    token.serviceType.fill(QChar(u'\0'));
    token.repositoryName.fill(QChar(u'\0'));
    token.host.fill(QChar(u'\0'));
    token.userName.fill(QChar(u'\0'));
    token.accessToken.fill(QChar(u'\0'));
  }
  _tokens.clear();

  LOG_PERF(0, "Exit");
}

TokensStorage &TokensStorage::getInstance() {
  static TokensStorage instance;
  return instance;
}

QString TokensStorage::makeEntryKey(const QString &serviceType,
                                    const QString &repositoryName) {
  return safeKeyPart(serviceType) + QStringLiteral("::") +
         safeKeyPart(repositoryName);
}

void TokensStorage::storeToken(const QString &serviceType,
                               const QString &repositoryName,
                               const QString &accessToken,
                               const QDateTime &expiresAt, bool remember) {
  storeToken(serviceType, repositoryName, QString(), accessToken, expiresAt,
             remember);
}

void TokensStorage::storeToken(const QString &serviceType,
                               const QString &repositoryName,
                               const QString &userName,
                               const QString &accessToken,
                               const QDateTime &expiresAt, bool remember) {
  LOG_PERF(0, "Enter");

  const QString entryKey = makeEntryKey(serviceType, repositoryName);

  LOCKGUARD(_mutex);

  TokenEntry entry;
  entry.serviceType = serviceType.trimmed();
  entry.repositoryName = repositoryName.trimmed();
  entry.userName = userName.trimmed();
  entry.accessToken = accessToken;
  entry.expiresAt = expiresAt;
  entry.createdTime = QDateTime::currentMSecsSinceEpoch();
  entry.rememberToken = remember;

  _tokens[entryKey] = entry;

  if (remember) {
    saveTokensToDisk(entryKey, entry);
  }

  LOG_PERF(0, "Exit");
}

void TokensStorage::storeTokenWithHost(const QString &serviceType,
                                       const QString &repositoryName,
                                       const QString &host,
                                       const QString &accessToken,
                                       const QDateTime &expiresAt,
                                       bool remember) {
  LOG_PERF(0, "Enter");

  const QString entryKey = makeEntryKey(serviceType, repositoryName);

  LOCKGUARD(_mutex);

  TokenEntry entry;
  entry.serviceType = serviceType.trimmed();
  entry.repositoryName = repositoryName.trimmed();
  entry.host = host.trimmed();
  entry.accessToken = accessToken;
  entry.expiresAt = expiresAt;
  entry.createdTime = QDateTime::currentMSecsSinceEpoch();
  entry.rememberToken = remember;

  _tokens[entryKey] = entry;

  if (remember) {
    saveTokensToDisk(entryKey, entry);
  }

  LOG_PERF(0, "Exit");
}

bool TokensStorage::getToken(const QString &serviceType,
                             const QString &repositoryName,
                             QString &accessToken, QDateTime &expiresAt) const {
  QString userName;
  return getToken(serviceType, repositoryName, userName, accessToken,
                  expiresAt);
}

bool TokensStorage::getToken(const QString &serviceType,
                             const QString &repositoryName, QString &userName,
                             QString &accessToken, QDateTime &expiresAt) const {
  LOG_PERF(0, "Enter");

  const QString entryKey = makeEntryKey(serviceType, repositoryName);

  LOCKGUARD(_mutex);

  if (!_tokens.contains(entryKey)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const TokenEntry &entry = _tokens[entryKey];
  userName = entry.userName;
  accessToken = entry.accessToken;
  expiresAt = entry.expiresAt;

  LOG_PERF(0, "Exit");
  return true;
}

bool TokensStorage::getTokenWithHost(const QString &serviceType,
                                     const QString &repositoryName,
                                     QString &host, QString &accessToken,
                                     QDateTime &expiresAt) const {
  LOG_PERF(0, "Enter");

  const QString entryKey = makeEntryKey(serviceType, repositoryName);

  LOCKGUARD(_mutex);

  if (!_tokens.contains(entryKey)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const TokenEntry &entry = _tokens[entryKey];
  host = entry.host;
  accessToken = entry.accessToken;
  expiresAt = entry.expiresAt;

  LOG_PERF(0, "Exit");
  return true;
}

bool TokensStorage::hasToken(const QString &serviceType,
                             const QString &repositoryName) const {
  LOCKGUARD(_mutex);
  return _tokens.contains(makeEntryKey(serviceType, repositoryName));
}

void TokensStorage::removeToken(const QString &serviceType,
                                const QString &repositoryName) {
  LOG_PERF(0, "Enter");

  const QString entryKey = makeEntryKey(serviceType, repositoryName);

  LOCKGUARD(_mutex);

  if (_tokens.contains(entryKey)) {
    _tokens[entryKey].serviceType.fill(QChar(u'\0'));
    _tokens[entryKey].repositoryName.fill(QChar(u'\0'));
    _tokens[entryKey].host.fill(QChar(u'\0'));
    _tokens[entryKey].userName.fill(QChar(u'\0'));
    _tokens[entryKey].accessToken.fill(QChar(u'\0'));
    _tokens.remove(entryKey);
  }

  removeTokenFromDisk(entryKey);

  LOG_PERF(0, "Exit");
}

void TokensStorage::clearMemoryTokens() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  for (auto &token : _tokens) {
    token.serviceType.fill(QChar(u'\0'));
    token.repositoryName.fill(QChar(u'\0'));
    token.host.fill(QChar(u'\0'));
    token.userName.fill(QChar(u'\0'));
    token.accessToken.fill(QChar(u'\0'));
  }
  _tokens.clear();

  loadPersistedTokens();

  LOG_PERF(0, "Exit");
}

void TokensStorage::clearAll() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  for (auto &token : _tokens) {
    token.serviceType.fill(QChar(u'\0'));
    token.repositoryName.fill(QChar(u'\0'));
    token.host.fill(QChar(u'\0'));
    token.userName.fill(QChar(u'\0'));
    token.accessToken.fill(QChar(u'\0'));
  }
  _tokens.clear();

  const QString tokensFile = getTokensFilePath();
  QFile::remove(tokensFile);

  LOG_PERF(0, "Exit");
}

QList<TokensStorage::TokenEntry> TokensStorage::getAllTokenEntries() const {
  LOCKGUARD(_mutex);
  return _tokens.values();
}

void TokensStorage::loadPersistedTokens() {
  LOG_PERF(0, "Enter");

  const QString tokensFile = getTokensFilePath();
  if (!QFile::exists(tokensFile)) {
    LOG_PERF(0, "Exit");
    return;
  }

  QFile file(tokensFile);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Failed to open token storage file: {}",
              file.errorString());
    LOG_PERF(0, "Exit");
    return;
  }

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);

  int version = 0;
  stream >> version;
  if (version < 1 || version > STORAGE_VERSION) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Unsupported token storage file version: {}", version);
    LOG_PERF(0, "Exit");
    return;
  }

  qsizetype count = 0;
  stream >> count;

  for (qsizetype i = 0; i < count; ++i) {
    QString entryKey;
    QByteArray encryptedData;
    stream >> entryKey >> encryptedData;

    try {
      const QByteArray decryptedData =
          decrypt(encryptedData,
                  deriveEntryKey(entryKey.section(QStringLiteral("::"), 0, 0),
                                 entryKey.section(QStringLiteral("::"), 1, 1)));
      QDataStream tokenStream(decryptedData);
      tokenStream.setVersion(QDataStream::Qt_6_0);

      TokenEntry entry;
      if (version >= 3) {
        tokenStream >> entry.serviceType >> entry.repositoryName >>
            entry.host >> entry.userName >> entry.accessToken >>
            entry.expiresAt >> entry.createdTime;
      } else if (version >= 2) {
        tokenStream >> entry.serviceType >> entry.repositoryName >>
            entry.userName >> entry.accessToken >> entry.expiresAt >>
            entry.createdTime;
      } else {
        tokenStream >> entry.serviceType >> entry.repositoryName >>
            entry.accessToken >> entry.expiresAt >> entry.createdTime;
      }
      entry.rememberToken = true;
      _tokens[entryKey] = entry;
    } catch (const std::exception &e) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Failed to decrypt token entry '{}': {}", entryKey, e.what());
    }
  }

  file.close();
  LOG_PERF(0, "Exit");
}

void TokensStorage::saveTokensToDisk(const QString &entryKey,
                                     const TokenEntry &tokenEntry) {
  LOG_PERF(0, "Enter");

  const QString tokensFile = getTokensFilePath();
  QFileInfo fileInfo(tokensFile);
  QDir dir = fileInfo.absoluteDir();
  if (!dir.exists()) {
    dir.mkpath(QStringLiteral("."));
  }

  QMap<QString, QByteArray> encryptedTokens;
  if (QFile::exists(tokensFile)) {
    QFile file(tokensFile);
    if (file.open(QIODevice::ReadOnly)) {
      QDataStream stream(&file);
      stream.setVersion(QDataStream::Qt_6_0);

      int version = 0;
      stream >> version;
      if (version >= 1 && version <= STORAGE_VERSION) {
        qsizetype count = 0;
        stream >> count;
        for (qsizetype i = 0; i < count; ++i) {
          QString key;
          QByteArray data;
          stream >> key >> data;
          encryptedTokens[key] = data;
        }
      }
      file.close();
    }
  }

  QByteArray tokenData;
  QDataStream tokenStream(&tokenData, QIODevice::WriteOnly);
  tokenStream.setVersion(QDataStream::Qt_6_0);
  tokenStream << tokenEntry.serviceType << tokenEntry.repositoryName
              << tokenEntry.host << tokenEntry.userName
              << tokenEntry.accessToken << tokenEntry.expiresAt
              << tokenEntry.createdTime;

  encryptedTokens[entryKey] =
      encrypt(tokenData, deriveEntryKey(tokenEntry.serviceType,
                                        tokenEntry.repositoryName));

  QFile file(tokensFile);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to open token storage file for writing: {}",
              file.errorString());
    LOG_PERF(0, "Exit");
    return;
  }

  file.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser);
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);

  stream << STORAGE_VERSION;
  stream << static_cast<qsizetype>(encryptedTokens.size());
  for (auto it = encryptedTokens.begin(); it != encryptedTokens.end(); ++it) {
    stream << it.key() << it.value();
  }

  file.close();
  LOG_PERF(0, "Exit");
}

void TokensStorage::removeTokenFromDisk(const QString &entryKey) {
  LOG_PERF(0, "Enter");

  const QString tokensFile = getTokensFilePath();
  if (!QFile::exists(tokensFile)) {
    LOG_PERF(0, "Exit");
    return;
  }

  QMap<QString, QByteArray> encryptedTokens;
  QFile file(tokensFile);
  if (file.open(QIODevice::ReadOnly)) {
    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);

    int version = 0;
    stream >> version;
    if (version >= 1 && version <= STORAGE_VERSION) {
      qsizetype count = 0;
      stream >> count;
      for (qsizetype i = 0; i < count; ++i) {
        QString key;
        QByteArray data;
        stream >> key >> data;
        if (key != entryKey) {
          encryptedTokens[key] = data;
        }
      }
    }
    file.close();
  }

  if (encryptedTokens.isEmpty()) {
    QFile::remove(tokensFile);
  } else if (file.open(QIODevice::WriteOnly)) {
    file.setPermissions(QFileDevice::ReadUser | QFileDevice::WriteUser);

    QDataStream stream(&file);
    stream.setVersion(QDataStream::Qt_6_0);
    stream << STORAGE_VERSION;
    stream << static_cast<qsizetype>(encryptedTokens.size());
    for (auto it = encryptedTokens.begin(); it != encryptedTokens.end(); ++it) {
      stream << it.key() << it.value();
    }
    file.close();
  }

  LOG_PERF(0, "Exit");
}

QByteArray TokensStorage::encrypt(const QByteArray &plaintext,
                                  const QByteArray &key) {
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

  QByteArray ciphertext;
  ciphertext.resize(plaintext.size() + AES_BLOCK_SIZE);

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

  QByteArray result(reinterpret_cast<const char *>(iv), AES_IV_SIZE);
  result.append(ciphertext);
  return result;
}

QByteArray TokensStorage::decrypt(const QByteArray &ciphertext,
                                  const QByteArray &key) {
  if (ciphertext.size() < AES_IV_SIZE) {
    throw std::runtime_error("Invalid ciphertext: too short");
  }

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

QByteArray TokensStorage::getEncryptionKey() {
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

QString TokensStorage::getTokensFilePath() const {
  const QString dataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return QDir(dataPath).filePath(QStringLiteral("issue_tracker_tokens.dat"));
}

QByteArray TokensStorage::deriveEntryKey(const QString &serviceType,
                                         const QString &repositoryName) {
  const QByteArray salt = QCryptographicHash::hash(
      makeEntryKey(serviceType, repositoryName).toUtf8(),
      QCryptographicHash::Sha256);

  QByteArray derivedKey;
  derivedKey.resize(AES_KEY_SIZE);

  if (PKCS5_PBKDF2_HMAC(
          _masterKey.data(), _masterKey.size(),
          reinterpret_cast<const unsigned char *>(salt.data()), salt.size(),
          PBKDF2_ITERATIONS, EVP_sha256(), AES_KEY_SIZE,
          reinterpret_cast<unsigned char *>(derivedKey.data())) != 1) {
    throw std::runtime_error("Failed to derive token key");
  }

  return derivedKey;
}

} // namespace IssueTrackers
