#include "models/GitHistoryModel.h"

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
    : QAbstractTableModel(parent), _quitUpdate(false) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

GitHistoryModel::~GitHistoryModel() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::updateHistory(const GitRepo::GitHistory &history, const std::function<void(int)> progressCallback) {
  LOG_PERF(0, "Enter");

  beginResetModel();

  _quitUpdate = false;

  _history = history;

  createDrawableSlices([&](int progress) {
    if(progressCallback) {
      progressCallback(progress / 2);
    }
  });

  std::reverse(_history.commits.begin(), _history.commits.end());

  _drawableSlices.invertVertically([&](int progress) {
    if(progressCallback) {
      progressCallback(50 + (progress / 4));
    }
  });

  for (qsizetype i = 0; i < _drawableSlices.sizeX(); ++i) {
    if(progressCallback) {
      const double progress = (i * 100.0) / _drawableSlices.sizeX();
      progressCallback(75 + (progress / 4));
    }

    for (qsizetype j = 0; j < _drawableSlices.sizeY(); ++j) {
      std::swap(_drawableSlices[i][j].top, _drawableSlices[i][j].bottom);
    }
  }

  if(progressCallback) {
    progressCallback(100);
  }

#ifdef QT_DEBUG
  // printDrawableSlices(_drawableSlices);
#endif

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

  if (role == HistorySliceWidthRole) {
    LOG_PERF(0, "Exit");
    return _drawableSlices.sizeX();
  }

  if (role != Qt::DisplayRole) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString &commit = _history.commits.at(index.row());
  const GitRepo::GitCommitInfo &commitInfo = _history.commitInfos.value(commit);

  QVariant result;

  switch (index.column()) {
  case COLUMN_HISTORY_SLICE: {
    result = QVariant::fromValue(historySlices(commit));
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
  case COLUMN_COMMIT_SHORT_SHA: {
    result = commit.left(6); // Short SHA
    break;
  }
  case COLUMN_COMMIT_FULL_SHA: {
    result = commit; // Full SHA
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
      case COLUMN_COMMIT_SHORT_SHA: {
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

void GitHistoryModel::createDrawableSlices(const std::function<void(int)> progressCallback) {
  LOG_PERF(0, "Enter");

  _drawableSlices.clear();

  try {
    QMap<QString, QPair<int, int>> commitPositions;

    for (int i = 0; i < _history.commits.size(); ++i) {
      if(_quitUpdate) {
        _drawableSlices.clear();
        LOG_PERF(0, "Exit");
        return;
      }

      const double progress = (i * 100.0) / _history.commits.size();
      if(progressCallback) {
        progressCallback(static_cast<int>(progress));
      }

      const QString &currentCommit = _history.commits.at(i);
      const auto it = _history.commitInfos.find(currentCommit);
      if (it == _history.commitInfos.end()) {
        continue;
      }

      const auto &commitInfo = it.value();

      if (commitInfo.parents.size() == 0) {
        _drawableSlices.insertRow(0);
        _drawableSlices.insertColumn(0);
        _drawableSlices[0][0].toCommit = currentCommit;
        _drawableSlices[0][0].main = true;

        for(auto& pos : commitPositions) {
          pos.first += 1;
        };

        commitPositions[currentCommit] = qMakePair(0, 0);

        continue;
      }

      QSet<qsizetype> emptyColumnIndexes;

      QVector<QString> missingParents;
      int maxDanglingParent = -1;

      for (const auto &parent : commitInfo.parents) {
        if (!commitPositions.contains(parent)) {
          int danglingParentIndex = -1;
          for(int j = 0; j < _history.commits.size(); ++j) {
            if(_history.commits[j] == parent) {
              danglingParentIndex = j;
              break;
            }
          }

          if(danglingParentIndex == -1) {
            if(!missingParents.contains(parent)) {
              missingParents += parent;
            }
          };

          maxDanglingParent = std::max(maxDanglingParent, danglingParentIndex);
        }
      }

      if(!missingParents.isEmpty()) {
        // Skip commits with missing parents (e.g., due to shallow clone)
        continue;
      } else if(maxDanglingParent != -1) {
        const int newRow = std::max(i + 1, maxDanglingParent + 1);

        _history.commits.insert(newRow, currentCommit);
        _history.commits.remove(i);

        --i;

        continue;
      }

      for (const auto &parent : commitInfo.parents) {

        qsizetype emptyColumnIndex = -1;
        qsizetype parentX = -1;
        qsizetype parentY = -1;

        if (commitPositions.contains(parent)) {
          const auto &[x, y] = commitPositions[parent];
          parentX = x;
          parentY = y;
        }

        Q_ASSERT(parentX != -1);
        Q_ASSERT(parentY != -1);

        if(parentX == -1 || parentY == -1) {
          continue;
        }

        QVector<qsizetype> indexes;

        indexes += parentX;

        for (qsizetype k = 0; k < _drawableSlices.sizeX(); ++k) {
          if(!indexes.contains(k)) {
            indexes += k;
          }
        }

        for (const auto &index : indexes) {
          bool columnIsEmpty = true;
          for (qsizetype l = parentY + 1; l < i; ++l) {
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

        QList<QString>::const_iterator itParent = commitInfo.parents.constEnd();

        if (startY > 0) {
          itParent = std::find(
              commitInfo.parents.begin(), commitInfo.parents.end(),
              _drawableSlices[emptyColumnIndex][startY - 1].toCommit);
        }

        for (qsizetype k = startY; k <= i; ++k) {
          if(!_drawableSlices[emptyColumnIndex][k].bottom) {
            _drawableSlices[emptyColumnIndex][k].bottom = k != i;
          }
          if(!_drawableSlices[emptyColumnIndex][k].top) {
            _drawableSlices[emptyColumnIndex][k].top =
              k == startY ? itParent != commitInfo.parents.end() : k != i;
          }
          _drawableSlices[emptyColumnIndex][k].toCommit =
              _drawableSlices[emptyColumnIndex][k].toCommit.isEmpty()
                  ? currentCommit
                  : _drawableSlices[emptyColumnIndex][k].toCommit;
          _drawableSlices[emptyColumnIndex][k].main = false;
        }

        if (parentX < emptyColumnIndex) {
          for (qsizetype k = parentX; k <= emptyColumnIndex; ++k) {
            if(!_drawableSlices[k][parentY].right) {
              _drawableSlices[k][parentY].right = k != emptyColumnIndex;
            }
            if(!_drawableSlices[k][parentY].left) {
              _drawableSlices[k][parentY].left = k != parentX;
            }
          }
        } else if (parentX > emptyColumnIndex) {
          for (qsizetype k = emptyColumnIndex; k <= parentX; ++k) {
            if(!_drawableSlices[k][parentY].right) {
              _drawableSlices[k][parentY].right = k != parentX;
            }
            if(!_drawableSlices[k][parentY].left) {
              _drawableSlices[k][parentY].left = k != emptyColumnIndex;
            }
          }
        } else {
          _drawableSlices[emptyColumnIndex][parentY].bottom = true;
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
      _drawableSlices[firstIndex][i].toCommit = currentCommit;
      _drawableSlices[firstIndex][i].main = true;

      for (qsizetype j = firstIndex; j <= lastIndex; ++j) {
        _drawableSlices[j][i].bottom = false;
        _drawableSlices[j][i].top = emptyColumnIndexes.contains(j);
        if(!_drawableSlices[j][i].left) {
          _drawableSlices[j][i].left = j != firstIndex;
        }
        if(!_drawableSlices[j][i].right) {
          _drawableSlices[j][i].right = j != lastIndex;
        }
      }
    }
  } catch (const std::out_of_range &e) {
    LOG_ERROR(Log::ErrorId::Generic, "Exception: {}", e.what());
    _drawableSlices.clear();
  }

  LOG_PERF(0, "Exit");
}

#ifdef QT_DEBUG
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
#endif

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
    for (int i = 0; i < _drawableSlices.sizeX(); ++i) {
      const Slice &slice = _drawableSlices[i][difference];
      slices += slice;
    }
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
