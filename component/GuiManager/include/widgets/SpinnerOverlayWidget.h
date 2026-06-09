#pragma once

#include "OverlayWidget.h"

#include <WaitingSpinnerWidget.h>

#include <QLabel>

namespace GuiManager::Widgets {

class SpinnerOverlayWidget final : public OverlayWidget {
public:
  explicit SpinnerOverlayWidget(QWidget *parent);
  ~SpinnerOverlayWidget() override;

  bool showing() const override { return _spinner->isSpinning(); }
  void setText(const QString &text) const { _labelStatus->setText(text); }
  void startSpinning() const;
  void stopSpinning() const;

private:
  WaitingSpinnerWidget *_spinner;
  QLabel *_labelStatus;
};

} // namespace GuiManager::Widgets