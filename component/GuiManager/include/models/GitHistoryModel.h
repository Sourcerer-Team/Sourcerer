#pragma once

#include <GitRepo.h>

#include <QAbstractTableModel>

#define COLUMN_HISTORY_SLICE 0
#define COLUMN_SUMMARY 1
#define COLUMN_AUTHOR_NAME 2
#define COLUMN_AUTHOR_EMAIL 3
#define COLUMN_COMMIT_DATETIME 4
#define COLUMN_COMMIT_SHA 5
#define COLUMN_FIRST COLUMN_HISTORY_SLICE
#define COLUMN_LAST COLUMN_COMMIT_SHA

namespace GuiManager {

namespace Models {

class GitHistoryModel : public QAbstractTableModel {
  Q_OBJECT
public:
  explicit GitHistoryModel(QObject *parent = nullptr);
  ~GitHistoryModel() override;

public:
  void updateHistory(const GitRepo::GitHistory &history);

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
  GitRepo::GitHistory historySlice(const QString &commit) const;

private:
  GitRepo::GitHistory _history;
};

} // namespace Models

} // namespace GuiManager
