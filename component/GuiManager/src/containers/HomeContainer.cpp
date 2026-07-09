#include "containers/HomeContainer.h"
#include "MousePositionCorrection.h"

#include "Constants.h"
#include "Dispatcher.h"
#include "containers/ui_HomeContainer.h"
#include "delegates/HomeRecentItemDelegate.h"
#include "widgets/RecentTableView.h"
#include "widgets/WorkspacesTableView.h"

#include <Log.h>

#include <QDir>
#include <QEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QStandardPaths>

#define THREAD_ID "HomeContainer"

namespace GuiManager {

using namespace Delegates;

namespace Containers {

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Home, parent), ui(new Ui::HomeContainer),
      _recentlyOpenedModel(new RecentlyOpenedReposModel(this)),
      _workspacesModel(new WorkspacesModel(this)),
      _itemDelegate(new HomeRecentItemDelegate(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()) {
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
  connect(ui->tableViewWorkspaces, &WorkspacesTableView::hoverEnter, this,
          &HomeContainer::hoverEnter);
  connect(ui->tableViewWorkspaces, &WorkspacesTableView::hoverLeave, this,
          &HomeContainer::hoverLeave);
  connect(ui->openButton, &QPushButton::clicked, this,
          &HomeContainer::onOpenClicked);
  connect(ui->cloneButton, &QPushButton::clicked, this,
          &HomeContainer::onCloneClicked);
  connect(ui->initButton, &QPushButton::clicked, this,
          &HomeContainer::onInitClicked);
  connect(this, &HomeContainer::recentRightClicked, this,
          &HomeContainer::onRecentRightClicked);
  connect(this, &HomeContainer::workspaceRightClicked, this,
          &HomeContainer::onWorkspaceRightClicked);

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
    const auto mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton) {
      if (const auto viewport = qobject_cast<QWidget *>(obj);
          viewport->parentWidget() == ui->tableViewRecent) {
        const auto view = qobject_cast<GuiManager::Widgets::RecentTableView *>(
            viewport->parentWidget());
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index =
                view->indexAt(viewport->mapFromGlobal(globalPos));
            index.isValid()) {
          ui->tableViewRecent->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewRecent->setCurrentIndex(index);
          const QModelIndex pathIndex =
              _recentlyOpenedModel->createIndex(index.row(), 1);
          const QString path = pathIndex.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(
              getSelf().toWeakRef(), [=]() { emit recentClicked(path); });
        }
      } else if (viewport->parentWidget() == ui->tableViewWorkspaces) {
        const auto view = qobject_cast<QTableView *>(viewport->parentWidget());
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index =
                view->indexAt(viewport->mapFromGlobal(globalPos));
            index.isValid()) {
          ui->tableViewWorkspaces->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewWorkspaces->setCurrentIndex(index);
          const QString name = index.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(
              getSelf().toWeakRef(), [=]() { emit workspaceClicked(name); });
        }
      }
    } else if (mouseEvent->button() == Qt::RightButton) {
      if (const auto viewport = qobject_cast<QWidget *>(obj);
          viewport->parentWidget() == ui->tableViewRecent) {
        const auto view = qobject_cast<GuiManager::Widgets::RecentTableView *>(
            viewport->parentWidget());
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index =
                view->indexAt(viewport->mapFromGlobal(globalPos));
            index.isValid()) {
          ui->tableViewRecent->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewRecent->setCurrentIndex(index);
          const QModelIndex pathIndex =
              _recentlyOpenedModel->createIndex(index.row(), 1);
          const QString path = pathIndex.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
            emit recentRightClicked(path, globalPos);
          });
        }
      } else if (viewport->parentWidget() == ui->tableViewWorkspaces) {
        const auto view = qobject_cast<QTableView *>(viewport->parentWidget());
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index =
                view->indexAt(viewport->mapFromGlobal(globalPos));
            index.isValid()) {
          ui->tableViewWorkspaces->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewWorkspaces->setCurrentIndex(index);
          const QString name = index.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
            emit workspaceRightClicked(name, globalPos);
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

void HomeContainer::onRecentRightClicked(const QString &path,
                                         const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction removeAction(tr("Remove"), this);
  connect(&removeAction, &QAction::triggered, this, [=]() {
    QString recentPaths =
        _configManager.getString(ConfigManager::ConfigKey::RecentlyOpenedRepos);
    QStringList pathsList = recentPaths.split(";", Qt::SkipEmptyParts);

    pathsList.removeAll(QDir::fromNativeSeparators(path));
    recentPaths = pathsList.join(";");

    _configManager.setString(ConfigManager::ConfigKey::RecentlyOpenedRepos,
                             recentPaths);
    _configManager.save();

    _recentlyOpenedModel->reload();
  });

  QMenu menu(this);
  menu.addAction(&removeAction);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void HomeContainer::onWorkspaceRightClicked(const QString &name,
                                            const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction removeAction(tr("Remove"), this);
  connect(&removeAction, &QAction::triggered, this, [=]() {
    const QString writableDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    const QString workspacesDirectory =
        writableDir + "/" + WORKSPACES_SUBDIRECTORY;
    const QString workspaceFilePath =
        workspacesDirectory + "/" + name + ".workspace";

    QFile::remove(workspaceFilePath);

    _workspacesModel->reload();
  });

  QMenu menu(this);
  menu.addAction(&removeAction);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
