#include "containers/HomeContainer.h"

#include "Dispatcher.h"
#include "containers/ui_HomeContainer.h"
#include "delegates/HomeRecentItemDelegate.h"
#include "widgets/RecentTableView.h"

#include <Log.h>

#include <QEvent>
#include <QMouseEvent>

#define THREAD_ID "HomeContainer"

namespace GuiManager {

using namespace Delegates;

namespace Containers {

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Home, parent), ui(new Ui::HomeContainer),
      _recentlyOpenedModel(new RecentlyOpenedReposModel(this)),
      _workspacesModel(new WorkspacesModel(this)),
      _itemDelegate(new HomeRecentItemDelegate(this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->tableViewRecent->setModel(_recentlyOpenedModel);
  ui->tableViewRecent->resizeColumnsToContents();
  ui->tableViewRecent->setItemDelegate(_itemDelegate);
  ui->tableViewRecent->viewport()->installEventFilter(this);
  ui->tableViewWorkspaces->setModel(_workspacesModel);
  ui->tableViewWorkspaces->resizeColumnsToContents();
  ui->tableViewWorkspaces->setItemDelegate(_itemDelegate);
  ui->tableViewWorkspaces->viewport()->installEventFilter(this);

  connect(ui->tableViewRecent, &RecentTableView::hoverEnter, this,
          &HomeContainer::hoverEnter);
  connect(ui->tableViewRecent, &RecentTableView::hoverLeave, this,
          &HomeContainer::hoverLeave);
  connect(ui->openButton, &QPushButton::clicked, this,
          &HomeContainer::onOpenClicked);
  connect(ui->cloneButton, &QPushButton::clicked, this,
          &HomeContainer::onCloneClicked);
  connect(ui->initButton, &QPushButton::clicked, this,
          &HomeContainer::onInitClicked);

  LOG_PERF(0, "Exit");
}

HomeContainer::~HomeContainer() {
  LOG_PERF(0, "Enter");

  delete ui;
  delete _recentlyOpenedModel;
  delete _workspacesModel;

  LOG_PERF(0, "Exit");
}

void HomeContainer::reloadRecentlyOpened() const {
  LOG_PERF(0, "Enter");

  _recentlyOpenedModel->reload();

  LOG_PERF(0, "Exit");
}

void HomeContainer::reloadWorkspaces() const {
  LOG_PERF(0, "Enter");

  _workspacesModel->reload();

  LOG_PERF(0, "Exit");
}

void HomeContainer::selected() {
  LOG_PERF(0, "Enter");

  reloadRecentlyOpened();
  reloadWorkspaces();

  LOG_PERF(0, "Exit");
}

bool HomeContainer::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    if (const auto mouseEvent = static_cast<QMouseEvent *>(event);
        mouseEvent->button() == Qt::LeftButton) {
      if (const auto viewport = qobject_cast<QWidget *>(obj);
          viewport->parentWidget() == ui->tableViewRecent) {
        const auto view = qobject_cast<GuiManager::Widgets::RecentTableView *>(
            viewport->parentWidget());
        if (const QModelIndex index = view->indexAt(mouseEvent->pos());
            index.isValid()) {
          ui->tableViewRecent->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewRecent->setCurrentIndex(index);
          const QModelIndex pathIndex =
              _recentlyOpenedModel->createIndex(index.row(), 1);
          const QString path = pathIndex.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
            emit recentClicked(path);
          });
        }
      } else if (viewport->parentWidget() == ui->tableViewWorkspaces) {
        const auto view = qobject_cast<QTableView *>(viewport->parentWidget());
        if (const QModelIndex index = view->indexAt(mouseEvent->pos());
            index.isValid()) {
          ui->tableViewWorkspaces->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewWorkspaces->setCurrentIndex(index);
          const QString name = index.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
            emit workspaceClicked(name);
          });
        }
      }
    }
  }

  return ContainerWidget::eventFilter(obj, event);
}

void HomeContainer::hoverEnter(const QModelIndex &index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Row: {}, Column: {}", index.row(), index.column());

  if (index.column() != 0) {
    setCursor(Qt::ArrowCursor);
    LOG_PERF(0, "Exit");
    return;
  }

  _itemDelegate->setHover(index);
  ui->tableViewRecent->viewport()->update();

  setCursor(Qt::PointingHandCursor);

  LOG_PERF(0, "Exit");
}

void HomeContainer::hoverLeave(const QModelIndex &index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Row: {}, Column: {}", index.row(), index.column());

  _itemDelegate->setHover(QModelIndex());
  ui->tableViewRecent->viewport()->update();

  setCursor(Qt::ArrowCursor);

  LOG_PERF(0, "Exit");
}

void HomeContainer::onCloneClicked() {
  LOG_PERF(0, "Enter");

  emit clone();

  LOG_PERF(0, "Exit");
}

void HomeContainer::onInitClicked() {
  LOG_PERF(0, "Enter");

  emit init();

  LOG_PERF(0, "Exit");
}

void HomeContainer::onOpenClicked(bool checked) {
  LOG_PERF(0, "Enter");

  emit open();

  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
