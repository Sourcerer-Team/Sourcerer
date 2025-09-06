#pragma once

#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>

namespace GuiManager::Widgets {

void enableBlur(const WId wid);

class OverlayWidget : public QWidget {
public:
  explicit OverlayWidget(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

    enableBlur(winId());

    QColor defaultBackgroundColor = palette().color(QPalette::Window);
    defaultBackgroundColor.setAlpha(128);

    setBackgroundColor(defaultBackgroundColor);

    raise();

    adjustToParent();

    // Track parent resize/move events
    if (parent) {
      parent->installEventFilter(this);
    }

    _layout = new QVBoxLayout(this);
    _layout->setAlignment(Qt::AlignCenter);
    _layout->setContentsMargins(16, 16, 16, 16);
    _layout->setSpacing(32);
  }

  ~OverlayWidget() override = default;

  QVBoxLayout *getLayout() const { return _layout; }

  void setBackgroundColor(const QColor &color) {
    setAttribute(Qt::WA_StyledBackground, true);
    // QColor::name(QColor::HexArgb) preserves alpha channel
    setStyleSheet(
        QString("background-color: %1;").arg(color.name(QColor::HexArgb)));
  }

protected:
  bool event(QEvent *e) override {
    // Eat all events
    return true;
  }

  void resizeEvent(QResizeEvent *) override {
    if (parentWidget()) {
      resize(parentWidget()->size()); // always match parent size
    }
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched == parentWidget() &&
        (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
      adjustToParent();
    }

    return QWidget::eventFilter(watched, event);
  }

private:
  void adjustToParent() {
    if (auto *p = parentWidget()) {
      setGeometry(p->rect()); // fills entire parent
      raise();
    }
  }

private:
  QVBoxLayout *_layout;
};

} // namespace GuiManager::Widgets
