#pragma once

#include "IssueTrackerRepo.h"

#include <QByteArray>
#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

#include <atomic>

namespace IssueTrackers {

class GitHubRepo final : public IssueTrackerRepo {
public:
  struct DeviceCodeResponse {
    QString deviceCode;
    QString userCode;
    QUrl verificationUri;
    QUrl verificationUriComplete;
    int expiresInSeconds = 0;
    int intervalSeconds = 5;
  };

  explicit GitHubRepo(QString owner = QString(), QString repository = QString(),
                      QString host = QStringLiteral("github.com"));
  ~GitHubRepo() override;

  static QUrl
  deviceLoginUrl(const QString &host = QStringLiteral("github.com"));

  Type type() const override;
  QString serviceName() const override;
  QString host() const override;
  QString owner() const override;
  QString repository() const override;
  QString repositoryPath() const override;
  QString webUrl() const override;
  QString issuesUrl() const override;
  bool isValid() const override;

  QVariantMap authenticate(const QVariantMap &parameters) override;

  QStringList listUserRepositories(QString *errorMessage = nullptr) const;

  QPair<QNetworkReply::NetworkError, QString>
  requestDeviceCode(const QString &clientId, DeviceCodeResponse *response,
                    const QString &scope = QStringLiteral("repo"),
                    QString *errorMessage = nullptr) const;

  QPair<QNetworkReply::NetworkError, QString>
  pollDeviceAccessToken(const QString &clientId, const QString &deviceCode,
                        int intervalSeconds, int expiresInSeconds,
                        QString *errorMessage = nullptr,
                        const std::atomic_bool *cancelled = nullptr);

  void setAccessToken(const QString &accessToken,
                      const QDateTime &expiresAt = QDateTime());
  const QString &accessToken() const;
  QDateTime accessTokenExpiresAt() const;
  bool hasValidAccessToken() const;
  void clearAccessToken();

private:
  static QString hostToWebBaseUrl(const QString &host);
  static QString hostToApiBaseUrl(const QString &host);
  static QByteArray formEncode(const QMap<QString, QString> &fields);
  QPair<QNetworkReply::NetworkError, QString>
  requestAccessTokenOnce(const QString &clientId, const QString &deviceCode,
                         QString *errorMessage = nullptr);

private:
  QString _host;
  QString _owner;
  QString _repository;
  QString _accessToken;
  QDateTime _accessTokenExpiresAt;
};

} // namespace IssueTrackers
