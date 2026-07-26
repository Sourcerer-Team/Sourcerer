#include "containers/HomeContainer.h"
#include "MousePositionCorrection.h"

#include "Constants.h"
#include "containers/ui_HomeContainer.h"
#include "delegates/HomeRecentItemDelegate.h"
#include "widgets/RecentTableView.h"
#include "widgets/WorkspacesTableView.h"

#include <GitHubRepo.h>
#include <Log.h>

#include <QAction>
#include <QDir>
#include <QEvent>
#include <QHeaderView>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QVariantMap>

#define THREAD_ID "HomeContainer"

namespace GuiManager {

using namespace Delegates;

namespace Containers {

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Home, parent), ui(new Ui::HomeContainer),
      _recentlyOpenedModel(new RecentlyOpenedReposModel(this)),
      _workspacesModel(new WorkspacesModel(this)),
      _issueTrackerConnectionsModel(new QStandardItemModel(0, 3, this)),
      _itemDelegate(new HomeRecentItemDelegate(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()),
      _dispatcher(1) {
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
  ui->tableViewIssueTrackerConnections->setModel(_issueTrackerConnectionsModel);
  ui->tableViewIssueTrackerConnections->horizontalHeader()
      ->setStretchLastSection(true);
  _issueTrackerConnectionsModel->setHorizontalHeaderLabels(
      {tr("Service"), tr("Repository"), tr("Status")});

  auto *connectToMenu = new QMenu(ui->connectToButton);
  auto *githubIssueTrackerAction =
      connectToMenu->addAction(tr("GitHub issue tracker"));
  connect(githubIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onGitHubIssueTrackerConnectClicked);

  ui->connectToButton->setMenu(connectToMenu);

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
  delete _issueTrackerConnectionsModel;

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

void HomeContainer::onGitHubIssueTrackerConnectClicked() {
  LOG_PERF(0, "Enter");

  const QString clientId =
      _configManager.getString(ConfigManager::ConfigKey::GitHubOAuthClientId);
  if (clientId.isEmpty()) {
    LOG_PERF(0, "Exit - cancelled while entering client id");
    return;
  }

  const auto authRepo =
      QSharedPointer<IssueTrackers::GitHubRepo>::create(QString(), QString());
  const WeakPtr guard = getSelf().toWeakRef();
  const QVariantMap authParameters = {
      {QStringLiteral("clientId"), clientId},
      {QStringLiteral("scope"), QStringLiteral("repo read:org")}};

  _dispatcher.dispatch([=]() {
    const QVariantMap authResult = authRepo->authenticate(authParameters);
    const bool success = authResult.value(QStringLiteral("success")).toBool();
    const bool cancelled = authResult.value(QStringLiteral("cancelled")).toBool();
    const QString errorMessage =
        authResult.value(QStringLiteral("errorMessage")).toString();

    if (!success) {
      if (cancelled) {
        LOG_INFO("GitHub OAuth authentication cancelled by user");
        return;
      }

      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::critical(this, tr("Failure"),
                              errorMessage.isEmpty()
                                  ? tr("Failed to authenticate with GitHub")
                                  : errorMessage);
      });
      return;
    }

    QString repositoriesErrorMessage;
    const QStringList repositories =
        authRepo->listUserRepositories(&repositoriesErrorMessage);
    if (repositories.isEmpty()) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::warning(
            this, tr("GitHub repositories"),
            repositoriesErrorMessage.isEmpty()
                ? tr("No GitHub repositories were found for the authenticated account.")
                : repositoriesErrorMessage);
      });
      return;
    }

    QString selectedRepository;
    bool repositorySelectionCancelled = false;
    const auto showRepositorySelectionDialog =
        [=, &selectedRepository, &repositorySelectionCancelled]() {
          QDialog dialog(nullptr);
          dialog.setWindowTitle(tr("Choose GitHub repository"));
          dialog.setModal(true);
          dialog.setMinimumWidth(420);

          auto *layout = new QVBoxLayout(&dialog);

          auto *label = new QLabel(
              tr("Select the GitHub repository to connect:"), &dialog);
          label->setWordWrap(true);
          layout->addWidget(label);

          auto *comboBox = new QComboBox(&dialog);
          comboBox->addItems(repositories);
          comboBox->setCurrentIndex(0);
          layout->addWidget(comboBox);

          auto *buttonBox = new QDialogButtonBox(
              QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
          connect(buttonBox, &QDialogButtonBox::accepted, &dialog,
                  &QDialog::accept);
          connect(buttonBox, &QDialogButtonBox::rejected, &dialog,
                  &QDialog::reject);
          layout->addWidget(buttonBox);

          if (dialog.exec() == QDialog::Accepted) {
            selectedRepository = comboBox->currentText();
          } else {
            repositorySelectionCancelled = true;
          }
        };

    if (QThread::currentThread() == qApp->thread()) {
      showRepositorySelectionDialog();
    } else {
      QMetaObject::invokeMethod(qApp, showRepositorySelectionDialog,
                                Qt::BlockingQueuedConnection);
    }

    if (repositorySelectionCancelled || selectedRepository.isEmpty()) {
      LOG_INFO("GitHub repository selection cancelled by user");
      return;
    }

    const QStringList selectedPath = selectedRepository.split(QStringLiteral("/"));
    if (selectedPath.size() < 2 || selectedPath.first().isEmpty() ||
        selectedPath.last().isEmpty()) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::critical(this, tr("Failure"),
                              tr("Failed to parse the selected repository."));
      });
      return;
    }

    const auto selectedRepo =
        QSharedPointer<IssueTrackers::GitHubRepo>::create(selectedPath.first(),
                                                          selectedPath.last(),
                                                          authRepo->host());
    selectedRepo->setAccessToken(authRepo->accessToken(),
                                 authRepo->accessTokenExpiresAt());

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _issueTrackerConnections += selectedRepo;

      const QList<QStandardItem *> row = {
          new QStandardItem(selectedRepo->serviceName()),
          new QStandardItem(selectedRepo->repositoryPath()),
          new QStandardItem(selectedRepo->hasValidAccessToken()
                                ? tr("Connected")
                                : tr("Authenticated"))};
      _issueTrackerConnectionsModel->appendRow(row);
      ui->tableViewIssueTrackerConnections->resizeColumnsToContents();
    });

    LOG_PERF(0, "Exit");
  });
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
