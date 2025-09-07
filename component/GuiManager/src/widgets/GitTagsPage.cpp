#include "widgets/GitTagsPage.h"
#include "widgets/ui_GitTagsPage.h"

#include <Log.h>

#define THREAD_ID "GitTagsPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

GitTagsPage::GitTagsPage(QWidget *parent)
    : QWidget(parent), _model(new TagsModel(this)), ui(new Ui::GitTagsPage),
      _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  connect(ui->listView, &QListView::doubleClicked, this,
          &GitTagsPage::onListViewDoubleClicked);

  LOG_PERF(0, "Exit");
}

GitTagsPage::~GitTagsPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void GitTagsPage::updateActiveBranch(const QString &tagName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index = _model->indexForBranch(tagName);
  _model->setActiveIndex(index);

  _previous = index;

  LOG_PERF(0, "Exit")
}

void GitTagsPage::updateModel(const QStringList &tags) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(tags);

  ui->listView->setModel(_model);

  _previous = -1;

  LOG_PERF(0, "Exit");
}

void GitTagsPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.row() == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_PERF(0, "Exit");

  emit tagSelected(_model->branchForIndex(index.row()));
}

} // namespace GuiManager::Widgets