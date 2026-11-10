// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVector>

#include <algorithm>
#include <functional>
#include <utility>

namespace GitRepo {

struct GraphitePullRequest {
  int number = 0;
  // OPEN, CLOSED or MERGED.
  QString state;
  bool draft = false;
  QString title;
  QString url;
};

struct GraphiteBranch {
  QString name;
  QString parent;
  QString parentRevision;
  QStringList children;
  bool trunk = false;
  bool needsRestack = false;
  GraphitePullRequest pullRequest;
};

// The stacks of the branches Graphite tracks, see `gt state`.
struct GraphiteStacks {
  bool valid = false;
  QMap<QString, GraphiteBranch> branches;

  bool isTracked(const QString &branch) const {
    return branches.contains(branch);
  }
  QStringList trunks() const {
    QStringList result;
    for (const GraphiteBranch &branch : branches) {
      if (branch.trunk) {
        result.append(branch.name);
      }
    }
    return result;
  }
};

// Parses the JSON printed by `gt state`:
//   {"main": {"trunk": true},
//    "feature": {"trunk": false, "needs_restack": false,
//                "parents": [{"ref": "main", "sha": "..."}]}}
inline GraphiteStacks parseGraphiteState(const QByteArray &json) {
  GraphiteStacks stacks;

  // gt may print notices before the JSON.
  const qsizetype start = json.indexOf('{');
  if (start < 0) {
    return stacks;
  }

  QJsonParseError error;
  const QJsonDocument document =
      QJsonDocument::fromJson(json.mid(start), &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return stacks;
  }

  const QJsonObject root = document.object();
  for (auto it = root.constBegin(); it != root.constEnd(); ++it) {
    if (!it.value().isObject()) {
      continue;
    }

    const QJsonObject object = it.value().toObject();
    GraphiteBranch branch;
    branch.name = it.key();
    branch.trunk = object.value(QStringLiteral("trunk")).toBool();
    branch.needsRestack =
        object.value(QStringLiteral("needs_restack")).toBool();
    const QJsonArray parents =
        object.value(QStringLiteral("parents")).toArray();
    if (!parents.isEmpty()) {
      const QJsonObject parent = parents.first().toObject();
      branch.parent = parent.value(QStringLiteral("ref")).toString();
      branch.parentRevision = parent.value(QStringLiteral("sha")).toString();
    }
    stacks.branches.insert(branch.name, branch);
  }

  QVector<QPair<QString, QString>> parentChild;
  for (const GraphiteBranch &branch : std::as_const(stacks.branches)) {
    if (!branch.parent.isEmpty() && stacks.branches.contains(branch.parent)) {
      parentChild.append({branch.parent, branch.name});
    }
  }
  for (const auto &[parent, child] : std::as_const(parentChild)) {
    stacks.branches[parent].children.append(child);
  }
  for (GraphiteBranch &branch : stacks.branches) {
    branch.children.sort();
  }

  stacks.valid = true;
  return stacks;
}

// Adds the pull requests Graphite cached in .git/.graphite_pr_info:
//   {"prInfos": [{"headRefName": "feature", "prNumber": 12, "state": "OPEN",
//                 "isDraft": false, "title": "...", "url": "..."}]}
inline void applyGraphitePrInfo(GraphiteStacks &stacks,
                                const QByteArray &json) {
  const QJsonDocument document = QJsonDocument::fromJson(json);
  if (!document.isObject()) {
    return;
  }

  const QJsonArray infos =
      document.object().value(QStringLiteral("prInfos")).toArray();
  for (const QJsonValue &value : infos) {
    const QJsonObject info = value.toObject();
    QString branch = info.value(QStringLiteral("headRefName")).toString();
    if (branch.isEmpty()) {
      branch = info.value(QStringLiteral("branchName")).toString();
    }
    if (!stacks.branches.contains(branch)) {
      continue;
    }

    GraphitePullRequest pullRequest;
    pullRequest.number = info.value(QStringLiteral("prNumber")).toInt();
    if (pullRequest.number == 0) {
      pullRequest.number = info.value(QStringLiteral("number")).toInt();
    }
    pullRequest.state =
        info.value(QStringLiteral("state")).toString().toUpper();
    pullRequest.draft = info.value(QStringLiteral("isDraft")).toBool();
    pullRequest.title = info.value(QStringLiteral("title")).toString();
    pullRequest.url = info.value(QStringLiteral("url")).toString();
    if (pullRequest.number > 0) {
      stacks.branches[branch].pullRequest = pullRequest;
    }
  }
}

// One row of the local branches panel in Graphite mode.
struct GraphiteBranchRow {
  enum class Kind { Branch, UntrackedHeader };

