#pragma once

#include "RepositoryType.h"

#include <ConfigManager.h>

#include <QAbstractTableModel>
#include <QColor>
#include <QIcon>

namespace GuiManager::Models {

struct RecentlyOpenedRepo {
  QString path;
  QString name;
  RepositoryType type;
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

  // A recent entry stores the path and the repository type, as "path|type".
  static QString makeRecentEntry(const QString &path, RepositoryType type);
  static QString recentEntryPath(const QString &entry);

private:
  void load();
  static RecentlyOpenedRepo parseRecentEntry(const QString &entry);
  QIcon repositoryTypeIcon(RepositoryType type) const;

private:
  ConfigManager::ConfigManager &_configManager;
  QVector<RecentlyOpenedRepo> _recentlyOpened;
  mutable QColor _iconColor;
  mutable QIcon _gitIcon;
  mutable QIcon _svnIcon;
  mutable QIcon _hgIcon;
};

} // namespace GuiManager::Models