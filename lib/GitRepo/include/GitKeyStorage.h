#pragma once

#include <LockGuard.h>

#include <QMap>
#include <QMutex>
#include <QString>
#include <QVector>

namespace GitRepo {

struct GitKeyPair {
  QString host;
  QString keyName;
  QString privateKeyPath;
  QString publicKeyPath;
  qint64 createdTime;
};

class GitKeyStorage {
public:
  static GitKeyStorage &getInstance();

  // Get all registered key pairs
  QVector<GitKeyPair> getAllKeyPairs() const;

  // Get key pair for a specific host
  bool getKeyPairForHost(const QString &host, GitKeyPair &keyPair) const;

  // Add or update a key pair for a host
  bool setKeyPair(const QString &host, const QString &keyName,
                  const QString &privateKeyPath, const QString &publicKeyPath);

  // Remove a key pair for a host
  bool removeKeyPair(const QString &host);

  // Generate a new SSH key pair
  bool generateKeyPair(const QString &keyName, const QString &passphrase,
                       QString &privateKeyPath, QString &publicKeyPath,
                       QString &errorMessage);

  // Get the .ssh directory path
  static QString getSshDirectoryPath();

  // Check if a key pair exists
  bool keyPairExists(const QString &keyName) const;

private:
  GitKeyStorage();
  ~GitKeyStorage();

  // Disable copy and assignment
  GitKeyStorage(const GitKeyStorage &) = delete;
  GitKeyStorage &operator=(const GitKeyStorage &) = delete;

  void loadKeyPairs();
  void saveKeyPairs();

  mutable Util::Mutex _mutex;
  QMap<QString, GitKeyPair> _keyPairs; // host -> key pair mapping
  QString _storageFilePath;
};

} // namespace GitRepo
