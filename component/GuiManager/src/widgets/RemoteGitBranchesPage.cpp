#include "widgets/RemoteGitBranchesPage.h"
#include "widgets/ui_RemoteGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "RemoteGitBranchesPage"

using namespace GuiManager::Models;

namespace GuiManager {

namespace Widgets {

RemoteGitBranchesPage::RemoteGitBranchesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::RemoteGitBranchesPage),
      _model(new BranchesModel(this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

RemoteGitBranchesPage::~RemoteGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateActiveBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index = _model->indexForBranch(branchName);
  _model->setActiveIndex(index);

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateModel(const QStringList &remoteBranches) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(remoteBranches);

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
