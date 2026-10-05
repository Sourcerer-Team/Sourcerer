#include "widgets/LocalGitBranchesPage.h"
#include "delegates/GraphiteBranchDelegate.h"
#include "widgets/MoveProxy.h"
#include "widgets/ui_LocalGitBranchesPage.h"

#include <GitRepo.h>
#include <Log.h>

#define THREAD_ID "LocalGitBranchesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

LocalGitBranchesPage::LocalGitBranchesPage(QWidget *parent)
    : QWidget(parent), _model(new BranchesModel(this)),
      ui(new Ui::LocalGitBranchesPage), _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->listView->setDragEnabled(true);
  ui->listView->setAcceptDrops(true);
  ui->listView->setDropIndicatorShown(true);
  ui->listView->setDragDropMode(QAbstractItemView::DragDrop);
  ui->listView->setDefaultDropAction(Qt::CopyAction);
  ui->listView->setDragDropOverwriteMode(false);
  ui->listView->viewport()->setAcceptDrops(true);

  connect(ui->listView, &QListView::doubleClicked, this,
          &LocalGitBranchesPage::onListViewDoubleClicked);
  connect(ui->listView, &QListView::customContextMenuRequested, this,
          &LocalGitBranchesPage::onListViewContextMenuRequested);

  connect(_model, &BranchesModel::branchesMoved, this,
          &LocalGitBranchesPage::onBranchesMoved);

  ui->listView->setModel(_model);

  _defaultDelegate = ui->listView->itemDelegate();
  _graphiteDelegate = new Delegates::GraphiteBranchDelegate(this);

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

  _activeBranch = branchName;

  _model->setActiveIndex(index);

  ui->listView->scrollTo(_model->index(index, 0));

  _previous = index;

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::updateModel(const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  _localBranches = localBranches;
  rebuild();

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::setGraphiteStacks(
    const GitRepo::GraphiteStacks &stacks) {
  LOG_PERF(0, "Enter");

  _graphiteStacks = stacks;
  rebuild();
  // The stacks arrive after the branches, so the active branch is restored
  // here rather than by the container.
  if (!_activeBranch.isEmpty()) {
    updateActiveBranch(_activeBranch);
  }

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::clearGraphiteStacks() {
  LOG_PERF(0, "Enter");

  if (!_graphiteStacks.valid) {
    LOG_PERF(0, "Exit");
    return;
  }

  _graphiteStacks = {};
  rebuild();
  if (!_activeBranch.isEmpty()) {
    updateActiveBranch(_activeBranch);
  }

  LOG_PERF(0, "Exit");
}

void LocalGitBranchesPage::rebuild() {
  _model->clear();
  _model->setActiveIndex(-1);

  // The first item shows the history of all branches.
  QStringList branches;
  if (!_localBranches.isEmpty()) {
    branches += QString(ALL_BRANCHES);
  }

  if (_graphiteStacks.valid) {
    const QVector<GitRepo::GraphiteBranchRow> rows =
        GitRepo::graphiteBranchRows(_graphiteStacks, _localBranches);
    int headerRow = -1;
    for (const GitRepo::GraphiteBranchRow &row : rows) {
      if (row.kind == GitRepo::GraphiteBranchRow::Kind::UntrackedHeader) {
        headerRow = int(branches.size());
        branches += QString();
      } else {
        branches += row.branch;
      }
    }
    _model->updateModel(branches);
    _model->setGraphite(rows, _graphiteStacks, headerRow);
    ui->listView->setItemDelegate(_graphiteDelegate);
  } else {
    _model->clearGraphite();
    branches += _localBranches;
    _model->updateModel(branches);
    ui->listView->setItemDelegate(_defaultDelegate);
  }

  _previous = -1;
}

void LocalGitBranchesPage::onBranchesMoved(const QString &sourceBranch,
                                           const QString &destBranch) {
  if (sourceBranch == ALL_BRANCHES || destBranch == ALL_BRANCHES) {
    // '*' is not a branch, there is nothing to merge or rebase.
    return;
  }

  emit branchesMoved(sourceBranch, destBranch);
}

void LocalGitBranchesPage::onListViewContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listView->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString branchName = _model->branchForIndex(index.row());
  if (branchName.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  emit branchRightClicked(branchName, ui->listView->mapToGlobal(pos));

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
  if (branchName.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  emit branchSelected(branchName);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
