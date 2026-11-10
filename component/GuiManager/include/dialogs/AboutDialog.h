// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <ConfigManager.h>

#include <QDialog>

namespace Ui {
class AboutDialog;
}

namespace GuiManager {

namespace Dialogs {

class AboutDialog : public QDialog {
  Q_OBJECT

public:
  explicit AboutDialog(QWidget *parent = nullptr);
  ~AboutDialog();

private slots:
  void onShowLicense();

private:
  Ui::AboutDialog *ui;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Dialogs
} // namespace GuiManager
