#include "GitHubRepo.h"

namespace IssueTrackers {

GitHubRepo::GitHubRepo(QString owner, QString repository, QString host)
    : _host(host), _owner(owner), _repository(repository) {}

IssueTrackerRepo::Type GitHubRepo::type() const {
  return IssueTrackerRepo::Type::GitHub;
}

QString GitHubRepo::serviceName() const {
  return QStringLiteral("GitHub");
}

QString GitHubRepo::host() const {
  return _host;
}

QString GitHubRepo::owner() const {
  return _owner;
}

QString GitHubRepo::repository() const {
  return _repository;
}

QString GitHubRepo::repositoryPath() const {
  return _owner + QStringLiteral("/") + _repository;
}

QString GitHubRepo::webUrl() const {
  return QStringLiteral("https://") + _host + QStringLiteral("/") +
         repositoryPath();
}

QString GitHubRepo::issuesUrl() const {
  return webUrl() + QStringLiteral("/issues");
}

bool GitHubRepo::isValid() const {
  return !_host.isEmpty() && !_owner.isEmpty() && !_repository.isEmpty();
}

} // namespace IssueTrackers
