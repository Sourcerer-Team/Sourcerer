#include "widgets/TitleBarMenuButton.h"
#include "MainWindow.h"

#include <Log.h>

#include <QActionGroup>
#include <QApplication>
#include <QKeySequence>

#include <StylesheetManager.h>
#include <qstyle.h>

#define THREAD_ID "TitleBarMenuButton"

namespace GuiManager::Widgets {

static void refreshAppStyle() {
  // 2. Unpolish and repolish every top-level window and its children
  for (QWidget *widget : QApplication::topLevelWidgets()) {
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);

    // Force full geometry recalculation and repaint
    widget->updateGeometry();
    widget->update();
  }
}

TitleBarMenuButton::TitleBarMenuButton(QWidget *parent)
    : QToolButton{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _openAction(nullptr), _cloneAction(nullptr), _initAction(nullptr),
      _saveWorkspaceAction(nullptr),
      _connectToGitHubIssueTrackerAction(nullptr),
      _connectToGitLabIssueTrackerAction(nullptr),
      _connectToAzureIssueTrackerAction(nullptr),
      _connectToBitbucketIssueTrackerAction(nullptr),
      _connectToCustomIssueTrackerAction(nullptr), _settingsAction(nullptr),
      _credentialsAction(nullptr), _sshKeysAction(nullptr),
      _terminalAction(nullptr), _aboutAction(nullptr) {
  LOG_PERF(0, "Enter");

  setMouseTracking(true);
  setAttribute(Qt::WA_Hover, true);
  setPopupMode(QToolButton::MenuButtonPopup);
  installEventFilter(this);

  createActions();

  connect(this, &TitleBarMenuButton::clicked, this,
          &TitleBarMenuButton::onClicked);

  LOG_PERF(0, "Exit");
}

bool TitleBarMenuButton::eventFilter(QObject *watched, QEvent *event) {
  if (!event || this != watched)
    return false;

  switch (event->type()) {
  case QEvent::HoverEnter: {
    return hover(static_cast<QHoverEvent *>(event), true);
  }
  case QEvent::HoverLeave: {
    return hover(static_cast<QHoverEvent *>(event), false);
  }
  default: {
    break;
  }
  }

  return QToolButton::eventFilter(watched, event);
}

void TitleBarMenuButton::onClicked() {
  LOG_PERF(0, "Enter");

  setMenu(createMenu());
  showMenu();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onOpen() {
  LOG_PERF(0, "Enter");

  emit open();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onClone() {
  LOG_PERF(0, "Enter");

  emit clone();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onInit() {
  LOG_PERF(0, "Enter");

  emit init();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onSaveWorkspace() {
  LOG_PERF(0, "Enter");

  emit saveWorkspace();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onConnectToGitHubIssueTracker() {
  LOG_PERF(0, "Enter");

  emit connectToGitHubIssueTracker();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onConnectToGitLabIssueTracker() {
  LOG_PERF(0, "Enter");

  emit connectToGitLabIssueTracker();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onConnectToAzureIssueTracker() {
  LOG_PERF(0, "Enter");

  emit connectToAzureIssueTracker();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onConnectToBitbucketIssueTracker() {
  LOG_PERF(0, "Enter");

  emit connectToBitbucketIssueTracker();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onConnectToCustomIssueTracker() {
  LOG_PERF(0, "Enter");

  emit connectToCustomIssueTracker();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onSettings() {
  LOG_PERF(0, "Enter");

  emit settings();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onCredentials() {
  LOG_PERF(0, "Enter");

  emit credentials();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onSshKeys() {
  LOG_PERF(0, "Enter");

  emit sshKeys();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onTerminal() {
  LOG_PERF(0, "Enter");

  emit terminal();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onBugReport() {
  LOG_PERF(0, "Enter");

  emit bugReport();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::onAbout() {
  LOG_PERF(0, "Enter");

  emit about();

  LOG_PERF(0, "Exit");
}

void TitleBarMenuButton::createActions() {
  LOG_PERF(0, "Enter");

  _openAction = new QAction(this);
  _openAction->setText(tr("&Open"));
  _openAction->setShortcut(QKeySequence::Open);
  _openAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_openAction, &QAction::triggered, this, &TitleBarMenuButton::onOpen);

  _cloneAction = new QAction(this);
  _cloneAction->setText(tr("&Clone"));
  connect(_cloneAction, &QAction::triggered, this,
          &TitleBarMenuButton::onClone);

  _initAction = new QAction(this);
  _initAction->setText(tr("&Init"));
  _initAction->setShortcut(QKeySequence::New);
  _initAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_initAction, &QAction::triggered, this, &TitleBarMenuButton::onInit);

  _saveWorkspaceAction = new QAction(this);
  _saveWorkspaceAction->setText(tr("Save &workspace"));
  _saveWorkspaceAction->setShortcut(QKeySequence("Ctrl+w"));
  _saveWorkspaceAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_saveWorkspaceAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSaveWorkspace);

  _connectToGitHubIssueTrackerAction = new QAction(this);
  _connectToGitHubIssueTrackerAction->setText(tr("GitHub issue tracker"));
  connect(_connectToGitHubIssueTrackerAction, &QAction::triggered, this,
          &TitleBarMenuButton::onConnectToGitHubIssueTracker);

  _connectToGitLabIssueTrackerAction = new QAction(this);
  _connectToGitLabIssueTrackerAction->setText(tr("GitLab issue tracker"));
  connect(_connectToGitLabIssueTrackerAction, &QAction::triggered, this,
          &TitleBarMenuButton::onConnectToGitLabIssueTracker);

  _connectToAzureIssueTrackerAction = new QAction(this);
  _connectToAzureIssueTrackerAction->setText(tr("Azure issue tracker"));
  connect(_connectToAzureIssueTrackerAction, &QAction::triggered, this,
          &TitleBarMenuButton::onConnectToAzureIssueTracker);

  _connectToBitbucketIssueTrackerAction = new QAction(this);
  _connectToBitbucketIssueTrackerAction->setText(tr("Bitbucket issue tracker"));
  connect(_connectToBitbucketIssueTrackerAction, &QAction::triggered, this,
          &TitleBarMenuButton::onConnectToBitbucketIssueTracker);

  _connectToCustomIssueTrackerAction = new QAction(this);
  _connectToCustomIssueTrackerAction->setText(tr("Custom URL issue tracker"));
  connect(_connectToCustomIssueTrackerAction, &QAction::triggered, this,
          &TitleBarMenuButton::onConnectToCustomIssueTracker);

  _settingsAction = new QAction(this);
  _settingsAction->setText(tr("Settings"));
  connect(_settingsAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSettings);

  _terminalAction = new QAction(this);
  _terminalAction->setText(tr("Show &terminal"));
  _terminalAction->setShortcut(QKeySequence("Ctrl+t"));
  _terminalAction->setShortcutContext(Qt::ApplicationShortcut);
  connect(_terminalAction, &QAction::triggered, this,
          &TitleBarMenuButton::onTerminal);

  _credentialsAction = new QAction(this);
  _credentialsAction->setText(tr("Credentials manager"));
  connect(_credentialsAction, &QAction::triggered, this,
          &TitleBarMenuButton::onCredentials);

  _sshKeysAction = new QAction(this);
  _sshKeysAction->setText(tr("SSH keys manager"));
  connect(_sshKeysAction, &QAction::triggered, this,
          &TitleBarMenuButton::onSshKeys);

  _bugReportAction = new QAction(this);
  _bugReportAction->setText(tr("Report bug"));
  connect(_bugReportAction, &QAction::triggered, this,
          &TitleBarMenuButton::onBugReport);

  _aboutAction = new QAction(this);
  _aboutAction->setText(tr("About"));
  connect(_aboutAction, &QAction::triggered, this,
          &TitleBarMenuButton::onAbout);

  // Add actions to the widget so their shortcuts become active
  addAction(_openAction);
  addAction(_initAction);
  addAction(_saveWorkspaceAction);
  addAction(_terminalAction);

  LOG_PERF(0, "Exit");
}

QMenu *TitleBarMenuButton::createMenu() {
  LOG_PERF(0, "Enter");

  const auto menu = new QMenu(this);

  const MainWindow *mainWindow_ = mainWindow();
  Q_ASSERT(mainWindow_ != nullptr);

  // Update save workspace action enabled state based on current containers
  _saveWorkspaceAction->setEnabled(mainWindow_ != nullptr &&
                                   !mainWindow_->containerPaths().isEmpty());

  menu->addAction(_openAction);
  menu->addAction(_cloneAction);
  menu->addAction(_initAction);
  menu->addSeparator();
  menu->addAction(_saveWorkspaceAction);
  menu->addSeparator();

  auto *connectToMenu = menu->addMenu(tr("Connect to"));
  connectToMenu->addAction(_connectToGitHubIssueTrackerAction);
  connectToMenu->addAction(_connectToGitLabIssueTrackerAction);
  connectToMenu->addAction(_connectToAzureIssueTrackerAction);
  connectToMenu->addAction(_connectToBitbucketIssueTrackerAction);
  connectToMenu->addAction(_connectToCustomIssueTrackerAction);
  menu->addSeparator();

  auto *themesMenu = menu->addMenu(tr("Themes"));
  auto *themeActions = new QActionGroup(menu);
  themeActions->setExclusive(true);

  const auto addThemeAction =
      [this, themesMenu, themeActions](const QString &text,
                                       StylesheetManager::StylesheetKey key) {
        auto *action = themesMenu->addAction(text);
        action->setCheckable(true);
        action->setChecked(
            StylesheetManager::StylesheetManager::currentStylesheet() == key);
        action->setData(static_cast<int>(key));
        themeActions->addAction(action);
        connect(action, &QAction::triggered, this, [this, key](bool) {
          StylesheetManager::StylesheetManager::setCurrentStylesheet(key);
          _configManager.setString(
              ConfigManager::ConfigKey::UiTheme,
              StylesheetManager::StylesheetManager::stylesheetKeyName(key));
          _configManager.save();
          StylesheetManager::StylesheetManager::applyStylesheet(qApp);
          refreshAppStyle();
          emit themeChanged();
        });
      };

  addThemeAction(tr("Sourcerer Light"),
                 StylesheetManager::StylesheetKey::SourcererLight);
  addThemeAction(tr("Sourcerer Dark"),
                 StylesheetManager::StylesheetKey::SourcererDark);

  menu->addSeparator();
  menu->addAction(_terminalAction);
  menu->addSeparator();
  menu->addAction(_credentialsAction);
  menu->addAction(_sshKeysAction);
  menu->addSeparator();
  menu->addAction(_settingsAction);
  menu->addAction(_bugReportAction);
  menu->addAction(_aboutAction);

  const auto menuColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();

  const auto setActionIcon = [&menuColor](QAction *action,
                                          const QString &path) {
    action->setProperty("iconSource", path);
    action->setIcon(
        QIcon(StylesheetManager::StylesheetManager::renderSvgToPixmap(
            path, menuColor)));
  };
  setActionIcon(_openAction, QStringLiteral(":/icons/repo-open.svg"));
  setActionIcon(_cloneAction, QStringLiteral(":/icons/repo-clone.svg"));
  setActionIcon(_initAction, QStringLiteral(":/icons/repo-init.svg"));
  setActionIcon(_terminalAction, QStringLiteral(":/icons/terminal.svg"));
  setActionIcon(_connectToGitHubIssueTrackerAction,
                QStringLiteral(":/icons/issue-trackers/github.svg"));
  setActionIcon(_connectToGitLabIssueTrackerAction,
                QStringLiteral(":/icons/issue-trackers/gitlab.svg"));
  setActionIcon(_connectToAzureIssueTrackerAction,
                QStringLiteral(":/icons/issue-trackers/azure.svg"));
  setActionIcon(_connectToBitbucketIssueTrackerAction,
                QStringLiteral(":/icons/issue-trackers/bitbucket.svg"));
  setActionIcon(_connectToCustomIssueTrackerAction,
                QStringLiteral(":/icons/issue-trackers/custom.svg"));

  _settingsAction->setIcon(
      QIcon(StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(QStringLiteral(":icons/settings.png")), menuColor)));

  const qlonglong menuMinimumWidth =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarMenuMinimumSize);

  LOG_DEBUG("Menu minimum width is {} pixels.", menuMinimumWidth);

  menu->setMinimumWidth(menuMinimumWidth);

  LOG_PERF(0, "Exit");

  return menu;
}

MainWindow *TitleBarMenuButton::mainWindow() {
  LOG_PERF(0, "Enter");

  foreach (QWidget *widget, QApplication::topLevelWidgets()) {
    if (auto *mainWin = qobject_cast<MainWindow *>(widget)) {
      LOG_PERF(0, "Exit");
      return mainWin;
    }
  }

  LOG_PERF(0, "Exit");
  return nullptr;
}

bool TitleBarMenuButton::hover(QHoverEvent *event, bool entered) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Entered is '{}'", entered);

  setDown(entered);

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace GuiManager::Widgets
