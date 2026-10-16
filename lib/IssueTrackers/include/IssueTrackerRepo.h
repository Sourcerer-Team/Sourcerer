#pragma once

#include <QString>
#include <QVariantMap>

namespace IssueTrackers {

class IssueTrackerRepo {
public:
  enum class Type {
    Unknown = 0,
    GitHub,
    GitLab,
    Bitbucket,
    Azure,

    last
  };

  virtual ~IssueTrackerRepo() = 0;

  virtual Type type() const = 0;
  virtual QString serviceName() const = 0;
  virtual QString host() const = 0;
  virtual QString owner() const = 0;
  virtual QString repository() const = 0;
  virtual QString repositoryPath() const = 0;
  virtual QString webUrl() const = 0;
  virtual QString issuesUrl() const = 0;
  virtual bool isValid() const = 0;

  virtual QVariantMap authenticate(const QVariantMap &parameters) = 0;
};

} // namespace IssueTrackers
