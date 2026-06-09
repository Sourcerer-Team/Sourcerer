#include "widgets/RemoteGitBranchesPage.h"
#include "widgets/ui_RemoteGitBranchesPage.h"

#include <Log.h>

#define THREAD_ID "RemoteGitBranchesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

RemoteGitBranchesPage::RemoteGitBranchesPage(QWidget *parent)
    : QWidget(parent), _model(new BranchesModel(this)),
      ui(new Ui::RemoteGitBranchesPage), _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(ui->listView, &QListView::doubleClicked, this,
          &RemoteGitBranchesPage::onListViewDoubleClicked);
  connect(ui->listView, &QListView::customContextMenuRequested, this,
          &RemoteGitBranchesPage::onListViewContextMenuRequested);

  LOG_PERF(0, "Exit");
}

RemoteGitBranchesPage::~RemoteGitBranchesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::clearCached() {
  LOG_PERF(0, "Enter");

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateActiveBranch(const QString &branchName) {
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

void RemoteGitBranchesPage::updateModel(const QStringList &remoteBranches) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(remoteBranches);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::updateModelWithStatus(
    const QVector<GitRepo::BranchInfo> &branchesWithStatus) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModelWithStatus(branchesWithStatus);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::onListViewContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listView->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString branchName = _model->branchForIndex(index.row());

  const QStringList splitted = branchName.split(' ');
  if (splitted.length() > 1) {
    branchName = splitted[0];
  }

  emit branchRightClicked(branchName, ui->listView->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void RemoteGitBranchesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.row() == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString branchName = _model->branchForIndex(index.row());

  const QStringList splitted = branchName.split(' ');
  if (splitted.length() > 1) {
    branchName = splitted[0];
  }

  emit branchSelected(branchName);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets