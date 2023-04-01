#include "widgets/LocalGitBranchesPage.h"
#include "widgets/ui_LocalGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "LocalGitBranchesPage"

using namespace GuiManager::Models;

namespace GuiManager {

namespace Widgets {

LocalGitBranchesPage::LocalGitBranchesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::LocalGitBranchesPage),
      _model(new BranchesModel(this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

LocalGitBranchesPage::~LocalGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::updateActiveBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index = _model->indexForBranch(branchName);
  _model->setActiveIndex(index);

  ui->listView->scrollTo(_model->index(index, 0));

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::updateModel(const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(localBranches);

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
