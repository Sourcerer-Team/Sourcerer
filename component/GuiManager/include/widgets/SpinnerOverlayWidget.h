#pragma once

#include "OverlayWidget.h"

#include <WaitingSpinnerWidget.h>

#include <QLabel>

namespace GuiManager::Widgets {

class SpinnerOverlayWidget final : public OverlayWidget {
public:
  explicit SpinnerOverlayWidget(QWidget *parent)
      : OverlayWidget(parent), _spinner(new WaitingSpinnerWidget(this)),
        _labelStatus(new QLabel(this)) {
    _labelStatus->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

    QPalette palette = _labelStatus->palette();
    palette.setColor(QPalette::WindowText, Qt::white);
    _labelStatus->setPalette(palette);

    _spinner->setColor(Qt::white);

    layout()->addWidget(_spinner);
    layout()->addWidget(_labelStatus);
  }

  ~SpinnerOverlayWidget() override { _spinner->stop(); }

  bool showing() const override { return _spinner->isSpinning(); }

  void setText(const QString &text) const { _labelStatus->setText(text); }

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