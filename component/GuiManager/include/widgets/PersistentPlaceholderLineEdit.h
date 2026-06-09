#pragma once

#include <QLineEdit>
#include <QPainter>

namespace GuiManager::Widgets {
class PersistentPlaceholderLineEdit final : public QLineEdit {
public:
  using QLineEdit::QLineEdit;

  explicit PersistentPlaceholderLineEdit(const QColor &placeholderColor,
                                         QWidget *parent = nullptr)
      : QLineEdit(parent), _placeholderColor(placeholderColor) {
    // Ensure the placeholder text is drawn initially
    connect(this, &QLineEdit::textChanged, this, [this]() { update(); });
  }

protected:
  void focusInEvent(QFocusEvent *event) override {
    QLineEdit::focusInEvent(event);
    update(); // ensure repaint
  }

  void paintEvent(QPaintEvent *event) override {
    QLineEdit::paintEvent(event);

    if (text().isEmpty() && hasFocus()) {
      QPainter painter(this);
      painter.setPen(_placeholderColor);
      painter.drawText(rect().adjusted(5, 0, -5, 0),
                       Qt::AlignVCenter | Qt::AlignHCenter, placeholderText());
    }
  }

private:
  QColor _placeholderColor;
};
} // namespace GuiManager::Widgets