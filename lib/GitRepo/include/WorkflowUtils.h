#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace GitRepo {

enum class WorkflowType {
  GitFlow = 0,
  GitLabFlow,
  GitHubFlow,

  last
};

enum class GitHubFlowAction {
  Unknown = 0,

  StartBranch,
  UpdateBranch,
  FinishBranch,

  last
};

enum class GitLabFlowAction {
  Unknown = 0,

  StartFeature,
  FinishFeature,
  Promote,
  StartRelease,
  CherryPickToRelease,

  last
};

// Value stored in the repository config under sourcerer.workflow.
inline QString workflowTypeToString(WorkflowType type) {
  switch (type) {
  case WorkflowType::GitLabFlow:
    return QStringLiteral("gitlabflow");
  case WorkflowType::GitHubFlow:
    return QStringLiteral("githubflow");
  case WorkflowType::GitFlow:
  case WorkflowType::last:
  default:
    return QStringLiteral("gitflow");
  }
}

// Missing or unknown values fall back to classic Git Flow.
inline WorkflowType workflowTypeFromString(const QString &value) {
  const QString trimmed = value.trimmed();
  if (trimmed.compare(QStringLiteral("gitlabflow"), Qt::CaseInsensitive) == 0) {
    return WorkflowType::GitLabFlow;
  }

  if (trimmed.compare(QStringLiteral("githubflow"), Qt::CaseInsensitive) == 0) {
    return WorkflowType::GitHubFlow;
  }

  return WorkflowType::GitFlow;
}

// Returns "owner/repo" for a github.com remote, or an empty string for any
// other remote. Accepts https://github.com/owner/repo(.git),
// git@github.com:owner/repo(.git) and ssh://git@github.com/owner/repo(.git).
inline QString gitHubRepositoryName(const QString &remoteUrl) {
  QString path = remoteUrl.trimmed();

  const QStringList prefixes = {
      QStringLiteral("https://github.com/"),
      QStringLiteral("http://github.com/"),
      QStringLiteral("git@github.com:"),
      QStringLiteral("ssh://git@github.com/"),
      QStringLiteral("ssh://git@github.com:22/"),
  };

  bool matched = false;
  for (const QString &prefix : prefixes) {
    if (path.startsWith(prefix, Qt::CaseInsensitive)) {
      path = path.mid(prefix.size());
      matched = true;
      break;
    }
  }

  if (!matched) {
    return {};
  }

  if (path.endsWith(QLatin1Char('/'))) {
    path.chop(1);
  }
  if (path.endsWith(QStringLiteral(".git"))) {
    path.chop(4);
  }

  const QStringList parts = path.split(QLatin1Char('/'));
  if (parts.size() != 2 || parts.at(0).isEmpty() || parts.at(1).isEmpty()) {
    return {};
  }

  return path;
}

// Returns the GitHub page that opens a pull request of headBranch into
// baseBranch, or an empty string if remoteUrl is not a github.com remote.
inline QString gitHubPullRequestUrl(const QString &remoteUrl,
                                    const QString &baseBranch,
                                    const QString &headBranch) {
  const QString repositoryName = gitHubRepositoryName(remoteUrl);
  if (repositoryName.isEmpty() || baseBranch.trimmed().isEmpty() ||
      headBranch.trimmed().isEmpty()) {
    return {};
  }

  const auto encode = [](const QString &branch) {
    return QString::fromLatin1(
        QUrl::toPercentEncoding(branch.trimmed(), QByteArrayLiteral("/")));
  };

  return QStringLiteral("https://github.com/%1/compare/%2...%3?expand=1")
      .arg(repositoryName, encode(baseBranch), encode(headBranch));
}

// Returns the main branch followed by the comma-separated environment
// branches, trimmed, without empty entries and duplicates.
inline QStringList
gitLabFlowEnvironmentChain(const QString &mainBranchName,
                           const QString &environmentBranches) {
  QStringList chain;

  const QString main = mainBranchName.trimmed();
  if (!main.isEmpty()) {
    chain.append(main);
  }

  const QStringList environments =
      environmentBranches.split(QLatin1Char(','), Qt::SkipEmptyParts);
  for (const QString &environment : environments) {
    const QString trimmed = environment.trimmed();
    if (!trimmed.isEmpty() && !chain.contains(trimmed)) {
      chain.append(trimmed);
    }
  }

  return chain;
}

// Returns the environment branch downstream of branchName, or an empty string
// if branchName is not in the chain or is its last entry.
inline QString gitLabFlowNextEnvironment(const QStringList &chain,
                                         const QString &branchName) {
  const qsizetype index = chain.indexOf(branchName);
  if (index < 0 || index + 1 >= chain.size()) {
    return {};
  }

  return chain.at(index + 1);
}

// Turns a version such as "1.2" into a release branch name such as
// "1-2-stable". Names that already end with the suffix are kept.
inline QString gitLabFlowReleaseBranchName(const QString &version,
                                           const QString &suffix) {
  QString name = version.trimmed();
  if (name.isEmpty()) {
    return {};
  }

  const QString trimmedSuffix = suffix.trimmed();
  if (!trimmedSuffix.isEmpty() && name.endsWith(trimmedSuffix)) {
    return name;
  }

  name.replace(QLatin1Char('.'), QLatin1Char('-'));
  return name + trimmedSuffix;
}

inline bool isGitLabFlowReleaseBranch(const QString &branchName,
                                      const QString &suffix) {
  const QString trimmedSuffix = suffix.trimmed();
  return !trimmedSuffix.isEmpty() && branchName.size() > trimmedSuffix.size() &&
         branchName.endsWith(trimmedSuffix);
}

} // namespace GitRepo
