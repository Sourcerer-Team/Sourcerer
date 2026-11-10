// MIT License
// Copyright (c) 2026 Sourcerer

#include "containers/IssueTrackerContainer.h"
#include "WebEngineProfile.h"

#include "src/containers/ui_IssueTrackerContainer.h"
#include "ui_IssueTrackerContainer.h"

#include <AzureRepo.h>
#include <BitbucketRepo.h>
#include <FileManager.h>
#include <GitHubRepo.h>
#include <GitLabRepo.h>
#include <Log.h>
#include <StylesheetManager.h>
#include <TokensStorage.h>

#include <QApplication>
#include <QAuthenticator>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QHash>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QMouseEvent>
#include <QNetworkCookie>
#ifndef Q_OS_WIN
#include <QProcess>
#include <QProcessEnvironment>
#endif
#include <QResizeEvent>
#include <QStringList>
#include <QTimer>
#include <QToolButton>
#include <QUrl>

#include <memory>

#if GUI_MANAGER_HAS_WEBENGINE
#include <QtWebEngineCore/QWebEngineCookieStore>
#include <QtWebEngineCore/QWebEngineHistory>
#include <QtWebEngineCore/QWebEngineLoadingInfo>
#include <QtWebEngineCore/QWebEngineNewWindowRequest>
#include <QtWebEngineCore/QWebEnginePage>
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineCore/QWebEngineScript>
#include <QtWebEngineCore/QWebEngineScriptCollection>
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
#include <QtWebEngineCore/QWebEngineWebAuthUxRequest>
#endif
#endif

#define THREAD_ID "IssueTrackerContainer"

