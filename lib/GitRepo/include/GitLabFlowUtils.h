#pragma once

#include <QString>
#include <QStringList>

namespace GitRepo {

enum class WorkflowType {
  GitFlow = 0,
  GitLabFlow,

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
  case WorkflowType::GitFlow:
  case WorkflowType::last:
  default:
    return QStringLiteral("gitflow");
  }
}

// Missing or unknown values fall back to classic Git Flow.
inline WorkflowType workflowTypeFromString(const QString &value) {
  if (value.trimmed().compare(QStringLiteral("gitlabflow"),
                              Qt::CaseInsensitive) == 0) {
    return WorkflowType::GitLabFlow;
  }

  return WorkflowType::GitFlow;
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
