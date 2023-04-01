#include "widgets/GitTagsPage.h"
#include "widgets/ui_GitTagsPage.h"

#include <Log.h>

#define THREAD_ID "GitTagsPage"

using namespace GuiManager::Models;

namespace GuiManager {

namespace Widgets {

GitTagsPage::GitTagsPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GitTagsPage), _model(new TagsModel(this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

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

  LOG_PERF(0, "Exit")
}

void GitTagsPage::updateModel(const QStringList &tags) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(tags);

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