  Kind kind = Kind::Branch;
  QString branch;
  bool tracked = false;
  // Rows are indented by depth; the trunk has depth 0 and its children 1.
  int depth = 0;
  // Column i (< depth - 1) has a vertical line passing through this row.
  QVector<bool> throughLines;
  // The connector into this row's dot ends here, no sibling follows below.
  bool lastChild = true;
  // A line leaves the dot downwards to the first child.
  bool hasChildren = false;
};

// Orders the local branches like Graphite's stacks with the trunk on top and
// children below their parent, followed by a section with the untracked
// branches.
inline QVector<GraphiteBranchRow>
graphiteBranchRows(const GraphiteStacks &stacks,
                   const QStringList &localBranches) {
  QVector<GraphiteBranchRow> rows;
  QStringList placed;

  const auto isLocal = [&](const QString &branch) {
    return localBranches.contains(branch);
  };
  const auto localChildren = [&](const GraphiteBranch &branch) {
    QStringList children;
    for (const QString &child : branch.children) {
      if (isLocal(child)) {
        children.append(child);
      }
    }
    return children;
  };

  std::function<void(const QString &, int, QVector<bool>, bool)> visit =
      [&](const QString &name, int depth, QVector<bool> throughLines,
          bool lastChild) {
        const GraphiteBranch branch = stacks.branches.value(name);
        const QStringList children = localChildren(branch);

        GraphiteBranchRow row;
        row.branch = name;
        row.tracked = true;
        row.depth = depth;
        row.throughLines = throughLines;
        row.lastChild = lastChild;
        row.hasChildren = !children.isEmpty();
        rows.append(row);
        placed.append(name);

        QVector<bool> childThroughLines = throughLines;
        if (depth > 0) {
          childThroughLines.append(!lastChild);
        }
        for (qsizetype i = 0; i < children.size(); ++i) {
          visit(children.at(i), depth + 1, childThroughLines,
                i == children.size() - 1);
        }
      };

  // Trunks first, then tracked branches whose parent is not available.
  QStringList roots = stacks.trunks();
  std::sort(roots.begin(), roots.end());
  QStringList orphans;
  for (const GraphiteBranch &branch : stacks.branches) {
    if (!branch.trunk && (branch.parent.isEmpty() || !isLocal(branch.parent) ||
                          !stacks.branches.contains(branch.parent))) {
      orphans.append(branch.name);
    }
  }
  std::sort(orphans.begin(), orphans.end());
  roots += orphans;

  for (const QString &root : std::as_const(roots)) {
    if (isLocal(root) && !placed.contains(root)) {
      visit(root, 0, {}, true);
    }
  }

  QStringList untracked;
  for (const QString &branch : localBranches) {
    if (!placed.contains(branch)) {
      untracked.append(branch);
    }
  }
  if (!untracked.isEmpty()) {
    GraphiteBranchRow header;
    header.kind = GraphiteBranchRow::Kind::UntrackedHeader;
    rows.append(header);
    for (const QString &branch : std::as_const(untracked)) {
      GraphiteBranchRow row;
      row.branch = branch;
      rows.append(row);
    }
  }

  return rows;
}

// owner/repo of a GitHub pull request URL such as
// https://github.com/owner/repo/pull/12, or an empty string.
inline QString gitHubRepositoryFromPullRequestUrl(const QString &url) {
  const QUrl parsed(url);
  if (parsed.host().compare(QStringLiteral("github.com"),
                            Qt::CaseInsensitive) != 0) {
    return {};
  }

  const QStringList parts =
      parsed.path().split(QLatin1Char('/'), Qt::SkipEmptyParts);
  if (parts.size() < 2) {
    return {};
  }
  return parts.at(0) + QLatin1Char('/') + parts.at(1);
}

} // namespace GitRepo
