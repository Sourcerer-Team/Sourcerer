// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/SettingsPaneDateTimeWidget.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QDateTimeEdit>
#include <QLabel>
#include <QVBoxLayout>

#define THREAD_ID "SettingsPaneDateTimeWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneDateTimeWidget::SettingsPaneDateTimeWidget(
    const ConfigManager::ConfigItem &item, QWidget *parent)
    : SettingsPaneItemWidget(item, parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

QWidget *SettingsPaneDateTimeWidget::createWidget() {
  LOG_PERF(0, "Enter");

  QWidget *placeholderWidget = new QWidget(this);

  QLabel *nameLabel = new QLabel(item().name(), placeholderWidget);
  nameLabel->setObjectName("nameLabel");
  nameLabel->setWordWrap(true);

  QLabel *descriptionLabel =
      new QLabel(item().description(), placeholderWidget);
  descriptionLabel->setObjectName("descriptionLabel");
  descriptionLabel->setWordWrap(true);

  QDateTimeEdit *dateTimeWidget =
      new QDateTimeEdit(item().value().toDateTime(), placeholderWidget);
  dateTimeWidget->setObjectName("dateTimeEdit");

  QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);
  layout->addWidget(nameLabel);
  if (!item().description().isEmpty())
    layout->addWidget(dateTimeWidget);
  layout->addWidget(descriptionLabel);

  connect(dateTimeWidget, &QDateTimeEdit::dateTimeChanged, this,
          &SettingsPaneDateTimeWidget::onDateTimeChanged);

  LOG_PERF(0, "Exit");

  return placeholderWidget;
}

void SettingsPaneDateTimeWidget::load() {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  const QDateTime value = configManager.getDateTime(key);
  item().setValue(value);

  LOG_PERF(0, "Exit");
}

void SettingsPaneDateTimeWidget::save() const {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  configManager.setDateTime(key, item().value().toDateTime());

  LOG_PERF(0, "Exit");
}

void SettingsPaneDateTimeWidget::onDateTimeChanged(const QDateTime &dateTime) {
  LOG_PERF(0, "Enter");

  item().setValue(dateTime);
  save();

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