namespace GuiManager::Containers {

namespace {

#if GUI_MANAGER_HAS_WEBENGINE
QString issueTrackerProfileId(const QString &serviceType,
                              const QString &repositoryName,
                              const QString &serviceHost) {
  const QString identity =
      QStringLiteral("%1|%2|%3")
          .arg(serviceType.trimmed().toLower(), repositoryName.trimmed(),
               serviceHost.trimmed().toLower());
  return QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256)
      .toHex();
}

QWebEngineProfile *
createIssueTrackerWebEngineProfile(const QString &serviceType,
                                   const QString &repositoryName,
                                   const QString &serviceHost) {
  static QHash<QString, QWebEngineProfile *> profiles;

  const QString profileId =
      issueTrackerProfileId(serviceType, repositoryName, serviceHost);
  if (QWebEngineProfile *profile = profiles.value(profileId, nullptr)) {
    return profile;
  }

  const QString storageBaseDir =
      webEngineStoragePath(QStringLiteral("issue-trackers/webengine"));
  if (storageBaseDir.isEmpty()) {
    return nullptr;
  }

  const QString storagePath = QDir(storageBaseDir).absoluteFilePath(profileId);
  QDir().mkpath(storagePath);

  // The profile is cached for the lifetime of the application and shared by
  // every container of the same issue tracker, so it must not be owned by the
  // container that happens to create it first.
  auto *profile = new QWebEngineProfile(
      QStringLiteral("issue-tracker-%1").arg(profileId.left(16)), qApp);
  profile->setPersistentStoragePath(storagePath);
  profile->setCachePath(
      QDir(storagePath).absoluteFilePath(QStringLiteral("cache")));
  profile->setPersistentCookiesPolicy(
      QWebEngineProfile::PersistentCookiesPolicy::ForcePersistentCookies);
  profile->setHttpCacheType(QWebEngineProfile::HttpCacheType::DiskHttpCache);
  profiles.insert(profileId, profile);
  return profile;
}
#endif

QString normalizedServiceType(const QString &serviceType) {
  return serviceType.trimmed().toLower();
}

#if GUI_MANAGER_HAS_WEBENGINE
bool isGitHubLoginCookie(const QNetworkCookie &cookie) {
  const QString domain = cookie.domain().trimmed().toLower();
  if (!domain.isEmpty() && !domain.endsWith(QStringLiteral("github.com"))) {
    return false;
  }

  const QString name = QString::fromUtf8(cookie.name()).trimmed().toLower();
  if (name == QStringLiteral("logged_in")) {
    const QString value = QString::fromUtf8(cookie.value()).trimmed().toLower();
    return !value.isEmpty() && value != QStringLiteral("false") &&
           value != QStringLiteral("no") && value != QStringLiteral("0");
  }

  return name == QStringLiteral("user_session") ||
         name == QStringLiteral("__host-user_session_same_site") ||
         name == QStringLiteral("dotcom_user");
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
void handleWebAuthUxRequest(QWebEngineWebAuthUxRequest *request,
                            QWidget *parent) {
  if (!request) {
    return;
  }

  using WebAuthState = QWebEngineWebAuthUxRequest::WebAuthUxState;
  switch (request->state()) {
  case WebAuthState::SelectAccount: {
    bool ok = false;
    const QStringList userNames = request->userNames();
    const QString selectedAccount = QInputDialog::getItem(
        parent, QObject::tr("Security sign-in"),
        QObject::tr("Select an account to continue signing in."), userNames, 0,
        false, &ok);
    if (ok && !selectedAccount.isEmpty()) {
      request->setSelectedAccount(selectedAccount);
    } else {
      request->cancel();
    }
    break;
  }
  case WebAuthState::CollectPin: {
    bool ok = false;
    const QString pin =
        QInputDialog::getText(parent, QObject::tr("Security sign-in"),
                              QObject::tr("Enter your security key PIN."),
                              QLineEdit::Password, QString(), &ok);
    if (ok) {
      request->setPin(pin);
    } else {
      request->cancel();
    }
    break;
  }
  case WebAuthState::FinishTokenCollection:
    QMessageBox::information(
        parent, QObject::tr("Security sign-in"),
        QObject::tr("Complete the security prompt on your device to continue "
                    "signing in."));
    break;
  case WebAuthState::RequestFailed:
    QMessageBox::warning(parent, QObject::tr("Security sign-in failed"),
                         QObject::tr("The security sign-in request failed."));
    break;
  case WebAuthState::Cancelled:
  case WebAuthState::Completed:
  case WebAuthState::NotStarted:
    break;
  }
}
#endif

// Time to wait for the cookie store to report the stored cookies before
// deciding whether the GitHub login page has to be shown first.
constexpr int kGitHubLoginCookieTimeoutMs = 500;

bool isPrivateGitHubRepository(const QString &serviceType,
                               const QString &repositoryName) {
  if (normalizedServiceType(serviceType) != QStringLiteral("github")) {
    return false;
  }

  const QString trimmedRepositoryName = repositoryName.trimmed();
  const QStringList parts =
      trimmedRepositoryName.split(QStringLiteral("/"), Qt::SkipEmptyParts);
  if (parts.size() < 2) {
    return false;
  }

  const IssueTrackers::GitHubRepo repo(parts.first(), parts.last());
  QString errorMessage;
  const auto visibility = repo.repositoryVisibility(&errorMessage);
  return visibility != IssueTrackers::GitHubRepo::RepositoryVisibility::Public;
}
#endif

QUrl buildGitHubLoginUrl(const QUrl &issuesUrl) {
  if (!issuesUrl.isValid()) {
    return QUrl();
  }

  QUrl loginUrl = issuesUrl;
  loginUrl.setPath(QStringLiteral("/login"));
  loginUrl.setFragment(QString());

  const QString returnTo = issuesUrl.toString(
      QUrl::RemoveScheme | QUrl::RemoveAuthority | QUrl::RemoveFragment);
  loginUrl.setQuery(QStringLiteral("return_to=") +
                        QString::fromLatin1(QUrl::toPercentEncoding(
                            returnTo, QByteArrayLiteral("/"))),
                    QUrl::StrictMode);

  return loginUrl;
}

QUrl buildIssueTrackerUrl(const QString &serviceType,
                          const QString &repositoryName,
                          const QString &serviceHost) {
  const QString normalized = normalizedServiceType(serviceType);
  const QString trimmedRepositoryName = repositoryName.trimmed();
  if (normalized == QStringLiteral("github")) {
    const QStringList parts =
        trimmedRepositoryName.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
      const IssueTrackers::GitHubRepo repo(parts.first(), parts.last());
      return QUrl(repo.issuesUrl());
    }
  }

  if (normalized == QStringLiteral("gitlab")) {
    const int separatorIndex = trimmedRepositoryName.lastIndexOf('/');
    if (separatorIndex > 0 &&
        separatorIndex < trimmedRepositoryName.size() - 1) {
      const QString owner = trimmedRepositoryName.left(separatorIndex);
      const QString repository = trimmedRepositoryName.mid(separatorIndex + 1);
      const IssueTrackers::GitLabRepo repo(owner, repository, serviceHost);
      return QUrl(repo.issuesUrl());
    }
  }

  if (normalized == QStringLiteral("azure")) {
    const QStringList parts =
        trimmedRepositoryName.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
      const IssueTrackers::AzureRepo repo(parts.first(), parts.last());
      return QUrl(repo.issuesUrl());
    }
  }

