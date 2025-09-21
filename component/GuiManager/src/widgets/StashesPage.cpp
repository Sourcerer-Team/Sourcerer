#include "widgets/StashesPage.h"
#include "widgets/ui_StashesPage.h"

#include <Log.h>
#include <QListView>

#define THREAD_ID "StashesPagePage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

StashesPage::StashesPage(QWidget *parent)
    : QWidget(parent), _model(new StashesModel(this)), ui(new Ui::StashesPage),
      _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  connect(ui->listView, &QListView::doubleClicked, this,
          &StashesPage::onListViewDoubleClicked);

  LOG_PERF(0, "Exit");
}

StashesPage::~StashesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void StashesPage::clearCached() {
  LOG_PERF(0, "Enter");

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void StashesPage::updateActiveBranch(const QString &stashName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index =
      !stashName.isEmpty() ? _model->indexForStash(stashName) : -1;

  if (stashName == "*") {
    _previous = index;
    LOG_PERF(0, "Exit");
    return;
  }

  _model->setActiveIndex(index);

  ui->listView->scrollTo(_model->index(index, 0));

  _previous = index;

  LOG_PERF(0, "Exit");
}

void StashesPage::updateModel(const QStringList &stashes) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(stashes);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void StashesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.row() == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString stashName = _model->stashForIndex(index.row());

  emit stashSelected(stashName);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
