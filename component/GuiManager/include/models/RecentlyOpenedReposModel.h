#pragma once

#include <ConfigManager.h>

#include <QAbstractTableModel>

namespace GuiManager::Models {

struct RecentlyOpenedRepo {
  QString path;
  QString name;
};

class RecentlyOpenedReposModel final : public QAbstractTableModel {
public:
  explicit RecentlyOpenedReposModel(QObject *parent = nullptr);

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
  ConfigManager::ConfigManager &_configManager;
  QVector<RecentlyOpenedRepo> _recentlyOpened;
};

} // namespace GuiManager::Models