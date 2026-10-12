#include "containers/HomeContainer.h"
#include "MousePositionCorrection.h"

#include "Constants.h"
#include "containers/ui_HomeContainer.h"
#include "delegates/HomeRecentItemDelegate.h"
#include "widgets/RecentTableView.h"
#include "widgets/WorkspacesTableView.h"

#include <AzureRepo.h>
#include <BitbucketRepo.h>
#include <FileManager.h>
#include <GitHubRepo.h>
#include <GitLabRepo.h>
#include <Log.h>
#include <StylesheetManager.h>
#include <TokensStorage.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QComboBox>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFormLayout>
#include <QHeaderView>
#include <QHostAddress>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPixmap>
#ifndef Q_OS_WIN
#include <QProcess>
#include <QProcessEnvironment>
#endif
#include <QPushButton>
#include <QRandomGenerator>
#include <QSize>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>
#include <QToolButton>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>

#include <QGuiApplication>
#include <QVariantMap>

#define THREAD_ID "HomeContainer"

namespace GuiManager {

using namespace Delegates;

namespace Containers {

namespace {

const QUrl OAUTH_CALLBACK_URL(QStringLiteral("http://127.0.0.1:8080/callback"));

QString htmlEncode(const QString &text) {
  return text.toHtmlEscaped();
}

QString oauthCallbackPage(const QString &serviceName, const QString &title,
                          const QString &heading, const QString &message,
                          const QString &detail = QString()) {
  const QString icon =
      title == QStringLiteral("Success")
          ? QStringLiteral(
                "<svg viewBox='0 0 120 120' aria-hidden='true'>"
                "<defs><linearGradient id='successGlow' x1='0%' y1='0%' "
                "x2='100%' y2='100%'>"
                "<stop offset='0%' stop-color='#8be9ff'/><stop offset='100%' "
                "stop-color='#1d4ed8'/></linearGradient></defs>"
                "<circle cx='60' cy='60' r='54' fill='url(#successGlow)' "
                "opacity='0.18'/>")
          : QStringLiteral(
                "<svg viewBox='0 0 120 120' aria-hidden='true'>"
                "<defs><linearGradient id='errorGlow' x1='0%' y1='0%' "
                "x2='100%' y2='100%'>"
                "<stop offset='0%' stop-color='#ffb4b4'/><stop offset='100%' "
                "stop-color='#ef4444'/></linearGradient></defs>"
                "<circle cx='60' cy='60' r='54' fill='url(#errorGlow)' "
                "opacity='0.18'/>");

  const QString innerGraphic =
      title == QStringLiteral("Success")
          ? QStringLiteral(
                "<circle cx='60' cy='60' r='34' fill='#0f766e'/>"
                "<path d='M45 60l10 10 22-26' fill='none' stroke='#ffffff' "
                "stroke-width='8' stroke-linecap='round' "
                "stroke-linejoin='round'/></svg>")
          : QStringLiteral(
                "<path d='M60 24l36 62H24z' fill='#dc2626'/>"
                "<path d='M60 46v18' fill='none' stroke='#ffffff' "
                "stroke-width='8' stroke-linecap='round'/>"
                "<circle cx='60' cy='72' r='4.5' fill='#ffffff'/></svg>");

  const QString statusBadge = QString();

  const QString detailHtml =
      detail.isEmpty() ? QString()
                       : QStringLiteral("<div class='detail'>%1</div>")
                             .arg(htmlEncode(detail));

  return QStringLiteral(R"HTML(<!doctype html>
<html lang='en'>
<head>
  <meta charset='utf-8'>
  <meta name='viewport' content='width=device-width, initial-scale=1'>
  <title>%1 OAuth %2</title>
  <style>
    :root {
      color-scheme: light;
      --bg1: #f7f9fc;
      --bg2: #eef3f9;
      --card: #ffffff;
      --border: rgba(15, 23, 42, 0.08);
      --text: #0f172a;
      --muted: #475569;
    }
    * { box-sizing: border-box; }
    body {
      margin: 0;
      min-height: 100vh;
      font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif;
      background:
        radial-gradient(circle at top, rgba(59, 130, 246, 0.10), transparent 36%),
        radial-gradient(circle at bottom right, rgba(16, 185, 129, 0.08), transparent 28%),
        linear-gradient(160deg, var(--bg1), var(--bg2));
      color: var(--text);
      display: grid;
      place-items: center;
      padding: 24px;
    }
    .card {
      width: min(640px, 100%);
      border: 1px solid var(--border);
      background: var(--card);
      backdrop-filter: blur(18px);
      border-radius: 28px;
      padding: 34px;
      box-shadow: 0 18px 50px rgba(15, 23, 42, 0.10);
    }
    .top {
      display: flex;
      gap: 20px;
      align-items: center;
      margin-bottom: 22px;
    }
    .icon {
      width: 120px;
      height: 120px;
      flex: 0 0 auto;
    }
    .icon svg { width: 100%; height: 100%; display: block; }
    .eyebrow {
      display: inline-flex;
      align-items: center;
      gap: 10px;
      margin-bottom: 8px;
      color: var(--muted);
      font-size: 13px;
      letter-spacing: .08em;
      text-transform: uppercase;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      border-radius: 999px;
      padding: 6px 12px;
      font-size: 12px;
      font-weight: 700;
      letter-spacing: .04em;
      text-transform: uppercase;
    }
    .badge.success {
      background: #0f766e;
      color: #ffffff;
      box-shadow: 0 8px 18px rgba(15, 118, 110, 0.24);
    }
    .badge.error {
      background: rgba(239, 68, 68, 0.16);
      color: #ffb3b3;
    }
    h1 {
      margin: 0 0 10px;
      font-size: 34px;
      line-height: 1.1;
    }
    p {
      margin: 0;
      line-height: 1.6;
      color: var(--muted);
      font-size: 16px;
    }
    .detail {
      margin-top: 18px;
      padding: 14px 16px;
      border-radius: 16px;
      background: #f8fafc;
      border: 1px solid rgba(15, 23, 42, 0.08);
      color: #b91c1c;
      word-break: break-word;
    }
    .footer {
      margin-top: 24px;
      padding-top: 18px;
      border-top: 1px solid rgba(15, 23, 42, 0.08);
      font-size: 14px;
      color: var(--muted);
    }
    @media (max-width: 540px) {
      .card { padding: 24px; border-radius: 22px; }
      .top { flex-direction: column; text-align: center; }
      h1 { font-size: 28px; }
    }
  </style>
</head>
<body>
  <main class='card'>
    <div class='top'>
      <div class='icon'>%4%5</div>
      <div>
        <div class='eyebrow'>%1</div>
        %6
        <h1>%3</h1>
        <p>%7</p>
        %8
      </div>
    </div>
    <div class='footer'>You can safely close this tab and return to Sourcerer.</div>
  </main>
</body>
</html>)HTML")
      .arg(htmlEncode(serviceName), htmlEncode(title), htmlEncode(heading),
           icon, innerGraphic, statusBadge, htmlEncode(message), detailHtml);
}

QString oauthCallbackSuccessPage(const QString &serviceName) {
  return oauthCallbackPage(
      serviceName, QStringLiteral("Success"),
      QStringLiteral("Signed in successfully"),
      QStringLiteral(
          "Your account has been authenticated and the app can continue."));
}

QString azurePkceCodeVerifier() {
  QByteArray verifier;
  verifier.reserve(64);

  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  const int alphabetSize = static_cast<int>(sizeof(alphabet) - 1);
  for (int i = 0; i < 96; ++i) {
    const int index = QRandomGenerator::global()->bounded(alphabetSize);
    verifier.append(alphabet[index]);
  }

  return QString::fromLatin1(verifier);
}

QString azurePkceCodeChallenge(const QString &codeVerifier) {
  const QByteArray verifierBytes = codeVerifier.toUtf8();
  const QByteArray hash =
      QCryptographicHash::hash(verifierBytes, QCryptographicHash::Sha256);
  return QString::fromLatin1(hash.toBase64(QByteArray::Base64UrlEncoding |
                                           QByteArray::OmitTrailingEquals));
}

QString oauthCallbackErrorPage(const QString &serviceName,
                               const QString &message) {
  return oauthCallbackPage(
      serviceName, QStringLiteral("Error"),
      QStringLiteral("We could not finish signing you in"),
      QStringLiteral(
          "The callback returned an error while completing authentication."),
      message);
}

QString bitbucketCallbackSuccessPage() {
  return oauthCallbackSuccessPage(QStringLiteral("Bitbucket"));
}

QString bitbucketCallbackErrorPage(const QString &message) {
  return oauthCallbackErrorPage(QStringLiteral("Bitbucket"), message);
}

QString azureCallbackSuccessPage() {
  return oauthCallbackSuccessPage(QStringLiteral("Azure"));
}

QString azureCallbackErrorPage(const QString &message) {
  return oauthCallbackErrorPage(QStringLiteral("Azure"), message);
}

void writeHttpResponse(QTcpSocket *socket, const QString &body,
                       const QString &statusLine = QStringLiteral("200 OK")) {
  const QByteArray bodyBytes = body.toUtf8();
  const QByteArray headers =
      QByteArray("HTTP/1.1 ") + statusLine.toUtf8() +
      QByteArray("\r\nContent-Type: text/html; charset=utf-8\r\nConnection: "
                 "close\r\nContent-Length: ") +
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
      _toastHost(ToastHostWidget::getInstance()), _dispatcher(1),
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

  const auto setupButtonIcon = [](QAbstractButton *button,
                                  const QString &path) {
    button->setProperty("iconSource", path);
    button->setIcon(
        QIcon(StylesheetManager::StylesheetManager::renderSvgToPixmap(
            path,
            StylesheetManager::StylesheetManager::menuBarButtonIconColor())));
    button->setIconSize(QSize(16, 16));
  };
  setupButtonIcon(ui->openButton, QStringLiteral(":/icons/repo-open.svg"));
  setupButtonIcon(ui->cloneButton, QStringLiteral(":/icons/repo-clone.svg"));
  setupButtonIcon(ui->initButton, QStringLiteral(":/icons/repo-init.svg"));

  auto *connectToMenu = new QMenu(ui->connectToButton);
  const auto addConnectToAction = [connectToMenu](const QString &text,
                                                  const QString &iconPath) {
    auto *action = connectToMenu->addAction(text);
    action->setProperty("iconSource", iconPath);
    action->setIcon(
        QIcon(StylesheetManager::StylesheetManager::renderSvgToPixmap(
            iconPath,
            StylesheetManager::StylesheetManager::menuBarButtonIconColor())));
    return action;
  };
  auto *githubIssueTrackerAction =
      addConnectToAction(tr("GitHub issue tracker"),
                         QStringLiteral(":/icons/issue-trackers/github.svg"));
  connect(githubIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onGitHubIssueTrackerConnectClicked);
  auto *gitlabIssueTrackerAction =
      addConnectToAction(tr("GitLab issue tracker"),
                         QStringLiteral(":/icons/issue-trackers/gitlab.svg"));
  connect(gitlabIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onGitLabIssueTrackerConnectClicked);
  auto *azureIssueTrackerAction =
      addConnectToAction(tr("Azure issue tracker"),
                         QStringLiteral(":/icons/issue-trackers/azure.svg"));
  connect(azureIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onAzureIssueTrackerConnectClicked);
  auto *bitbucketIssueTrackerAction = addConnectToAction(
      tr("Bitbucket issue tracker"),
      QStringLiteral(":/icons/issue-trackers/bitbucket.svg"));
  connect(bitbucketIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onBitbucketOAuthConnectClicked);
  auto *customIssueTrackerAction =
      addConnectToAction(tr("Custom URL issue tracker"),
                         QStringLiteral(":/icons/issue-trackers/custom.svg"));
  connect(customIssueTrackerAction, &QAction::triggered, this,
          &HomeContainer::onCustomIssueTrackerConnectClicked);

  ui->connectToButton->setMenu(connectToMenu);

  auto *cloneMenu = new QMenu(ui->cloneButton);
  auto *cloneGitAction = cloneMenu->addAction(tr("Clone Git"));
  connect(cloneGitAction, &QAction::triggered, this,
          &HomeContainer::onCloneClicked);
  auto *cloneSubversionAction = cloneMenu->addAction(tr("Checkout Subversion"));
  connect(cloneSubversionAction, &QAction::triggered, this,
          &HomeContainer::onCloneSubversionClicked);

  ui->cloneButton->setMenu(cloneMenu);

  auto *initMenu = new QMenu(ui->initButton);
  auto *initGitAction = initMenu->addAction(tr("Git Init"));
  connect(initGitAction, &QAction::triggered, this,
          &HomeContainer::onInitClicked);
  auto *initSubversionAction = initMenu->addAction(tr("Subversion Init"));
  connect(initSubversionAction, &QAction::triggered, this,
          &HomeContainer::onInitSubversionClicked);

  ui->initButton->setMenu(initMenu);

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

void HomeContainer::deselected() {
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

void HomeContainer::onCloneSubversionClicked() {
  LOG_PERF(0, "Enter");

  emit cloneSubversion();

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

void HomeContainer::onGitLabIssueTrackerConnectClicked() {
  LOG_PERF(0, "Enter");

  bool serviceUrlAccepted = false;
  const QString serviceUrl =
      QInputDialog::getText(this, tr("GitLab service URL"),
                            tr("Enter the GitLab service URL (for example, "
                               "https://gitlab.com or\n"
                               "https://gitlab.example.com):"),
                            QLineEdit::Normal,
                            QStringLiteral("https://gitlab.com"),
                            &serviceUrlAccepted)
          .trimmed();
  if (!serviceUrlAccepted || serviceUrl.isEmpty()) {
    LOG_INFO(
        "GitLab issue tracker connection cancelled while entering service URL");
    LOG_PERF(0, "Exit");
    return;
  }

  QString serviceBaseUrl = serviceUrl;
  while (serviceBaseUrl.endsWith('/')) {
    serviceBaseUrl.chop(1);
  }
  if (!serviceBaseUrl.startsWith(QStringLiteral("http://")) &&
      !serviceBaseUrl.startsWith(QStringLiteral("https://"))) {
    serviceBaseUrl.prepend(QStringLiteral("https://"));
  }

  bool tokenAccepted = false;
  const QString personalAccessToken =
      QInputDialog::getText(
          this, tr("GitLab personal access token"),
          tr("Enter the GitLab personal access token. It needs these scopes:\n"
             "api, read_api, ai_features"),
          QLineEdit::Password, QString(), &tokenAccepted)
          .trimmed();
  if (!tokenAccepted || personalAccessToken.isEmpty()) {
    LOG_INFO("GitLab issue tracker connection cancelled while entering token");
    LOG_PERF(0, "Exit");
    return;
  }

  const auto authRepo = QSharedPointer<IssueTrackers::GitLabRepo>::create(
      QString(), QString(), serviceBaseUrl);
  const WeakPtr guard = getSelf().toWeakRef();
  const QVariantMap authParameters = {
      {QStringLiteral("accessToken"), personalAccessToken}};

  _dispatcher.dispatch([=]() {
    const QVariantMap authResult = authRepo->authenticate(authParameters);
    const bool success = authResult.value(QStringLiteral("success")).toBool();
    const QString errorMessage =
        authResult.value(QStringLiteral("errorMessage")).toString();

    if (!success) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::critical(this, tr("Failure"),
                              errorMessage.isEmpty()
                                  ? tr("Failed to authenticate with GitLab")
                                  : errorMessage);
      });
      LOG_ERROR(Log::ErrorId::Security, "{}", errorMessage.toStdString());
      return;
    }

    const QStringList repositories =
        authResult.value(QStringLiteral("repositories")).toStringList();
    if (repositories.isEmpty()) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::warning(this, tr("GitLab repositories"),
                             tr("No GitLab repositories were found for the "
                                "authenticated account."));
      });
      return;
    }

