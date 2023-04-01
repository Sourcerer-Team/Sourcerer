#include "widgets/GitSubmodulesPage.h"
#include "widgets/ui_GitSubmodulesPage.h"

#include <Log.h>

#define THREAD_ID "GitSubmodulesPage"

namespace GuiManager {

namespace Widgets {

GitSubmodulesPage::GitSubmodulesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GitSubmodulesPage), _model(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

GitSubmodulesPage::~GitSubmodulesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void GitSubmodulesPage::updateModel(const QStringList &submodules) {
  LOG_PERF(0, "Enter");

  delete _model;

  _model = new QStandardItemModel(this);
  auto rit = submodules.rbegin();
  while (rit != submodules.rend()) {
    QStandardItem *item =
        new QStandardItem(QIcon(":/icons/git_submodule.png"), *rit);
    item->setToolTip(*rit);
    _model->insertRow(0, item);
    ++rit;
  }

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
