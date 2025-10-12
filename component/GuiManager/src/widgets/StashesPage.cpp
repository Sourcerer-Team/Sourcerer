#include "widgets/StashesPage.h"
#include "widgets/ui_StashesPage.h"

#include <Log.h>

#include <QListView>
#include <QMenu>

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
  connect(this, &StashesPage::rightClickOnStash, this,
          &StashesPage::onStashRightClicked);

  ui->listView->viewport()->installEventFilter(this);

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

int StashesPage::lastStash() const {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return _model->rowCount() > 0 ? 0 : -1;
}

bool StashesPage::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    if (const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        mouseEvent->button() == Qt::RightButton) {
      if (const auto *listView = qobject_cast<QListView *>(obj->parent())) {
        if (const QModelIndex index = listView->indexAt(mouseEvent->pos());
            index.isValid()) {
          emit rightClickOnStash(index.row(), mouseEvent->globalPos());
        }
      }
    }
  }

  return QObject::eventFilter(obj, event);
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

  emit stashSelected(index.row());

  LOG_PERF(0, "Exit");
}

void StashesPage::onStashRightClicked(int stashIndex, const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction actionApply(tr("Apply"), this);
  QAction actionPop(tr("Pop"), this);
  QAction actionDelete(tr("Delete"), this);

  connect(&actionApply, &QAction::triggered, this,
          [=]() { emit applyStash(stashIndex); });
  connect(&actionPop, &QAction::triggered, this,
          [=]() { emit popStash(stashIndex); });
  connect(&actionDelete, &QAction::triggered, this,
          [=]() { emit deleteStash(stashIndex); });

  QMenu menu(this);
  menu.addAction(&actionApply);
  menu.addAction(&actionPop);
  menu.addSeparator();
  menu.addAction(&actionDelete);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
