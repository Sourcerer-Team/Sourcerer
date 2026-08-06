#include "containers/HomeContainer.h"
#include "MousePositionCorrection.h"

#include "Constants.h"
#include "containers/ui_HomeContainer.h"
#include "delegates/HomeRecentItemDelegate.h"
#include "widgets/RecentTableView.h"
#include "widgets/WorkspacesTableView.h"

#include <BitbucketRepo.h>
#include <GitHubRepo.h>
#include <Log.h>
#include <StylesheetManager.h>
#include <TokensStorage.h>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QLabel>
#include <QList>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QString>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QVariantMap>

#define THREAD_ID "HomeContainer"

namespace GuiManager {

using namespace Delegates;

namespace Containers {

namespace {

const QUrl BITBUCKET_CALLBACK_URL(
    QStringLiteral("http://127.0.0.1:8080/callback"));

QString htmlEncode(const QString &text) {
  return text.toHtmlEscaped();
}

QString bitbucketCallbackSuccessPage() {
  return QStringLiteral(
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<title>Bitbucket OAuth complete</title></head><body>"
      "<h1>Authorization complete</h1>"
      "<p>You can return to Sourcerer now.</p>"
      "</body></html>");
}

QString bitbucketCallbackErrorPage(const QString &message) {
  return QStringLiteral(
      "<!doctype html><html><head><meta charset=\"utf-8\">"
      "<title>Bitbucket OAuth error</title></head><body>"
      "<h1>Authorization failed</h1><p>%1</p>"
      "</body></html>")
      .arg(htmlEncode(message));
}

void writeHttpResponse(QTcpSocket *socket, const QString &body,
                       const QString &statusLine = QStringLiteral("200 OK")) {
  const QByteArray bodyBytes = body.toUtf8();
  const QByteArray headers =
      QByteArray("HTTP/1.1 ") + statusLine.toUtf8() +
      QByteArray("\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\nContent-Length: ") +
      QByteArray::number(bodyBytes.size()) + QByteArray("\r\n\r\n");
  socket->write(headers);
  socket->write(bodyBytes);
  socket->flush();
  socket->disconnectFromHost();
}

} // namespace

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Home, parent), ui(new Ui::HomeContainer),
      _recentlyOpenedModel(new RecentlyOpenedReposModel(this)),
      _workspacesModel(new WorkspacesModel(this)),
      _issueTrackersModel(new IssueTrackersModel(this)),
      _itemDelegate(new HomeRecentItemDelegate(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()),
      _toastHost(ToastHostWidget::getInstance()),
      _dispatcher(1),
      _selected(false) {
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
  ui->tableViewIssueTrackerConnections->setModel(_issueTrackersModel);
  ui->tableViewIssueTrackerConnections->setItemDelegate(_itemDelegate);
  ui->tableViewIssueTrackerConnections->resizeColumnsToContents();
  ui->tableViewIssueTrackerConnections->viewport()->installEventFilter(this);
  ui->tableViewIssueTrackerConnections->horizontalHeader()
      ->setStretchLastSection(true);
  ui->tableViewIssueTrackerConnections->horizontalHeader()
      ->setSectionResizeMode(0, QHeaderView::Fixed);
  ui->tableViewIssueTrackerConnections->setColumnWidth(0, 24);
  ui->tableViewIssueTrackerConnections->setIconSize(QSize(16, 16));

  auto *connectToMenu = new QMenu(ui->connectToButton);
  auto *githubIssueTrackerAction =
      connectToMenu->addAction(tr("GitHub issue tracker"));
  connect(githubIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onGitHubIssueTrackerConnectClicked);
  auto *bitbucketIssueTrackerAction =
      connectToMenu->addAction(tr("Bitbucket issue tracker"));
  connect(bitbucketIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onBitbucketOAuthConnectClicked);

  ui->connectToButton->setMenu(connectToMenu);

  connect(ui->tableViewIssueTrackerConnections,
          &IssueTrackersTableView::hoverEnter, this,
          &HomeContainer::hoverEnter);
  connect(ui->tableViewIssueTrackerConnections,
          &IssueTrackersTableView::hoverLeave, this,
          &HomeContainer::hoverLeave);
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
  connect(this, &HomeContainer::issueTrackerRightClicked, this,
          &HomeContainer::onIssueTrackerRightClicked);
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
  delete _issueTrackersModel;

  LOG_PERF(0, "Exit");
}

void HomeContainer::reloadIssueTrackers() const {
  LOG_PERF(0, "Enter");

  _issueTrackersModel->reload();

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

void HomeContainer::deselected()
{
  LOG_PERF(0, "Enter");

  _selected = false;

  LOG_PERF(0, "Exit");
}

void HomeContainer::selected() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  LOG_DEBUG("Title bar height retrieved: {} px", titleBarHeight);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);
  _toastHost.raise();
  LOG_DEBUG("Toast host configured and raised");

  reloadRecentlyOpened();
  reloadWorkspaces();
  reloadIssueTrackers();

  ui->tableViewIssueTrackerConnections->resizeColumnsToContents();
  ui->tableViewIssueTrackerConnections->setColumnWidth(0, 24);

  _selected = true;

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
      } else if (viewport->parentWidget() ==
                 ui->tableViewIssueTrackerConnections) {
        const auto view = qobject_cast<QTableView *>(viewport->parentWidget());
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index =
                view->indexAt(viewport->mapFromGlobal(globalPos));
            index.isValid()) {
          ui->tableViewIssueTrackerConnections->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewIssueTrackerConnections->setCurrentIndex(index);
          const QModelIndex serviceIndex =
              _issueTrackersModel->index(index.row(), 0);
          const QModelIndex repoIndex =
              _issueTrackersModel->index(index.row(), 1);
          const QString serviceType =
              serviceIndex.data(Qt::UserRole).toString();
          const QString name = repoIndex.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
            emit issueTrackerClicked(serviceType, name);
          });
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
      } else if (viewport->parentWidget() ==
                 ui->tableViewIssueTrackerConnections) {
        const auto view = qobject_cast<QTableView *>(viewport->parentWidget());
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index =
                view->indexAt(viewport->mapFromGlobal(globalPos));
            index.isValid()) {
          ui->tableViewIssueTrackerConnections->selectionModel()->select(
              index, QItemSelectionModel::Select);
          ui->tableViewIssueTrackerConnections->setCurrentIndex(index);
          const QModelIndex serviceIndex =
              _issueTrackersModel->index(index.row(), 0);
          const QModelIndex repoIndex =
              _issueTrackersModel->index(index.row(), 1);
          const QString serviceType =
              serviceIndex.data(Qt::UserRole).toString();
          const QString name = repoIndex.data(Qt::DisplayRole).toString();
          Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
            emit issueTrackerRightClicked(serviceType, name, globalPos);
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
  ui->tableViewWorkspaces->viewport()->update();
  ui->tableViewIssueTrackerConnections->viewport()->update();

  setCursor(Qt::PointingHandCursor);

  LOG_PERF(0, "Exit");
}

