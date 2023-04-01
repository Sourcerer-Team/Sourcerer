#include "widgets/GitTagsPage.h"
#include "widgets/ui_GitTagsPage.h"

#include <Log.h>

#define THREAD_ID "GitTagsPage"

namespace GuiManager {

namespace Widgets {

GitTagsPage::GitTagsPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GitTagsPage), _model(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

GitTagsPage::~GitTagsPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void GitTagsPage::updateModel(const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  delete _model;

  _model = new QStandardItemModel(this);
  auto rit = localBranches.rbegin();
  while (rit != localBranches.rend()) {
    QStandardItem *item =
        new QStandardItem(QIcon(":/icons/git_tag.png"), *rit);
    item->setToolTip(*rit);
    _model->insertRow(0, item);
    ++rit;
  }

  ui->listView->setModel(_model);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
