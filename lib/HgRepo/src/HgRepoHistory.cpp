// History pages and commit details.

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QJsonObject>

#include <algorithm>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

QString historyRevset(const QString &branchName, GitRepo::BranchType type) {
  if (branchName == QStringLiteral(ALL_BRANCHES)) {
    return QStringLiteral("all()");
  }

  if (branchName.isEmpty() || branchName == QStringLiteral("HEAD")) {
    return QStringLiteral("::.");
  }

  switch (type) {
  case GitRepo::BranchType::Tag:
    return QStringLiteral("::tag(%1)").arg(revsetString(branchName));
  case GitRepo::BranchType::Remote:
    // "path/name" and "path/@name" of the remotenames extension.
    return QStringLiteral("::(%1)").arg(branchRevset(branchName, type));
  default:
    if (branchName.startsWith('@')) {
      return QStringLiteral("::bookmark(%1)")
          .arg(revsetString(branchName.mid(1)));
    }
    return QStringLiteral("::branch(%1)").arg(revsetString(branchName));
  }
}

void addRef(HistoryCursor *cursor, const QString &node, const QString &key,
            const QString &label) {
  QStringList &keys = cursor->refTips[node];
  if (!keys.contains(key)) {
    keys.push_back(key);
  }
  QStringList &labels = cursor->refLabels[node];
  if (!labels.contains(label)) {
    labels.push_back(label);
  }
}

// Branch heads, bookmarks and tags, used as graph color keys and badges.
bool collectRefs(Repository *repo, HistoryCursor *cursor) {
  RunOptions options;
  options.readOnly = true;

  QJsonArray heads;
  if (!runJson(repo, HgError::HistoryFailed,
               {QStringLiteral("log"), QStringLiteral("--rev"),
                QStringLiteral("head() and not closed()")},
               &heads, options)) {
    return false;
  }
  for (const QJsonValue &value : heads) {
    const QJsonObject object = value.toObject();
    const QString branch = object.value("branch").toString();
    addRef(cursor, object.value("node").toString(), branch,
           QStringLiteral("refs/heads/") + branch);
  }

  QJsonArray bookmarks;
  if (!runJson(repo, HgError::HistoryFailed, {QStringLiteral("bookmarks")},
               &bookmarks, options)) {
    return false;
  }
  for (const QJsonValue &value : bookmarks) {
    const QJsonObject object = value.toObject();
    const QString bookmark = object.value("bookmark").toString();
    addRef(cursor, object.value("node").toString(), '@' + bookmark,
           QStringLiteral("refs/bookmarks/") + bookmark);
  }

  QJsonArray tags;
  if (!runJson(repo, HgError::HistoryFailed, {QStringLiteral("tags")}, &tags,
               options)) {
    return false;
  }
  for (const QJsonValue &value : tags) {
    const QJsonObject object = value.toObject();
    const QString tag = object.value("tag").toString();
    if (tag == QStringLiteral("tip")) {
      continue;
    }
    addRef(cursor, object.value("node").toString(), tag,
           QStringLiteral("refs/tags/") + tag);
  }

  return true;
}

bool buildCursor(Repository *repo, const QString &branchName,
                 GitRepo::BranchType type, const QString &key) {
  HistoryCursor cursor;
  cursor.key = key;

  RunOptions options;
  options.readOnly = true;

  HgCommandResult result;
  if (!runCommand(
          repo, HgError::HistoryFailed,
          {QStringLiteral("log"), QStringLiteral("--rev"),
           QStringLiteral("reverse(%1)").arg(historyRevset(branchName, type)),
           QStringLiteral("--template"), QStringLiteral("{node}\n")},
          &result, options)) {
    return false;
  }
  cursor.nodes =
      QString::fromLatin1(result.out).split('\n', Qt::SkipEmptyParts);

  if (!runCommand(repo, HgError::HistoryFailed,
                  {QStringLiteral("log"), QStringLiteral("--rev"),
                   QStringLiteral("."), QStringLiteral("--template"),
                   QStringLiteral("{node}")},
                  &result, options)) {
    return false;
  }
  cursor.headCommit = QString::fromLatin1(result.out).trimmed();
  if (cursor.headCommit == QLatin1String(kNullNode)) {
    cursor.headCommit.clear();
  }

  if (!collectRefs(repo, &cursor)) {
    return false;
  }

  repo->historyCursor = cursor;
  return true;
}

GitRepo::CommitPhase phaseFromString(const QString &phase) {
  if (phase == QLatin1String("public")) {
    return GitRepo::CommitPhase::Public;
  }
  if (phase == QLatin1String("draft")) {
    return GitRepo::CommitPhase::Draft;
  }
  if (phase == QLatin1String("secret")) {
    return GitRepo::CommitPhase::Secret;
  }
  return GitRepo::CommitPhase::None;
}

GitRepo::GitCommitInfo commitInfoFromJson(const QJsonObject &object,
                                          const QString &branchName) {
  GitRepo::GitCommitInfo info;
  const QString node = object.value("node").toString();
  nodeToOid(node, &info.oid);

  info.message = object.value("desc").toString();
  info.summary = summaryOf(info.message);
  splitUser(object.value("user").toString(), &info.authorName,
            &info.authorEmail);
  info.timeStamp = timeStampFromHgDate(object.value("date").toArray());
  info.signatureType = QStringLiteral("Unsigned");
  info.branches += branchName;
  info.phase = phaseFromString(object.value("phase").toString());

  for (const QJsonValue &parent : object.value("parents").toArray()) {
    const QString parentNode = parent.toString();
    if (parentNode != QLatin1String(kNullNode) && parentNode != node) {
      info.parents.push_back(parentNode);
    }
  }

  return info;
}

} // namespace

