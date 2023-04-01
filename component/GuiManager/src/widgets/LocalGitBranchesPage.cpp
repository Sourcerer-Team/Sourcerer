#include "widgets/LocalGitBranchesPage.h"
#include "widgets/ui_LocalGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "LocalGitBranchesPage"

namespace GuiManager {

namespace Widgets {

LocalGitBranchesPage::LocalGitBranchesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::LocalGitBranchesPage), _model(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

LocalGitBranchesPage::~LocalGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::updateModel(const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  delete _model;

  _model = new QStandardItemModel(this);
  auto rit = localBranches.rbegin();
  while (rit != localBranches.rend()) {
    QStandardItem *item =
        new QStandardItem(QIcon(":/icons/git_branch.png"), *rit);
    item->setToolTip(*rit);
    _model->insertRow(0, item);
    ++rit;
  }

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
