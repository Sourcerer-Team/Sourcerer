#include "widgets/TitlebarWidget.h"
#include "ui_TitlebarWidget.h"

#include <Log.h>

#include <QLabel>
#include <QMouseEvent>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>

#include <iostream>

#define THREAD_ID "TitleBarWidget"

namespace GuiManager {

namespace Widgets {

TitlebarWidget::TitlebarWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TitlebarWidget),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->tabBar->setUsesScrollButtons(false);

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  setMaximumHeight(titleBarHeight);

  connect(ui->homeButton, &TitleBarHomeButton::clicked, this,
          &TitlebarWidget::onHome);
  connect(ui->menuButton, &TitleBarMenuButton::open, this,
          &TitlebarWidget::onOpen);
  connect(ui->menuButton, &TitleBarMenuButton::clone, this,
          &TitlebarWidget::onClone);
  connect(ui->menuButton, &TitleBarMenuButton::init, this,
          &TitlebarWidget::onInit);
  connect(ui->menuButton, &TitleBarMenuButton::settings, this,
          &TitlebarWidget::onSettings);
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

void TitlebarWidget::addTab(const QString &text) {
  LOG_PERF(0, "Enter");

  ui->tabBar->addTab(text);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::removeTab(int index) {
  LOG_PERF(0, "Enter");

  ui->tabBar->removeTab(index);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::selectTab(int index) {
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

void TitlebarWidget::onHome() {
  LOG_PERF(0, "Enter");

  emit home();

  LOG_PERF(0, "Exit");
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

void TitlebarWidget::onSettings() {
  LOG_PERF(0, "Enter");

  emit settings();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onAbout() {
  LOG_PERF(0, "Enter");

  emit about();

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onClosedTab(int index) {
  LOG_PERF(0, "Enter");

  emit closedTab(index);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onSelectedTab(int index) {
  LOG_PERF(0, "Enter");

  emit selectedTab(index);

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::onTabBarResized()
{
  LOG_PERF(0, "Enter");

  emit resizeTabBar();

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
