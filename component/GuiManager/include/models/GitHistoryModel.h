#pragma once

#include <GitRepo.h>
#include <Graph.h>
#include <ResizableMatrix.h>

#include <QAbstractTableModel>

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

class GitHistoryModel final : public QAbstractTableModel {
  Q_OBJECT
public:
  explicit GitHistoryModel(QObject *parent = nullptr);
  ~GitHistoryModel() override;

public:
  void setSelectionModel(QItemSelectionModel *selectionModel) {
    _selectionModel = selectionModel;
  }

  void updateHistory(const GitRepo::GitHistory &history,
                     const std::function<void(int)> progressCallback = nullptr);

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
  void createDrawableSlices(const std::function<void(int)> progressCallback);

#ifdef QT_DEBUG
  static void
  printDrawableSlices(const Util::ResizableMatrix<Slice> &resizableMatrix);
#endif

private:
  Slices historySlices(const QString &commit) const;

  int indexDifference(const QString &commitA, const QString &commitB) const;

private:
  GitRepo::GitHistory _history;
  Util::ResizableMatrix<Slice> _drawableSlices;
  bool _quitUpdate;
  QItemSelectionModel *_selectionModel;
};

} // namespace GuiManager::Models
