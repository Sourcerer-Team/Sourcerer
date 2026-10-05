#pragma once

#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

namespace GitRepo {

enum class WorkflowType {
  GitFlow = 0,
  GitLabFlow,
  GitHubFlow,
  GitFlowNext,
  Graphite,

  last
};

enum class GitFlowNextAction {
  Unknown = 0,

  Init,
  Start,
  Finish,
  Update,

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
  case WorkflowType::GitFlowNext:
    return QStringLiteral("gitflownext");
  case WorkflowType::Graphite:
    return QStringLiteral("graphite");
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

  if (trimmed.compare(QStringLiteral("gitflownext"), Qt::CaseInsensitive) ==
      0) {
    return WorkflowType::GitFlowNext;
  }

  if (trimmed.compare(QStringLiteral("graphite"), Qt::CaseInsensitive) == 0) {
    return WorkflowType::Graphite;
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

// git-flow-next (https://github.com/gittower/git-flow-next) keeps its branch
// model in the repository config under gitflow.branch.<name>.<property>.
// Base branches (e.g. main, develop) are long lived, topic branch types (e.g.
// feature, release, hotfix) describe the short lived branches started from
// and finished into them.
struct GitFlowNextBranch {
  // Branch name for base branches, branch type name for topic branches.
  QString name;
  bool topic = false;
  QString parent;
  QString startPoint;
  QString prefix;
  // merge, rebase or squash when finishing into the parent.
  QString upstreamStrategy = QStringLiteral("merge");
  // merge or rebase when updating from the parent.
  QString downstreamStrategy = QStringLiteral("merge");
  bool tag = false;
  QString tagPrefix;
  bool autoUpdate = false;
  // gitflow.<type>.finish.keep
  bool keepOnFinish = false;
};

struct GitFlowNextConfig {
  bool initialized = false;
  QVector<GitFlowNextBranch> branches;

  QVector<GitFlowNextBranch> topicTypes() const {
    QVector<GitFlowNextBranch> result;
    for (const GitFlowNextBranch &branch : branches) {
      if (branch.topic) {
        result.append(branch);
      }
    }
    return result;
  }
};

inline bool gitFlowNextIsTrue(const QString &value) {
  const QString normalized = value.trimmed().toLower();
  return normalized == QStringLiteral("true") ||
         normalized == QStringLiteral("yes") ||
         normalized == QStringLiteral("on") ||
         normalized == QStringLiteral("1");
}

// Builds the model from gitflow.* config entries. Keys are matched case
// insensitively, as git does for section and variable names.
inline GitFlowNextConfig
parseGitFlowNextConfig(const QList<QPair<QString, QString>> &entries) {
  GitFlowNextConfig config;
  QStringList order;
  QHash<QString, GitFlowNextBranch> branches;
  QHash<QString, bool> hasType;
  QHash<QString, QString> keep;
  QHash<QString, QString> noTag;

  const QString branchPrefix = QStringLiteral("gitflow.branch.");
  for (const auto &[key, value] : entries) {
    if (key.compare(QStringLiteral("gitflow.initialized"),
                    Qt::CaseInsensitive) == 0) {
      config.initialized = gitFlowNextIsTrue(value);
      continue;
    }

    if (!key.startsWith(QStringLiteral("gitflow."), Qt::CaseInsensitive)) {
      continue;
    }

    if (!key.startsWith(branchPrefix, Qt::CaseInsensitive)) {
      // Command options: gitflow.<type>.finish.<option>
      const QStringList parts = key.mid(8).split(QLatin1Char('.'));
      if (parts.size() == 3 && parts.at(1).compare(QStringLiteral("finish"),
                                                   Qt::CaseInsensitive) == 0) {
        const QString option = parts.at(2).toLower();
        if (option == QStringLiteral("keep")) {
          keep.insert(parts.at(0), value);
        } else if (option == QStringLiteral("notag")) {
          noTag.insert(parts.at(0), value);
        }
      }
      continue;
    }

    const QString rest = key.mid(branchPrefix.size());
    const qsizetype dot = rest.lastIndexOf(QLatin1Char('.'));
    if (dot <= 0) {
      continue;
    }

    const QString name = rest.left(dot);
    const QString property = rest.mid(dot + 1).toLower();
    if (!branches.contains(name)) {
      order.append(name);
      GitFlowNextBranch branch;
      branch.name = name;
      branches.insert(name, branch);
    }

    GitFlowNextBranch &branch = branches[name];
    const QString trimmed = value.trimmed();
    if (property == QStringLiteral("type")) {
      branch.topic =
          trimmed.compare(QStringLiteral("topic"), Qt::CaseInsensitive) == 0;
      hasType.insert(name, true);
    } else if (property == QStringLiteral("parent")) {
      branch.parent = trimmed;
    } else if (property == QStringLiteral("startpoint")) {
      branch.startPoint = trimmed;
    } else if (property == QStringLiteral("prefix")) {
      branch.prefix = trimmed;
    } else if (property == QStringLiteral("upstreamstrategy")) {
      branch.upstreamStrategy = trimmed.toLower();
    } else if (property == QStringLiteral("downstreamstrategy")) {
      branch.downstreamStrategy = trimmed.toLower();
    } else if (property == QStringLiteral("tag")) {
      branch.tag = gitFlowNextIsTrue(trimmed);
    } else if (property == QStringLiteral("tagprefix")) {
      branch.tagPrefix = trimmed;
    } else if (property == QStringLiteral("autoupdate")) {
      branch.autoUpdate = gitFlowNextIsTrue(trimmed);
    }
  }

  for (const QString &name : order) {
    if (!hasType.value(name)) {
      continue;
    }

    GitFlowNextBranch branch = branches.value(name);
    if (branch.topic) {
      if (branch.prefix.isEmpty()) {
        branch.prefix = name + QLatin1Char('/');
      }
      if (keep.contains(name)) {
        branch.keepOnFinish = gitFlowNextIsTrue(keep.value(name));
      }
      if (noTag.contains(name) && gitFlowNextIsTrue(noTag.value(name))) {
        branch.tag = false;
      }
    }
    config.branches.append(branch);
  }

  if (!config.branches.isEmpty()) {
    config.initialized = true;
  }

  return config;
}

// The configuration `git flow init --preset=classic` writes.
inline QList<QPair<QString, QString>>
gitFlowNextClassicPreset(const QString &mainBranchName,
                         const QString &developBranchName) {
  const QString main = mainBranchName.trimmed();
  const QString develop = developBranchName.trimmed();
  const auto key = [](const QString &branch, const char *property) {
    return QStringLiteral("gitflow.branch.%1.%2")
        .arg(branch, QString::fromLatin1(property));
  };

  QList<QPair<QString, QString>> entries = {
      {QStringLiteral("gitflow.version"), QStringLiteral("1.0")},
      {QStringLiteral("gitflow.initialized"), QStringLiteral("true")},

      {key(main, "type"), QStringLiteral("base")},
      {key(main, "upstreamStrategy"), QStringLiteral("merge")},
      {key(main, "downstreamStrategy"), QStringLiteral("merge")},
      {key(main, "autoUpdate"), QStringLiteral("false")},

      {key(develop, "type"), QStringLiteral("base")},
      {key(develop, "parent"), main},
      {key(develop, "startPoint"), main},
      {key(develop, "upstreamStrategy"), QStringLiteral("merge")},
      {key(develop, "downstreamStrategy"), QStringLiteral("merge")},
      {key(develop, "autoUpdate"), QStringLiteral("true")},
  };

  struct Topic {
    const char *name;
    QString parent;
    QString startPoint;
    const char *downstreamStrategy;
    const char *tag;
  };
  const Topic topics[] = {
      {"feature", develop, develop, "rebase", "false"},
      {"release", main, develop, "merge", "true"},
      {"hotfix", main, main, "merge", "true"},
  };
  for (const Topic &topic : topics) {
    const QString name = QString::fromLatin1(topic.name);
    entries.append({key(name, "type"), QStringLiteral("topic")});
    entries.append({key(name, "parent"), topic.parent});
    entries.append({key(name, "startPoint"), topic.startPoint});
    entries.append({key(name, "prefix"), name + QLatin1Char('/')});
    entries.append({key(name, "upstreamStrategy"), QStringLiteral("merge")});
    entries.append({key(name, "downstreamStrategy"),
                    QString::fromLatin1(topic.downstreamStrategy)});
    entries.append({key(name, "tag"), QString::fromLatin1(topic.tag)});
  }

  return entries;
}

// The topic branch type a branch belongs to, by the longest matching prefix.
// Returns nullptr for base branches and unrelated branches.
inline const GitFlowNextBranch *
gitFlowNextTopicTypeForBranch(const GitFlowNextConfig &config,
                              const QString &branchName) {
  const GitFlowNextBranch *match = nullptr;
  for (const GitFlowNextBranch &branch : config.branches) {
    if (!branch.topic || branch.prefix.isEmpty() ||
        branchName.size() <= branch.prefix.size() ||
        !branchName.startsWith(branch.prefix)) {
      continue;
    }
    if (!match || branch.prefix.size() > match->prefix.size()) {
      match = &branch;
    }
  }
  return match;
}

inline QString gitFlowNextStartPoint(const GitFlowNextBranch &type) {
  return type.startPoint.isEmpty() ? type.parent : type.startPoint;
}

inline QString gitFlowNextBranchNameForInput(const GitFlowNextBranch &type,
                                             const QString &input) {
  const QString trimmed = input.trimmed();
  if (trimmed.isEmpty() || trimmed.startsWith(type.prefix)) {
    return trimmed;
  }
  return type.prefix + trimmed;
}

// Tag created when finishing branchName, e.g. release/1.2 -> v1.2 for the
// tag prefix "v".
inline QString gitFlowNextTagName(const GitFlowNextBranch &type,
                                  const QString &branchName) {
  const QString shortName = branchName.startsWith(type.prefix)
                                ? branchName.mid(type.prefix.size())
                                : branchName;
  return type.tagPrefix + shortName;
}

// Base branches kept up to date with parentBranch, e.g. develop after a
// release was finished into main.
inline QStringList
gitFlowNextAutoUpdateBranches(const GitFlowNextConfig &config,
                              const QString &parentBranch) {
  QStringList result;
  for (const GitFlowNextBranch &branch : config.branches) {
    if (!branch.topic && branch.autoUpdate && branch.parent == parentBranch &&
        branch.name != parentBranch) {
      result.append(branch.name);
    }
  }
  return result;
}

// Branch names passed to the Graphite CLI. On Windows gt is usually an npm
// .cmd script run by cmd.exe, so only characters cmd.exe treats literally are
// accepted, within git's reference name rules.
inline bool isSafeGraphiteBranchName(const QString &name) {
  if (name.isEmpty() || name.startsWith(QLatin1Char('-')) ||
      name.startsWith(QLatin1Char('/')) || name.endsWith(QLatin1Char('/')) ||
      name.endsWith(QLatin1Char('.')) ||
      name.endsWith(QStringLiteral(".lock")) ||
      name.contains(QStringLiteral("..")) ||
      name.contains(QStringLiteral("//")) ||
      name.contains(QStringLiteral("/."))) {
    return false;
  }

  for (const QChar c : name) {
    const bool allowed = (c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
                         (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) ||
                         (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                         c == QLatin1Char('.') || c == QLatin1Char('_') ||
                         c == QLatin1Char('-') || c == QLatin1Char('/');
    if (!allowed) {
      return false;
    }
  }
  return true;
}

// The branches gt lists when `gt up` or `gt top` can't choose in
// non-interactive mode because the stack forks, e.g.
//   ERROR: Cannot get upstack branch in non-interactive mode; multiple choices
//   available:
//   left
//   right
// Empty for any other output.
inline QStringList graphiteAmbiguousBranches(const QString &output) {
  const QStringList lines = output.split(QLatin1Char('\n'));
  QStringList branches;
  bool listing = false;
  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();
    if (!listing) {
      listing = trimmed.contains(QStringLiteral("multiple choices available"),
                                 Qt::CaseInsensitive);
      continue;
    }
    if (trimmed.isEmpty()) {
      break;
    }
    // Lines such as tips are not branch names.
    if (trimmed.contains(QLatin1Char(' ')) ||
        trimmed.contains(QLatin1Char(':'))) {
      break;
    }
    branches.append(trimmed);
  }
  return branches;
}

// What the workflow menu of a repository is built from.
struct WorkflowState {
  WorkflowType type = WorkflowType::GitFlow;
  GitFlowNextConfig gitFlowNext;
};

} // namespace GitRepo
