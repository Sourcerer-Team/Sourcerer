// MIT License
// Copyright (c) 2026 Sourcerer


#pragma once

#include "widgets/ToastWidget.h"

#include <ConfigManager.h>

#include <QTimer>
#include <QWidget>

namespace GuiManager::Widgets {

class InfoToast : public ToastWidget {
  Q_OBJECT
public:
  explicit InfoToast(const QString &message, const QString &title = QString(),
                     bool stayForever = false, QWidget *parent = nullptr);
  ~InfoToast() override;

  // ToastWidget interface
public:
  void stop() override;

private:
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets