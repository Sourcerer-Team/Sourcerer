#include "models/GitHistoryModel.h"

#include "GitRepoManagerStructs.h"

#include <EventManager.h>
#include <Graph.h>
#include <Log.h>

#include <QQueue>
#include <QSet>

#include <iostream>

#define THREAD_ID "GitHistoryModel"

namespace GuiManager {

namespace Models {

static const Slices emptySlices;

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

  createDrawableSlices();

  printDrawableSlices(_drawableSlices);

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
    result = QVariant::fromValue(historySlices(commit));
    break;
  }
  case COLUMN_HISTORY_SLICE_WIDTH: {
    result = _drawableSlices.sizeX();
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

QVariant GitHistoryModel::headerData(const int section,
                                     const Qt::Orientation orientation,
                                     const int role) const {
  LOG_PERF(0, "Enter");

  if (role == Qt::DisplayRole) {
    QVariant result;

    if (orientation == Qt::Horizontal) {
      switch (section) {
      case COLUMN_HISTORY_SLICE: {
        result = tr("Graph");
        break;
      }
      case COLUMN_SUMMARY: {
        result = tr("Summary");
        break;
      }
      case COLUMN_AUTHOR_NAME: {
        result = tr("Author");
        break;
      }
      case COLUMN_AUTHOR_EMAIL: {
        result = tr("Email");
        break;
      }
      case COLUMN_COMMIT_DATETIME: {
        result = tr("Date");
        break;
      }
      case COLUMN_COMMIT_SHA: {
        result = tr("SHA");
        break;
      }
      default: {
        break;
      }
      }

      LOG_PERF(0, "Exit");
      return result; // No row names
    }

    LOG_PERF(0, "Exit");
    return result; // No row names
  }

  LOG_PERF(0, "Exit");

  return QAbstractTableModel::headerData(section, orientation, role);
}

void GitHistoryModel::createDrawableSlices() {
  LOG_PERF(0, "Enter");

  _drawableSlices.clear();

  for (int i = 0; i < _history.commits.size(); ++i) {
    const QString &currentCommit = _history.commits.at(i);
    const auto it = _history.commitInfos.find(currentCommit);
    if (it == _history.commitInfos.end()) {
      continue;
    }

    const auto &commitInfo = it.value();

    if (i == 0) {
      _drawableSlices[0][0].toCommit = currentCommit;
      _drawableSlices[0][0].main = true;
      continue;
    }

    QMap<QString, int> previousRowCommitToColumnMap;

    for (int j = 0; j < _drawableSlices.sizeX(); ++j) {
      const auto itParent =
          std::find(commitInfo.parents.begin(), commitInfo.parents.end(),
                    _drawableSlices[j][i - 1].toCommit);
      _drawableSlices[j][i - 1].bottom = itParent != commitInfo.parents.end();
      const int index = itParent != commitInfo.parents.end()
                            ? itParent - commitInfo.parents.begin()
                            : -1;
      if (index >= 0 && index < _drawableSlices.sizeX()) {
        _drawableSlices[index][i].top = true;
        _drawableSlices[index][i].toCommit = currentCommit;
        _drawableSlices[index][i].main = true;
      } else if (_drawableSlices[j][i].toCommit.isEmpty()) {
        _drawableSlices[j][i].top = true;
        _drawableSlices[j][i].toCommit = _drawableSlices[j][i - 1].toCommit;
        _drawableSlices[j][i].main = false;
      }

      if (!_drawableSlices[j][i - 1].toCommit.isEmpty()) {
        previousRowCommitToColumnMap[_drawableSlices[j][i - 1].toCommit] = j;
      }
    }

    int currentCommitIndex = -1;

    for (int j = 0; j < _drawableSlices.sizeX(); ++j) {
      if (_drawableSlices[j][i].toCommit == currentCommit) {
        currentCommitIndex = j;
        break;
      }
    }

    if (currentCommitIndex == -1) {
      continue;
    }

    int startLeft = std::numeric_limits<int>::max();
    int startRight = std::numeric_limits<int>::max();

    for (int j = 0; j < currentCommitIndex; ++j) {
      const auto itParent =
          std::find(commitInfo.parents.begin(), commitInfo.parents.end(),
                    _drawableSlices[j][i - 1].toCommit);
      if (itParent != commitInfo.parents.end()) {
        startLeft = std::min(j + 1, startLeft);
        startRight = std::min(j, startRight);
      }
    }

    for (int j = startLeft; j <= currentCommitIndex; ++j) {
      _drawableSlices[j][i].left = true;
    }

    for (int j = startRight; j < currentCommitIndex; ++j) {
      _drawableSlices[j][i].right = true;
    }

    int endLeft = std::numeric_limits<int>::min();
    int endRight = std::numeric_limits<int>::min();

    for (int j = currentCommitIndex + 1; j < _drawableSlices.sizeX(); ++j) {
      const auto itParent =
          std::find(commitInfo.parents.begin(), commitInfo.parents.end(),
                    _drawableSlices[j][i - 1].toCommit);
      if (itParent != commitInfo.parents.end()) {
        endLeft = std::max(j, endLeft);
        endRight = std::max(j - 1, endRight);
      }
    }

    for (int j = currentCommitIndex + 1; j < endLeft; ++j) {
      _drawableSlices[j][i].left = true;
    }

    for (int j = currentCommitIndex; j < endRight; ++j) {
      _drawableSlices[j][i].right = true;
    }
  }

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::printDrawableSlices(
    const Util::ResizableMatrix<Slice> &resizableMatrix) {

  std::cout << std::endl << std::endl;
  std::cout << "Drawable slices: " << std::endl;

  for (int i = 0; i < resizableMatrix.sizeY(); ++i) {
    QString line0;
    QString line1;
    QString line2;
    for (int j = 0; j < resizableMatrix.sizeX(); ++j) {
      const auto &[toCommit, left, right, top, bottom, main] =
          resizableMatrix[j][i];
      if (!toCommit.isEmpty()) {
        if (top) {
          line0 += ".|.";
        } else {
          line0 += "...";
        }

        if (left) {
          line1 += "-";
          if (main) {
            line1 += "O";
          } else {
            line1 += ".";
          }
        } else {
          line1 += ".";
          if (main) {
            line1 += "O";
          } else {
            line1 += ".";
          }
        }

        if (right) {
          line1 += "-";
        } else {
          line1 += ".";
        }

        if (bottom) {
          line2 += ".|.";
        } else {
          line2 += "...";
        }

      } else {
        line0 += "...";
        line1 += "...";
        line2 += "...";
      }
    }

    std::cout << line0.toStdString() << std::endl
              << line1.toStdString() << std::endl
              << line2.toStdString() << std::endl;
  }
}

const Slices &GitHistoryModel::historySlices(const QString &commit) const {
  LOG_PERF(0, "Enter");

  if (_history.commits.size() == 0) {
    LOG_PERF(0, "Exit");
    return emptySlices;
  }

  const int difference = indexDifference(_history.commits.first(), commit);

  if (difference < 0) {
    LOG_PERF(0, "Exit");
    return emptySlices;
  }

  LOG_PERF(0, "Exit");

  return _drawableSlices[_drawableSlices.sizeX() - difference];
}

int GitHistoryModel::indexDifference(const QString &commitA,
                                     const QString &commitB) const {
  LOG_PERF(0, "Enter");

  const auto itA = std::find_if(
      _history.commits.begin(), _history.commits.end(),
      [&](const QString &historyCommit) { return historyCommit == commitA; });

  if (itA == _history.commits.end()) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  const auto itB = std::find_if(
      _history.commits.begin(), _history.commits.end(),
      [&](const QString &historyCommit) { return historyCommit == commitB; });

  if (itB == _history.commits.end()) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  LOG_PERF(0, "Exit");

  return itB - itA;
}

} // namespace Models

} // namespace GuiManager
