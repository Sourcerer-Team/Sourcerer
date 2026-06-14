#include "models/RemotesModel.h"

#include <Log.h>

#include <QStandardItem>

#define THREAD_ID "RemotesModel"

namespace GuiManager::Models {

static auto ICON_PATH = ":/icons/git_remote.png";
static auto ICON_ACTIVE_PATH = ":/icons/git_branch_active.png";

RemotesModel::RemotesModel(QObject *parent)
    : QStandardItemModel{parent}, _prevIndex(-1) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

RemotesModel::~RemotesModel() = default;

QString RemotesModel::iconActivePath() const {
  return ICON_ACTIVE_PATH;
}

QString RemotesModel::iconPath() const {
  return ICON_PATH;
}

int RemotesModel::indexForRemote(const QString &remoteName) const {
  LOG_PERF(0, "Enter");

  const int _rowCount = rowCount();
  for (int i = 0; i < _rowCount; ++i) {
    QStandardItem *item = RemotesModel::item(i, 0);
    if (item && item->data(Qt::UserRole + 2) == remoteName) {
      LOG_PERF(0, "Exit");
      return i;
    }
  }

  LOG_PERF(0, "Exit");
  return -1;
}

QString RemotesModel::remoteForIndex(int index) const {
  LOG_PERF(0, "Enter");

  if (index < 0 || index >= rowCount()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStandardItem *item = RemotesModel::item(index, 0);
  if (item) {
    LOG_PERF(0, "Exit");
    return item->data(Qt::UserRole + 2).toString();
  }

  LOG_PERF(0, "Exit");
  return {};
}

bool RemotesModel::isRemoteItem(const QModelIndex &index) const {
  if (!index.isValid()) {
    return false;
  }
  QStandardItem *item = RemotesModel::item(index.row(), index.column());
  return item && item->data(Qt::UserRole + 3).toString() == "remote";
}

bool RemotesModel::isTargetItem(const QModelIndex &index) const {
  if (!index.isValid()) {
    return false;
  }
  QStandardItem *item = RemotesModel::item(index.row(), index.column());
  return item && item->data(Qt::UserRole + 3).toString() == "target";
}

QString RemotesModel::getRemoteName(const QModelIndex &index) const {
  if (!index.isValid()) {
    return {};
  }

  QStandardItem *item = RemotesModel::item(index.row(), index.column());
  if (!item) {
    return {};
  }

  // If it's a target item, get parent's remote name
  if (isTargetItem(index)) {
    QStandardItem *parentItem = item->parent();
    if (parentItem) {
      return parentItem->data(Qt::UserRole + 2).toString();
    }
  }
  // If it's a remote item, get its name
  else if (isRemoteItem(index)) {
    return item->data(Qt::UserRole + 2).toString();
  }

  return {};
}

GitRepo::RemoteTarget RemotesModel::getTargetInfo(const QModelIndex &index) const {
  GitRepo::RemoteTarget target;

  if (!isTargetItem(index)) {
    return target;
  }

  QStandardItem *item = RemotesModel::item(index.row(), index.column());
  if (!item) {
    return target;
  }

  target.name = item->text();
  target.fetchUrl = item->data(Qt::UserRole + 4).toString();
  target.pushUrl = item->data(Qt::UserRole + 5).toString();

  return target;
}

QModelIndex RemotesModel::getRemoteIndex(const QString &remoteName) const {
  const int row = indexForRemote(remoteName);
  if (row >= 0) {
    return index(row, 0);
  }
  return {};
}

void RemotesModel::setActiveIndex(const int index) {
  LOG_PERF(0, "Enter");

  if (index >= rowCount() && index != -1) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index == -1) {
    if (_prevIndex != -1 && _prevIndex < rowCount()) {
      QStandardItem *prevItem = item(_prevIndex, 0);
      if (prevItem) {
        prevItem->setIcon(QIcon(iconPath()));
      }
    }
    _prevIndex = -1;
    LOG_PERF(0, "Exit");
    return;
  }

  if (_prevIndex != -1 && _prevIndex < rowCount()) {
    QStandardItem *prevItem = item(_prevIndex, 0);
    if (prevItem) {
      prevItem->setIcon(QIcon(iconPath()));
    }
  }

  QStandardItem *activeItem = item(index, 0);
  if (activeItem) {
    activeItem->setIcon(QIcon(iconActivePath()));
  }

  _prevIndex = index;

  LOG_PERF(0, "Exit");
}

void RemotesModel::updateModel(const QVector<GitRepo::RemoteInfo> &remotes,
                               const int activeIndex) {
  LOG_PERF(0, "Enter");

  clear();
  _prevIndex = -1;
  _remoteItems.clear();

  for (const auto &remote : remotes) {
    // Create parent item for remote
    QStandardItem *remoteItem = new QStandardItem(QIcon(iconPath()), remote.name);
    remoteItem->setToolTip(remote.fetchUrl);
    remoteItem->setData(remote.isDefault, Qt::UserRole + 1); // Store default flag
    remoteItem->setData(remote.name, Qt::UserRole + 2);      // Store remote name
    remoteItem->setData("remote", Qt::UserRole + 3);         // Mark as remote
    insertRow(rowCount(), remoteItem);

    // Cache the remote item for quick lookups
    _remoteItems[remote.name] = remoteItem;

    // Create child items for each target
    for (const auto &target : remote.targets) {
      QStandardItem *targetItem = new QStandardItem(target.name);
      targetItem->setData(remote.name, Qt::UserRole + 2);    // Store parent remote name
      targetItem->setData("target", Qt::UserRole + 3);       // Mark as target
      targetItem->setData(target.fetchUrl, Qt::UserRole + 4); // Fetch URL
      targetItem->setData(target.pushUrl, Qt::UserRole + 5);  // Push URL
      remoteItem->appendRow(targetItem);
    }
  }

  // Set active remote based on default flag or provided index
  int activeIdx = activeIndex;
  if (activeIdx == -1) {
    // Find the default remote
    for (int i = 0; i < rowCount(); ++i) {
      QStandardItem *remoteItem = item(i, 0);
      if (remoteItem && remoteItem->data(Qt::UserRole + 1).toBool()) {
        activeIdx = i;
        break;
      }
    }
  }

  if (activeIdx >= 0 && activeIdx < rowCount()) {
    setActiveIndex(activeIdx);
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Models
