// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "OverlayWidget.h"

#include <ConfigManager.h>

#include <WaitingSpinnerWidget.h>

#include <QLabel>
#include <QTimer>

namespace GuiManager::Widgets {

class SpinnerOverlayWidget final : public OverlayWidget {
public:
  explicit SpinnerOverlayWidget(QWidget *parent);
  ~SpinnerOverlayWidget() override;

  bool showing() const override { return _spinner->isSpinning(); }
  void setText(const QString &text) const { _labelStatus->setText(text); }
  void startSpinning();
  void stopSpinning();

private:
  ConfigManager::ConfigManager &_configManager;
  QTimer *_singleShotTimer;
  WaitingSpinnerWidget *_spinner;
  QLabel *_labelStatus;
};

} // namespace GuiManager::Widgets