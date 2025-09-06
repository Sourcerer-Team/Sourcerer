#include "widgets/RemoteGitBranchesPage.h"
#include "widgets/ui_RemoteGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "RemoteGitBranchesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

RemoteGitBranchesPage::RemoteGitBranchesPage(QWidget *parent)
    : QWidget(parent), _model(new BranchesModel(this)),
      ui(new Ui::RemoteGitBranchesPage) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  connect(ui->listView, &QListView::doubleClicked, this,
          &RemoteGitBranchesPage::onListViewDoubleClicked);

  LOG_PERF(0, "Exit");
}

RemoteGitBranchesPage::~RemoteGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateActiveBranch(
    const QString &branchName) const {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index = !branchName.isEmpty() ?_model->indexForBranch(branchName) : -1;
  _model->setActiveIndex(index);

  ui->listView->scrollTo(_model->index(index, 0));

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateModel(
    const QStringList &remoteBranches) const {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(remoteBranches);

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_PERF(0, "Exit");

  emit branchSelected(_model->branchForIndex(index.row()));
}

} // namespace GuiManager::Widgets