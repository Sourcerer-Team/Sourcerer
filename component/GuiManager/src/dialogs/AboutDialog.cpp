#include "dialogs/AboutDialog.h"
#include "dialogs/ui_AboutDialog.h"

#include <Log.h>

#define THREAD_ID "AboutDialog"

namespace GuiManager {

namespace Dialogs {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AboutDialog),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setWindowFlags(Qt::CustomizeWindowHint |Qt::WindowTitleHint);

  ui->setupUi(this);

  const QString appVersion = _configManager.getAppVersion();

  LOG_DEBUG("Application version is '%1'.", appVersion);

  ui->versionLabel->setText(tr("Version:") + " " + appVersion);

  LOG_PERF(0, "Exit");
}

AboutDialog::~AboutDialog() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

} // namespace Dialogs
} // namespace GuiManager
