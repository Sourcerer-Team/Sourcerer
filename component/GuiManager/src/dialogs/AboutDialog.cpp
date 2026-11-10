// MIT License
// Copyright (c) 2026 Sourcerer

#include "dialogs/AboutDialog.h"
#include "dialogs/ui_AboutDialog.h"

#include <Log.h>
#include <QApplication>
#include <QDate>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
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
  ui->versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  // Application description
  ui->descriptionLabel->setText(
      tr("A powerful, small and fast Git repository management application "
         "with an intuitive "
         "graphical interface. Sourcerer simplifies version control workflows "
         "with support for branches, commits, stashes, tags, submodules, and "
         "more."));
  ui->descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  // Qt version
  ui->qtVersionLabel->setText(tr("Built with Qt:") + " " + QT_VERSION_STR);
  ui->qtVersionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  // Build date
  ui->buildDateLabel->setText(tr("Build Date:") + " " + QString(__DATE__) +
                              " " + QString(__TIME__));
  ui->buildDateLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  // Copyright
  /*
  ui->copyrightLabel->setText(
      tr("Copyright © %1 Sourcerer").arg(QDate::currentDate().year()));
  ui->copyrightLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  // Website
  ui->websiteLabel->setText(
      tr("Website: ") +
      "<a "
      "href='https://www.sourcererapp.com'>https://www.sourcererapp.com</a>");
  ui->websiteLabel->setTextInteractionFlags(Qt::TextBrowserInteraction |
                                            Qt::TextSelectableByMouse);
  ui->websiteLabel->setOpenExternalLinks(true);
  */

  // Add License button to the button box
  QPushButton *licenseButton =
      ui->buttonBox->addButton(tr("License"), QDialogButtonBox::ActionRole);
  connect(licenseButton, &QPushButton::clicked, this,
          &AboutDialog::onShowLicense);

  LOG_PERF(0, "Exit");
}

void AboutDialog::onShowLicense() {
  LOG_PERF(0, "Enter");

  // Get the application executable directory
  QString appDir = QApplication::applicationDirPath();
  QString licenseFilePath = appDir + "/LICENSE.txt";

  // Try to read the license file
  QFile licenseFile(licenseFilePath);
  if (!licenseFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Failed to open license file: {}",
              licenseFilePath.toStdString());
    return;
  }

  // Read the entire file content
  QString licenseText = QString::fromUtf8(licenseFile.readAll());
  licenseFile.close();

  // Create a custom dialog with a scrollable text area
  QDialog licenseDialog(this);
  licenseDialog.setWindowTitle(tr("License"));
  licenseDialog.setMinimumSize(650, 400);
  licenseDialog.resize(780, 500);

  QVBoxLayout *layout = new QVBoxLayout(&licenseDialog);

  QPlainTextEdit *textEdit = new QPlainTextEdit(&licenseDialog);
  textEdit->setPlainText(licenseText);
  textEdit->setReadOnly(true);
  layout->addWidget(textEdit);

  QDialogButtonBox *buttonBox =
      new QDialogButtonBox(QDialogButtonBox::Ok, &licenseDialog);
  connect(buttonBox, &QDialogButtonBox::accepted, &licenseDialog,
          &QDialog::accept);
  layout->addWidget(buttonBox);

  licenseDialog.exec();

  LOG_PERF(0, "Exit");
}

AboutDialog::~AboutDialog() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

} // namespace Dialogs
} // namespace GuiManager
