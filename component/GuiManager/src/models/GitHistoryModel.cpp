#include "models/GitHistoryModel.h"

#include <EventManager.h>
#include <Graph.h>
#include <Log.h>

#include <QDateTime>
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QLocale>
#include <QQueue>
#include <QSet>

#include <iostream>

#define THREAD_ID "GitHistoryModel"

#define MULTIPLIER 5

namespace GuiManager::Models {

static StatusFieldsMap createStatusFieldsMap(const GitRepo::GitStatus &status) {
  LOG_PERF(0, "Enter");

  if (!status.hasChanges()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const StatusFieldsMap statusMap{
      {StatusFields::New, status.newFiles.size()},
      {StatusFields::Modified, status.modifiedFiles.size()},
      {StatusFields::Deleted, status.deletedFiles.size()},
      {StatusFields::TypeChanged, status.typeChangedFiles.size()},
      {StatusFields::Renamed, status.renamedFiles.size()},
      {StatusFields::Unreadable, status.unreadableFiles.size()},
      {StatusFields::NewStaged, status.newStagedFiles.size()},
      {StatusFields::ModifiedStaged, status.modifiedStagedFiles.size()},
      {StatusFields::DeletedStaged, status.deletedStagedFiles.size()},
      {StatusFields::TypeChangedStaged, status.typeChangedStagedFiles.size()},
      {StatusFields::RenamedStaged, status.renamedStagedFiles.size()},
      {StatusFields::Ignored, status.ignoredFiles.size()},
      {StatusFields::Conflicted, status.conflictedFiles.size()}};

  LOG_PERF(0, "Exit");

  return statusMap;
}

GitHistoryModel::GitHistoryModel(QObject *parent)
    : QAbstractTableModel(parent), _quitUpdate(false),
      _selectionModel(nullptr) {
  LOG_PERF(0, "Enter");

  _columnWidths = QVector<int>(COLUMN_LAST + 1, 1);
  const QFontMetrics metrics(QGuiApplication::font());
  const QStringList headers = {tr("Graph"), tr("Summary"), tr("Author"),
                               tr("Email"), tr("Date"),    tr("SHA")};
  const qsizetype one0 = metrics.horizontalAdvance("0");
  for (int column = COLUMN_FIRST; column <= COLUMN_LAST; ++column) {
    _columnWidths[column] = std::max(
        qsizetype(1),
        metrics.horizontalAdvance(headers.value(column)) + MULTIPLIER * one0);
  }

  LOG_PERF(0, "Exit");
}

GitHistoryModel::~GitHistoryModel() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::updateStatus(const GitRepo::GitStatus &status) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  beginResetModel();

  _historyWithStatus.status = status;
  rebuildVisibleRows();

  endResetModel();

  LOG_PERF(0, "Exit");
}

void GitHistoryModel::setDateRange(const QDate &fromDate, const QDate &toDate) {
  LOCKGUARD(_mutex);

  if (_fromDate == fromDate && _toDate == toDate) {
    return;
  }

  _fromDate = fromDate;
  _toDate = toDate;

  beginResetModel();
  rebuildVisibleRows();
  endResetModel();
}

void GitHistoryModel::setFilter(const int column, const QString &filter) {
  if (column <= COLUMN_HISTORY_SLICE || column == COLUMN_COMMIT_DATETIME ||
      column > COLUMN_COMMIT_SHORT_SHA) {
    return;
  }

  const QString normalizedFilter = filter.trimmed();

  LOCKGUARD(_mutex);
  if (normalizedFilter.isEmpty()) {
    _filters.remove(column);
  } else {
    _filters.insert(column, normalizedFilter);
  }

  beginResetModel();
  rebuildVisibleRows();
  endResetModel();
}

void GitHistoryModel::updateCommitRefs(
    const QHash<QString, QStringList> &commitRefs) {
  LOCKGUARD(_mutex);

  _commitRefs.insert(commitRefs);
}

bool GitHistoryModel::commitMatchesFilters(const QString &commit) const {
  const GitRepo::GitCommitInfo &commitInfo =
      _historyWithStatus.history.commitInfos.value(commit);

  const auto matches = [this](const int column, const QString &value) {
    const QString filter = _filters.value(column);
    return filter.isEmpty() || value.contains(filter, Qt::CaseInsensitive);
  };

  if (!matches(COLUMN_SUMMARY, commitInfo.summary.trimmed()) ||
      !matches(COLUMN_AUTHOR_NAME, commitInfo.authorName.trimmed()) ||
      !matches(COLUMN_AUTHOR_EMAIL, commitInfo.authorEmail.trimmed())) {
    return false;
  }

  const QDate commitDate =
      QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp).toUTC().date();
  if ((_fromDate.isValid() && commitDate < _fromDate) ||
      (_toDate.isValid() && commitDate > _toDate)) {
    return false;
  }

  const QString shortSha = commit.left(7);
  const QString shaFilter = _filters.value(COLUMN_COMMIT_SHORT_SHA);
  return shaFilter.isEmpty() ||
         shortSha.contains(shaFilter, Qt::CaseInsensitive) ||
         commit.contains(shaFilter, Qt::CaseInsensitive);
}

void GitHistoryModel::rebuildVisibleRows() {
  _visibleRows.clear();
  if (_historyWithStatus.status.hasChanges()) {
    _visibleRows.append(0);
  }

  const int sourceRowOffset = _historyWithStatus.status.hasChanges() ? 1 : 0;
  for (int row = 0; row < _historyWithStatus.history.commits.size(); ++row) {
    if (commitMatchesFilters(_historyWithStatus.history.commits.at(row))) {
      _visibleRows.append(row + sourceRowOffset);
    }
  }
}

void GitHistoryModel::updateHistoryAndStatus(
    const GitRepo::GitHistoryWithStatus &historyWithStatus,
    const std::function<void(int)> &progressCallback) {
  LOG_PERF(0, "Enter");

  {
    LOCKGUARD(_mutex);
    // Save us from updates with same data
    if (_historyWithStatus == historyWithStatus) {
      LOG_PERF(0, "Exit");
      return;
    }
  }

  const auto self = getSelf();

  _quitUpdate = false;

  {
    LOCKGUARD(_mutex);
    // Release memory especialy for large repositories
    _drawableSlices.clear();
    _historyWithStatus = GitRepo::GitHistoryWithStatus();
  }

  GitRepo::GitHistoryWithStatus newHistoryWithStatus = historyWithStatus;

  QVector<int> newColumnWidths(COLUMN_LAST + 1, 0);
  Util::ResizableMatrix<Slice> newDrawableSlices =
      createDrawableSlices(newHistoryWithStatus, _quitUpdate, &newColumnWidths,
                           [&](const int progress) {
                             if (progressCallback) {
                               progressCallback(progress / 2);
                             }
                           });

  std::reverse(newHistoryWithStatus.history.commits.begin(),
               newHistoryWithStatus.history.commits.end());

  newDrawableSlices.invertVertically(_quitUpdate, [&](const int progress) {
    if (progressCallback) {
      progressCallback(50 + (progress / 4));
    }
  });

  for (qsizetype i = 0; i < newDrawableSlices.sizeX() && !_quitUpdate; ++i) {
    if (progressCallback) {
      const double progress = (i * 100.0) / newDrawableSlices.sizeX();
      progressCallback(75 + (progress / 4));
    }

    for (qsizetype j = 0; j < newDrawableSlices.sizeY() && !_quitUpdate; ++j) {
      std::swap(newDrawableSlices[i][j].top, newDrawableSlices[i][j].bottom);
    }
  }

  if (progressCallback) {
    progressCallback(100);
  }

  if (!_quitUpdate) {
    LOCKGUARD(_mutex);

    beginResetModel();

    _drawableSlices = newDrawableSlices;
    _historyWithStatus = newHistoryWithStatus;
    _columnWidths = newColumnWidths;
    rebuildVisibleRows();

    endResetModel();
  }

  LOG_PERF(0, "Exit");
}

bool GitHistoryModel::hasStatus() const {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  LOG_PERF(0, "Exit");
  return _historyWithStatus.status.hasChanges();
}

QModelIndex GitHistoryModel::index(const int row, const int column,
                                   const QModelIndex &parent) const {
  LOCKGUARD(_mutex);

  const int count = _visibleRows.size();

  if (parent.isValid() || row < 0 || row >= count) {
    return {};
  }

  return createIndex(row, column);
}

int GitHistoryModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid()) {
    return 0;
  }

  LOCKGUARD(_mutex);

  return _visibleRows.size();
}

int GitHistoryModel::columnCount(const QModelIndex &parent) const {
  return COLUMN_LAST - COLUMN_FIRST + 1;
}

QVariant GitHistoryModel::data(const QModelIndex &index, const int role) const {
  if (!index.isValid()) {
    return {};
  }

  LOCKGUARD(_mutex);

  if (index.row() < 0 || index.row() >= _visibleRows.size()) {
    return {};
  }

  if (_selectionModel && role == Qt::FontRole &&
      _selectionModel->isSelected(index)) {
    QFont font;
    font.setBold(true);
    return font;
  }

  const int sourceRow = _visibleRows.at(index.row());

  if (role == HistorySliceWidthRole) {
    if (sourceRow == 0) {
      int width = _drawableSlices.sizeX();
      if (_historyWithStatus.status.hasChanges()) {
        width = 0;
      }

      return width;
    }

    return _drawableSlices.sizeX();
  }

  if (role == StatusRole) {
    if (sourceRow == 0 && _historyWithStatus.status.hasChanges()) {
      return QVariant::fromValue(
          createStatusFieldsMap(_historyWithStatus.status));
    }

    return {};
  }

  if (role == CommitRefsRole) {
    if (_historyWithStatus.status.hasChanges() && sourceRow == 0) {
      return {};
    }

    const int indexRow =
        _historyWithStatus.status.hasChanges() ? sourceRow - 1 : sourceRow;
    if (indexRow < 0 ||
        indexRow >= _historyWithStatus.history.commits.count()) {
      return {};
    }

    const QString &commit = _historyWithStatus.history.commits.at(indexRow);
    return _commitRefs.value(commit);
  }

  if (role != Qt::DisplayRole) {
    return {};
  }

  if (_historyWithStatus.status.hasChanges() && sourceRow == 0) {
    return {};
  }

  const int indexRow =
      _historyWithStatus.status.hasChanges() ? sourceRow - 1 : sourceRow;

  if (indexRow < 0 || indexRow >= _historyWithStatus.history.commits.count()) {
    return {};
  }

  const QString &commit = _historyWithStatus.history.commits.at(indexRow);
  const GitRepo::GitCommitInfo &commitInfo =
      _historyWithStatus.history.commitInfos.value(commit);

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
    // commitInfo.timeStamp is already in local time
    result = QLocale::system().toString(
        QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp).toUTC(),
        QLocale::ShortFormat);
    break;
  }
  case COLUMN_COMMIT_SHORT_SHA: {
    result = commit.left(7); // Short SHA
    break;
  }
  case COLUMN_COMMIT_FULL_SHA: {
    result = commit; // Full SHA
    break;
  }
  default:
    break;
  }

  return result;
}

QVariant GitHistoryModel::headerData(const int section,
                                     const Qt::Orientation orientation,
                                     const int role) const {
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

      return result; // No row names
    }

    return result; // No row names
  }

  return QAbstractTableModel::headerData(section, orientation, role);
}