  if (normalized == QStringLiteral("bitbucket")) {
    const QStringList parts =
        trimmedRepositoryName.split(QStringLiteral("/"), Qt::SkipEmptyParts);
    if (parts.size() >= 2) {
      const IssueTrackers::BitbucketRepo repo(parts.first(), parts.last());
      return QUrl(repo.issuesUrl());
    }
  }

  if (normalized == QStringLiteral("custom")) {
    const QUrl url(serviceHost.trimmed(), QUrl::StrictMode);
    const QString scheme = url.scheme().toLower();
    if (url.isValid() && !url.host().isEmpty() &&
        (scheme == QStringLiteral("http") ||
         scheme == QStringLiteral("https"))) {
      return url;
    }
  }

  return QUrl();
}

QString unsupportedMessage(const QString &serviceType,
                           const QString &repositoryName) {
  if (serviceType.trimmed().isEmpty() || repositoryName.trimmed().isEmpty()) {
    return QObject::tr("No issue tracker is selected.");
  }

  return QObject::tr("This issue tracker is not supported yet: %1 %2")
      .arg(serviceType.trimmed(), repositoryName.trimmed());
}

} // namespace

IssueTrackerContainer::IssueTrackerContainer(QString serviceType,
                                             QString repositoryName,
                                             QUrl startUrl, QWidget *parent)
    : ContainerWidget(ContainerType::IssueTracker, parent),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _toastHost(ToastHostWidget::getInstance()),
      ui(new Ui::IssueTrackerContainer), _selected(false),
      _serviceType(serviceType.trimmed()),
      _repositoryName(repositoryName.trimmed()), _serviceHost(QString()),
      _startUrl(std::move(startUrl)), _toolbarWidget(nullptr),
      _toolbarHandle(nullptr), _backButton(nullptr), _forwardButton(nullptr),
      _refreshButton(nullptr), _draggingToolbar(false),
      _externalBrowserOpened(false), _pageLoaded(false),
      _toolbarDragOffset(0, 0), _toolbarRelativePosition(0, 0),
      _webEngineProfile(nullptr), _browserView(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  QString storedHost;
  QString storedAccessToken;
  QDateTime storedExpiresAt;
  if (IssueTrackers::TokensStorage::getInstance().getTokenWithHost(
          _serviceType, _repositoryName, storedHost, storedAccessToken,
          storedExpiresAt)) {
    _serviceHost = storedHost.trimmed();
  }

  updateIssueUrl();

#if GUI_MANAGER_HAS_WEBENGINE
  _webEngineProfile = createIssueTrackerWebEngineProfile(
      _serviceType, _repositoryName, _serviceHost);
  _browserView = new QWebEngineView(this);
  if (_webEngineProfile) {
    // prepare script
    QWebEngineScript disableWebAuthn;
    disableWebAuthn.setName("disable-webauthn");
    disableWebAuthn.setInjectionPoint(QWebEngineScript::DocumentCreation);
    disableWebAuthn.setRunsOnSubFrames(true);
    disableWebAuthn.setWorldId(QWebEngineScript::MainWorld);
    disableWebAuthn.setSourceCode(R"(
(function() {
  try {
    const reject = () => Promise.reject(new DOMException('WebAuthn disabled', 'NotAllowedError'));
    if (navigator.credentials) {
      navigator.credentials.get = function() { return reject(); };
      navigator.credentials.create = function() { return reject(); };
    }
    try { window.PublicKeyCredential = undefined; } catch(e) {}
  } catch(e) {}
})();
)");

    _webEngineProfile->scripts()->insert(disableWebAuthn);

    auto *page = new QWebEnginePage(_webEngineProfile, _browserView);
    connect(page, &QWebEnginePage::renderProcessTerminated, this,
            [this, page](QWebEnginePage::RenderProcessTerminationStatus status,
                         int exitCode) {
              LOG_ERROR(Log::ErrorId::Gui,
                        "WebEngine render process terminated: status={}, "
                        "exitCode={}, pageUrl={}, requestedUrl={}",
                        static_cast<int>(status), exitCode,
                        page ? page->url().toString() : QString(),
                        page ? page->requestedUrl().toString() : QString());
              openIssueTrackerExternallyAndClose();
            });
    connect(
        page, &QWebEnginePage::newWindowRequested, this,
        [page](QWebEngineNewWindowRequest &request) { request.openIn(page); });
#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
    connect(page, &QWebEnginePage::webAuthUxRequested, this,
            [this](QWebEngineWebAuthUxRequest *request) {
              if (!request) {
                return;
              }

              connect(
                  request, &QWebEngineWebAuthUxRequest::stateChanged, this,
                  [this, request](QWebEngineWebAuthUxRequest::WebAuthUxState) {
                    handleWebAuthUxRequest(request, this);
                  });
              handleWebAuthUxRequest(request, this);
            });
#endif

    connect(page, &QWebEnginePage::authenticationRequired, this,
            [this](const QUrl &requestUrl, QAuthenticator *authenticator) {
              if (!authenticator) {
                return;
              }

              if (normalizedServiceType(_serviceType) !=
                  QStringLiteral("bitbucket")) {
                return;
              }

              if (!requestUrl.host().contains(QStringLiteral("bitbucket"),
                                              Qt::CaseInsensitive)) {
                return;
              }

              QString userName;
              QString accessToken;
              QDateTime expiresAt;
              if (!IssueTrackers::TokensStorage::getInstance().getToken(
                      _serviceType, _repositoryName, userName, accessToken,
                      expiresAt) ||
                  userName.isEmpty() || accessToken.isEmpty()) {
                return;
              }

              authenticator->setUser(userName);
              authenticator->setPassword(accessToken);
            });

    _browserView->setPage(page);
  }
#else
  _browserView = new QTextBrowser(this);
  _browserView->setOpenExternalLinks(true);
  _browserView->setOpenLinks(true);
#endif

  ui->mainLayout->setContentsMargins(0, 0, 0, 0);
  ui->mainLayout->setSpacing(0);
  ui->mainLayout->addWidget(_browserView);

  buildToolbar();
  if (qApp) {
    qApp->installEventFilter(this);
  }
  applyToolbarTheme();
  restoreToolbarPosition();

  // The first page is loaded by init().

  LOG_PERF(0, "Exit");
}

