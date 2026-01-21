#pragma once

#include <LockGuard.h>
#include <QByteArray>
#include <QMap>
#include <QString>

namespace GitRepo {

/**
 * @brief Encrypted credential storage for Git repositories
 *
 * Stores credentials per repository path with AES-256 encryption.
 * Credentials are persisted to secure storage and loaded on demand.
 */
class GitCredentialStorage {
public:
  struct Credentials {
    QString username;
    QString password;
    qint64 createdTime;
    bool rememberCredentials; // Whether to persist credentials
  };

  /**
   * @brief Get the singleton instance
   */
  static GitCredentialStorage &getInstance();

  /**
   * @brief Store credentials for a repository
   * @param repoPath The absolute path to the repository
   * @param username The username
   * @param password The password (will be encrypted)
   * @param remember Whether to persist credentials to disk
   */
  void storeCredentials(const QString &repoPath, const QString &username,
                        const QString &password, bool remember = true);

  /**
   * @brief Retrieve credentials for a repository
   * @param repoPath The absolute path to the repository
   * @param username Output parameter for username
   * @param password Output parameter for password (decrypted)
   * @return true if credentials exist and are valid, false otherwise
   */
  bool getCredentials(const QString &repoPath, QString &username,
                      QString &password);

  /**
   * @brief Check if credentials exist for a repository
   * @param repoPath The absolute path to the repository
   * @return true if credentials are stored
   */
  bool hasCredentials(const QString &repoPath) const;

  /**
   * @brief Remove credentials for a repository
   * @param repoPath The absolute path to the repository
   */
  void removeCredentials(const QString &repoPath);

  /**
   * @brief Clear all in-memory credentials
   * Persisted credentials remain on disk
   */
  void clearMemoryCredentials();

  /**
   * @brief Clear all credentials including persisted ones
   */
  void clearAll();

  /**
   * @brief Check if credentials have expired
   * @param repoPath The absolute path to the repository
   * @param expirationTimeMs Expiration time in milliseconds
   * @return true if expired or don't exist
   */
  bool areCredentialsExpired(const QString &repoPath,
                             qint64 expirationTimeMs) const;

private:
  GitCredentialStorage();
  ~GitCredentialStorage();

  // Prevent copying
  GitCredentialStorage(const GitCredentialStorage &) = delete;
  GitCredentialStorage &operator=(const GitCredentialStorage &) = delete;

  /**
   * @brief Load persisted credentials from disk
   */
  void loadPersistedCredentials();

  /**
   * @brief Save credentials to disk (encrypted)
   * @param repoPath The repository path
   * @param credentials The credentials to save
   */
  void saveCredentialsToDisk(const QString &repoPath,
                             const Credentials &credentials);

  /**
   * @brief Remove credentials from disk
   * @param repoPath The repository path
   */
  void removeCredentialsFromDisk(const QString &repoPath);

  /**
   * @brief Encrypt data using AES-256
   * @param plaintext The data to encrypt
   * @param key The encryption key
   * @return Encrypted data
   */
  QByteArray encrypt(const QByteArray &plaintext, const QByteArray &key);

  /**
   * @brief Decrypt data using AES-256
   * @param ciphertext The encrypted data
   * @param key The encryption key
   * @return Decrypted data
   */
  QByteArray decrypt(const QByteArray &ciphertext, const QByteArray &key);

  /**
   * @brief Get or generate encryption key
   * @return The encryption key
   */
  QByteArray getEncryptionKey();

  /**
   * @brief Get the credentials file path
   * @return Path to credentials file
   */
  QString getCredentialsFilePath() const;

  /**
   * @brief Derive repository-specific key
   * @param repoPath The repository path
   * @return Repository-specific encryption key
   */
  QByteArray deriveRepoKey(const QString &repoPath);

private:
  mutable Util::Mutex _mutex;
  QMap<QString, Credentials> _credentials; // repoPath -> Credentials
  QByteArray _masterKey;
};

} // namespace GitRepo
