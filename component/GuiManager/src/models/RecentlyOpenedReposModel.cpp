#include "models/RecentlyOpenedReposModel.h"

#include <GitRepo.h>
#include <Log.h>
#include <TokenString.h>

#define THREAD_ID "RecentlyOpenedReposModel"

namespace GuiManager {

namespace Models {

RecentlyOpenedReposModel::RecentlyOpenedReposModel(QObject *parent)
    : QAbstractTableModel{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  reload();
}

int RecentlyOpenedReposModel::columnCount(const QModelIndex &parent) const {
  return 2;
}

int RecentlyOpenedReposModel::rowCount(const QModelIndex &parent) const {
  return _recentlyOpened.length();
}

QVariant RecentlyOpenedReposModel::data(const QModelIndex &index,
                                        int role) const {
  if (role != Qt::ItemDataRole::DisplayRole)
    return QVariant();

  const int column = index.column();
  if (column < 0 || column > 2)
    return QVariant();

  const int row = index.row();
  if (row < 0 || row > _recentlyOpened.length() - 1)
    return QVariant();

  return column == 0 ? _recentlyOpened[row].name : _recentlyOpened[row].url;
}

void RecentlyOpenedReposModel::clear() { _recentlyOpened.clear(); }

void RecentlyOpenedReposModel::reload() {
  clear();
  load();
}

void RecentlyOpenedReposModel::load() {
  const QString recents =
      _configManager.getString(ConfigManager::ConfigKey::RecentlyOpenedRepos);
  Util::TokenString tokenizer(recents);

  QString url;
  QString name;

  while (tokenizer.hasNext()) {
    url = tokenizer.getNextToken();
    name = Util::GitRepo::parseRepoName(url);
    _recentlyOpened += RecentlyOpenedRepo{url, name};
  }

  emit dataChanged(index(0, 0), index(_recentlyOpened.length() - 1, 1));
}

} // namespace Models
} // namespace GuiManager
