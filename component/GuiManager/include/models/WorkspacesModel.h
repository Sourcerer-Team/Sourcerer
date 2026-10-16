#pragma once

#include <ConfigManager.h>

#include <QAbstractTableModel>

namespace GuiManager::Models {

class WorkspacesModel final : public QAbstractTableModel {
public:
  explicit WorkspacesModel(QObject *parent = nullptr);

  QModelIndex createIndex(int row, int column) const;

  // QAbstractItemModel interface
  int columnCount(const QModelIndex &parent) const override;
  int rowCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;

  void clear();
  void reload();

private:
  void load();

private:
  QVector<QString> _workspaces;
};

} // namespace GuiManager::Models