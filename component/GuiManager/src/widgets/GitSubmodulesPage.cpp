#include "widgets/GitSubmodulesPage.h"
#include "widgets/ui_GitSubmodulesPage.h"

#include <Log.h>

#define THREAD_ID "GitSubmodulesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

GitSubmodulesPage::GitSubmodulesPage(QWidget *parent)
    : QWidget(parent), _model(new SubmodulesModel(this)),
      ui(new Ui::GitSubmodulesPage), _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->listView->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(ui->listView, &QListView::doubleClicked, this,
          &GitSubmodulesPage::onListViewDoubleClicked);
  connect(ui->listView, &QListView::customContextMenuRequested, this,
          &GitSubmodulesPage::onListViewContextMenuRequested);

  LOG_PERF(0, "Exit");
}

GitSubmodulesPage::~GitSubmodulesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void GitSubmodulesPage::updateActiveBranch(const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index = _model->indexForBranch(submoduleName);
  _model->setActiveIndex(index);

  _previous = index;

  LOG_PERF(0, "Exit")
}

void GitSubmodulesPage::updateModel(const QStringList &submodules) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(submodules);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void GitSubmodulesPage::updateModelWithStatus(
    const QVector<GitRepo::SubmoduleInfo> &submodulesWithStatus) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModelWithStatus(submodulesWithStatus);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void GitSubmodulesPage::onListViewContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listView->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    emit rightClicked(ui->listView->mapToGlobal(pos));
    return;
  }

  QString submoduleName = _model->branchForIndex(index.row());

  const QStringList splitted = submoduleName.split(' ');
  if (splitted.length() > 1) {
    submoduleName = splitted[0];
  }

  emit submoduleRightClicked(submoduleName, ui->listView->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitSubmodulesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.row() == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString submoduleName = _model->branchForIndex(index.row());

  const QStringList splitted = submoduleName.split(' ');
  if (splitted.length() > 1) {
    submoduleName = splitted[0];
  }

  LOG_PERF(0, "Exit");

  emit submoduleSelected(submoduleName);
}

} // namespace GuiManager::Widgets