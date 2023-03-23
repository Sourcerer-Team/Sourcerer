#include "widgets/TitlebarWidget.h"
#include "ui_TitlebarWidget.h"

#include <Log.h>

#include <QLabel>

#define THREAD_ID "TitleBarWidget"

namespace GuiManager {

namespace Widgets {

TitlebarWidget::TitlebarWidget(QWidget *parent)
    : QWidget(parent), ui(new Ui::TitlebarWidget),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  setMaximumHeight(titleBarHeight);

  const qlonglong titleBarFontSize =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarFontSize);

  LOG_DEBUG("Title bar font size is %1 pixels.", titleBarFontSize);

  QFont font = ui->tabWidget->font();
  font.setPixelSize(titleBarFontSize);

  ui->tabWidget->setFont(font);

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

  LOG_PERF(0, "Exit");
}

TitlebarWidget::~TitlebarWidget() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void TitlebarWidget::addTab(const QString &text) {
  LOG_PERF(0, "Enter");

  ui->tabWidget->addTab(new QLabel(""), text);

  LOG_PERF(0, "Exit");
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

} // namespace Widgets
} // namespace GuiManager
