#include "models/RecentlyOpenedReposModel.h"

#include <Git.h>
#include <Log.h>
#include <TokenString.h>

#include <QBrush>
#include <QDir>
#include <QFileInfo>
#include <QFont>

#define THREAD_ID "RecentlyOpenedReposModel"

namespace GuiManager::Models {

RecentlyOpenedReposModel::RecentlyOpenedReposModel(QObject *parent)
    : QAbstractTableModel{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  reload();
}

QModelIndex RecentlyOpenedReposModel::createIndex(int row, int column) const {
  return QAbstractItemModel::createIndex(row, column);
}

int RecentlyOpenedReposModel::columnCount(const QModelIndex &parent) const {
  return 2;
}

int RecentlyOpenedReposModel::rowCount(const QModelIndex &parent) const {
  return _recentlyOpened.length();
}

QVariant RecentlyOpenedReposModel::data(const QModelIndex &index,
                                        const int role) const {
  const int column = index.column();
  if (role == Qt::ForegroundRole && column == 0) {
    return QBrush(QColor::fromString("#384D59")); // or any QColor
  }

  if (role == Qt::FontRole && column == 0) {
    QFont boldFont;
    boldFont.setBold(true);
    return boldFont;
  }

  if (role != Qt::ItemDataRole::DisplayRole)
    return QVariant();

  if (column < 0 || column > 2)
    return QVariant();

  const int row = index.row();
  if (row < 0 || row > _recentlyOpened.length() - 1)
    return QVariant();

  for (int i = 0; i < _recentlyOpened.length(); i++) {
    qDebug() << "i = " << i << ", name = " << _recentlyOpened[i].name
             << ", path = "
             << QDir::toNativeSeparators(_recentlyOpened[i].path);
  }

  return column == 0 ? _recentlyOpened[row].name
                     : QDir::toNativeSeparators(_recentlyOpened[row].path);
}

void RecentlyOpenedReposModel::clear() {
  _recentlyOpened.clear();
}

void RecentlyOpenedReposModel::reload() {
  clear();
  load();
}

void RecentlyOpenedReposModel::load() {
  beginResetModel();

  const QString recents =
      _configManager.getString(ConfigManager::ConfigKey::RecentlyOpenedRepos);
  Util::TokenString tokenizer(recents, ';');

  while (tokenizer.hasNext()) {
    const QString path = QDir::fromNativeSeparators(tokenizer.getNextToken());
    const QStringList parts = path.split("/", Qt::SkipEmptyParts);
    if (path.length() < 1) {
      continue;
    }
    const QString name = parts.last();
    _recentlyOpened += RecentlyOpenedRepo{QDir::toNativeSeparators(path), name};
  }

  endResetModel();
}

} // namespace GuiManager::Models