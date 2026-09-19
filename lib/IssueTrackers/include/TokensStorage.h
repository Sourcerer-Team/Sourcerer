#pragma once

#include <LockGuard.h>

#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QMap>
#include <QString>
#include <QVector>

namespace IssueTrackers {

class TokensStorage {
public:
  struct TokenEntry {
    QString serviceType;
    QString repositoryName;
    QString userName;
    QString host;
    QString accessToken;
    QDateTime expiresAt;
    qint64 createdTime = 0;
    bool rememberToken = true;
  };

  static TokensStorage &getInstance();

  void storeToken(const QString &serviceType, const QString &repositoryName,
                  const QString &accessToken,
                  const QDateTime &expiresAt = QDateTime(),
                  bool remember = true);
  void storeToken(const QString &serviceType, const QString &repositoryName,
                  const QString &userName, const QString &accessToken,
                  const QDateTime &expiresAt = QDateTime(),
                  bool remember = true);
  void storeTokenWithHost(const QString &serviceType,
                          const QString &repositoryName, const QString &host,
                          const QString &accessToken,
                          const QDateTime &expiresAt = QDateTime(),
                          bool remember = true);

  bool getToken(const QString &serviceType, const QString &repositoryName,
                QString &accessToken, QDateTime &expiresAt) const;
  bool getToken(const QString &serviceType, const QString &repositoryName,
                QString &userName, QString &accessToken,
                QDateTime &expiresAt) const;
  bool getTokenWithHost(const QString &serviceType,
                        const QString &repositoryName, QString &host,
                        QString &accessToken, QDateTime &expiresAt) const;

  bool hasToken(const QString &serviceType,
                const QString &repositoryName) const;

  void removeToken(const QString &serviceType, const QString &repositoryName);
  void clearMemoryTokens();
  void clearAll();

  QList<TokenEntry> getAllTokenEntries() const;

private:
  TokensStorage();
  ~TokensStorage();

  TokensStorage(const TokensStorage &) = delete;
  TokensStorage &operator=(const TokensStorage &) = delete;

  void loadPersistedTokens();
  void saveTokensToDisk(const QString &entryKey, const TokenEntry &tokenEntry);
  void removeTokenFromDisk(const QString &entryKey);

  QByteArray encrypt(const QByteArray &plaintext, const QByteArray &key);
  QByteArray decrypt(const QByteArray &ciphertext, const QByteArray &key);

  QByteArray getEncryptionKey();
  QString getTokensFilePath() const;
  QByteArray deriveEntryKey(const QString &serviceType,
                            const QString &repositoryName);
  static QString makeEntryKey(const QString &serviceType,
                              const QString &repositoryName);

private:
  mutable Util::Mutex _mutex;
  QMap<QString, TokenEntry> _tokens;
  QByteArray _masterKey;
};

} // namespace IssueTrackers
