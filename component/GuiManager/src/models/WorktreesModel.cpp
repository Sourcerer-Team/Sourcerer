// MIT License
// Copyright (c) 2026 Sourcerer


#include "models/WorktreesModel.h"

#include <Log.h>

#include <QDir>
#include <QFont>
#include <QIcon>
#include <QStandardItem>

#define THREAD_ID "WorktreesModel"

namespace GuiManager::Models {

static const char *ICON_PATH = ":/icons/git_branch.png";
static const char *ICON_ACTIVE_PATH = ":/icons/git_branch_active.png";

WorktreesModel::WorktreesModel(QObject *parent) : QStandardItemModel{parent} {}

WorktreesModel::~WorktreesModel() = default;

void WorktreesModel::updateModel(
    const QVector<GitRepo::WorktreeInfo> &worktrees) {
  LOG_PERF(0, "Enter");

  clear();

  for (const GitRepo::WorktreeInfo &info : worktrees) {
    QString displayText = QDir(info.path).dirName();
    if (displayText.isEmpty()) {
      displayText = info.name;
    }

    if (!info.branch.isEmpty()) {
      displayText += QStringLiteral(" [%1]").arg(info.branch);
    } else if (!info.head.isEmpty()) {
      displayText += tr(" [detached @%1]").arg(info.head);
    }

    if (info.isMain) {
      displayText += tr(" (main)");
    }
    if (info.locked) {
      displayText += QStringLiteral(" 🔒");
    }
    if (!info.valid) {
      displayText += QStringLiteral(" ✗");
    }

    auto *item = new QStandardItem(
        QIcon(info.isCurrent ? ICON_ACTIVE_PATH : ICON_PATH), displayText);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

    if (info.isCurrent) {
      QFont font = item->font();
      font.setBold(true);
      item->setFont(font);
    }

    QString tooltip = info.path;
    if (!info.branch.isEmpty()) {
      tooltip += tr("\nBranch: %1").arg(info.branch);
    }
    if (!info.head.isEmpty()) {
      tooltip += tr("\nCommit: %1").arg(info.head);
    }
    if (info.isMain) {
      tooltip += tr("\nMain working tree");
    }
    if (info.isCurrent) {
      tooltip += tr("\nCurrently opened working tree");
    }
    if (info.locked) {
      tooltip += tr("\nLocked");
    }
    if (!info.valid) {
      tooltip += tr("\nWorking tree directory is missing or invalid");
    }
    item->setToolTip(tooltip);

    item->setData(info.name, NameRole);
    item->setData(info.path, PathRole);
    item->setData(info.isMain, IsMainRole);
    item->setData(info.isCurrent, IsCurrentRole);
    item->setData(info.locked, LockedRole);
    item->setData(info.valid, ValidRole);
    item->setData(info.branch, BranchRole);

    appendRow(item);
  }

  LOG_PERF(0, "Exit");
}

GitRepo::WorktreeInfo WorktreesModel::worktreeForIndex(const int row) const {
  GitRepo::WorktreeInfo info;

  const QStandardItem *item = this->item(row);
  if (!item) {
    return info;
  }

  info.name = item->data(NameRole).toString();
  info.path = item->data(PathRole).toString();
  info.isMain = item->data(IsMainRole).toBool();
  info.isCurrent = item->data(IsCurrentRole).toBool();
  info.locked = item->data(LockedRole).toBool();
  info.valid = item->data(ValidRole).toBool();
  info.branch = item->data(BranchRole).toString();

  return info;
}

} // namespace GuiManager::Models