void HomeContainer::hoverLeave(const QModelIndex &index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Row: {}, Column: {}", index.row(), index.column());

  _itemDelegate->setHover(QModelIndex());

  ui->tableViewRecent->viewport()->update();
  ui->tableViewWorkspaces->viewport()->update();
  ui->tableViewIssueTrackerConnections->viewport()->update();

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
    const bool cancelled =
        authResult.value(QStringLiteral("cancelled")).toBool();
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
        QMessageBox::warning(this, tr("GitHub repositories"),
                             repositoriesErrorMessage.isEmpty()
                                 ? tr("No GitHub repositories were found for "
                                      "the authenticated account.")
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

    const QStringList selectedPath =
        selectedRepository.split(QStringLiteral("/"));
    if (selectedPath.size() < 2 || selectedPath.first().isEmpty() ||
        selectedPath.last().isEmpty()) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::critical(this, tr("Failure"),
                              tr("Failed to parse the selected repository."));
      });
      return;
    }

    const auto selectedRepo = QSharedPointer<IssueTrackers::GitHubRepo>::create(
        selectedPath.first(), selectedPath.last(), authRepo->host());
    selectedRepo->setAccessToken(authRepo->accessToken(),
                                 authRepo->accessTokenExpiresAt());
    IssueTrackers::TokensStorage::getInstance().storeToken(
        selectedRepo->serviceName(), selectedRepo->repositoryPath(),
        selectedRepo->accessToken(), selectedRepo->accessTokenExpiresAt(),
        true);

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _issueTrackersModel->reload();
      ui->tableViewIssueTrackerConnections->resizeColumnsToContents();
    });

    LOG_PERF(0, "Exit");
  });
}

