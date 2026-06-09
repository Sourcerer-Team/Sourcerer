#include "widgets/factories/SettingsPaneItemWidgetFactory.h"
#include "widgets/SettingsPaneBooleanWidget.h"
#include "widgets/SettingsPaneColorWidget.h"
#include "widgets/SettingsPaneDateTimeWidget.h"
#include "widgets/SettingsPaneLongWidget.h"
#include "widgets/SettingsPaneStringWidget.h"

#include <Log.h>

#define THREAD_ID "SettingsPaneItemWidgetFactory"

namespace GuiManager {

namespace Widgets {

namespace Factories {

SettingsPaneItemWidgetFactory &SettingsPaneItemWidgetFactory::getInstance() {
  static SettingsPaneItemWidgetFactory instance;
  return instance;
}

SettingsPaneItemWidget *
SettingsPaneItemWidgetFactory::create(const ConfigManager::ConfigItem &item,
                                      QWidget *parent) const {
  LOG_PERF(0, "Enter");

  SettingsPaneItemWidget *widget = nullptr;

  switch (item.type()) {
  case ConfigManager::ConfigKeyType::Boolean: {
    widget = new SettingsPaneBooleanWidget(item, parent);
    break;
  }
  case ConfigManager::ConfigKeyType::Color: {
    widget = new SettingsPaneColorWidget(item, parent);
    break;
  }
  case ConfigManager::ConfigKeyType::DateTime: {
    widget = new SettingsPaneDateTimeWidget(item, parent);
    break;
  }
  case ConfigManager::ConfigKeyType::Long: {
    widget = new SettingsPaneLongWidget(item, parent);
    break;
  }
  case ConfigManager::ConfigKeyType::String: {
    widget = new SettingsPaneStringWidget(item, parent);
    break;
  }
  case ConfigManager::ConfigKeyType::Unknown:
  default: {
    LOG_WARN("A widget for an unknown widget type requested.");
    break;
  }
  }

  LOG_PERF(0, "Exit)");

  return widget;
}

SettingsPaneItemWidgetFactory::SettingsPaneItemWidgetFactory() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit)");
}

SettingsPaneItemWidgetFactory::~SettingsPaneItemWidgetFactory() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit)");
}

} // namespace Factories
} // namespace Widgets
} // namespace GuiManager
