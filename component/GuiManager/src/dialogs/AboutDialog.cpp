#include "dialogs/AboutDialog.h"
#include "dialogs/ui_AboutDialog.h"

#include <Log.h>
#include <QDate>
#include <qglobal.h>

#define THREAD_ID "AboutDialog"

namespace GuiManager {

namespace Dialogs {

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::AboutDialog),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setWindowFlags(Qt::CustomizeWindowHint | Qt::WindowTitleHint);

  ui->setupUi(this);

  const QString appVersion = _configManager.getAppVersion();

  LOG_DEBUG("Application version is '{}'.", appVersion);

  // Version information
  ui->versionLabel->setText(tr("Version:") + " " + appVersion);

  // Application description
  ui->descriptionLabel->setText(
      tr("A powerful Git repository management application with an intuitive "
         "graphical interface. Sourcerer simplifies version control workflows "
         "with support for branches, commits, stashes, tags, submodules, and "
         "more."));

  // Qt version
  ui->qtVersionLabel->setText(tr("Built with Qt:") + " " + QT_VERSION_STR);

  // Build date
  ui->buildDateLabel->setText(tr("Build Date:") + " " + QString(__DATE__) +
                              " " + QString(__TIME__));

  // Copyright
  ui->copyrightLabel->setText(
      tr("Copyright © %1").arg(QDate::currentDate().year()));

  // Website
  ui->websiteLabel->setText(
      tr("Website: ") +
      "<a href='https://www.sourcerer.com'>https://www.sourcerer.com</a>");

  LOG_PERF(0, "Exit");
}

AboutDialog::~AboutDialog() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

} // namespace Dialogs
} // namespace GuiManager
