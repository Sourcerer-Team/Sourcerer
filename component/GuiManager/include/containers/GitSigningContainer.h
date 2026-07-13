#pragma once

#include "widgets/ContainerWidget.h"

#include <ConfigManager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QWidget>

namespace GuiManager::Containers {

class GitSigningContainer final : public Widgets::ContainerWidget {
  Q_OBJECT

public:
  explicit GitSigningContainer(QWidget *parent = nullptr);
  ~GitSigningContainer() override;

  void selected() override;
  void deselected() override;

private slots:
  void onEnabledToggled(bool enabled);
  void onFormatChanged(int index);
  void onBrowseKeyClicked();
  void onBrowseProgramClicked();
  void onSaveClicked();
  void onResetClicked();

private:
  void loadSettings();
  void saveSettings();
  void updateFormatUi();
  void updateDefaultProgram();
  void updateSaveButtonState();
  QString currentFormat() const;

private:
  ConfigManager::ConfigManager &_configManager;
  QCheckBox *_enabledCheckBox;
  QComboBox *_formatComboBox;
  QLineEdit *_signingKeyEdit;
  QPushButton *_signingKeyBrowseButton;
  QLineEdit *_programEdit;
  QPushButton *_programBrowseButton;
  QPushButton *_saveButton;
  QPushButton *_resetButton;
  QLabel *_titleLabel;
  QLabel *_descriptionLabel;
  QLabel *_formatLabel;
  QLabel *_keyLabel;
  QLabel *_programLabel;
  bool _savedEnabled;
  QString _savedFormat;
  QString _savedSigningKey;
  QString _savedProgram;
};

} // namespace GuiManager::Containers
