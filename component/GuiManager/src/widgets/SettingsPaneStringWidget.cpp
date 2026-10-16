#include "widgets/SettingsPaneStringWidget.h"

#include <Log.h>

#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

#define THREAD_ID "SettingsPaneStringWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneStringWidget::SettingsPaneStringWidget(
    const ConfigManager::ConfigItem &item, QWidget *parent)
    : SettingsPaneItemWidget(item, parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

QWidget *SettingsPaneStringWidget::createWidget() {
  LOG_PERF(0, "Enter");

  QWidget *placeholderWidget = new QWidget(this);

  QLabel *nameLabel = new QLabel(item().name(), placeholderWidget);
  nameLabel->setObjectName("nameLabel");
  nameLabel->setWordWrap(true);

  QLabel *descriptionLabel =
      new QLabel(item().description(), placeholderWidget);
  descriptionLabel->setObjectName("descriptionLabel");
  descriptionLabel->setWordWrap(true);

  QLineEdit *stringWidget = new QLineEdit(placeholderWidget);
  stringWidget->setObjectName("stringLineEdit");
  stringWidget->setText(item().value().toString());

  QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);
  layout->addWidget(nameLabel);
  if (!item().description().isEmpty())
    layout->addWidget(descriptionLabel);
  layout->addWidget(stringWidget);

  connect(stringWidget, &QLineEdit::textChanged, this,
          &SettingsPaneStringWidget::onTextChanged);

  LOG_PERF(0, "Exit");

  return placeholderWidget;
}

void SettingsPaneStringWidget::load() {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  const QString value = configManager.getString(key);
  item().setValue(value);

  LOG_PERF(0, "Exit");
}

void SettingsPaneStringWidget::save() const {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  configManager.setString(key, item().value().toString());

  LOG_PERF(0, "Exit");
}

void SettingsPaneStringWidget::onTextChanged(const QString &text) {
  LOG_PERF(0, "Enter");

  item().setValue(text);
  save();

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
