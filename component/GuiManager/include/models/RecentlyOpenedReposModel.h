#pragma once

#include <ConfigManager.h>

#include <QAbstractTableModel>

namespace GuiManager {

namespace Models {

struct RecentlyOpenedRepo {
  QString url;
  QString name;
};

class RecentlyOpenedReposModel : public QAbstractTableModel {
public:
  explicit RecentlyOpenedReposModel(QObject *parent = nullptr);

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

} // namespace Models
} // namespace GuiManager