void HomeContainer::onBitbucketOAuthConnectClicked() {
  LOG_PERF(0, "Enter");

  const QString clientId = _configManager.getString(
      ConfigManager::ConfigKey::BitbucketOAuthClientId);
  const QString clientSecret = _configManager.getString(
      ConfigManager::ConfigKey::BitbucketOAuthClientSecret);
  const QString authorizeUrl = _configManager.getString(
      ConfigManager::ConfigKey::BitbucketOAuthAuthorizeUrl);
  const QString tokenUrl = _configManager.getString(
      ConfigManager::ConfigKey::BitbucketOAuthTokenUrl);
  const QString redirectUri = BITBUCKET_CALLBACK_URL.toString(QUrl::FullyEncoded);
  const QString scope =
      _configManager.getString(ConfigManager::ConfigKey::BitbucketOAuthScope);

  if (clientId.trimmed().isEmpty()) {
    QMessageBox::critical(
        this, tr("Failure"),
        tr("Bitbucket OAuth client id is not configured."));
    LOG_PERF(0, "Exit");
    return;
  }

  QDialog oauthDialog(nullptr);
  oauthDialog.setWindowTitle(tr("Bitbucket OAuth authentication"));
  oauthDialog.setModal(true);
  oauthDialog.setMinimumWidth(560);

  auto *layout = new QVBoxLayout(&oauthDialog);
  layout->setSpacing(10);

  auto *introLabel = new QLabel(
      tr("1. Open the Bitbucket authorization page.\n"
         "2. Sign in and approve the app."),
      &oauthDialog);
  introLabel->setWordWrap(true);
  layout->addWidget(introLabel);

  auto *openAuthPageButton = new QPushButton(
      tr("Open Bitbucket authorization page"), &oauthDialog);
  openAuthPageButton->setCursor(Qt::PointingHandCursor);
  const QUrl authUrl = IssueTrackers::BitbucketRepo::authorizationUrl(
      authorizeUrl, clientId, redirectUri,
      scope.isEmpty() ? QStringLiteral("account repository") : scope);
  openAuthPageButton->setEnabled(authUrl.isValid());
  connect(openAuthPageButton, &QPushButton::clicked, this, [authUrl]() {
    if (authUrl.isValid()) {
      QDesktopServices::openUrl(authUrl);
    }
  });
  layout->addWidget(openAuthPageButton, 0, Qt::AlignLeft);


  auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel, &oauthDialog);
  connect(buttonBox, &QDialogButtonBox::rejected, &oauthDialog,
          &QDialog::reject);
  layout->addWidget(buttonBox);

  QTcpServer callbackServer(&oauthDialog);
  QString authorizationCode;
  QString callbackErrorMessage;
  bool callbackCaptured = false;

  connect(&callbackServer, &QTcpServer::newConnection, &oauthDialog,
          [&]() {
            while (callbackServer.hasPendingConnections()) {
              QTcpSocket *socket = callbackServer.nextPendingConnection();
              if (!socket) {
                continue;
              }

              socket->setParent(&oauthDialog);
              QObject::connect(socket, &QTcpSocket::disconnected, socket,
                               &QTcpSocket::deleteLater);
              QObject::connect(socket, &QTcpSocket::readyRead, &oauthDialog,
                               [&, socket]() {
                                 const QByteArray requestData = socket->readAll();
                                 const int requestLineEnd = requestData.indexOf('\n');
                                 const QByteArray requestLine =
                                     requestLineEnd >= 0
                                         ? requestData.left(requestLineEnd).trimmed()
                                         : requestData.trimmed();
                                 const QList<QByteArray> requestParts =
                                     requestLine.split(' ');
                                 if (requestParts.size() < 2) {
                                   callbackErrorMessage =
                                       tr("Invalid callback request.");
                                   writeHttpResponse(
                                       socket,
                                       bitbucketCallbackErrorPage(
                                           callbackErrorMessage),
                                       QStringLiteral("400 Bad Request"));
                                   callbackCaptured = true;
                                   oauthDialog.reject();
                                   return;
                                 }

                                 const QString requestTarget =
                                     QString::fromUtf8(requestParts.at(1));
                                 const QUrlQuery query(requestTarget.contains('?')
                                                           ? requestTarget
                                                                 .section('?', 1)
                                                           : QString());
                                 const QString error = query.queryItemValue(
                                     QStringLiteral("error"));
                                 if (!error.isEmpty()) {
                                   const QString errorDescription =
                                       query.queryItemValue(
                                           QStringLiteral("error_description"));
                                   callbackErrorMessage = errorDescription.isEmpty()
                                                              ? error
                                                              : error +
                                                                    QStringLiteral(": ") +
                                                                    errorDescription;
                                   writeHttpResponse(
                                       socket,
                                       bitbucketCallbackErrorPage(
                                           callbackErrorMessage),
                                       QStringLiteral("400 Bad Request"));
                                   callbackCaptured = true;
                                   oauthDialog.reject();
                                   return;
                                 }

                                 authorizationCode = query.queryItemValue(
                                     QStringLiteral("code"));
                                 if (authorizationCode.trimmed().isEmpty()) {
                                   callbackErrorMessage = tr(
                                       "Authorization code was not provided.");
                                   writeHttpResponse(
                                       socket,
                                       bitbucketCallbackErrorPage(
                                           callbackErrorMessage),
                                       QStringLiteral("400 Bad Request"));
                                   callbackCaptured = true;
                                   oauthDialog.reject();
                                   return;
                                 }

                                 writeHttpResponse(socket,
                                                   bitbucketCallbackSuccessPage());
                                 callbackCaptured = true;
                                 oauthDialog.accept();
                               });
            }
          });

  const QHostAddress listenAddress(QHostAddress::LocalHost);
  if (!callbackServer.listen(listenAddress, 8080)) {
    QMessageBox::critical(
        this, tr("Failure"),
        tr("Unable to start the Bitbucket callback server on %1: %2")
            .arg(BITBUCKET_CALLBACK_URL.toString(), callbackServer.errorString()));
    LOG_PERF(0, "Exit");
    return;
  }

  if (!authUrl.isValid()) {
    QMessageBox::critical(
        this, tr("Failure"),
        tr("Bitbucket authorization URL is invalid."));
    LOG_PERF(0, "Exit");
    return;
  }

  if (oauthDialog.exec() != QDialog::Accepted) {
    if (!callbackCaptured) {
      LOG_INFO("Bitbucket OAuth authentication cancelled by user");
      LOG_PERF(0, "Exit");
      return;
    }

    if (!callbackErrorMessage.isEmpty()) {
      QMessageBox::critical(this, tr("Failure"), callbackErrorMessage);
      LOG_PERF(0, "Exit");
      return;
    }
  }

  if (authorizationCode.isEmpty()) {
    QMessageBox::critical(this, tr("Failure"),
                          tr("Bitbucket authorization code is required."));
    LOG_PERF(0, "Exit");
    return;
  }

  const auto authRepo =
      QSharedPointer<IssueTrackers::BitbucketRepo>::create(QString(), QString());
  const QVariantMap authParameters = {
      {QStringLiteral("clientId"), clientId},
      {QStringLiteral("clientSecret"), clientSecret},
      {QStringLiteral("authorizationCode"), authorizationCode},
      {QStringLiteral("authorizeUrl"), authorizeUrl},
      {QStringLiteral("tokenUrl"), tokenUrl},
      {QStringLiteral("redirectUri"), redirectUri},
      {QStringLiteral("scope"),
       scope.isEmpty() ? QStringLiteral("account repository") : scope}};
  const QVariantMap authResult = authRepo->authenticate(authParameters);
  const bool success = authResult.value(QStringLiteral("success")).toBool();
  const QString errorMessage =
      authResult.value(QStringLiteral("errorMessage")).toString();
  if (!success) {
    QMessageBox::critical(this, tr("Failure"),
                          errorMessage.isEmpty()
                              ? tr("Failed to authenticate with Bitbucket")
                              : errorMessage);
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList repositories =
      authResult.value(QStringLiteral("repositories")).toStringList();
  if (repositories.isEmpty()) {
    QMessageBox::warning(this, tr("Bitbucket repositories"),
                         tr("No Bitbucket repositories were found for the "
                            "authenticated account."));
    LOG_PERF(0, "Exit");
    return;
  }

  QString selectedRepository;
  bool repositorySelectionCancelled = false;
  const auto showRepositorySelectionDialog =
      [=, &selectedRepository, &repositorySelectionCancelled]() {
        QDialog dialog(nullptr);
        dialog.setWindowTitle(tr("Choose Bitbucket repository"));
        dialog.setModal(true);
        dialog.setMinimumWidth(420);

        auto *selectionLayout = new QVBoxLayout(&dialog);

        auto *label = new QLabel(
            tr("Select the Bitbucket repository to connect:"), &dialog);
        label->setWordWrap(true);
        selectionLayout->addWidget(label);

        auto *comboBox = new QComboBox(&dialog);
        comboBox->addItems(repositories);
        comboBox->setCurrentIndex(0);
        selectionLayout->addWidget(comboBox);

        auto *selectionButtonBox = new QDialogButtonBox(
            QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
        connect(selectionButtonBox, &QDialogButtonBox::accepted, &dialog,
                &QDialog::accept);
        connect(selectionButtonBox, &QDialogButtonBox::rejected, &dialog,
                &QDialog::reject);
        selectionLayout->addWidget(selectionButtonBox);

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
    LOG_INFO("Bitbucket repository selection cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList selectedPath =
      selectedRepository.split(QStringLiteral("/"), Qt::SkipEmptyParts);
  if (selectedPath.size() < 2 || selectedPath.first().isEmpty() ||
      selectedPath.last().isEmpty()) {
    QMessageBox::critical(this, tr("Failure"),
                          tr("Failed to parse the selected repository."));
    LOG_PERF(0, "Exit");
    return;
  }

  const auto selectedRepo =
      QSharedPointer<IssueTrackers::BitbucketRepo>::create(
          selectedPath.first(), selectedPath.last(), authRepo->host());
  const QString currentUserName = authResult.value(QStringLiteral("username"))
                                    .toString()
                                    .trimmed();
  const QString storedUserName = !currentUserName.isEmpty()
                                     ? currentUserName
                                     : selectedPath.first();
  IssueTrackers::TokensStorage::getInstance().storeToken(
      selectedRepo->serviceName(), selectedRepo->repositoryPath(), storedUserName,
      authResult.value(QStringLiteral("accessToken")).toString(),
      authResult.value(QStringLiteral("expiresAt")).toDateTime(), true);

  _issueTrackersModel->reload();
  ui->tableViewIssueTrackerConnections->resizeColumnsToContents();

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

void HomeContainer::onIssueTrackerRightClicked(const QString &serviceType,
                                               const QString &name,
                                               const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction removeAction(tr("Remove"), this);
  connect(&removeAction, &QAction::triggered, this, [=]() {
    IssueTrackers::TokensStorage::getInstance().removeToken(serviceType, name);
    _issueTrackersModel->reload();
  });

  QMenu menu(this);
  menu.addAction(&removeAction);

  menu.exec(pos);

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
