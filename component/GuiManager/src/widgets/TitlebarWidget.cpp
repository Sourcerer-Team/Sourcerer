#include "widgets/TitlebarWidget.h"
#include "ui_TitlebarWidget.h"

#include <Log.h>
#include <StylesheetManager.h>

#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>

#include <iostream>

#define THREAD_ID "TitleBarWidget"

namespace GuiManager::Widgets {

TitlebarWidget::TitlebarWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TitlebarWidget),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->tabBar->setUsesScrollButtons(false);
  ui->tabBar->setMouseTracking(true);
  ui->menuButton->setProperty("iconSource",
                              QStringLiteral(":/icons/titlebar_menu.png"));
  ui->menuButton->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":/icons/titlebar_menu.png"),
      StylesheetManager::StylesheetManager::menuBarButtonIconColor()));

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  setMaximumHeight(titleBarHeight);

  connect(ui->menuButton, &TitleBarMenuButton::open, this,
          &TitlebarWidget::onOpen);
  connect(ui->menuButton, &TitleBarMenuButton::clone, this,
          &TitlebarWidget::onClone);
  connect(ui->menuButton, &TitleBarMenuButton::init, this,
          &TitlebarWidget::onInit);
  connect(ui->menuButton, &TitleBarMenuButton::saveWorkspace, this,
          &TitlebarWidget::onSaveWorkspace);
  connect(ui->menuButton, &TitleBarMenuButton::connectToGitHubIssueTracker,
          this, &TitlebarWidget::connectToGitHubIssueTracker);
  connect(ui->menuButton, &TitleBarMenuButton::settings, this,
          &TitlebarWidget::onSettings);
  connect(ui->menuButton, &TitleBarMenuButton::credentials, this,
          &TitlebarWidget::onCredentials);
  connect(ui->menuButton, &TitleBarMenuButton::sshKeys, this,
          &TitlebarWidget::onSshKeys);
  connect(ui->menuButton, &TitleBarMenuButton::terminal, this,
          &TitlebarWidget::onTerminal);
  connect(ui->menuButton, &TitleBarMenuButton::bugReport, this,
          &TitlebarWidget::onBugReport);
  connect(ui->menuButton, &TitleBarMenuButton::about, this,
          &TitlebarWidget::onAbout);

  connect(ui->tabBar, &TitleBarTabBar::currentChanged, this,
          &TitlebarWidget::onSelectedTab);
  connect(ui->tabBar, &TitleBarTabBar::tabCloseRequested, this,
          &TitlebarWidget::onClosedTab);
  connect(ui->tabBar, &TitleBarTabBar::resizeTabBar, this,
          &TitlebarWidget::onTabBarResized);

  LOG_PERF(0, "Exit");
}

TitlebarWidget::~TitlebarWidget() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::addTab(const QString &text, const QString &toolTip) const {
  LOG_PERF(0, "Enter");

  const int index = ui->tabBar->addTab(text);
  ui->tabBar->setTabToolTip(index, toolTip.isEmpty() ? text : toolTip);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::removeTab(const int index) const {
  LOG_PERF(0, "Enter");

  ui->tabBar->removeTab(index);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::selectTab(const int index) const {
  LOG_PERF(0, "Enter");

  const int count = tabsCount();
  if (index < 0 || index > count - 1) {
    LOG_WARN("Tried to selected invalid tab with index {}.", index);
    LOG_PERF(0, "Exit");
    return;
  }

  ui->tabBar->setCurrentIndex(index);

  LOG_PERF(0, "Exit");
}

int TitlebarWidget::tabsCount() const {
  LOG_PERF(0, "Enter");

  const int count = ui->tabBar->count();

  LOG_DEBUG("Tabs count is {}.", count);

  LOG_PERF(0, "Exit");

  return count;
}

TitleBarMenuButton *TitlebarWidget::menuButton() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return ui->menuButton;
}

void TitlebarWidget::onOpen() {
  LOG_PERF(0, "Enter");

  emit open();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onClone() {
  LOG_PERF(0, "Enter");

  emit clone();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onInit() {
  LOG_PERF(0, "Enter");

  emit init();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onSaveWorkspace() {
  LOG_PERF(0, "Enter");

  emit saveWorkspace();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onSettings() {
  LOG_PERF(0, "Enter");

  emit settings();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onCredentials() {
  LOG_PERF(0, "Enter");

  emit credentials();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onSshKeys() {
  LOG_PERF(0, "Enter");

  emit sshKeys();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onTerminal() {
  LOG_PERF(0, "Enter");

  emit terminal();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onBugReport() {
  LOG_PERF(0, "Enter");

  emit bugReport();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onAbout() {
  LOG_PERF(0, "Enter");

  emit about();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onClosedTab(const int index) {
  LOG_PERF(0, "Enter");

  emit closedTab(index);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onSelectedTab(const int index) {
  LOG_PERF(0, "Enter");

  emit selectedTab(index);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onTabBarResized() {
  LOG_PERF(0, "Enter");

  emit resizeTabBar();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