IssueTrackerContainer::~IssueTrackerContainer() {
  LOG_PERF(0, "Enter");

  if (qApp) {
    qApp->removeEventFilter(this);
  }

  delete ui;

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::init() {
  LOG_PERF(0, "Enter");

  loadIssuePage();

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::deselected() {
  LOG_PERF(0, "Enter");

  _selected = false;

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::selected() {
  LOG_PERF(0, "Enter");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  LOG_DEBUG("Title bar height retrieved: {} px", titleBarHeight);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);
  _toastHost.raise();
  LOG_DEBUG("Toast host configured and raised");

  _selected = true;
  QTimer::singleShot(0, this, [this]() { restoreToolbarPosition(); });

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  restoreToolbarPosition();
}

void IssueTrackerContainer::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);

  if (!event) {
    return;
  }

  switch (event->type()) {
  case QEvent::StyleChange:
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
    applyToolbarTheme();
    break;
  default:
    break;
  }
}

bool IssueTrackerContainer::eventFilter(QObject *watched, QEvent *event) {
  if (watched == qApp && event) {
    switch (event->type()) {
    case QEvent::StyleChange:
    case QEvent::PaletteChange:
    case QEvent::ApplicationPaletteChange:
      applyToolbarTheme();
      break;
    default:
      break;
    }
  }

  if (watched == _toolbarHandle && event) {
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton) {
        _draggingToolbar = true;
        _toolbarHandle->grabMouse();
        _toolbarDragOffset = mouseEvent->globalPosition().toPoint() -
                             _toolbarWidget->frameGeometry().topLeft();
        return true;
      }
      break;
    }
    case QEvent::MouseMove: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (_draggingToolbar && (mouseEvent->buttons() & Qt::LeftButton)) {
        const QPoint desiredGlobalTopLeft =
            mouseEvent->globalPosition().toPoint() - _toolbarDragOffset;
        const QPoint desiredInContainer = mapFromGlobal(desiredGlobalTopLeft);
        moveToolbarTo(desiredInContainer - _browserView->pos(), true);
        return true;
      }
      break;
    }
    case QEvent::MouseButtonRelease: {
      auto *mouseEvent = static_cast<QMouseEvent *>(event);
      if (mouseEvent->button() == Qt::LeftButton && _draggingToolbar) {
        _draggingToolbar = false;
        _toolbarHandle->releaseMouse();
        persistToolbarPosition(_toolbarRelativePosition);
        return true;
      }
      break;
    }
    default:
      break;
    }
  }

  return QWidget::eventFilter(watched, event);
}

