#include "widgets/SpinnerOverlayWidget.h"

#include <Log.h>

#define THREAD_ID "SpinnerOverlayWidget"

#define ALPHA 160

namespace GuiManager::Widgets {

SpinnerOverlayWidget::SpinnerOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA), _spinner(new WaitingSpinnerWidget(this)),
      _labelStatus(new QLabel(this)) {
  _labelStatus->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  QPalette palette = _labelStatus->palette();
  palette.setColor(QPalette::WindowText, Qt::white);
  _labelStatus->setPalette(palette);

  _spinner->setColor(QColor(100, 200, 255));
  _spinner->setLineLength(3);
  _spinner->setLineWidth(2);

  layout()->addWidget(_spinner);
  layout()->addWidget(_labelStatus);
}

SpinnerOverlayWidget::~SpinnerOverlayWidget() {
  _spinner->stop();
}

void SpinnerOverlayWidget::startSpinning() const {
  if (_spinner && !_spinner->isSpinning())
    _spinner->start();
}

void SpinnerOverlayWidget::stopSpinning() const {
  if (_spinner && _spinner->isSpinning()) {
    _spinner->stop();
  }
}

} // namespace GuiManager::Widgets
