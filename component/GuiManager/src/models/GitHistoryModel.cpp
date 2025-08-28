#include "models/GitHistoryModel.h"

#include "GitRepoManagerStructs.h"

#include <EventManager.h>
#include <Log.h>
#include <Uuid.h>

#define THREAD_ID "GitHistoryModel"

namespace GuiManager {

namespace Models {

GitHistoryModel::GitHistoryModel(QObject *parent) : QAbstractListModel(parent) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

GitHistoryModel::~GitHistoryModel() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::setRepoUuid(const QString &uuid) {
  LOG_PERF(0, "Enter");

  _repoUuid = uuid;

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::updateHistory(const GitRepo::GitHistory &history) {
  LOG_PERF(0, "Enter");

  beginResetModel();

  _history = history;

  endResetModel();

  LOG_PERF(0, "Exit");
}

QModelIndex GitHistoryModel::index(int row, int column,
                                   const QModelIndex &parent) const {
  LOG_PERF(0, "Enter");

  if (parent.isValid() || column != 0 || row < 0 ||
      row >= _history.commits.size()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_PERF(0, "Exit");

  return createIndex(row, column);
}

int GitHistoryModel::rowCount(const QModelIndex &parent) const {
  LOG_PERF(0, "Enter");

  if (parent.isValid()) {
    LOG_PERF(0, "Exit");
    return 0;
  }

  LOG_PERF(0, "Exit");

  return _history.commits.size();
}

int GitHistoryModel::columnCount(const QModelIndex &parent) const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return ROLE_LAST - ROLE_FIRST + 1;
}

QVariant GitHistoryModel::data(const QModelIndex &index, int role) const {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  if (index.row() < 0 || index.row() >= _history.commits.size()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString &commit = _history.commits.at(index.row());
  const GitRepo::GitCommitInfo &commitInfo = _history.commitInfos.value(commit);

  QVariant result;

  switch (role) {
  case ROLE_HISTORY_SLICE: {
    result = QVariant::fromValue(historySlice(commit));
    break;
  }
  case ROLE_SUMMARY: {
    result = commitInfo.summary;
    break;
  }
  case ROLE_AUTHOR_NAME: {
    result = commitInfo.authorName;
    break;
  }
  case ROLE_AUTHOR_EMAIL: {
    result = commitInfo.authorEmail;
    break;
  }
  case ROLE_COMMIT_DATETIME: {
    result = QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp)
                 .toString(Qt::ISODate);
    break;
  }
  case ROLE_COMMIT_SHA: {
    result = commit;
    break;
  }
  default:
    break;
  }

  LOG_PERF(0, "Exit");

  return result;
}

QHash<int, QByteArray> GitHistoryModel::roleNames() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return {{ROLE_HISTORY_SLICE, "historySlice"},
          {ROLE_SUMMARY, "summary"},
          {ROLE_AUTHOR_NAME, "authorName"},
          {ROLE_AUTHOR_EMAIL, "authorEmail"},
          {ROLE_COMMIT_DATETIME, "dateTime"},
          {ROLE_COMMIT_SHA, "sha"}};
}

GitRepo::GitHistory GitHistoryModel::historySlice(const QString &commit) const {
  LOG_PERF(0, "Enter");

  GitRepo::GitHistory historySlice;

  historySlice.valid = true;

  historySlice.commits += commit;

  const auto& gitCommitInfo = _history.commitInfos.value(commit);

  historySlice.commitInfos.insert(commit, gitCommitInfo);

  for (const auto& childCommit : gitCommitInfo.children) {
    historySlice.commits += childCommit;
    historySlice.commitInfos.insert(childCommit, _history.commitInfos.value(childCommit));
  }

  for (const auto& parentCommit : gitCommitInfo.parents) {
    historySlice.commits += parentCommit;
    historySlice.commitInfos.insert(parentCommit, _history.commitInfos.value(parentCommit));
  }

  LOG_PERF(0, "Exit");

  return historySlice;
}

} // namespace Models

} // namespace GuiManager
