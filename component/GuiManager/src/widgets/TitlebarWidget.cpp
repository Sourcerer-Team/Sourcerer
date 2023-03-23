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

} // namespace Widgets
} // namespace GuiManager
