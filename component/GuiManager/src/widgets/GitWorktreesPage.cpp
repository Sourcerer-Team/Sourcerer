// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/GitWorktreesPage.h"
#include "widgets/ui_GitWorktreesPage.h"

#include <Log.h>

#define THREAD_ID "GitWorktreesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

GitWorktreesPage::GitWorktreesPage(QWidget *parent)
    : QWidget(parent), _model(new WorktreesModel(this)),
      ui(new Ui::GitWorktreesPage) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->listView->setModel(_model);

  connect(ui->listView, &QListView::doubleClicked, this,
          &GitWorktreesPage::onListViewDoubleClicked);
  connect(ui->listView, &QListView::customContextMenuRequested, this,
          &GitWorktreesPage::onListViewContextMenuRequested);

  LOG_PERF(0, "Exit");
}

GitWorktreesPage::~GitWorktreesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void GitWorktreesPage::updateModel(
    const QVector<GitRepo::WorktreeInfo> &worktrees) {
  LOG_PERF(0, "Enter");

  _model->updateModel(worktrees);

  LOG_PERF(0, "Exit");
}

void GitWorktreesPage::onListViewContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listView->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    emit rightClicked(ui->listView->mapToGlobal(pos));
    return;
  }

  emit worktreeRightClicked(_model->worktreeForIndex(index.row()),
                            ui->listView->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitWorktreesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const GitRepo::WorktreeInfo worktree = _model->worktreeForIndex(index.row());

  LOG_PERF(0, "Exit");

  emit worktreeSelected(worktree);
}

} // namespace GuiManager::Widgets
