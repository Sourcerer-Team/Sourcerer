
#include "models/SubmodulesModel.h"

#include <Log.h>

#include <QIODevice>
#include <QMimeData>
#include <QStandardItem>

#define THREAD_ID "SubmodulesModel"

namespace GuiManager {

namespace Models {

static const char *ICON_PATH = ":/icons/git_submodule.png";

SubmodulesModel::SubmodulesModel(QObject *parent) : BranchesModel{parent} {}

SubmodulesModel::~SubmodulesModel() {}

QString SubmodulesModel::iconActivePath() const {
  return ICON_PATH;
}

QString SubmodulesModel::iconPath() const {
  return ICON_PATH;
}

void SubmodulesModel::updateModelWithStatus(
    const QVector<GitRepo::SubmoduleInfo> &submodulesWithStatus,
    const int activeIndex) {
  LOG_PERF(0, "Enter");

  clear();

  // Add submodules in reverse order to maintain the order
  auto rit = submodulesWithStatus.rbegin();
  while (rit != submodulesWithStatus.rend()) {
    const GitRepo::SubmoduleInfo &submoduleInfo = *rit;

    // Create the display text with status information
    QString displayText = submoduleInfo.name;
    if (!submoduleInfo.commit.isEmpty()) {
      displayText += QStringLiteral(" @%1")
                         .arg(submoduleInfo.commit.left(7));
    }

    // Add status indicator based on the status type
    if (submoduleInfo.statusInfo.status ==
        GitRepo::SubmoduleStatusType::UpToDate) {
      displayText += " ✓";
    } else if (submoduleInfo.statusInfo.status ==
               GitRepo::SubmoduleStatusType::Outdated) {
      displayText += " ↑";
    } else if (submoduleInfo.statusInfo.status ==
               GitRepo::SubmoduleStatusType::Error) {
      displayText += " ✗"; // Error symbol
    }

    QStandardItem *item = new QStandardItem(QIcon(iconPath()), displayText);
    item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled |
                   Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);

    // Set tooltip with full submodule name and status details
    QString tooltip = submoduleInfo.name;
    if (!submoduleInfo.commit.isEmpty()) {
      tooltip += QStringLiteral("\nCommit: %1").arg(submoduleInfo.commit);
    }
    if (submoduleInfo.statusInfo.status ==
        GitRepo::SubmoduleStatusType::UpToDate) {
      tooltip += tr("\nSubmodule is up to date");
    } else if (submoduleInfo.statusInfo.status ==
               GitRepo::SubmoduleStatusType::Outdated) {
      tooltip += tr("\nSubmodule is outdated (different commit than expected)");
    } else if (submoduleInfo.statusInfo.status ==
               GitRepo::SubmoduleStatusType::Error) {
      tooltip += tr("\nError checking submodule status");
    }

    item->setToolTip(tooltip);

    // Store the submodule name as data for easier retrieval
    item->setData(submoduleInfo.name, Qt::UserRole);

    insertRow(0, item);
    ++rit;
  }

  setActiveIndex(activeIndex);

  LOG_PERF(0, "Exit");
}

} // namespace Models
} // namespace GuiManager
