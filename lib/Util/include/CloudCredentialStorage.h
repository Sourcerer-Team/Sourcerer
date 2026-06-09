#pragma once

#include <LockGuard.h>
#include <QByteArray>
#include <QMap>
#include <QString>

namespace Util {

/**
 * @brief Encrypted credential storage for cloud services
 *
 * Stores cloud credentials including JWT tokens with AES-256 encryption.
 * Credentials are persisted to secure storage and loaded on demand.
 */
class CloudCredentialtorage {
public:
  struct CloudCredential {
    QString username;
    QString password;
    QString jwt; // JWT token returned from cloud service
    qint64 createdTime;
    qint64 expirationTime;    // When JWT expires
    bool rememberCredentials; // Whether to persist credentials
  };

  /**
   * @brief Get the singleton instance
   */
  static CloudCredentialtorage &getInstance();

  /**
   * @brief Store cloud credentials
   * @param username The username
   * @param password The password (will be encrypted)
   * @param jwt The JWT token (will be encrypted)
   * @param expirationTime When the JWT expires (milliseconds since epoch)
   * @param remember Whether to persist credentials to disk
   */
  void storeCredentials(const QString &username, const QString &password,
                        const QString &jwt, qint64 expirationTime,
                        bool remember = true);

  /**
   * @brief Retrieve stored cloud credentials
   * @param username Output parameter for username
   * @param password Output parameter for password (decrypted)
   * @param jwt Output parameter for JWT token (decrypted)
   * @return true if credentials exist and are valid, false otherwise
   */
  bool getCredentials(QString &username, QString &password, QString &jwt);

  /**
   * @brief Get JWT token only
   * @param jwt Output parameter for JWT token (decrypted)
   * @return true if JWT exists, false otherwise
   */
  bool getJWT(QString &jwt);

  /**
   * @brief Check if credentials exist
   * @return true if credentials are stored
   */
  bool hasCredentials() const;

  /**
   * @brief Check if JWT is valid (not expired)
   * @return true if JWT exists and hasn't expired
   */
  bool isJWTValid() const;

  /**
   * @brief Remove all stored credentials
   */
  void removeCredentials();

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
   * @brief Get credentials info without passwords
   * @param username Output parameter for username
   * @param createdTime Output parameter for creation timestamp
   * @param expirationTime Output parameter for JWT expiration timestamp
   * @return true if credentials exist
   */
  bool getCredentialsInfo(QString &username, qint64 &createdTime,
                          qint64 &expirationTime) const;

  /**
   * @brief Check if credentials have expired
   * @return true if expired or don't exist
   */
  bool areCredentialsExpired() const;

private:
  CloudCredentialtorage();
  ~CloudCredentialtorage();

  // Prevent copying
  CloudCredentialtorage(const CloudCredentialtorage &) = delete;
  CloudCredentialtorage &operator=(const CloudCredentialtorage &) = delete;

  /**
   * @brief Load persisted credentials from disk
   */
  void loadPersistedCredentials();

  /**
   * @brief Save credentials to disk (encrypted)
   * @param credentials The credentials to save
   */
  void saveCredentialsToDisk(const CloudCredential &credentials);

  /**
   * @brief Remove credentials from disk
   */
  void removeCredentialsFromDisk();

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

private:
  mutable Util::Mutex _mutex;
  CloudCredential _credentials;
  QByteArray _masterKey;
  bool _credentialsLoaded = false;
};

} // namespace Util
