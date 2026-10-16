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

class GitLabRepo final : public IssueTrackerRepo {
public:
  explicit GitLabRepo(QString owner = QString(), QString repository = QString(),
                      QString host = QStringLiteral("gitlab.com"));
  ~GitLabRepo() override;

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

private:
  static QString hostToWebBaseUrl(const QString &host);
  static QString hostToApiBaseUrl(const QString &host);
  static QByteArray formEncode(const QMap<QString, QString> &fields);

private:
  QString _host;
  QString _owner;
  QString _repository;
  QString _accessToken;
  QDateTime _accessTokenExpiresAt;
};

} // namespace IssueTrackers