void IssueTrackerContainer::updateIssueUrl() {
  LOG_PERF(0, "Enter");

  _issuesUrl =
      _startUrl.isValid()
          ? _startUrl
          : buildIssueTrackerUrl(_serviceType, _repositoryName, _serviceHost);

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::buildToolbar() {
  LOG_PERF(0, "Enter");

  _toolbarWidget = new QWidget(this);
  _toolbarWidget->setObjectName("issueTrackerToolbar");
  _toolbarWidget->setAttribute(Qt::WA_StyledBackground, true);
  _toolbarWidget->setAutoFillBackground(false);

  auto *toolbarLayout = new QVBoxLayout(_toolbarWidget);
  toolbarLayout->setContentsMargins(8, 6, 8, 8);
  toolbarLayout->setSpacing(6);

  _toolbarHandle = new QWidget(_toolbarWidget);
  _toolbarHandle->setObjectName("issueTrackerToolbarHandle");
  _toolbarHandle->setCursor(Qt::SizeAllCursor);
  _toolbarHandle->setFixedHeight(5);
  _toolbarHandle->installEventFilter(this);

  _backButton = new QToolButton(_toolbarWidget);
  _backButton->setToolTip(tr("Go back"));
  _backButton->setAutoRaise(true);

  _forwardButton = new QToolButton(_toolbarWidget);
  _forwardButton->setToolTip(tr("Go forward"));
  _forwardButton->setAutoRaise(true);

  _refreshButton = new QToolButton(_toolbarWidget);
  _refreshButton->setToolTip(tr("Reload page"));
  _refreshButton->setAutoRaise(true);

  auto *buttonRow = new QWidget(_toolbarWidget);
  auto *buttonLayout = new QHBoxLayout(buttonRow);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(4);
  buttonLayout->addWidget(_backButton);
  buttonLayout->addWidget(_forwardButton);
  buttonLayout->addWidget(_refreshButton);

  const QSize iconSize(18, 18);
  _backButton->setIconSize(iconSize);
  _forwardButton->setIconSize(iconSize);
  _refreshButton->setIconSize(iconSize);
  _backButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
  _forwardButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
  _refreshButton->setToolButtonStyle(Qt::ToolButtonIconOnly);

  toolbarLayout->addWidget(_toolbarHandle);
  toolbarLayout->addWidget(buttonRow);

#if GUI_MANAGER_HAS_WEBENGINE
  connect(_backButton, &QToolButton::clicked, this, [this]() {
    if (_browserView) {
      _browserView->back();
      updateNavigationState();
    }
  });
  connect(_forwardButton, &QToolButton::clicked, this, [this]() {
    if (_browserView) {
      _browserView->forward();
      updateNavigationState();
    }
  });
  connect(_refreshButton, &QToolButton::clicked, this, [this]() {
    if (_browserView) {
      _browserView->reload();
      updateNavigationState();
    }
  });
  connect(_browserView->page(), &QWebEnginePage::loadingChanged, this,
          [this](const QWebEngineLoadingInfo &info) {
            switch (info.status()) {
            case QWebEngineLoadingInfo::LoadStartedStatus:
              return;
            case QWebEngineLoadingInfo::LoadSucceededStatus:
              _pageLoaded = true;
              break;
            case QWebEngineLoadingInfo::LoadStoppedStatus:
              // Aborted or superseded navigation, not a failure.
              break;
            case QWebEngineLoadingInfo::LoadFailedStatus: {
              LOG_ERROR(Log::ErrorId::Gui,
                        "WebEngine load failed: url={}, issuesUrl={}, "
                        "errorDomain={}, errorCode={}, error={}",
                        info.url().toString(), _issuesUrl.toString(),
                        static_cast<int>(info.errorDomain()), info.errorCode(),
                        info.errorString());

              // An HTTP error comes with the server's own error page, and
              // once a page was shown the view can display later errors
              // itself. Only fall back to the external browser when the
              // first page could not be loaded at all.
              const bool httpError =
                  info.errorDomain() ==
                  QWebEngineLoadingInfo::HttpStatusCodeDomain;
              if (!httpError && !_pageLoaded) {
                openIssueTrackerExternallyAndClose();
              } else {
                _pageLoaded = true;
              }
              break;
            }
            }
            updateNavigationState();
          });
  connect(_browserView, &QWebEngineView::urlChanged, this,
          [this](const QUrl &) { updateNavigationState(); });
#else
  connect(_backButton, &QToolButton::clicked, this, [this]() {
    if (_browserView) {
      _browserView->backward();
      updateNavigationState();
    }
  });
  connect(_forwardButton, &QToolButton::clicked, this, [this]() {
    if (_browserView) {
      _browserView->forward();
      updateNavigationState();
    }
  });
  connect(_refreshButton, &QToolButton::clicked, this, [this]() {
    if (_browserView) {
      _browserView->reload();
      updateNavigationState();
    }
  });
  connect(_browserView, &QTextBrowser::sourceChanged, this,
          [this](const QUrl &) { updateNavigationState(); });
#endif

  _toolbarWidget->adjustSize();
  _toolbarWidget->raise();
  _toolbarWidget->show();

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::applyToolbarTheme() {
  if (!_toolbarWidget) {
    return;
  }

  _toolbarWidget->setStyleSheet(
      StylesheetManager::StylesheetManager::issueTrackerToolbarStyleSheet());
  if (_backButton) {
    _backButton->setIcon(
        StylesheetManager::StylesheetManager::issueTrackerToolbarBackIcon());
  }
  if (_forwardButton) {
    _forwardButton->setIcon(
        StylesheetManager::StylesheetManager::issueTrackerToolbarForwardIcon());
  }
  if (_refreshButton) {
    _refreshButton->setIcon(
        StylesheetManager::StylesheetManager::issueTrackerToolbarRefreshIcon());
  }
}

void IssueTrackerContainer::restoreToolbarPosition() {
  LOG_PERF(0, "Enter");

  if (!_toolbarWidget || !_browserView) {
    LOG_PERF(0, "Exit");
    return;
  }

  applyToolbarTheme();
  const QPoint saved = loadToolbarPosition();
  moveToolbarTo(saved, false);
  _toolbarWidget->raise();
  _toolbarWidget->show();

  LOG_PERF(0, "Exit");
}

QPoint IssueTrackerContainer::defaultToolbarPosition() const {
  if (!_browserView || !_toolbarWidget) {
    return QPoint(8, 8);
  }

  const QRect availableRect = _browserView->rect();
  const QSize toolbarSize = _toolbarWidget->sizeHint();
  if (availableRect.width() <= 0 || availableRect.height() <= 0 ||
      toolbarSize.isEmpty()) {
    return QPoint(8, 8);
  }

  const int x = qMax(availableRect.left(),
                     (availableRect.width() - toolbarSize.width()) / 2);
  const int y = availableRect.top() + 8;

  return QPoint(x, y);
}

QPoint IssueTrackerContainer::loadToolbarPosition() const {
  const QString stored = _configManager.getString(
      ConfigManager::ConfigKey::IssueTrackerToolbarPosition);
  const QStringList parts = stored.split(',', Qt::SkipEmptyParts);
  if (parts.size() != 2) {
    return defaultToolbarPosition();
  }

  bool okX = false;
  bool okY = false;
  const int x = parts[0].trimmed().toInt(&okX);
  const int y = parts[1].trimmed().toInt(&okY);
  if (!okX || !okY) {
    return defaultToolbarPosition();
  }

  return QPoint(x, y);
}

QPoint IssueTrackerContainer::clampToolbarPosition(
    const QPoint &relativePosition) const {
  if (!_browserView || !_toolbarWidget) {
    return relativePosition;
  }

  const QRect availableRect = _browserView->rect();
  const QSize toolbarSize = _toolbarWidget->sizeHint();
  if (availableRect.width() <= 0 || availableRect.height() <= 0 ||
      toolbarSize.isEmpty()) {
    return relativePosition;
  }

  const int maxX = qMax(0, availableRect.width() - toolbarSize.width());
  const int maxY = qMax(0, availableRect.height() - toolbarSize.height());

  const int x = qBound(availableRect.left(), relativePosition.x(), maxX);
  const int y = qBound(availableRect.top(), relativePosition.y(), maxY);

  return QPoint(x, y);
}

void IssueTrackerContainer::moveToolbarTo(const QPoint &relativePosition,
                                          bool persist) {
  if (!_toolbarWidget || !_browserView) {
    return;
  }

  const QPoint clamped = clampToolbarPosition(relativePosition);
  _toolbarRelativePosition = clamped;
  const QPoint absolutePosition = _browserView->mapTo(this, clamped);
  _toolbarWidget->move(absolutePosition);
  _toolbarWidget->raise();

  if (persist) {
    persistToolbarPosition(clamped);
  }
}

void IssueTrackerContainer::persistToolbarPosition(
    const QPoint &relativePosition) {
  const QString serialized = QStringLiteral("%1,%2")
                                 .arg(relativePosition.x())
                                 .arg(relativePosition.y());
  _configManager.setString(
      ConfigManager::ConfigKey::IssueTrackerToolbarPosition, serialized);
  _configManager.save();
}

void IssueTrackerContainer::loadIssuePage() {
  LOG_PERF(0, "Enter");

  updateIssueUrl();

  if (_issuesUrl.isValid()) {
#if GUI_MANAGER_HAS_WEBENGINE
    if (isPrivateGitHubRepository(_serviceType, _repositoryName)) {
      loadAfterGitHubLoginCheck();
    } else {
      _browserView->load(_issuesUrl);
    }
#else
    _browserView->setSource(_issuesUrl);
#endif
  } else {
    showUnsupportedMessage(unsupportedMessage(_serviceType, _repositoryName));
  }

  updateNavigationState();
  restoreToolbarPosition();

  LOG_PERF(0, "Exit");
}

#if GUI_MANAGER_HAS_WEBENGINE
void IssueTrackerContainer::loadAfterGitHubLoginCheck() {
  LOG_PERF(0, "Enter");

  QWebEngineCookieStore *cookieStore =
      _webEngineProfile ? _webEngineProfile->cookieStore() : nullptr;
  if (!cookieStore) {
    _browserView->load(buildGitHubLoginUrl(_issuesUrl));
    LOG_PERF(0, "Exit");
    return;
  }

  // Collect the stored cookies asynchronously. Waiting for them in a nested
  // event loop re-enters Chromium while the view is being set up, which trips
  // WebEngine's internal checks.
  auto *check = new QObject(this);
  auto hasLoginCookie = std::make_shared<bool>(false);
  connect(cookieStore, &QWebEngineCookieStore::cookieAdded, check,
          [hasLoginCookie](const QNetworkCookie &cookie) {
            if (isGitHubLoginCookie(cookie)) {
              *hasLoginCookie = true;
            }
          });
  QTimer::singleShot(
      kGitHubLoginCookieTimeoutMs, check, [this, check, hasLoginCookie]() {
        check->deleteLater();
        _browserView->load(*hasLoginCookie ? _issuesUrl
                                           : buildGitHubLoginUrl(_issuesUrl));
        updateNavigationState();
      });
  cookieStore->loadAllCookies();

  LOG_PERF(0, "Exit");
}
#endif

void IssueTrackerContainer::updateNavigationState() {
  LOG_PERF(0, "Enter");

#if GUI_MANAGER_HAS_WEBENGINE
  const auto *page = _browserView ? _browserView->page() : nullptr;
  const bool canGoBack =
      page && page->history() && page->history()->canGoBack();
  const bool canGoForward =
      page && page->history() && page->history()->canGoForward();
#else
  const bool canGoBack = _browserView && _browserView->isBackwardAvailable();
  const bool canGoForward = _browserView && _browserView->isForwardAvailable();
#endif

  if (_backButton) {
    _backButton->setEnabled(canGoBack);
  }
  if (_forwardButton) {
    _forwardButton->setEnabled(canGoForward);
  }
  if (_refreshButton) {
    _refreshButton->setEnabled(_browserView != nullptr);
  }

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::showUnsupportedMessage(const QString &message) {
  LOG_PERF(0, "Enter");

#if GUI_MANAGER_HAS_WEBENGINE
  openIssueTrackerExternallyAndClose();
#else
  _browserView->setHtml(
      QStringLiteral("<html><body style='font-family:sans-serif;padding:16px;'>"
                     "<h2>Issue tracker</h2>"
                     "<p>%1</p>"
                     "</body></html>")
          .arg(message.toHtmlEscaped()));
#endif

  updateNavigationState();

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::showLoadFailedMessage(const QString &message) {
  LOG_PERF(0, "Enter");

#if GUI_MANAGER_HAS_WEBENGINE
  openIssueTrackerExternallyAndClose();
#else
  _browserView->setHtml(
      QStringLiteral("<html><body style='font-family:sans-serif;padding:16px;'>"
                     "<h2>Issue tracker failed to load</h2>"
                     "<p>%1</p>"
                     "</body></html>")
          .arg(message.toHtmlEscaped()));
#endif

  updateNavigationState();

  LOG_PERF(0, "Exit");
}

void IssueTrackerContainer::openIssueTrackerExternallyAndClose() {
  if (_externalBrowserOpened) {
    return;
  }

  _externalBrowserOpened = true;
  if (_issuesUrl.isValid()) {
    FileManager::openExternalUrl(_issuesUrl);
  }
  emit close();
}

} // namespace GuiManager::Containers
