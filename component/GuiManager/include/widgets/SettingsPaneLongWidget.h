// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "widgets/SettingsPaneItemWidget.h"

#include <ConfigGroup.h>

#include <QWidget>

namespace GuiManager {

namespace Widgets {

class SettingsPaneLongWidget : public SettingsPaneItemWidget {
  Q_OBJECT
public:
  SettingsPaneLongWidget(const ConfigManager::ConfigItem &item,
                         QWidget *parent = nullptr);

  // SettingsPaneItemWidget interface
protected:
  QWidget *createWidget() override;
  void load() override;
  void save() const override;

private slots:
  void onValueChanged(int value);
};

} // namespace Widgets
} // namespace GuiManager
