#include "GitKeyStorage.h"

#include <Log.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include <QByteArray>
#include <QDataStream>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QtCrypto>
#include <qplugin.h>

#define THREAD_ID "GitKeyStorage"

Q_IMPORT_PLUGIN(opensslPlugin)

namespace GitRepo {

GitKeyStorage::GitKeyStorage() {
  LOG_PERF(0, "Enter");

  // Store key mappings in application config directory
  const QString configPath =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  QDir().mkpath(configPath);
  _storageFilePath = configPath + "/git_key_mappings.dat";

  loadKeyPairs();

  LOG_PERF(0, "Exit");
}

GitKeyStorage::~GitKeyStorage() {
  LOG_PERF(0, "Enter");
  saveKeyPairs();
  LOG_PERF(0, "Exit");
}

GitKeyStorage &GitKeyStorage::getInstance() {
  static GitKeyStorage instance;
  return instance;
}

QString GitKeyStorage::getSshDirectoryPath() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  const QString homePath =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  return homePath + "/.ssh";
}

QVector<GitKeyPair> GitKeyStorage::getAllKeyPairs() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  LOCKGUARD(_mutex);
  return _keyPairs.values().toVector();
}

bool GitKeyStorage::getKeyPairForHost(const QString &host,
                                      GitKeyPair &keyPair) const {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  if (!_keyPairs.contains(host)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  keyPair = _keyPairs[host];

  LOG_PERF(0, "Exit");
  return true;
}

bool GitKeyStorage::setKeyPair(const QString &host, const QString &keyName,
                               const QString &privateKeyPath,
                               const QString &publicKeyPath) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  LOG_INFO("Setting key pair for host '{}': {}", host, keyName);

  // Verify that the key files exist
  if (!QFile::exists(privateKeyPath)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Private key file not found: {}",
              privateKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!QFile::exists(publicKeyPath)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Public key file not found: {}",
              publicKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  GitKeyPair keyPair;
  keyPair.host = host;
  keyPair.keyName = keyName;
  keyPair.privateKeyPath = privateKeyPath;
  keyPair.publicKeyPath = publicKeyPath;
  keyPair.createdTime = QDateTime::currentMSecsSinceEpoch();

  _keyPairs[host] = keyPair;
  saveKeyPairs();

  LOG_INFO("Key pair set successfully for host '{}'", host);

  LOG_PERF(0, "Exit");
  return true;
}

bool GitKeyStorage::removeKeyPair(const QString &host) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  LOG_INFO("Removing key pair for host '{}'", host);

  if (!_keyPairs.contains(host)) {
    LOG_WARN("No key pair found for host '{}'", host);
    LOG_PERF(0, "Exit");
    return false;
  }

  _keyPairs.remove(host);
  saveKeyPairs();

  LOG_INFO("Key pair removed successfully for host '{}'", host);

  LOG_PERF(0, "Exit");
  return true;
}

bool GitKeyStorage::keyPairExists(const QString &keyName) const {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  const QString sshDir = getSshDirectoryPath();
  const QString privateKeyPath = sshDir + "/" + keyName;
  const QString publicKeyPath = privateKeyPath + ".pub";

  LOG_PERF(0, "Exit");

  return QFile::exists(privateKeyPath) && QFile::exists(publicKeyPath);
}

// Helper: write length-prefixed binary string
static void writeString(QByteArray &buf, const QByteArray &data) {
  quint32 len = data.size();
  buf.append(char((len >> 24) & 0xFF));
  buf.append(char((len >> 16) & 0xFF));
  buf.append(char((len >> 8) & 0xFF));
  buf.append(char(len & 0xFF));
  buf.append(data);
}

// Convert QCA PublicKey (RSA) to OpenSSH wire format
QString toOpenSSH(const QCA::PublicKey &pub,
                  const QString &comment = QStringLiteral("qt@host")) {
  LOG_PERF(0, "Enter");

  if (pub.isNull() || !pub.isRSA()) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  const QCA::RSAPublicKey rsa = pub.toRSA();

  QByteArray blob;
  const QByteArray alg("ssh-rsa");
  writeString(blob, alg);

  const QByteArray e = rsa.e().toArray().toByteArray();
  const QByteArray n = rsa.n().toArray().toByteArray();
  writeString(blob, e);
  writeString(blob, n);

  const QByteArray b64 = blob.toBase64();

  LOG_PERF(0, "Exit");

  return QStringLiteral("ssh-rsa %1 %2").arg(QString::fromLatin1(b64), comment);
}

