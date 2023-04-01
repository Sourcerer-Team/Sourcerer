#include "widgets/RemoteGitBranchesPage.h"
#include "widgets/ui_RemoteGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "RemoteGitBranchesPage"

namespace GuiManager {

namespace Widgets {

RemoteGitBranchesPage::RemoteGitBranchesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::RemoteGitBranchesPage), _model(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

RemoteGitBranchesPage::~RemoteGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateModel(const QStringList &remoteBranches) {
  LOG_PERF(0, "Enter");

  delete _model;

  _model = new QStandardItemModel(this);
  auto rit = remoteBranches.rbegin();
  while (rit != remoteBranches.rend()) {
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
