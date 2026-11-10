// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/ElidedLabel.h"

#include <QSizePolicy>

namespace GuiManager {
namespace Widgets {

ElidedLabel::ElidedLabel(QWidget *parent)
    : QLabel(parent), _textElideMode(Qt::ElideRight) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setMinimumWidth(0);
  setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
}

void ElidedLabel::setTextElideMode(const Qt::TextElideMode mode) {
  _textElideMode = mode;
  updateElidedText();
}

void ElidedLabel::setFullText(const QString &text) {
  _fullText = text;
  setToolTip(text);
  setStatusTip(text);
  setAccessibleName(text);
  updateElidedText();
}

void ElidedLabel::updateElidedText() {
  const int availableWidth = contentsRect().width();
  const QString visibleText =
      availableWidth > 0
          ? fontMetrics().elidedText(_fullText, _textElideMode, availableWidth)
          : _fullText;
  QLabel::setText(visibleText);
}

void ElidedLabel::resizeEvent(QResizeEvent *event) {
  QLabel::resizeEvent(event);
  updateElidedText();
}

} // namespace Widgets
} // namespace GuiManager