#include "models/RecentlyOpenedReposModel.h"

#include <StylesheetManager.h>

#include <Git.h>
#include <Log.h>
#include <TokenString.h>

#include <QBrush>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>

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
    return QBrush(QGuiApplication::palette().color(QPalette::WindowText));
  }

  if (role == Qt::FontRole && column == 0) {
    QFont boldFont;
    boldFont.setBold(true);
    return boldFont;
  }

  if (role != Qt::ItemDataRole::DisplayRole &&
      role != Qt::ItemDataRole::DecorationRole)
    return QVariant();

  if (column < 0 || column > 2)
    return QVariant();

  const int row = index.row();
  if (row < 0 || row > _recentlyOpened.length() - 1)
    return QVariant();

  if (role == Qt::ItemDataRole::DecorationRole) {
    if (column != 0) {
      return QVariant();
    }
    return repositoryTypeIcon(_recentlyOpened[row].type);
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

QString RecentlyOpenedReposModel::makeRecentEntry(const QString &path,
                                                  const RepositoryType type) {
  return QDir::fromNativeSeparators(path) + QStringLiteral("|") +
         repositoryTypeKey(type);
}

QString RecentlyOpenedReposModel::recentEntryPath(const QString &entry) {
  return parseRecentEntry(entry).path;
}

RecentlyOpenedRepo
RecentlyOpenedReposModel::parseRecentEntry(const QString &entry) {
  const int separator = entry.lastIndexOf(QLatin1Char('|'));
  RepositoryType type = RepositoryType::Git;

  // Entries saved before the type was stored have no suffix, their type is
  // detected from the repository itself.
  if (separator == -1 ||
      !repositoryTypeFromKey(entry.mid(separator + 1), &type)) {
    const QString path = QDir::fromNativeSeparators(entry);
    const QStringList parts = path.split("/", Qt::SkipEmptyParts);
    return RecentlyOpenedRepo{path, parts.isEmpty() ? QString() : parts.last(),
                              repositoryTypeOf(path)};
  }

  const QString path = QDir::fromNativeSeparators(entry.left(separator));
  const QStringList parts = path.split("/", Qt::SkipEmptyParts);
  return RecentlyOpenedRepo{path, parts.isEmpty() ? QString() : parts.last(),
                            type};
}

QIcon RecentlyOpenedReposModel::repositoryTypeIcon(
    const RepositoryType type) const {
  // The icons are recoloured like the other Home icons and rendered again only
  // when the theme icon colour changes.
  const QColor color =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();
  if (_gitIcon.isNull() || color != _iconColor) {
    _iconColor = color;
    _gitIcon = QIcon(StylesheetManager::StylesheetManager::renderSvgToPixmap(
        QStringLiteral(":/icons/repo-type-git.svg"), color));
    _svnIcon = QIcon(StylesheetManager::StylesheetManager::renderSvgToPixmap(
        QStringLiteral(":/icons/repo-type-svn.svg"), color));
    _hgIcon = QIcon(StylesheetManager::StylesheetManager::renderSvgToPixmap(
        QStringLiteral(":/icons/repo-type-hg.svg"), color));
  }

  switch (type) {
  case RepositoryType::Subversion:
    return _svnIcon;
  case RepositoryType::Mercurial:
    return _hgIcon;
  case RepositoryType::Git:
    break;
  }
  return _gitIcon;
}

void RecentlyOpenedReposModel::load() {
  beginResetModel();

  const QString recents =
      _configManager.getString(ConfigManager::ConfigKey::RecentlyOpenedRepos);
  Util::TokenString tokenizer(recents, ';');

  while (tokenizer.hasNext()) {
    const RecentlyOpenedRepo repo = parseRecentEntry(tokenizer.getNextToken());
    if (repo.path.length() < 1) {
      continue;
    }
    _recentlyOpened += RecentlyOpenedRepo{QDir::toNativeSeparators(repo.path),
                                          repo.name, repo.type};
  }

  endResetModel();
}

} // namespace GuiManager::Models
