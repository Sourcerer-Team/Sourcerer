#pragma once

#include <GitRepo.h>
#include <Graph.h>
#include <LockGuard.h>
#include <ResizableMatrix.h>

#include <QEnableSharedFromThis>

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

namespace GuiManager::Models {

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

  void
  updateHistory(const GitRepo::GitHistory &history,
                const std::function<void(int)> &progressCallback = nullptr);

  void quitUpdate() { _quitUpdate = true; }

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
  void createDrawableSlices(const std::function<void(int)> &progressCallback);

private:
  Slices historySlices(const QString &commit) const;

  int indexDifference(const QString &commitA, const QString &commitB) const;

private:
  mutable Util::Mutex _mutex;
  GitRepo::GitHistory _history;
  mutable Util::ResizableMatrix<Slice> _drawableSlices;
  std::atomic_bool _quitUpdate;
  QItemSelectionModel *_selectionModel;
};

} // namespace GuiManager::Models
