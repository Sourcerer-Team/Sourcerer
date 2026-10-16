#include "widgets/SettingsPaneColorWidget.h"
#include "widgets/ColorButton.h"

#include <Log.h>

#include <QColor>
#include <QColorDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#define THREAD_ID "SettingsPaneColorWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneColorWidget::SettingsPaneColorWidget(
    const ConfigManager::ConfigItem &item, QWidget *parent)
    : SettingsPaneItemWidget(item, parent), _colorButton{nullptr} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

QWidget *SettingsPaneColorWidget::createWidget() {
  LOG_PERF(0, "Enter");

  QWidget *placeholderWidget = new QWidget(this);

  QLabel *nameLabel = new QLabel(item().name(), placeholderWidget);
  nameLabel->setObjectName("nameLabel");
  nameLabel->setWordWrap(true);

  QLabel *descriptionLabel =
      new QLabel(item().description(), placeholderWidget);
  descriptionLabel->setObjectName("descriptionLabel");
  descriptionLabel->setWordWrap(true);

  _colorButton =
      new ColorButton(item().value().value<QColor>(), placeholderWidget);
  _colorButton->setObjectName("colorButton");

  QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);
  layout->addWidget(nameLabel);
  if (!item().description().isEmpty())
    layout->addWidget(descriptionLabel);
  layout->addWidget(_colorButton);

  connect(_colorButton, &ColorButton::clicked, this,
          &SettingsPaneColorWidget::onClicked);

  LOG_PERF(0, "Exit");

  return placeholderWidget;
}

void SettingsPaneColorWidget::load() {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  const QColor value = configManager.getColor(key);
  item().setValue(value);

  LOG_PERF(0, "Exit");
}

void SettingsPaneColorWidget::save() const {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  configManager.setColor(key, item().value().value<QColor>());

  LOG_PERF(0, "Exit");
}

void SettingsPaneColorWidget::onClicked() {
  LOG_PERF(0, "Enter");

  QWidget *rootWindow = window();
  if (!rootWindow) {
    LOG_ERROR(Log::ErrorId::Gui, "Can't get the root window.");
    LOG_PERF(0, "Exit");
    return;
  }

  QColorDialog colorDialog(item().value().value<QColor>(), rootWindow);
  colorDialog.move(
      rootWindow->x() + (rootWindow->width() - colorDialog.width()) / 2,
      rootWindow->y() + (rootWindow->height() - colorDialog.height()) / 2);
  const int ret = colorDialog.exec();

  if (ret == QDialog::Accepted) {
    const QColor &currentColor = colorDialog.currentColor();
    if (_colorButton)
      _colorButton->setColor(currentColor);
    item().setValue(currentColor);
    save();
  }

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
