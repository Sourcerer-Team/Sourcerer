#include "widgets/LocalGitBranchesPage.h"
#include "widgets/ui_LocalGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "LocalGitBranchesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

LocalGitBranchesPage::LocalGitBranchesPage(QWidget *parent)
    : QWidget(parent), _model(new BranchesModel(this)),
      ui(new Ui::LocalGitBranchesPage), _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  connect(ui->listView, &QListView::doubleClicked, this,
          &LocalGitBranchesPage::onListViewDoubleClicked);

  LOG_PERF(0, "Exit");
}

LocalGitBranchesPage::~LocalGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::clearCached() {
  LOG_PERF(0, "Enter");

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::updateActiveBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index =
      !branchName.isEmpty() ? _model->indexForBranch(branchName) : -1;

  if (branchName == "*") {
    _previous = index;
    LOG_PERF(0, "Exit");
    return;
  }

  _model->setActiveIndex(index);

  ui->listView->scrollTo(_model->index(index, 0));

  _previous = index;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::updateModel(const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(localBranches);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.row() == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString branchName = _model->branchForIndex(index.row());

  emit branchSelected(branchName);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
