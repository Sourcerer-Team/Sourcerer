// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/SettingsPaneBooleanWidget.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>

#define THREAD_ID "SettingsPaneBooleanWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneBooleanWidget::SettingsPaneBooleanWidget(
    const ConfigManager::ConfigItem &item, QWidget *parent)
    : SettingsPaneItemWidget(item, parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

QWidget *SettingsPaneBooleanWidget::createWidget() {
  LOG_PERF(0, "Enter");

  QWidget *placeholderWidget = new QWidget(this);

  QCheckBox *checkBox = new QCheckBox(placeholderWidget);
  checkBox->setObjectName("checkboxValue");
  checkBox->setCheckState(item().value().toBool() ? Qt::CheckState::Checked
                                                  : Qt::CheckState::Unchecked);

  QLabel *descriptionLabel =
      new QLabel(item().description(), placeholderWidget);
  descriptionLabel->setObjectName("descriptionLabel");
  descriptionLabel->setWordWrap(true);

  QHBoxLayout *innerLayout = new QHBoxLayout;
  innerLayout->setContentsMargins(0, 0, 0, 0);
  innerLayout->setSpacing(4);
  innerLayout->addWidget(checkBox);
  innerLayout->addWidget(descriptionLabel);
  innerLayout->addStretch();

  QLabel *nameLabel = new QLabel(item().name(), placeholderWidget);
  nameLabel->setObjectName("nameLabel");

  QVBoxLayout *outerLayout = new QVBoxLayout(placeholderWidget);
  outerLayout->addWidget(nameLabel);
  outerLayout->addLayout(innerLayout);

  connect(checkBox, &QCheckBox::checkStateChanged, this,
          &SettingsPaneBooleanWidget::onStateChanged);

  LOG_PERF(0, "Exit");

  return placeholderWidget;
}

void SettingsPaneBooleanWidget::save() const {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  configManager.setBoolean(key, item().value().toBool());

  LOG_PERF(0, "Exit");
}

void SettingsPaneBooleanWidget::onStateChanged(Qt::CheckState state) {
  LOG_PERF(0, "Enter");

  item().setValue(state == Qt::CheckState::Checked);
  save();

  LOG_PERF(0, "Exit");
}

void SettingsPaneBooleanWidget::load() {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  const bool value = configManager.getBoolean(key);
  item().setValue(value);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
