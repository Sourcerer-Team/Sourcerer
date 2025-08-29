#include "models/GitHistoryModel.h"

#include "GitRepoManagerStructs.h"

#include <EventManager.h>
#include <Log.h>
#include <Uuid.h>

#define THREAD_ID "GitHistoryModel"

namespace GuiManager {

namespace Models {

GitHistoryModel::GitHistoryModel(QObject *parent)
    : QAbstractTableModel(parent) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

GitHistoryModel::~GitHistoryModel() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::updateHistory(const GitRepo::GitHistory &history) {
  LOG_PERF(0, "Enter");

  beginResetModel();

  _history = history;

  endResetModel();

  LOG_PERF(0, "Exit");
}

QModelIndex GitHistoryModel::index(const int row, const int column,
                                   const QModelIndex &parent) const {
  LOG_PERF(0, "Enter");

  if (parent.isValid() || row < 0 || row >= _history.commits.size()) {
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

  return COLUMN_LAST - COLUMN_FIRST + 1;
}

QVariant GitHistoryModel::data(const QModelIndex &index, const int role) const {
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

  if (role != Qt::DisplayRole) {
    LOG_PERF(0, "Exit");
    return {};
  }

  switch (index.column()) {
  case COLUMN_HISTORY_SLICE: {
    // result = QVariant::fromValue(historySlice(commit));
    break;
  }
  case COLUMN_SUMMARY: {
    result = commitInfo.summary.trimmed();
    break;
  }
  case COLUMN_AUTHOR_NAME: {
    result = commitInfo.authorName.trimmed();
    break;
  }
  case COLUMN_AUTHOR_EMAIL: {
    result = commitInfo.authorEmail.trimmed();
    break;
  }
  case COLUMN_COMMIT_DATETIME: {
    result = QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp)
                 .toString("yyyy-MM-dd HH:mm:ss");
    break;
  }
  case COLUMN_COMMIT_SHA: {
    result = commit.left(6); // Short SHA
    break;
  }
  default:
    break;
  }

  LOG_PERF(0, "Exit");

  return result;
}

QVariant GitHistoryModel::headerData(int section, Qt::Orientation orientation,
                                     int role) const {
  if (role == Qt::DisplayRole) {
    if (orientation == Qt::Horizontal) {
      switch (section) {
      case COLUMN_HISTORY_SLICE:
        return tr("Graph");
      case COLUMN_SUMMARY:
        return tr("Summary");
      case COLUMN_AUTHOR_NAME:
        return tr("Author");
      case COLUMN_AUTHOR_EMAIL:
        return tr("Email");
      case COLUMN_COMMIT_DATETIME:
        return tr("Date");
      case COLUMN_COMMIT_SHA:
        return tr("SHA");
      default:
        return QVariant();
      }
    }
    if (orientation == Qt::Vertical)
      return QVariant(); // No row names
  }
  return QAbstractTableModel::headerData(section, orientation, role);
}

GitRepo::GitHistory GitHistoryModel::historySlice(const QString &commit) const {
  LOG_PERF(0, "Enter");

  GitRepo::GitHistory historySlice;

  historySlice.valid = true;

  historySlice.commits += commit;

  const auto &gitCommitInfo = _history.commitInfos.value(commit);

  historySlice.commitInfos.insert(commit, gitCommitInfo);

  for (const auto &childCommit : gitCommitInfo.children) {
    historySlice.commits += childCommit;
    historySlice.commitInfos.insert(childCommit,
                                    _history.commitInfos.value(childCommit));
  }

  for (const auto &parentCommit : gitCommitInfo.parents) {
    historySlice.commits += parentCommit;
    historySlice.commitInfos.insert(parentCommit,
                                    _history.commitInfos.value(parentCommit));
  }

  LOG_PERF(0, "Exit");

  return historySlice;
}

} // namespace Models

} // namespace GuiManager
