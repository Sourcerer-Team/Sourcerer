#pragma once

#include "OverlayWidget.h"

#include <WaitingSpinnerWidget.h>

#include <QLabel>

namespace GuiManager::Widgets {

class SpinnerOverlayWidget : public OverlayWidget {
public:
  explicit SpinnerOverlayWidget(QWidget *parent)
      : OverlayWidget(parent), _spinner(new WaitingSpinnerWidget(this)),
        _labelStatus(new QLabel(this)) {
    setObjectName("spinnerOverlayWidget");

    _labelStatus->setObjectName("labelStatus");
    _labelStatus->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    _spinner->setObjectName("spinner");
    _spinner->setColor(_spinner->palette().color(QPalette::WindowText));

    layout()->addWidget(_spinner);
    layout()->addWidget(_labelStatus);

  }

  ~SpinnerOverlayWidget() override { _spinner->stop(); }

  void setText(const QString &t) const { _labelStatus->setText(t); }

  void startSpinning() const {
    if (_spinner && !_spinner->isSpinning())
      _spinner->start();
  }

  void stopSpinning() const {
    if (_spinner && _spinner->isSpinning()) {
      _spinner->stop();
    }
  }

private:
  WaitingSpinnerWidget *_spinner;
  QLabel *_labelStatus;
};
} // namespace GuiManager::Widgets