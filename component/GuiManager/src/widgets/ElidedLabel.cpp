#include "widgets/ElidedLabel.h"

namespace GuiManager {
namespace Widgets {

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent), _textElideMode(Qt::ElideRight) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ElidedLabel::setTextElideMode(const Qt::TextElideMode mode) {
  _textElideMode = mode;
}

void ElidedLabel::resizeEvent(QResizeEvent *event) {
  QLabel::resizeEvent(event);

  const QString elidedText =
      fontMetrics().elidedText(text(), _textElideMode, width());
  setText(elidedText);
}

} // namespace Widgets
} // namespace GuiManager