    QString selectedRepository;
    bool repositorySelectionCancelled = false;
    const auto showRepositorySelectionDialog =
        [=, &selectedRepository, &repositorySelectionCancelled]() {
          QDialog dialog(nullptr);
          dialog.setWindowTitle(tr("Choose GitLab repository"));
          dialog.setModal(true);
          dialog.setMinimumWidth(420);

          auto *layout = new QVBoxLayout(&dialog);

          auto *label = new QLabel(
              tr("Select the GitLab repository to connect:"), &dialog);
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
      LOG_INFO("GitLab repository selection cancelled by user");
      return;
    }

    const int pathSeparatorIndex = selectedRepository.lastIndexOf('/');
    if (pathSeparatorIndex <= 0 ||
        pathSeparatorIndex >= selectedRepository.size() - 1) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::critical(this, tr("Failure"),
                              tr("Failed to parse the selected repository."));
      });
      return;
    }

    const QString owner = selectedRepository.left(pathSeparatorIndex);
    const QString repository = selectedRepository.mid(pathSeparatorIndex + 1);
    const auto selectedRepo = QSharedPointer<IssueTrackers::GitLabRepo>::create(
        owner, repository, authRepo->host());

    IssueTrackers::TokensStorage::getInstance().storeTokenWithHost(
        selectedRepo->serviceName(), selectedRepo->repositoryPath(),
        selectedRepo->host(),
        authResult.value(QStringLiteral("accessToken")).toString(),
        authResult.value(QStringLiteral("expiresAt")).toDateTime(), true);

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _issueTrackersModel->reload();
      ui->tableViewIssueTrackerConnections->resizeColumnsToContents();
    });

    LOG_PERF(0, "Exit");
  });
}

