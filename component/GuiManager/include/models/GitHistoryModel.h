#pragma once

#include <GitRepo.h>

#include <QAbstractListModel>

#define ROLE_HISTORY_SLICE Qt::UserRole
#define ROLE_SUMMARY Qt::UserRole + 1
#define ROLE_AUTHOR_NAME Qt::UserRole + 2
#define ROLE_AUTHOR_EMAIL Qt::UserRole + 3
#define ROLE_COMMIT_DATETIME Qt::UserRole + 4
#define ROLE_COMMIT_SHA Qt::UserRole + 5
#define ROLE_FIRST ROLE_HISTORY_SLICE
#define ROLE_LAST ROLE_COMMIT_SHA

namespace GuiManager {

namespace Models {

class GitHistoryModel : public QAbstractListModel {
  Q_OBJECT
public:
  explicit GitHistoryModel(QObject *parent = nullptr);
  virtual ~GitHistoryModel();

public:
  void setRepoUuid(const QString &uuid);
  void updateHistory(const GitRepo::GitHistory &history);

  // QAbstractItemModel interface
public:
  QModelIndex index(int row, int column,
                    const QModelIndex &parent) const override;
  int rowCount(const QModelIndex &parent) const override;
  int columnCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

private:
  GitRepo::GitHistory historySlice(const QString &commit) const;

private:
  GitRepo::GitHistory _history;
  QString _repoUuid;
};

} // namespace Models

} // namespace GuiManager
