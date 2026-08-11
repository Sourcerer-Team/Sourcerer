#pragma once

#include "IssueTrackerRepo.h"

#include <QDateTime>
#include <QMap>
#include <QNetworkReply>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariantMap>

namespace IssueTrackers {

class AzureRepo final : public IssueTrackerRepo {
public:
  explicit AzureRepo(QString owner = QString(),
                     QString repository = QString(),
                     QString host = QStringLiteral("dev.azure.com"));
  ~AzureRepo() override;

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
  QMap<QString, QString> discoverTenants(const QString &accessToken,
                              QString *errorMessage = nullptr) const;
  QVariantMap listProjects(const QString &organization,
                           const QString &accessToken) const;

  static QUrl authorizationUrl(const QString &authorityUrl,
                               const QString &tenantId,
                               const QString &clientId,
                               const QString &redirectUri,
                               const QString &scope,
                               const QString &state = QString(),
                               const QString &codeChallenge = QString());

private:
  static QString hostToWebBaseUrl(const QString &host);
  static QString hostToApiBaseUrl(const QString &host);

private:
  QString _host;
  QString _owner;
  QString _repository;
};

} // namespace IssueTrackers
