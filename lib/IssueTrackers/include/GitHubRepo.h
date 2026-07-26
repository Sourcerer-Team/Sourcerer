#pragma once

#include "IssueTrackerRepo.h"

#include <QString>

namespace IssueTrackers {

class GitHubRepo final : public IssueTrackerRepo {
public:
  explicit GitHubRepo(QString owner, QString repository,
                      QString host = QStringLiteral("github.com"));

  Type type() const override;
  QString serviceName() const override;
  QString host() const override;
  QString owner() const override;
  QString repository() const override;
  QString repositoryPath() const override;
  QString webUrl() const override;
  QString issuesUrl() const override;
  bool isValid() const override;

private:
  QString _host;
  QString _owner;
  QString _repository;
};

} // namespace IssueTrackers
