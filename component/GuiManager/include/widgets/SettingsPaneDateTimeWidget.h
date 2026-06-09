#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QDateTime>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneDateTimeWidget : public SettingsPaneItemWidget {
  Q_OBJECT
public:
  SettingsPaneDateTimeWidget(const ConfigManager::ConfigItem &item,
                             QWidget *parent = nullptr);

  // SettingsPaneItemWidget interface
protected:
  QWidget *createWidget() override;
  void load() override;
  void save() const override;

private slots:
  void onDateTimeChanged(const QDateTime &dateTime);
};

} // namespace Widgets
} // namespace GuiManager
