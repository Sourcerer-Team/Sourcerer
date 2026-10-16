
#include "models/StashesModel.h"

#include <Log.h>

#include <QStandardItem>

#define THREAD_ID "StashesModel"

namespace GuiManager::Models {

static auto ICON_PATH = ":/icons/git_branch.png";
static auto ICON_ACTIVE_PATH = ":/icons/git_branch_active.png";

StashesModel::StashesModel(QObject *parent)
    : QStandardItemModel{parent}, _prevIndex(-1) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

StashesModel::~StashesModel() = default;

QString StashesModel::iconActivePath() const {
  return ICON_ACTIVE_PATH;
}

QString StashesModel::iconPath() const {
  return ICON_PATH;
}

int StashesModel::indexForStash(const QString &stash) const {
  LOG_PERF(0, "Enter");

  const int _rowCount = rowCount();
  for (int i = 0; i < _rowCount; ++i) {
    if (index(i, 0).data().toString() == stash) {
      LOG_PERF(0, "Exit");
      return i;
    }
  }

  LOG_PERF(0, "Exit");

  return -1;
}

QString StashesModel::stashForIndex(int index) const {
  LOG_PERF(0, "Enter");

  if (index < 0 || index >= rowCount()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_PERF(0, "Exit");

  return StashesModel::index(index, 0).data().toString();
}

void StashesModel::setActiveIndex(const int index) {
  LOG_PERF(0, "Enter");

  if (index >= rowCount()) {
    LOG_PERF(0, "EXit");
    return;
  }

  if (index == -1) {
    QStandardItem *prevItem = new QStandardItem(
        QIcon(iconPath()), StashesModel::index(_prevIndex, 0)
                               .data()
                               .toString()); // Recreate item with normal icon
    setItem(_prevIndex, prevItem);
    _prevIndex = -1;
    LOG_PERF(0, "EXit");
    return;
  }

  if (_prevIndex != -1 && _prevIndex < rowCount()) {
    QStandardItem *prevItem = new QStandardItem(
        QIcon(iconPath()), StashesModel::index(_prevIndex, 0)
                               .data()
                               .toString()); // Recreate item with normal icon
    setItem(_prevIndex, prevItem);
  }

  const QString activeIconPath = index < 0 ? iconPath() : iconActivePath();
  const int changeIndex = index < 0 ? _prevIndex : index;

  QStandardItem *activeItem =
      new QStandardItem(QIcon(activeIconPath),
                        StashesModel::index(changeIndex, 0).data().toString());

  setItem(index, activeItem);

  _prevIndex = index;

  LOG_PERF(0, "Exit");
}

void StashesModel::updateModel(const QStringList &branches,
                               const int activeIndex) {
  LOG_PERF(0, "Enter");

  clear();

  auto rit = branches.rbegin();
  while (rit != branches.rend()) {
    QStandardItem *item = new QStandardItem(QIcon(iconPath()), *rit);
    item->setToolTip(*rit);
    insertRow(0, item);
    ++rit;
  }

  setActiveIndex(activeIndex);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Models