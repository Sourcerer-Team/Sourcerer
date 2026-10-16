#include "widgets/SettingsPaneLongWidget.h"

#include <Log.h>

#include <QLabel>
#include <QSpinBox>
#include <QVBoxLayout>

#include <limits>

#define THREAD_ID "SettingsPaneLongWidget"

namespace GuiManager {

namespace Widgets {

SettingsPaneLongWidget::SettingsPaneLongWidget(
    const ConfigManager::ConfigItem &item, QWidget *parent)
    : SettingsPaneItemWidget(item, parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

QWidget *SettingsPaneLongWidget::createWidget() {
  LOG_PERF(0, "Enter");

  QWidget *placeholderWidget = new QWidget(this);
  placeholderWidget->setSizePolicy(QSizePolicy::Policy::Expanding,
                                   QSizePolicy::Minimum);

  QLabel *nameLabel = new QLabel(item().name(), placeholderWidget);
  nameLabel->setObjectName("nameLabel");
  nameLabel->setWordWrap(true);

  QLabel *descriptionLabel =
      new QLabel(item().description(), placeholderWidget);
  descriptionLabel->setObjectName("descriptionLabel");
  descriptionLabel->setWordWrap(true);

  QSpinBox *longWidget = new QSpinBox(placeholderWidget);
  longWidget->setObjectName("longSpinBox");
  longWidget->setRange(std::numeric_limits<int>::min(),
                       std::numeric_limits<int>::max());
  longWidget->setValue(item().value().toLongLong());

  QVBoxLayout *layout = new QVBoxLayout(placeholderWidget);
  layout->addWidget(nameLabel);
  if (!item().description().isEmpty())
    layout->addWidget(descriptionLabel);
  layout->addWidget(longWidget);

  connect(longWidget, &QSpinBox::valueChanged, this,
          &SettingsPaneLongWidget::onValueChanged);

  LOG_PERF(0, "Exit");

  return placeholderWidget;
}

void SettingsPaneLongWidget::load() {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  const qlonglong value = configManager.getLong(key);
  item().setValue(QVariant::fromValue<qlonglong>(value));

  LOG_PERF(0, "Exit");
}

void SettingsPaneLongWidget::save() const {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const ConfigManager::ConfigKey key = configManager.getKey(item().key());
  configManager.setLong(key, item().value().toLongLong());

  LOG_PERF(0, "Exit");
}

void SettingsPaneLongWidget::onValueChanged(int value) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Value is {}", value);

  item().setValue(value);
  save();

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
