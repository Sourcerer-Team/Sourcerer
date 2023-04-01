#include "widgets/GitSubmodulesPage.h"
#include "widgets/ui_GitSubmodulesPage.h"

#include <Log.h>

#define THREAD_ID "GitSubmodulesPage"

using namespace GuiManager::Models;

namespace GuiManager {

namespace Widgets {

GitSubmodulesPage::GitSubmodulesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GitSubmodulesPage),
      _model(new SubmodulesModel(this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

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

  LOG_PERF(0, "Exit")
}

void GitSubmodulesPage::updateModel(const QStringList &submodules) {
  LOG_PERF(0, "Enter");

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(submodules);

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