GitRepo::GitHistory getBranchHistory(Repository *repo,
                                     const QString &branchName,
                                     GitRepo::BranchType type, int page,
                                     const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return {};
  }

  const QuitScope quitScope(&quitFlag);

  const qint64 pageSize = std::max<qint64>(
      1, ConfigManager::ConfigManager::getInstance().getLong(
             ConfigManager::ConfigKey::GitRepoHistoryPageSize));

  const QString key =
      QStringLiteral("%1|%2").arg(static_cast<int>(type)).arg(branchName);

  // The commit order is computed once per history (on page 0) and reused for
  // the following pages, like GitRepo does.
  if (page <= 0 || repo->historyCursor.key != key) {
    if (!buildCursor(repo, branchName, type, key)) {
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  const HistoryCursor &cursor = repo->historyCursor;
  const qint64 total = cursor.nodes.size();
  const qint64 begin = page > 0 ? std::min<qint64>(total, page * pageSize) : 0;
  const qint64 end =
      page < 0 ? total : std::min<qint64>(total, begin + pageSize);

  GitRepo::GitHistory history;
  const QStringList pageNodes =
      cursor.nodes.mid(static_cast<int>(begin), static_cast<int>(end - begin));

  if (!pageNodes.isEmpty()) {
    QJsonArray array;
    RunOptions options;
    options.readOnly = true;
    if (!runJson(repo, HgError::HistoryFailed,
                 {QStringLiteral("log"), QStringLiteral("--rev"),
                  pageNodes.join('+')},
                 &array, options)) {
      LOG_PERF(0, "Exit");
      return {};
    }

    for (const QJsonValue &value : array) {
      const QJsonObject object = value.toObject();
      history.commitInfos.insert(object.value("node").toString(),
                                 commitInfoFromJson(object, branchName));
    }
  }

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // In the order of the cursor, whatever order hg printed them in.
  history.commits.reserve(pageNodes.size());
  for (const QString &node : pageNodes) {
    if (history.commitInfos.contains(node)) {
      history.commits.push_back(node);
    }
  }

  history.valid = true;
  history.total = total;
  history.hasMore = end < total;

  if (page <= 0) {
    history.headCommit = cursor.headCommit;
    history.refTips = cursor.refTips;
    history.refLabels = cursor.refLabels;
    history.bisectMarks = getBisectMarks(repo);
  }

  LOG_PERF(0, "Exit");
  return history;
}

GitRepo::GitCommitStatus getCommitStatus(Repository *repo,
                                         const QString &commitOid) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return {};
  }

  RunOptions options;
  options.readOnly = true;

  QJsonArray array;
  if (!runJson(repo, HgError::CommitStatusFailed,
               {QStringLiteral("log"), QStringLiteral("--rev"), commitOid},
               &array, options)) {
    return {};
  }
  if (array.isEmpty()) {
    setLastError(HgError::CommitStatusFailed, -1,
                 QStringLiteral("Unknown commit %1").arg(commitOid));
    return {};
  }

  const QJsonObject object = array.first().toObject();

  GitRepo::GitCommitStatus status;
  status.commit = object.value("node").toString();
  splitUser(object.value("user").toString(), &status.authorName,
            &status.authorEmail);
  status.committerDate = dateStringFromHgDate(object.value("date").toArray());
  status.summary = summaryOf(object.value("desc").toString());
  status.phase = phaseFromString(object.value("phase").toString());
  for (const QJsonValue &parent : object.value("parents").toArray()) {
    if (parent.toString() != QLatin1String(kNullNode)) {
      status.parentShas += parent.toString();
    }
  }

  QJsonArray changes;
  if (!runJson(repo, HgError::CommitStatusFailed,
               {QStringLiteral("status"), QStringLiteral("--change"),
                status.commit, QStringLiteral("-C")},
               &changes, options)) {
    return {};
  }

  QSet<QString> removed;
  for (const QJsonValue &value : changes) {
    const QJsonObject change = value.toObject();
    if (change.value("status").toString() == QStringLiteral("R")) {
      removed.insert(change.value("path").toString());
    }
  }

  QSet<QString> renameSources;
  for (const QJsonValue &value : changes) {
    const QJsonObject change = value.toObject();
    const QString source = change.value("source").toString();
    if (change.value("status").toString() == QStringLiteral("A") &&
        removed.contains(source)) {
      renameSources.insert(source);
    }
  }

  for (const QJsonValue &value : changes) {
    const QJsonObject change = value.toObject();
    const QString path = change.value("path").toString();
    const QString kind = change.value("status").toString();
    if (kind == QStringLiteral("A")) {
      if (renameSources.contains(change.value("source").toString())) {
        status.renamedFiles += path;
      } else {
        status.newFiles += path;
      }
    } else if (kind == QStringLiteral("M")) {
      status.modifiedFiles += path;
    } else if (kind == QStringLiteral("R") && !renameSources.contains(path)) {
      status.deletedFiles += path;
    }
  }

  return status;
}

} // namespace HgRepo