void HomeContainer::onCustomIssueTrackerConnectClicked() {
  LOG_PERF(0, "Enter");

  QDialog dialog(this);
  dialog.setWindowTitle(tr("Custom URL issue tracker"));
  dialog.setModal(true);
  dialog.setMinimumWidth(420);

  auto *layout = new QVBoxLayout(&dialog);
  auto *formLayout = new QFormLayout();

  auto *urlEdit = new QLineEdit(&dialog);
  urlEdit->setPlaceholderText(tr("https://issues.example.com/project"));
  formLayout->addRow(tr("URL:"), urlEdit);

  auto *nameEdit = new QLineEdit(&dialog);
  nameEdit->setPlaceholderText(tr("Defaults to the URL host"));
  formLayout->addRow(tr("Name:"), nameEdit);

  layout->addLayout(formLayout);

  auto *buttonBox = new QDialogButtonBox(
      QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
  connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
  connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
  layout->addWidget(buttonBox);

  QUrl url;
  QString name;
  while (true) {
    if (dialog.exec() != QDialog::Accepted) {
      LOG_INFO("Custom URL issue tracker connection cancelled");
      LOG_PERF(0, "Exit");
      return;
    }

    QString urlText = urlEdit->text().trimmed();
    if (urlText.isEmpty()) {
      QMessageBox::warning(this, tr("Custom URL issue tracker"),
                           tr("Please enter the issue tracker URL."));
      continue;
    }
    if (!urlText.contains(QStringLiteral("://"))) {
      urlText.prepend(QStringLiteral("https://"));
    }

    url = QUrl(urlText, QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (!url.isValid() || url.host().isEmpty() ||
        (scheme != QStringLiteral("http") &&
         scheme != QStringLiteral("https"))) {
      QMessageBox::warning(
          this, tr("Custom URL issue tracker"),
          tr("The URL is invalid. Only http and https URLs are supported."));
      continue;
    }

    name = nameEdit->text().trimmed();
    if (name.isEmpty()) {
      name = url.host();
    }

    if (IssueTrackers::TokensStorage::getInstance().hasToken(
            QStringLiteral("Custom"), name)) {
      QMessageBox::warning(
          this, tr("Custom URL issue tracker"),
          tr("A custom issue tracker named \"%1\" already exists. Please "
             "choose another name.")
              .arg(name));
      continue;
    }

    break;
  }

  IssueTrackers::TokensStorage::getInstance().storeTokenWithHost(
      QStringLiteral("Custom"), name, url.toString(), QString(), QDateTime(),
      true);

  _issueTrackersModel->reload();
  ui->tableViewIssueTrackerConnections->resizeColumnsToContents();

  LOG_PERF(0, "Exit");
}

void HomeContainer::onAzureIssueTrackerConnectClicked() {
  LOG_PERF(0, "Enter");

  const QString clientId =
      _configManager.getString(ConfigManager::ConfigKey::AzureOAuthClientId);

  const QString authorizeUrl = _configManager.getString(
      ConfigManager::ConfigKey::AzureOAuthAuthorizeUrl);
  const QString tokenUrl =
      _configManager.getString(ConfigManager::ConfigKey::AzureOAuthTokenUrl);
  const QString clientSecret = _configManager.getString(
      ConfigManager::ConfigKey::AzureOAuthClientSecret);
  const QString tenantId =
      _configManager.getString(ConfigManager::ConfigKey::AzureOAuthTenantId);
  const QString redirectUri =
      _configManager.getString(ConfigManager::ConfigKey::AzureOAuthRedirectUrl);
  const QString scope =
      _configManager.getString(ConfigManager::ConfigKey::AzureOAuthScope);

  if (clientId.trimmed().isEmpty()) {
    QMessageBox::critical(this, tr("Failure"),
                          tr("Azure OAuth client id is not configured."));
    LOG_PERF(0, "Exit");
    return;
  }

  const QUrl redirectUriUrl(redirectUri);
  if (!redirectUriUrl.isValid()) {
    QMessageBox::critical(this, tr("Failure"),
                          tr("Azure OAuth redirect URL is invalid."));
    LOG_PERF(0, "Exit");
    return;
  }

  if (!redirectUriUrl.host().isEmpty() &&
      redirectUriUrl.host() != QStringLiteral("127.0.0.1") &&
      redirectUriUrl.host() != QStringLiteral("localhost")) {
    QMessageBox::critical(this, tr("Failure"),
                          tr("Azure OAuth redirect URL must use localhost for "
                             "the local callback server."));
    LOG_PERF(0, "Exit");
    return;
  }

  const QString callbackPath = redirectUriUrl.path().isEmpty()
                                   ? QStringLiteral("/callback")
                                   : redirectUriUrl.path();
  const QString oauthState = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QString pkceCodeVerifier = azurePkceCodeVerifier();
  const QString pkceCodeChallenge = azurePkceCodeChallenge(pkceCodeVerifier);

  QDialog oauthDialog(nullptr);
  oauthDialog.setWindowTitle(tr("Azure OAuth authentication"));
  oauthDialog.setModal(true);
  oauthDialog.setMinimumWidth(560);

  auto *layout = new QVBoxLayout(&oauthDialog);
  layout->setSpacing(10);

  auto *introLabel = new QLabel(
      tr("1. Open the Azure authorization page and approve the app.\n"
         "2. Enter your organization and choose a project."),
      &oauthDialog);
  introLabel->setWordWrap(true);
  layout->addWidget(introLabel);

  const QString defaultAzureScope = scope;

  const QUrl authUrl = IssueTrackers::AzureRepo::authorizationUrl(
      authorizeUrl, tenantId, clientId, redirectUri, defaultAzureScope,
      oauthState, pkceCodeChallenge);

  auto *openAuthPageButton =
      new QPushButton(tr("Open Azure authorization page"), &oauthDialog);
  openAuthPageButton->setCursor(Qt::PointingHandCursor);
  openAuthPageButton->setEnabled(authUrl.isValid());
  connect(openAuthPageButton, &QPushButton::clicked, this, [authUrl]() {
    if (authUrl.isValid()) {
      FileManager::openExternalUrl(authUrl);
    } else {
      QMessageBox::critical(nullptr, tr("Failure"),
                            tr("Azure authorization URL is invalid."));
    }
  });
  layout->addWidget(openAuthPageButton, 0, Qt::AlignLeft);

  auto *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Cancel, &oauthDialog);
  connect(buttonBox, &QDialogButtonBox::rejected, &oauthDialog,
          &QDialog::reject);
  layout->addWidget(buttonBox);

  QTcpServer callbackServer(&oauthDialog);
  QString authorizationCode;
  QString callbackErrorMessage;
  bool callbackCaptured = false;

  connect(&callbackServer, &QTcpServer::newConnection, &oauthDialog, [&]() {
    while (callbackServer.hasPendingConnections()) {
      QTcpSocket *socket = callbackServer.nextPendingConnection();
      if (!socket) {
        continue;
      }

      socket->setParent(&oauthDialog);
      QObject::connect(socket, &QTcpSocket::disconnected, socket,
                       &QTcpSocket::deleteLater);
      QObject::connect(
          socket, &QTcpSocket::readyRead, &oauthDialog, [&, socket]() {
            const QByteArray requestData = socket->readAll();
            const int requestLineEnd = requestData.indexOf('\n');
            const QByteArray requestLine =
                requestLineEnd >= 0 ? requestData.left(requestLineEnd).trimmed()
                                    : requestData.trimmed();
            const QList<QByteArray> requestParts = requestLine.split(' ');
            if (requestParts.size() < 2) {
              callbackErrorMessage = tr("Invalid callback request.");
              writeHttpResponse(socket,
                                azureCallbackErrorPage(callbackErrorMessage),
                                QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            const QString requestTarget = QString::fromUtf8(requestParts.at(1));
            const QString requestPath = requestTarget.section('?', 0, 0);
            if (!callbackPath.isEmpty() && requestPath != callbackPath) {
              callbackErrorMessage = tr("Unexpected callback path.");
              writeHttpResponse(socket,
                                azureCallbackErrorPage(callbackErrorMessage),
                                QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            const QUrlQuery query(requestTarget.contains('?')
                                      ? requestTarget.section('?', 1)
                                      : QString());
            const QString error = query.queryItemValue(QStringLiteral("error"));
            if (!error.isEmpty()) {
              const QString errorDescription =
                  query.queryItemValue(QStringLiteral("error_description"));
              callbackErrorMessage =
                  errorDescription.isEmpty()
                      ? error
                      : error + QStringLiteral(": ") + errorDescription;
              writeHttpResponse(socket,
                                azureCallbackErrorPage(callbackErrorMessage),
                                QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            const QString callbackState =
                query.queryItemValue(QStringLiteral("state"));
            if (!oauthState.isEmpty() && callbackState != oauthState) {
              callbackErrorMessage = tr("Invalid callback state.");
              writeHttpResponse(socket,
                                azureCallbackErrorPage(callbackErrorMessage),
                                QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            authorizationCode = query.queryItemValue(QStringLiteral("code"));
            if (authorizationCode.trimmed().isEmpty()) {
              authorizationCode =
                  query.queryItemValue(QStringLiteral("assertion"));
            }
            if (authorizationCode.trimmed().isEmpty()) {
              callbackErrorMessage = tr("Authorization code was not provided.");
              writeHttpResponse(socket,
                                azureCallbackErrorPage(callbackErrorMessage),
                                QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            writeHttpResponse(socket, azureCallbackSuccessPage());
            callbackCaptured = true;
            oauthDialog.accept();
          });
    }
  });

  const QHostAddress listenAddress(QHostAddress::LocalHost);
  const quint16 listenPort = static_cast<quint16>(redirectUriUrl.port(8080));
  if (!callbackServer.listen(listenAddress, listenPort)) {
    QMessageBox::critical(
        this, tr("Failure"),
        tr("Unable to start the Azure callback server on %1: %2")
            .arg(redirectUri, callbackServer.errorString()));
    LOG_PERF(0, "Exit");
    return;
  }

  if (oauthDialog.exec() != QDialog::Accepted) {
    if (!callbackCaptured) {
      LOG_INFO("Azure OAuth authentication cancelled by user");
      LOG_PERF(0, "Exit");
      return;
    }

    if (!callbackErrorMessage.isEmpty()) {
      LOG_PERF(0, "Exit");
      return;
    }
  }

  if (authorizationCode.isEmpty()) {
    QMessageBox::critical(this, tr("Failure"),
                          tr("Azure authorization code is required."));
    LOG_PERF(0, "Exit");
    return;
  }

  const auto authRepo =
      QSharedPointer<IssueTrackers::AzureRepo>::create(QString(), QString());
  const WeakPtr guard = getSelf().toWeakRef();
  const QVariantMap authParameters = {
      {QStringLiteral("clientId"), clientId},
      {QStringLiteral("authorizationCode"), authorizationCode},
      {QStringLiteral("authorizeUrl"), authorizeUrl},
      {QStringLiteral("tokenUrl"), tokenUrl},
      {QStringLiteral("clientSecret"), clientSecret},
      {QStringLiteral("tenantId"), tenantId},
      {QStringLiteral("redirectUri"), redirectUri},
      {QStringLiteral("resource"), defaultAzureScope},
      {QStringLiteral("state"), oauthState},
      {QStringLiteral("codeVerifier"), pkceCodeVerifier}};

  _dispatcher.dispatch([=]() {
    const QVariantMap authResult = authRepo->authenticate(authParameters);
    const bool success = authResult.value(QStringLiteral("success")).toBool();
    if (!success) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        QMessageBox::critical(this, tr("Failure"),
                              tr("Failed to authenticate with Azure"));
      });
      const QString errorMessage =
          authResult.value(QStringLiteral("errorMessage")).toString();
      LOG_ERROR(Log::Security, errorMessage);
      return;
    }

    const QString accessToken =
        authResult.value(QStringLiteral("accessToken")).toString();
    const QDateTime expiresAt =
        authResult.value(QStringLiteral("expiresAt")).toDateTime();
    QVariantMap organizations =
        authResult.value(QStringLiteral("organizations")).toMap();
    QString tenantDiscoveryErrorMessage;

    if (!tenantDiscoveryErrorMessage.isEmpty()) {
      LOG_WARN("Azure tenant discovery failed: {}",
               tenantDiscoveryErrorMessage.toStdString());
    }

    Util::Dispatcher::dispatchToMainThread(guard, [=]() mutable {
      if (organizations.isEmpty()) {
        QMessageBox::warning(this, tr("Azure organizations"),
                             tr("No Azure organizations were discovered for "
                                "the authenticated account."));
        LOG_PERF(0, "Exit");
        return;
      }

      QDialog organizationDialog(nullptr);
      organizationDialog.setWindowTitle(tr("Choose Azure organization"));
      organizationDialog.setModal(true);
      organizationDialog.setMinimumWidth(420);

      auto *layout = new QVBoxLayout(&organizationDialog);
      auto *label = new QLabel(tr("Select the Azure organization to connect:"),
                               &organizationDialog);
      label->setWordWrap(true);
      layout->addWidget(label);

      auto *organizationCombo = new QComboBox(&organizationDialog);
      organizationCombo->addItems(organizations.keys());
      organizationCombo->setCurrentIndex(0);
      layout->addWidget(organizationCombo);

      auto *buttonBox = new QDialogButtonBox(
          QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &organizationDialog);
      connect(buttonBox, &QDialogButtonBox::accepted, &organizationDialog,
              &QDialog::accept);
      connect(buttonBox, &QDialogButtonBox::rejected, &organizationDialog,
              &QDialog::reject);
      layout->addWidget(buttonBox);

      if (organizationDialog.exec() != QDialog::Accepted) {
        LOG_INFO("Azure organization selection cancelled by user");
        LOG_PERF(0, "Exit");
        return;
      }

      const QString organization = organizationCombo->currentText().trimmed();
      if (organization.isEmpty()) {
        QMessageBox::critical(this, tr("Failure"),
                              tr("Azure organization is required."));
        LOG_PERF(0, "Exit");
        return;
      }

      const WeakPtr projectsGuard = guard;
      _dispatcher.dispatch([=]() {
        const QVariantMap projectsResult =
            authRepo->listProjects(organization, accessToken);
        const bool projectsSuccess =
            projectsResult.value(QStringLiteral("success")).toBool();

        if (!projectsSuccess) {
          Util::Dispatcher::dispatchToMainThread(projectsGuard, [=]() {
            QMessageBox::critical(
                this, tr("Failure"),
                tr("Failed to list Azure projects. Ensure that the user has "
                   "the required permissions."));
          });
          const QString projectsErrorMessage =
              projectsResult.value(QStringLiteral("errorMessage")).toString();
          LOG_ERROR(Log::Generic, projectsErrorMessage);
          return;
        }

        const QStringList repositories =
            projectsResult.value(QStringLiteral("repositories")).toStringList();
        if (repositories.isEmpty()) {
          Util::Dispatcher::dispatchToMainThread(projectsGuard, [=]() {
            QMessageBox::warning(this, tr("Azure projects"),
                                 tr("No Azure projects were found for the "
                                    "selected organization."));
          });
          return;
        }

        QStringList projectNames;
        for (const QString &repository : repositories) {
          const QStringList selectedPath =
              repository.split(QStringLiteral("/"), Qt::SkipEmptyParts);
          if (selectedPath.size() < 2) {
            continue;
          }
          const QString projectName = selectedPath.last();
          if (!projectNames.contains(projectName)) {
            projectNames.append(projectName);
          }
        }
        projectNames.sort(Qt::CaseInsensitive);

        QString selectedProject;
        bool projectSelectionCancelled = false;
        const auto showProjectSelectionDialog = [=, &selectedProject,
                                                 &projectSelectionCancelled]() {
          QDialog dialog(nullptr);
          dialog.setWindowTitle(tr("Choose Azure project"));
          dialog.setModal(true);
          dialog.setMinimumWidth(420);

          auto *layout = new QVBoxLayout(&dialog);

          auto *label =
              new QLabel(tr("Select the Azure project to connect:"), &dialog);
          label->setWordWrap(true);
          layout->addWidget(label);

          auto *projectCombo = new QComboBox(&dialog);
          projectCombo->addItems(projectNames);
          projectCombo->setCurrentIndex(0);
          layout->addWidget(projectCombo);

          auto *buttonBox = new QDialogButtonBox(
              QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
          connect(buttonBox, &QDialogButtonBox::accepted, &dialog,
                  &QDialog::accept);
          connect(buttonBox, &QDialogButtonBox::rejected, &dialog,
                  &QDialog::reject);
          layout->addWidget(buttonBox);

          if (dialog.exec() == QDialog::Accepted) {
            selectedProject = projectCombo->currentText();
          } else {
            projectSelectionCancelled = true;
          }
        };

        if (QThread::currentThread() == qApp->thread()) {
          showProjectSelectionDialog();
        } else {
          QMetaObject::invokeMethod(qApp, showProjectSelectionDialog,
                                    Qt::BlockingQueuedConnection);
        }

        if (projectSelectionCancelled || selectedProject.isEmpty()) {
          LOG_INFO("Azure project selection cancelled by user");
          LOG_PERF(0, "Exit");
          return;
        }

        const auto selectedRepo =
            QSharedPointer<IssueTrackers::AzureRepo>::create(
                organization, selectedProject, authRepo->host());
        IssueTrackers::TokensStorage::getInstance().storeToken(
            selectedRepo->serviceName(), selectedRepo->repositoryPath(),
            organization, accessToken, expiresAt, true);

        Util::Dispatcher::dispatchToMainThread(projectsGuard, [=]() {
          _issueTrackersModel->reload();
          ui->tableViewIssueTrackerConnections->resizeColumnsToContents();
        });

        LOG_PERF(0, "Exit");
      });
    });
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
  const QString redirectUri = OAUTH_CALLBACK_URL.toString(QUrl::FullyEncoded);
  const QString scope =
      _configManager.getString(ConfigManager::ConfigKey::BitbucketOAuthScope);

  if (clientId.trimmed().isEmpty()) {
    QMessageBox::critical(this, tr("Failure"),
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

  auto *introLabel = new QLabel(tr("1. Open the Bitbucket authorization page.\n"
                                   "2. Sign in and approve the app."),
                                &oauthDialog);
  introLabel->setWordWrap(true);
  layout->addWidget(introLabel);

  auto *openAuthPageButton =
      new QPushButton(tr("Open Bitbucket authorization page"), &oauthDialog);
  openAuthPageButton->setCursor(Qt::PointingHandCursor);
  const QUrl authUrl = IssueTrackers::BitbucketRepo::authorizationUrl(
      authorizeUrl, clientId, redirectUri,
      scope.isEmpty() ? QStringLiteral("account repository") : scope);
  openAuthPageButton->setEnabled(authUrl.isValid());
  connect(openAuthPageButton, &QPushButton::clicked, this, [authUrl]() {
    if (authUrl.isValid()) {
      FileManager::openExternalUrl(authUrl);
    }
  });
  layout->addWidget(openAuthPageButton, 0, Qt::AlignLeft);

  auto *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Cancel, &oauthDialog);
  connect(buttonBox, &QDialogButtonBox::rejected, &oauthDialog,
          &QDialog::reject);
  layout->addWidget(buttonBox);

  QTcpServer callbackServer(&oauthDialog);
  QString authorizationCode;
  QString callbackErrorMessage;
  bool callbackCaptured = false;

  connect(&callbackServer, &QTcpServer::newConnection, &oauthDialog, [&]() {
    while (callbackServer.hasPendingConnections()) {
      QTcpSocket *socket = callbackServer.nextPendingConnection();
      if (!socket) {
        continue;
      }

      socket->setParent(&oauthDialog);
      QObject::connect(socket, &QTcpSocket::disconnected, socket,
                       &QTcpSocket::deleteLater);
      QObject::connect(
          socket, &QTcpSocket::readyRead, &oauthDialog, [&, socket]() {
            const QByteArray requestData = socket->readAll();
            const int requestLineEnd = requestData.indexOf('\n');
            const QByteArray requestLine =
                requestLineEnd >= 0 ? requestData.left(requestLineEnd).trimmed()
                                    : requestData.trimmed();
            const QList<QByteArray> requestParts = requestLine.split(' ');
            if (requestParts.size() < 2) {
              callbackErrorMessage = tr("Invalid callback request.");
              writeHttpResponse(
                  socket, bitbucketCallbackErrorPage(callbackErrorMessage),
                  QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            const QString requestTarget = QString::fromUtf8(requestParts.at(1));
            const QUrlQuery query(requestTarget.contains('?')
                                      ? requestTarget.section('?', 1)
                                      : QString());
            const QString error = query.queryItemValue(QStringLiteral("error"));
            if (!error.isEmpty()) {
              const QString errorDescription =
                  query.queryItemValue(QStringLiteral("error_description"));
              callbackErrorMessage =
                  errorDescription.isEmpty()
                      ? error
                      : error + QStringLiteral(": ") + errorDescription;
              writeHttpResponse(
                  socket, bitbucketCallbackErrorPage(callbackErrorMessage),
                  QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            authorizationCode = query.queryItemValue(QStringLiteral("code"));
            if (authorizationCode.trimmed().isEmpty()) {
              callbackErrorMessage = tr("Authorization code was not provided.");
              writeHttpResponse(
                  socket, bitbucketCallbackErrorPage(callbackErrorMessage),
                  QStringLiteral("400 Bad Request"));
              callbackCaptured = true;
              oauthDialog.reject();
              return;
            }

            writeHttpResponse(socket, bitbucketCallbackSuccessPage());
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
            .arg(OAUTH_CALLBACK_URL.toString(), callbackServer.errorString()));
    LOG_PERF(0, "Exit");
    return;
  }

  if (!authUrl.isValid()) {
    QMessageBox::critical(this, tr("Failure"),
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

  const auto authRepo = QSharedPointer<IssueTrackers::BitbucketRepo>::create(
      QString(), QString());
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
  const auto showRepositorySelectionDialog = [=, &selectedRepository,
                                              &repositorySelectionCancelled]() {
    QDialog dialog(nullptr);
    dialog.setWindowTitle(tr("Choose Bitbucket repository"));
    dialog.setModal(true);
    dialog.setMinimumWidth(420);

    auto *selectionLayout = new QVBoxLayout(&dialog);

    auto *label =
        new QLabel(tr("Select the Bitbucket repository to connect:"), &dialog);
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
  const QString currentUserName =
      authResult.value(QStringLiteral("username")).toString().trimmed();
  const QString storedUserName =
      !currentUserName.isEmpty() ? currentUserName : selectedPath.first();
  IssueTrackers::TokensStorage::getInstance().storeToken(
      selectedRepo->serviceName(), selectedRepo->repositoryPath(),
      storedUserName,
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

void HomeContainer::onInitSubversionClicked() {
  LOG_PERF(0, "Enter");

  emit initSubversion();

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
