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

  try {
    QMap<QString, QPair<int, int>> commitPositions;

    for (int i = 0; i < _history.commits.size(); ++i) {
      const QString &currentCommit = _history.commits.at(i);
      const auto it = _history.commitInfos.find(currentCommit);
      if (it == _history.commitInfos.end()) {
        continue;
      }

      const auto &commitInfo = it.value();

      if (i == 0) {
        commitPositions[currentCommit] = qMakePair(0, 0);
        _drawableSlices[0][0].toCommit = currentCommit;
        _drawableSlices[0][0].main = true;
        continue;
      }

      QSet<qsizetype> emptyColumnIndexes;

      for (int j = 0; j < commitInfo.parents.size(); ++j) {

        qsizetype emptyColumnIndex = -1;
        qsizetype parentX = -1;
        qsizetype parentY = -1;

        if (commitPositions.contains(commitInfo.parents[j])) {
          const auto &[x, y] = commitPositions[commitInfo.parents[j]];
          parentX = x;
          parentY = y;
        }

        if (parentX == -1 || parentY == -1) {
          for (int k = 0; k < _drawableSlices.sizeX(); ++k) {
            if (_drawableSlices[k][i - 1].toCommit.isEmpty()) {
              emptyColumnIndex = k;
              break;
            }
          }

          if (emptyColumnIndex == -1) {
            emptyColumnIndex = _drawableSlices.sizeX();
          }

          commitPositions[currentCommit] = qMakePair(emptyColumnIndex, i);
          _drawableSlices[emptyColumnIndex][i].top = false;
          _drawableSlices[emptyColumnIndex][i].toCommit = currentCommit;
          _drawableSlices[emptyColumnIndex][i].main = true;

        } else {
          QVector<qsizetype> indexes;

          indexes += parentX;

          for (qsizetype k = 0; k < _drawableSlices.sizeX(); ++k) {
            indexes += k;
          }

          for (const auto &index : indexes) {
            bool columnIsEmpty = true;
            for (int l = parentY + 1; l < i; ++l) {
              columnIsEmpty &= _drawableSlices[index][l].toCommit.isEmpty();
              if (!columnIsEmpty) {
                break;
              }
            }

            if (columnIsEmpty) {
              emptyColumnIndex = index;
              break;
            }
          }

          if (emptyColumnIndex == -1) {
            emptyColumnIndex = _drawableSlices.sizeX();
          }

          const qsizetype startY =
              emptyColumnIndex == parentX ? parentY + 1 : parentY;

          for (qsizetype k = startY; k <= i; ++k) {
            _drawableSlices[emptyColumnIndex][k].bottom = k != i;
            _drawableSlices[emptyColumnIndex][k].top = k != parentY;
            _drawableSlices[emptyColumnIndex][k].toCommit =
                _drawableSlices[emptyColumnIndex][k].toCommit.isEmpty()
                    ? currentCommit
                    : _drawableSlices[emptyColumnIndex][k].toCommit;
            _drawableSlices[emptyColumnIndex][k].main = false;
          }

          if (parentX < emptyColumnIndex) {
            for (qsizetype k = parentX; k <= emptyColumnIndex; ++k) {
              _drawableSlices[k][parentY].right = k != emptyColumnIndex;
              _drawableSlices[k][parentY].left = k != parentX;
            }
          } else if (parentX > emptyColumnIndex) {
            for (qsizetype k = emptyColumnIndex; k <= parentX; ++k) {
              _drawableSlices[k][parentY].right = k != parentX;
              _drawableSlices[k][parentY].left = k != emptyColumnIndex;
            }
          } else {
            _drawableSlices[emptyColumnIndex][parentY].bottom = true;
          }
        }

        emptyColumnIndexes += emptyColumnIndex;
      }

      QList<qsizetype> indexes = emptyColumnIndexes.values();

      std::sort(indexes.begin(), indexes.end());

      if (indexes.isEmpty()) {
        _drawableSlices.clear();
        LOG_PERF(0, "Exit");
        return;
      }

      const qsizetype firstIndex = indexes.first();
      const qsizetype lastIndex = indexes.last();

      commitPositions[currentCommit] = qMakePair(firstIndex, i);
      _drawableSlices[firstIndex][i].bottom = false;
      _drawableSlices[firstIndex][i].top = true;
      _drawableSlices[firstIndex][i].toCommit = currentCommit;
      _drawableSlices[firstIndex][i].main = true;

      _drawableSlices[lastIndex][i].bottom = false;
      _drawableSlices[lastIndex][i].top = true;

      for (qsizetype j = firstIndex; j <= lastIndex; ++j) {
        _drawableSlices[firstIndex][i].bottom = false;
        _drawableSlices[firstIndex][i].top = emptyColumnIndexes.contains(j);
        _drawableSlices[firstIndex][i].left = j != firstIndex;
        _drawableSlices[firstIndex][i].right = j != lastIndex;
      }
    }
  } catch (const std::out_of_range &e) {
    LOG_ERROR(Log::ErrorId::Generic, "Exception: {}", e.what());
    _drawableSlices.clear();
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

Slices GitHistoryModel::historySlices(const QString &commit) const {
  LOG_PERF(0, "Enter");

  if (_history.commits.size() == 0) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const int difference = indexDifference(_history.commits.first(), commit);

  if (difference < 0) {
    LOG_PERF(0, "Exit");
    return {};
  }

  Slices slices;

  try {
    slices = _drawableSlices[difference];
  } catch (const std::out_of_range &e) {
    LOG_ERROR(Log::ErrorId::Generic, "Exception: {}", e.what());
  }

  LOG_PERF(0, "Exit");

  return slices;
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
