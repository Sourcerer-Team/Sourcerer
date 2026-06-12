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
    if (index(i, 0).data().toString() == remoteName) {
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

  LOG_PERF(0, "Exit");

  return RemotesModel::index(index, 0).data().toString();
}

void RemotesModel::setActiveIndex(const int index) {
  LOG_PERF(0, "Enter");

  if (index >= rowCount() && index != -1) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index == -1) {
    if (_prevIndex != -1 && _prevIndex < rowCount()) {
      QStandardItem *prevItem = new QStandardItem(
          QIcon(iconPath()), RemotesModel::index(_prevIndex, 0)
                                 .data()
                                 .toString()); // Recreate item with normal icon
      setItem(_prevIndex, prevItem);
    }
    _prevIndex = -1;
    LOG_PERF(0, "Exit");
    return;
  }

  if (_prevIndex != -1 && _prevIndex < rowCount()) {
    QStandardItem *prevItem = new QStandardItem(
        QIcon(iconPath()), RemotesModel::index(_prevIndex, 0)
                               .data()
                               .toString()); // Recreate item with normal icon
    setItem(_prevIndex, prevItem);
  }

  const QString activeIconPath = iconActivePath();

  QStandardItem *activeItem = new QStandardItem(
      QIcon(activeIconPath), RemotesModel::index(index, 0).data().toString());

  setItem(index, activeItem);

  _prevIndex = index;

  LOG_PERF(0, "Exit");
}

void RemotesModel::updateModel(const QVector<GitRepo::RemoteInfo> &remotes,
                               const int activeIndex) {
  LOG_PERF(0, "Enter");

  clear();
  _prevIndex = -1;

  for (const auto &remote : remotes) {
    QStandardItem *item = new QStandardItem(QIcon(iconPath()), remote.name);
    item->setToolTip(remote.fetchUrl);
    item->setData(remote.isDefault, Qt::UserRole + 1); // Store default flag
    insertRow(rowCount(), item);
  }

  // Set active remote based on default flag or provided index
  int activeIdx = activeIndex;
  if (activeIdx == -1) {
    // Find the default remote
    for (int i = 0; i < rowCount(); ++i) {
      if (index(i, 0).data(Qt::UserRole + 1).toBool()) {
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