bool GitKeyStorage::generateKeyPair(const QString &keyName,
                                    const QString &passphrase,
                                    QString &privateKeyPath,
                                    QString &publicKeyPath,
                                    QString &errorMessage) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Generating new SSH key pair: {}", keyName);
  // Ensure .ssh directory exists
  const QString sshDir = getSshDirectoryPath();
  QDir dir;
  if (!dir.mkpath(sshDir)) {
    errorMessage = QString("Failed to create .ssh directory: %1").arg(sshDir);
    LOG_ERROR(Log::ErrorId::FileSystem, "{}", errorMessage);
    LOG_PERF(0, "Exit");
    return false;
  }

  privateKeyPath = sshDir + "/" + keyName;
  publicKeyPath = privateKeyPath + ".pub";

  // Check if key already exists
  if (QFile::exists(privateKeyPath) || QFile::exists(publicKeyPath)) {
    errorMessage = QString("Key pair already exists: %1").arg(privateKeyPath);
    LOG_ERROR(Log::ErrorId::FileSystem, "{}", errorMessage);
    LOG_PERF(0, "Exit");
    return false;
  }

  QCA::Initializer init;

  LOG_INFO("QCA plugin paths: [{}]", QCA::pluginPaths().join(','));
  LOG_INFO("QCA supported features: [{}]", QCA::supportedFeatures().join(','));

  qDebug() << "Plugin paths: " << QCA::pluginPaths();
  qDebug() << "Supported features: " << QCA::supportedFeatures();

  // Generate RSA 4096-bit key pair
  QCA::PrivateKey priv = QCA::KeyGenerator().createRSA(4096);
  if (priv.isNull()) {
    errorMessage = QString("Failed to load SSL plugin");
    LOG_ERROR(Log::ErrorId::FileSystem, "Key generation failed!");
    LOG_PERF(0, "Exit");
    return false;
  }

  QCA::PublicKey pub = priv.toPublicKey();

  // Save private key in PEM
  QFile privFile(privateKeyPath);
  if (!privFile.open(QIODevice::WriteOnly)) {
    errorMessage = QString("Failed to open private key file for writing.");
    LOG_ERROR(Log::ErrorId::FileSystem, "Writing private key to file failed!");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (passphrase.isEmpty()) {
    privFile.write(priv.toPEM().toUtf8());
  } else {
    QCA::ConvertResult result;
    const QCA::SecureArray saPassphrase(passphrase.toUtf8());
    privFile.write(
        priv.toPEM(saPassphrase, QCA::PBEAlgorithm::PBES2_TripleDES_SHA1)
            .toUtf8());
  }

  privFile.close();

  // Save public key in OpenSSH format
  QFile pubFile(publicKeyPath);
  if (!pubFile.open(QIODevice::WriteOnly)) {
    errorMessage = QString("Failed to open public key file for writing.");
    LOG_ERROR(Log::ErrorId::FileSystem, "Writing public key to file failed!");
    LOG_PERF(0, "Exit");
    return false;
  }

  QString pubOpenSSH = toOpenSSH(pub);
  pubFile.write(pubOpenSSH.toUtf8());
  pubFile.close();

  // Set private key file permissions (600 - owner read/write only)
#ifndef Q_OS_WIN
  QFile privateKeyFile(privateKeyPath);
  privateKeyFile.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
#endif

  LOG_INFO("SSH key pair generated successfully: {}", keyName);
  LOG_PERF(0, "Exit");

  return true;
}

void GitKeyStorage::loadKeyPairs() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  LOG_INFO("Loading key pair mappings from: {}", _storageFilePath);

  QFile file(_storageFilePath);
  if (!file.exists()) {
    LOG_INFO("No existing key pair mappings file found");
    LOG_PERF(0, "Exit");
    return;
  }

  if (!file.open(QIODevice::ReadOnly)) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to open key pair mappings file for reading: {}",
              _storageFilePath);
    LOG_PERF(0, "Exit");
    return;
  }

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);

  // Read version
  quint32 version;
  stream >> version;

  if (version != 1) {
    LOG_ERROR(Log::ErrorId::Security,
              "Unsupported key pair mappings file version: {}", version);
    LOG_PERF(0, "Exit");
    return;
  }

  // Read number of entries
  quint32 count;
  stream >> count;

  LOG_INFO("Loading {} key pair mappings", count);

  // Read entries
  for (quint32 i = 0; i < count; ++i) {
    GitKeyPair keyPair;
    stream >> keyPair.host >> keyPair.keyName >> keyPair.privateKeyPath >>
        keyPair.publicKeyPath >> keyPair.createdTime;

    // Verify that the key files still exist
    if (QFile::exists(keyPair.privateKeyPath) &&
        QFile::exists(keyPair.publicKeyPath)) {
      _keyPairs[keyPair.host] = keyPair;
    } else {
      LOG_WARN("Key pair for host '{}' no longer exists, skipping",
               keyPair.host);
    }
  }

  LOG_INFO("Loaded {} key pair mappings successfully", _keyPairs.size());

  LOG_PERF(0, "Exit");
}

void GitKeyStorage::saveKeyPairs() {
  LOG_PERF(0, "Enter");

  // Mutex should already be locked by caller

  LOG_INFO("Saving {} key pair mappings to: {}", _keyPairs.size(),
           _storageFilePath);

  QFile file(_storageFilePath);
  if (!file.open(QIODevice::WriteOnly)) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to open key pair mappings file for writing: {}",
              _storageFilePath);
    LOG_PERF(0, "Exit");
    return;
  }

  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);

  // Write version
  stream << static_cast<quint32>(1);

  // Write number of entries
  stream << static_cast<quint32>(_keyPairs.size());

  // Write entries
  for (const GitKeyPair &keyPair : _keyPairs) {
    stream << keyPair.host << keyPair.keyName << keyPair.privateKeyPath
           << keyPair.publicKeyPath << keyPair.createdTime;
  }

  LOG_INFO("Key pair mappings saved successfully");

  LOG_PERF(0, "Exit");
}

} // namespace GitRepo
