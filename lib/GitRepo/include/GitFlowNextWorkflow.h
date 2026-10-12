#pragma once

#include "WorkflowUtils.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <functional>

namespace GitRepo {

// A git-flow-next branch model defined by the user. It is stored globally by
// Sourcerer and written into the configuration of the repositories that use
// it.
struct GitFlowNextWorkflow {
  QString id;
  QString name;
  // Increases with every saved change, so repositories that were written with
  // an older revision can offer the update.
  int revision = 1;
  QVector<GitFlowNextBranch> branches;

  QVector<GitFlowNextBranch> baseBranches() const {
    QVector<GitFlowNextBranch> result;
    for (const GitFlowNextBranch &branch : branches) {
      if (!branch.topic) {
        result.append(branch);
      }
    }
    return result;
  }
};

inline QJsonObject
gitFlowNextWorkflowToJson(const GitFlowNextWorkflow &workflow) {
  QJsonArray branches;
  for (const GitFlowNextBranch &branch : workflow.branches) {
    QJsonObject object;
    object.insert(QStringLiteral("name"), branch.name);
    object.insert(QStringLiteral("type"), branch.topic
                                              ? QStringLiteral("topic")
                                              : QStringLiteral("base"));
    object.insert(QStringLiteral("parent"), branch.parent);
    object.insert(QStringLiteral("startPoint"), branch.startPoint);
    object.insert(QStringLiteral("prefix"), branch.prefix);
    object.insert(QStringLiteral("upstreamStrategy"), branch.upstreamStrategy);
    object.insert(QStringLiteral("downstreamStrategy"),
                  branch.downstreamStrategy);
    object.insert(QStringLiteral("tag"), branch.tag);
    object.insert(QStringLiteral("tagPrefix"), branch.tagPrefix);
    object.insert(QStringLiteral("autoUpdate"), branch.autoUpdate);
    object.insert(QStringLiteral("keep"), branch.keepOnFinish);
    branches.append(object);
  }

  QJsonObject object;
  object.insert(QStringLiteral("id"), workflow.id);
  object.insert(QStringLiteral("name"), workflow.name);
  object.insert(QStringLiteral("revision"), workflow.revision);
  object.insert(QStringLiteral("branches"), branches);
  return object;
}

inline GitFlowNextWorkflow
gitFlowNextWorkflowFromJson(const QJsonObject &object) {
  GitFlowNextWorkflow workflow;
  workflow.id = object.value(QStringLiteral("id")).toString();
  workflow.name = object.value(QStringLiteral("name")).toString();
  workflow.revision = qMax(1, object.value(QStringLiteral("revision")).toInt());

  const QJsonArray branches =
      object.value(QStringLiteral("branches")).toArray();
  for (const QJsonValue &value : branches) {
    const QJsonObject entry = value.toObject();
    GitFlowNextBranch branch;
    branch.name = entry.value(QStringLiteral("name")).toString();
    branch.topic = entry.value(QStringLiteral("type")).toString() ==
                   QStringLiteral("topic");
    branch.parent = entry.value(QStringLiteral("parent")).toString();
    branch.startPoint = entry.value(QStringLiteral("startPoint")).toString();
    branch.prefix = entry.value(QStringLiteral("prefix")).toString();
    branch.upstreamStrategy = entry.value(QStringLiteral("upstreamStrategy"))
                                  .toString(QStringLiteral("merge"));
    branch.downstreamStrategy =
        entry.value(QStringLiteral("downstreamStrategy"))
            .toString(QStringLiteral("merge"));
    branch.tag = entry.value(QStringLiteral("tag")).toBool();
    branch.tagPrefix = entry.value(QStringLiteral("tagPrefix")).toString();
    branch.autoUpdate = entry.value(QStringLiteral("autoUpdate")).toBool();
    branch.keepOnFinish = entry.value(QStringLiteral("keep")).toBool();
    workflow.branches.append(branch);
  }
  return workflow;
}

// The content of the global workflows file.
inline QByteArray
serializeGitFlowNextWorkflows(const QVector<GitFlowNextWorkflow> &workflows) {
  QJsonArray array;
  for (const GitFlowNextWorkflow &workflow : workflows) {
    array.append(gitFlowNextWorkflowToJson(workflow));
  }
  QJsonObject root;
  root.insert(QStringLiteral("version"), 1);
  root.insert(QStringLiteral("workflows"), array);
  return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

inline QVector<GitFlowNextWorkflow>
parseGitFlowNextWorkflows(const QByteArray &json) {
  QVector<GitFlowNextWorkflow> workflows;
  const QJsonArray array = QJsonDocument::fromJson(json)
                               .object()
                               .value(QStringLiteral("workflows"))
                               .toArray();
  for (const QJsonValue &value : array) {
    GitFlowNextWorkflow workflow =
        gitFlowNextWorkflowFromJson(value.toObject());
    if (!workflow.id.isEmpty()) {
      workflows.append(workflow);
    }
  }
  return workflows;
}

// The workflow described by a repository's configuration, for "Save this
// repository's configuration as...".
inline GitFlowNextWorkflow
gitFlowNextWorkflowFromConfig(const GitFlowNextConfig &config) {
  GitFlowNextWorkflow workflow;
  workflow.branches = config.branches;
  return workflow;
}

// The gitflow.* entries that describe workflow in a repository configuration.
inline QList<QPair<QString, QString>>
gitFlowNextConfigEntries(const GitFlowNextWorkflow &workflow) {
  const auto key = [](const QString &branch, const char *property) {
    return QStringLiteral("gitflow.branch.%1.%2")
        .arg(branch, QString::fromLatin1(property));
  };
  const auto boolean = [](bool value) {
    return value ? QStringLiteral("true") : QStringLiteral("false");
  };

  QList<QPair<QString, QString>> entries = {
      {QStringLiteral("gitflow.version"), QStringLiteral("1.0")},
      {QStringLiteral("gitflow.initialized"), QStringLiteral("true")},
  };

  for (const GitFlowNextBranch &branch : workflow.branches) {
    entries.append({key(branch.name, "type"), branch.topic
                                                  ? QStringLiteral("topic")
                                                  : QStringLiteral("base")});
    if (!branch.parent.isEmpty()) {
      entries.append({key(branch.name, "parent"), branch.parent});
    }
    if (!branch.startPoint.isEmpty()) {
      entries.append({key(branch.name, "startPoint"), branch.startPoint});
    }
    entries.append(
        {key(branch.name, "upstreamStrategy"), branch.upstreamStrategy});
    entries.append(
        {key(branch.name, "downstreamStrategy"), branch.downstreamStrategy});

    if (branch.topic) {
      entries.append({key(branch.name, "prefix"), branch.prefix});
      entries.append({key(branch.name, "tag"), boolean(branch.tag)});
      if (!branch.tagPrefix.isEmpty()) {
        entries.append({key(branch.name, "tagprefix"), branch.tagPrefix});
      }
      if (branch.keepOnFinish) {
        entries.append(
            {QStringLiteral("gitflow.%1.finish.keep").arg(branch.name),
             QStringLiteral("true")});
      }
    } else {
      entries.append(
          {key(branch.name, "autoUpdate"), boolean(branch.autoUpdate)});
    }
  }

  return entries;
}

// Base branch names follow git's reference name rules.
inline bool isValidGitFlowNextBaseName(const QString &name) {
  if (name.isEmpty() || name.startsWith(QLatin1Char('-')) ||
      name.startsWith(QLatin1Char('/')) || name.endsWith(QLatin1Char('/')) ||
      name.endsWith(QLatin1Char('.')) ||
      name.endsWith(QStringLiteral(".lock")) ||
      name.contains(QStringLiteral("..")) ||
      name.contains(QStringLiteral("//")) ||
      name.contains(QStringLiteral("/.")) || name == QStringLiteral("@") ||
      name.contains(QStringLiteral("@{"))) {
    return false;
  }
  for (const QChar c : name) {
    if (c.unicode() < 0x20 || c == QLatin1Char(' ') || c == QLatin1Char('~') ||
        c == QLatin1Char('^') || c == QLatin1Char(':') ||
        c == QLatin1Char('?') || c == QLatin1Char('*') ||
        c == QLatin1Char('[') || c == QLatin1Char('\\') || c == QChar(0x7f)) {
      return false;
    }
  }
  return true;
}

// Topic branch type names are part of config keys like
// gitflow.<type>.finish.keep, so they can't contain dots.
inline bool isValidGitFlowNextTypeName(const QString &name) {
  if (name.isEmpty()) {
    return false;
  }
  for (const QChar c : name) {
    const bool allowed = (c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
                         (c >= QLatin1Char('A') && c <= QLatin1Char('Z')) ||
                         (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                         c == QLatin1Char('_') || c == QLatin1Char('-');
    if (!allowed) {
      return false;
    }
  }
  return true;
}

// Problems that keep the workflow from being saved, empty if there are none.
inline QStringList
validateGitFlowNextWorkflow(const GitFlowNextWorkflow &workflow) {
  const auto tr = [](const char *text) {
    return QCoreApplication::translate("GitFlowNextWorkflow", text);
  };

  QStringList problems;
  if (workflow.name.trimmed().isEmpty()) {
    problems.append(tr("Enter a name for the workflow."));
  }

  QSet<QString> baseNames;
  QSet<QString> allNames;
  int roots = 0;
  for (const GitFlowNextBranch &branch : workflow.branches) {
    if (allNames.contains(branch.name)) {
      problems.append(tr("'%1' is defined more than once.").arg(branch.name));
    }
    allNames.insert(branch.name);
    if (!branch.topic) {
      baseNames.insert(branch.name);
      if (branch.parent.isEmpty()) {
        ++roots;
      }
    }
  }

  if (baseNames.isEmpty()) {
    problems.append(tr("Add at least one base branch."));
  } else if (roots != 1) {
    problems.append(
        tr("Exactly one base branch must have no parent, it is the root of "
           "the workflow."));
  }

  QSet<QString> prefixes;
  for (const GitFlowNextBranch &branch : workflow.branches) {
    if (!branch.topic) {
      if (!isValidGitFlowNextBaseName(branch.name)) {
        problems.append(
            tr("'%1' is not a valid branch name.").arg(branch.name));
      }
      if (!branch.parent.isEmpty() && !baseNames.contains(branch.parent)) {
        problems.append(
            tr("The parent of '%1' must be a base branch.").arg(branch.name));
      }
      if (branch.parent == branch.name && !branch.name.isEmpty()) {
        problems.append(tr("'%1' can't be its own parent.").arg(branch.name));
      }
      if (branch.downstreamStrategy != QStringLiteral("merge") &&
          branch.downstreamStrategy != QStringLiteral("rebase")) {
        problems.append(
            tr("'%1' must be updated by merge or rebase.").arg(branch.name));
      }
      continue;
    }

    if (!isValidGitFlowNextTypeName(branch.name)) {
      problems.append(
          tr("'%1' is not a valid branch type name. Use letters, digits, '_' "
             "and '-'.")
              .arg(branch.name));
    }
    if (branch.prefix.isEmpty() || !branch.prefix.endsWith(QLatin1Char('/')) ||
        !isValidGitFlowNextBaseName(branch.prefix + QStringLiteral("x"))) {
      problems.append(tr("The prefix of '%1' must be a valid branch name "
                         "ending with '/'.")
                          .arg(branch.name));
    } else if (prefixes.contains(branch.prefix)) {
      problems.append(
          tr("The prefix '%1' is used more than once.").arg(branch.prefix));
    }
    prefixes.insert(branch.prefix);
    if (!baseNames.contains(branch.parent)) {
      problems.append(
          tr("The parent of '%1' must be a base branch.").arg(branch.name));
    }
    if (!branch.startPoint.isEmpty() &&
        !baseNames.contains(branch.startPoint)) {
      problems.append(
          tr("'%1' must start from a base branch.").arg(branch.name));
    }
    if (branch.upstreamStrategy != QStringLiteral("merge") &&
        branch.upstreamStrategy != QStringLiteral("rebase") &&
        branch.upstreamStrategy != QStringLiteral("squash")) {
      problems.append(tr("'%1' must be finished by merge, rebase or squash.")
                          .arg(branch.name));
    }
    if (branch.downstreamStrategy != QStringLiteral("merge") &&
        branch.downstreamStrategy != QStringLiteral("rebase")) {
      problems.append(
          tr("'%1' must be updated by merge or rebase.").arg(branch.name));
    }
    if (!branch.tagPrefix.isEmpty() &&
        !isValidGitFlowNextBaseName(branch.tagPrefix + QStringLiteral("1"))) {
      problems.append(tr("The tag prefix of '%1' is not valid in a tag name.")
                          .arg(branch.name));
    }
  }

  // Parents of base branches must not form a cycle.
  QHash<QString, QString> parents;
  for (const GitFlowNextBranch &branch : workflow.branches) {
    if (!branch.topic) {
      parents.insert(branch.name, branch.parent);
    }
  }
  bool cycle = false;
  for (auto it = parents.cbegin(); it != parents.cend() && !cycle; ++it) {
    QSet<QString> seen{it.key()};
    QString current = it.value();
    while (!current.isEmpty() && parents.contains(current)) {
      if (seen.contains(current)) {
        cycle = true;
        break;
      }
      seen.insert(current);
      current = parents.value(current);
    }
  }
  if (cycle) {
    problems.append(tr("The parents of the base branches form a cycle."));
  }

  problems.removeDuplicates();
  return problems;
}

// Base branches of workflow that are not in localBranches, parents first, so
// each can be created from its parent. The root is not included: it must
// exist already.
inline QStringList
gitFlowNextBaseBranchesToCreate(const GitFlowNextWorkflow &workflow,
                                const QStringList &localBranches) {
  QHash<QString, QString> parents;
  for (const GitFlowNextBranch &branch : workflow.branches) {
    if (!branch.topic) {
      parents.insert(branch.name, branch.parent);
    }
  }

  QStringList order;
  QSet<QString> visited;
  std::function<void(const QString &)> visit = [&](const QString &name) {
    if (visited.contains(name)) {
      return;
    }
    visited.insert(name);
    const QString parent = parents.value(name);
    if (!parent.isEmpty() && parents.contains(parent)) {
      visit(parent);
    }
    if (!parent.isEmpty() && !localBranches.contains(name)) {
      order.append(name);
    }
  };

  for (const GitFlowNextBranch &branch : workflow.branches) {
    if (!branch.topic) {
      visit(branch.name);
    }
  }
  return order;
}

} // namespace GitRepo
