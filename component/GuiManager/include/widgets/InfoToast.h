
#pragma once

#include "widgets/ToastWidget.h"

#include <ConfigManager.h>

#include <QTimer>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class InfoToast : public ToastWidget {
  Q_OBJECT
public:
  explicit InfoToast(const QString &message, QWidget *parent = nullptr);
  virtual ~InfoToast();

  // ToastWidget interface
public:
  void stop() override;

private:
  QTimer *_stopTimer;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Widgets
} // namespace GuiManager