Util::ResizableMatrix<Slice> GitHistoryModel::createDrawableSlices(
    GitRepo::GitHistoryWithStatus &historyWithStatus, const bool &quitUpdate,
    QVector<int> *columnWidths,
    const std::function<void(int)> &progressCallback) {
  LOG_PERF(0, "Enter");

  Util::ResizableMatrix<Slice> drawableSlices;
  QVector<int> widths(COLUMN_LAST + 1, 0);
  const QFontMetrics metrics(QGuiApplication::font());
  const qsizetype one0 = metrics.horizontalAdvance("0");
  const auto textWidth = [&](const QString &text) {
    return std::max(qsizetype(1),
                    metrics.horizontalAdvance(text) + MULTIPLIER * one0);
  };
  const auto updateWidth = [&](const int column, const int width) {
    if (column >= 0 && column < widths.size()) {
      widths[column] = std::max(widths[column], width);
    }
  };
  const auto updateSliceWidth = [&]() {
    updateWidth(COLUMN_HISTORY_SLICE, static_cast<int>(drawableSlices.sizeX()) *
                                          std::max(1, metrics.height()));
  };

  try {
    QMap<QString, QPair<int, int>> commitPositions;

    for (int i = 0; i < historyWithStatus.history.commits.size() && !quitUpdate;
         ++i) {
      if (quitUpdate) {
        drawableSlices.clear();
        LOG_PERF(0, "Exit");
        return {};
      }

      const double progress =
          (i * 100.0) / historyWithStatus.history.commits.size();
      if (progressCallback) {
        progressCallback(static_cast<int>(progress));
      }

      const QString &currentCommit = historyWithStatus.history.commits.at(i);
      const auto it = historyWithStatus.history.commitInfos.find(currentCommit);
      if (it == historyWithStatus.history.commitInfos.end()) {
        continue;
      }

      const auto &commitInfo = it.value();

      updateWidth(COLUMN_SUMMARY, textWidth(commitInfo.summary.trimmed()));
      updateWidth(COLUMN_AUTHOR_NAME,
                  textWidth(commitInfo.authorName.trimmed()));
      updateWidth(COLUMN_AUTHOR_EMAIL,
                  textWidth(commitInfo.authorEmail.trimmed()));
      updateWidth(
          COLUMN_COMMIT_DATETIME,
          textWidth(QLocale::system().toString(
              QDateTime::fromMSecsSinceEpoch(commitInfo.timeStamp).toUTC(),
              QLocale::ShortFormat)));
      updateWidth(COLUMN_COMMIT_SHORT_SHA, textWidth(currentCommit.left(7)));
      updateWidth(COLUMN_COMMIT_FULL_SHA, textWidth(currentCommit));

      if (commitInfo.parents.empty()) {
        drawableSlices.insertRow(0);
        drawableSlices.insertColumn(0);
        drawableSlices[0][0].toCommit = currentCommit;
        drawableSlices[0][0].main = true;

        for (auto &pos : commitPositions) {
          pos.first += 1;
        };

        commitPositions[currentCommit] = qMakePair(0, 0);
        updateSliceWidth();

        continue;
      }

      QSet<qsizetype> emptyColumnIndexes;

      QVector<QString> missingParents;
      int maxDanglingParent = -1;

      for (const auto &parent : commitInfo.parents) {
        if (!commitPositions.contains(parent)) {
          int danglingParentIndex = -1;
          for (int j = 0; j < historyWithStatus.history.commits.size(); ++j) {
            if (historyWithStatus.history.commits[j] == parent) {
              danglingParentIndex = j;
              break;
            }
          }

          if (danglingParentIndex == -1) {
            if (!missingParents.contains(parent)) {
              missingParents += parent;
            }
          };

          maxDanglingParent = std::max(maxDanglingParent, danglingParentIndex);
        }
      }

      if (!missingParents.isEmpty()) {
        // Skip commits with missing parents (e.g., due to shallow clone)
        continue;
      } else if (maxDanglingParent != -1) {
        const int newRow = std::max(i + 1, maxDanglingParent + 1);

        historyWithStatus.history.commits.insert(newRow, currentCommit);
        historyWithStatus.history.commits.remove(i);

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

        if (parentX == -1 || parentY == -1) {
          continue;
        }

        QVector<qsizetype> indexes;

        indexes += parentX;

        for (qsizetype k = 0; k < drawableSlices.sizeX(); ++k) {
          if (!indexes.contains(k)) {
            indexes += k;
          }
        }

        for (const auto &index : indexes) {
          bool columnIsEmpty = true;
          for (qsizetype l = parentY + 1; l < i; ++l) {
            columnIsEmpty &= drawableSlices[index][l].toCommit.isEmpty();
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
          emptyColumnIndex = drawableSlices.sizeX();
        }

        const qsizetype startY =
            emptyColumnIndex == parentX ? parentY + 1 : parentY;

        QList<QString>::const_iterator itParent = commitInfo.parents.constEnd();

        if (startY > 0) {
          itParent =
              std::find(commitInfo.parents.begin(), commitInfo.parents.end(),
                        drawableSlices[emptyColumnIndex][startY - 1].toCommit);
        }

        for (qsizetype k = startY; k <= i; ++k) {
          if (!drawableSlices[emptyColumnIndex][k].bottom) {
            drawableSlices[emptyColumnIndex][k].bottom = k != i;
          }
          if (!drawableSlices[emptyColumnIndex][k].top) {
            drawableSlices[emptyColumnIndex][k].top =
                k == startY ? itParent != commitInfo.parents.end() : k != i;
          }
          drawableSlices[emptyColumnIndex][k].toCommit =
              drawableSlices[emptyColumnIndex][k].toCommit.isEmpty()
                  ? currentCommit
                  : drawableSlices[emptyColumnIndex][k].toCommit;
          drawableSlices[emptyColumnIndex][k].main = false;
        }

        if (parentX < emptyColumnIndex) {
          for (qsizetype k = parentX; k <= emptyColumnIndex; ++k) {
            if (!drawableSlices[k][parentY].right) {
              drawableSlices[k][parentY].right = k != emptyColumnIndex;
            }
            if (!drawableSlices[k][parentY].left) {
              drawableSlices[k][parentY].left = k != parentX;
            }
          }
        } else if (parentX > emptyColumnIndex) {
          for (qsizetype k = emptyColumnIndex; k <= parentX; ++k) {
            if (!drawableSlices[k][parentY].right) {
              drawableSlices[k][parentY].right = k != parentX;
            }
            if (!drawableSlices[k][parentY].left) {
              drawableSlices[k][parentY].left = k != emptyColumnIndex;
            }
          }
        } else {
          drawableSlices[emptyColumnIndex][parentY].bottom = true;
        }

        emptyColumnIndexes += emptyColumnIndex;
      }

      QList<qsizetype> indexes = emptyColumnIndexes.values();

      std::sort(indexes.begin(), indexes.end());

      if (indexes.isEmpty()) {
        drawableSlices.clear();
        LOG_PERF(0, "Exit");
        return {};
      }

      const qsizetype firstIndex = indexes.first();
      const qsizetype lastIndex = indexes.last();

      commitPositions[currentCommit] = qMakePair(firstIndex, i);
      drawableSlices[firstIndex][i].toCommit = currentCommit;
      drawableSlices[firstIndex][i].main = true;

      for (qsizetype j = firstIndex; j <= lastIndex; ++j) {
        drawableSlices[j][i].bottom = false;
        drawableSlices[j][i].top = emptyColumnIndexes.contains(j);
        if (!drawableSlices[j][i].left) {
          drawableSlices[j][i].left = j != firstIndex;
        }
        if (!drawableSlices[j][i].right) {
          drawableSlices[j][i].right = j != lastIndex;
        }
      }

      updateSliceWidth();
    }
  } catch (const std::out_of_range &e) {
    LOG_ERROR(Log::ErrorId::Generic, "Exception: {}", e.what());
    drawableSlices.clear();
  }

  if (quitUpdate) {
    drawableSlices.clear();
  }

  if (columnWidths) {
    const QStringList headers = {tr("Graph"), tr("Summary"), tr("Author"),
                                 tr("Email"), tr("Date"),    tr("SHA")};
    for (int column = COLUMN_FIRST; column <= COLUMN_LAST; ++column) {
      updateWidth(column, textWidth(headers.value(column)));
    }

    *columnWidths = widths;
  }

  LOG_PERF(0, "Exit");

  return drawableSlices;
}

Slices GitHistoryModel::historySlices(const QString &commit) const {
  if (_historyWithStatus.history.commits.size() == 0) {
    return {};
  }

  const int difference =
      indexDifference(_historyWithStatus.history.commits.first(), commit);

  if (difference < 0) {
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

  return slices;
}

int GitHistoryModel::indexDifference(const QString &commitA,
                                     const QString &commitB) const {
  const auto itA = std::find_if(
      _historyWithStatus.history.commits.begin(),
      _historyWithStatus.history.commits.end(),
      [&](const QString &historyCommit) { return historyCommit == commitA; });

  if (itA == _historyWithStatus.history.commits.end()) {
    return -1;
  }

  const auto itB = std::find_if(
      _historyWithStatus.history.commits.begin(),
      _historyWithStatus.history.commits.end(),
      [&](const QString &historyCommit) { return historyCommit == commitB; });

  if (itB == _historyWithStatus.history.commits.end()) {
    return -1;
  }

  return itB - itA;
}

} // namespace GuiManager::Models
