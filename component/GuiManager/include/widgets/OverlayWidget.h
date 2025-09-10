#pragma once

#include <QEvent>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

namespace GuiManager::Widgets {

class OverlayWidget : public QWidget {
public:
  explicit OverlayWidget(QWidget *parent, int alpha) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setAttribute(Qt::WA_NativeWindow);

    QColor defaultBackgroundColor = palette().color(QPalette::Window);
    defaultBackgroundColor.setAlpha(alpha); // semi-transparent

    setBackgroundColor(defaultBackgroundColor);

    raise();

    // Track parent resize/move events
    if (parent) {
      adjustToParent();
      parent->installEventFilter(this);
    }

    _layout = new QVBoxLayout(this);
    _layout->setAlignment(Qt::AlignCenter);
    _layout->setContentsMargins(16, 16, 16, 16);
    _layout->setSpacing(32);
  }

  ~OverlayWidget() override = default;

  void setParent(QWidget *parent) {
    QWidget::setParent(parent);

    // Track parent resize/move events
    if (parent) {
      adjustToParent();
      parent->installEventFilter(this);
    }
  }

  QVBoxLayout *getLayout() const { return _layout; }

  virtual bool showing() const = 0;

  void setBackgroundColor(const QColor &color) {
    setAttribute(Qt::WA_StyledBackground, true);
    // QColor::name(QColor::HexArgb) preserves alpha channel
    setStyleSheet(
        QString("background-color: %1;").arg(color.name(QColor::HexArgb)));
  }

protected:
  void resizeEvent(QResizeEvent *) override {
    if (parentWidget()) {
      resize(parentWidget()->size()); // always match parent size
    }
  }

  bool eventFilter(QObject *watched, QEvent *event) override {
    if (watched != parentWidget()) {
      return QWidget::eventFilter(watched, event);
    }

    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
      adjustToParent();
    } else if (event->type() == QEvent::Hide) {
      hide(); // hide overlay when parent hides
    } else if (event->type() == QEvent::Show) {
      if (showing()) {
        show();  // show overlay when parent shows
        raise(); // ensure it's on top
      }
    }

    return QWidget::eventFilter(watched, event);
  }

  void adjustToParent() {
    if (auto *p = parentWidget()) {
      setGeometry(p->rect()); // fills entire parent
      raise();
    }
  }

private:
  static QRect toGlobalRect(const QWidget *w, const QRect &r) {
    return QRect(w->mapToGlobal(r.topLeft()), w->mapToGlobal(r.bottomRight()));
  }

private:
  QVBoxLayout *_layout;
};

} // namespace GuiManager::Widgets
