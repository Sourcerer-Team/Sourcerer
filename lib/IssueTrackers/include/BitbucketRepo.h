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

namespace IssueTrackers {

class BitbucketRepo final : public IssueTrackerRepo {
public:
  explicit BitbucketRepo(QString owner = QString(),
                         QString repository = QString(),
                         QString host = QStringLiteral("bitbucket.org"));
  ~BitbucketRepo() override;

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

  QStringList listUserRepositories(const QString &username,
                                   const QString &appPassword,
                                   QString *errorMessage = nullptr) const;
  QStringList
  listUserRepositoriesWithAccessToken(const QString &accessToken,
                                      QString *errorMessage = nullptr) const;

  static QUrl authorizationUrl(const QString &authorizeUrl,
                               const QString &clientId,
                               const QString &redirectUri,
                               const QString &scope);

private:
  static QString hostToWebBaseUrl(const QString &host);
  static QString hostToApiBaseUrl(const QString &host);
  static QByteArray formEncode(const QMap<QString, QString> &fields);
  static QString basicAuthHeaderValue(const QString &username,
                                      const QString &appPassword);
  static QString bearerAuthHeaderValue(const QString &accessToken);

  QPair<QNetworkReply::NetworkError, QString> requestOAuthAccessToken(
      const QString &tokenUrl, const QString &clientId,
      const QString &clientSecret, const QString &authorizationCode,
      const QString &redirectUri, const QString &accessTokenLabel,
      QDateTime *expiresAtOut = nullptr, QString *errorMessage = nullptr);
  QStringList
  listRepositoriesForWorkspace(const QString &workspace,
                               const QString &accessToken,
                               QString *errorMessage = nullptr) const;
  QString currentWorkspaceName(const QString &accessToken,
                               QString *errorMessage = nullptr) const;

private:
  QString _host;
  QString _owner;
  QString _repository;
  QString _username;
  QString _appPassword;
  QString _accessToken;
};

} // namespace IssueTrackers
