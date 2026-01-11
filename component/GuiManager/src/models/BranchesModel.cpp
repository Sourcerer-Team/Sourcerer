
#include "models/BranchesModel.h"

#include <Log.h>

#include <QIODevice>
#include <QMimeData>
#include <QStandardItem>

#define THREAD_ID "BranchesModel"

namespace GuiManager::Models {

static auto ICON_PATH = ":/icons/git_branch.png";
static auto ICON_ACTIVE_PATH = ":/icons/git_branch_active.png";
static auto MIME_TYPE = "application/x-qabstractitemmodeldatalist";

BranchesModel::BranchesModel(QObject *parent)
    : QStandardItemModel{parent}, _prevIndex(-1) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

BranchesModel::~BranchesModel() = default;

QString BranchesModel::iconActivePath() const {
  return ICON_ACTIVE_PATH;
}

QString BranchesModel::iconPath() const {
  return ICON_PATH;
}

Qt::ItemFlags BranchesModel::flags(const QModelIndex &index) const {
  Qt::ItemFlags defaultFlags = QStandardItemModel::flags(index);
  if (index.isValid())
    return defaultFlags | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
  else
    return defaultFlags | Qt::ItemIsDropEnabled;
}

Qt::DropActions BranchesModel::supportedDropActions() const {
  return Qt::CopyAction;
}

QStringList BranchesModel::mimeTypes() const {
  return {MIME_TYPE};
}

QMimeData *BranchesModel::mimeData(const QModelIndexList &indexes) const {
  QMimeData *mime = new QMimeData();
  QByteArray encoded;

  QDataStream stream(&encoded, QIODevice::WriteOnly);
  for (const QModelIndex &index : indexes) {
    if (index.isValid()) {
      const QString text = index.data().toString();
      stream << text;
    }
  }

  mime->setData(MIME_TYPE, encoded);

  return mime;
}

bool BranchesModel::dropMimeData(const QMimeData *data, Qt::DropAction action,
                                 int row, int column,
                                 const QModelIndex &parent) {
  if (action == Qt::IgnoreAction) {
    return true;
  }

  if (action != Qt::CopyAction) {
    return true;
  }

  if (!data->hasFormat(MIME_TYPE))
    return false;

  QByteArray encoded = data->data(MIME_TYPE);
  QDataStream stream(&encoded, QIODevice::ReadOnly);

  QStringList branches;

  while (!stream.atEnd()) {
    QString text;
    stream >> text;

    branches += text;
  }

  if (branches.isEmpty()) {
    return true;
  }

  const QString sourceBranch = branches.first();
  QString destBranch;

  if (row == -1 && column == -1) {
    destBranch = parent.data().toString();
  } else {
    destBranch = index(row, column, parent).data().toString();
  }

  if(!sourceBranch.isEmpty() && !destBranch.isEmpty()) {
    emit branchesMoved(sourceBranch, destBranch);
  }

  return true;
}

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

QString BranchesModel::branchForIndex(int index) const {
  LOG_PERF(0, "Enter");

  if (index < 0 || index >= rowCount()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_PERF(0, "Exit");

  return BranchesModel::index(index, 0).data().toString();
}

void BranchesModel::setActiveIndex(const int index) {
  LOG_PERF(0, "Enter");

  if (index >= rowCount()) {
    LOG_PERF(0, "EXit");
    return;
  }

  if (index == -1) {
    QStandardItem *prevItem = new QStandardItem(
        QIcon(iconPath()), BranchesModel::index(_prevIndex, 0)
                               .data()
                               .toString()); // Recreate item with normal icon
    prevItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                       Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    setItem(_prevIndex, prevItem);
    _prevIndex = -1;
    LOG_PERF(0, "EXit");
    return;
  }

  if (_prevIndex != -1 && _prevIndex < rowCount()) {
    QStandardItem *prevItem = new QStandardItem(
        QIcon(iconPath()), BranchesModel::index(_prevIndex, 0)
                               .data()
                               .toString()); // Recreate item with normal icon
    prevItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                       Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    setItem(_prevIndex, prevItem);
  }

  const QString activeIconPath = index < 0 ? iconPath() : iconActivePath();
  const int changeIndex = index < 0 ? _prevIndex : index;

  QStandardItem *activeItem =
      new QStandardItem(QIcon(activeIconPath),
                        BranchesModel::index(changeIndex, 0).data().toString());
  activeItem->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                       Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

  setItem(index, activeItem);

  _prevIndex = index;

  LOG_PERF(0, "Exit");
}

void BranchesModel::updateModel(const QStringList &branches,
                                const int activeIndex) {
  LOG_PERF(0, "Enter");

  clear();

  auto rit = branches.rbegin();
  while (rit != branches.rend()) {
    QStandardItem *item = new QStandardItem(QIcon(iconPath()), *rit);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                   Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
    item->setToolTip(*rit);
    insertRow(0, item);
    ++rit;
  }

  setActiveIndex(activeIndex);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Models
