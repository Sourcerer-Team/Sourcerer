#pragma once

#include <GitRepo.h>
#include <Graph.h>
#include <LockGuard.h>
#include <ResizableMatrix.h>

#include <QDate>
#include <QEnableSharedFromThis>

#include <QHash>
#include <QStringList>
#include <QVector>
#include <atomic>
#include <functional>
#include <qitemselectionmodel.h>

#define COLUMN_HISTORY_SLICE 0
#define COLUMN_SUMMARY 1
#define COLUMN_AUTHOR_NAME 2
#define COLUMN_AUTHOR_EMAIL 3
#define COLUMN_COMMIT_DATETIME 4
#define COLUMN_COMMIT_SHORT_SHA 5
#define COLUMN_COMMIT_FULL_SHA 6
#define COLUMN_FIRST COLUMN_HISTORY_SLICE
#define COLUMN_LAST COLUMN_COMMIT_SHORT_SHA

#define HistorySliceWidthRole Qt::UserRole
#define StatusRole (Qt::UserRole + 1)
#define CommitRefsRole (Qt::UserRole + 2)

namespace GuiManager::Models {

enum class StatusFields {
  Unknown = 0,

  New,
  Modified,
  Deleted,
  TypeChanged,
  Renamed,
  Unreadable,
  NewStaged,
  ModifiedStaged,
  DeletedStaged,
  TypeChangedStaged,
  RenamedStaged,
  Ignored,
  Conflicted,

  last
};

typedef QMap<StatusFields, int> StatusFieldsMap;

struct Slice {
  QStringView toCommit;
  bool left = false;
  bool right = false;
  bool top = false;
  bool bottom = false;
  bool main = false;
};

typedef QVector<Slice> Slices;

class GitHistoryModel final : public QAbstractTableModel,
                              public QEnableSharedFromThis<GitHistoryModel> {
  Q_OBJECT

  typedef QSharedPointer<GitHistoryModel> SharedPtr;
  typedef QWeakPointer<GitHistoryModel> WeakPtr;

public:
  explicit GitHistoryModel(QObject *parent = nullptr);
  ~GitHistoryModel() override;

  SharedPtr getSelf() { return sharedFromThis(); }

public:
  void setSelectionModel(QItemSelectionModel *selectionModel) {
    _selectionModel = selectionModel;
  }

  void updateStatus(const GitRepo::GitStatus &status);
  void setDateRange(const QDate &fromDate, const QDate &toDate);
  void setFilter(int column, const QString &filter);

  void updateHistoryAndStatus(
      const GitRepo::GitHistoryWithStatus &historyWithStatus,
      const std::function<void(int)> &progressCallback = nullptr);

  void updateCommitRefs(const QHash<QString, QStringList> &commitRefs);

  GitRepo::GitHistoryWithStatus historyWithStatus() const {
    LOCKGUARD(_mutex);
    return _historyWithStatus;
  }

  QVector<int> columnWidths() const { return _columnWidths; }
  int columnWidth(int section) const { return _columnWidths.value(section); }

  void quitUpdate() { _quitUpdate = true; }

  bool hasStatus() const;

  // QAbstractTableModel interface
public:
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

private:
  bool commitMatchesFilters(const QString &commit) const;
  void rebuildVisibleRows();

  static Util::ResizableMatrix<Slice>
  createDrawableSlices(GitRepo::GitHistoryWithStatus &historyWithStatus,
                       const bool &quitUpdate, QVector<int> *columnWidths,
                       const std::function<void(int)> &progressCallback);

private:
  Slices historySlices(const QString &commit) const;

  int indexDifference(const QString &commitA, const QString &commitB) const;

private:
  mutable Util::RecursiveMutex _mutex;
  GitRepo::GitHistoryWithStatus _historyWithStatus;
  mutable Util::ResizableMatrix<Slice> _drawableSlices;
  QVector<int> _columnWidths;
  QHash<QString, QStringList> _commitRefs;
  QHash<int, QString> _filters;
  QDate _fromDate;
  QDate _toDate;
  QVector<int> _visibleRows;
  std::atomic_bool _quitUpdate;
  QItemSelectionModel *_selectionModel;
};

} // namespace GuiManager::Models
