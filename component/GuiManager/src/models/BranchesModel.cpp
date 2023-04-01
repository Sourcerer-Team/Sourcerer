
#include "models/BranchesModel.h"

#include <Log.h>

#include <QStandardItem>

#define THREAD_ID "BranchesModel"

namespace GuiManager {

namespace Models {

static const char *ICON_PATH = ":/icons/git_branch.png";
static const char *ICON_ACTIVE_PATH = ":/icons/git_branch_active.png";

BranchesModel::BranchesModel(QObject *parent)
    : QStandardItemModel{parent}, _prevIndex(-1) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

BranchesModel::~BranchesModel() {}

QString BranchesModel::iconActivePath() const { return ICON_ACTIVE_PATH; }

QString BranchesModel::iconPath() const { return ICON_PATH; }

int BranchesModel::indexForBranch(const QString &branch) const {
  LOG_PERF(0, "Enter");

  const int _rowCount = rowCount();
  for (int i = 0; i < _rowCount; ++i) {
    if (index(i, 0).data().toString() == branch) {
      LOG_PERF(0, "Exit");
      return i;
    }
  }

  LOG_PERF(0, "Exit");

  return -1;
}

void BranchesModel::setActiveIndex(int index) {
  LOG_PERF(0, "Enter");

  if (index >= rowCount()) {
    LOG_PERF(0, "EXit");
    return;
  }

  const QString activeIconPath = index < 0 ? iconPath() : iconActivePath();
  const int changeIndex = index < 0 ? _prevIndex : index;

  QStandardItem *activeItem =
      new QStandardItem(QIcon(activeIconPath),
                        BranchesModel::index(changeIndex, 0).data().toString());

  setItem(index, activeItem);

  _prevIndex = index;

  LOG_PERF(0, "Exit");
}

void BranchesModel::updateModel(const QStringList &branches, int activeIndex) {
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

} // namespace Models
} // namespace GuiManager
