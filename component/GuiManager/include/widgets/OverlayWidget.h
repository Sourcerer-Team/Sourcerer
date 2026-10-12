#pragma once

#include <StylesheetManager.h>

#include <QApplication>
#include <QEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QVBoxLayout>
#include <QWidget>

namespace GuiManager::Widgets {

class OverlayWidget : public QWidget {
public:
  explicit OverlayWidget(QWidget *parent, const int alpha,
                         const bool nativeWindow = true)
      : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents, false);

#ifdef Q_OS_WIN
    if (nativeWindow) {
      setAttribute(Qt::WA_NativeWindow);
    }
#else
    Q_UNUSED(nativeWindow);
#endif

    const auto effect = new QGraphicsOpacityEffect(this);
    effect->setOpacity((1.0 / 255.0) *
                       alpha); // 0.0 = fully transparent, 1.0 = fully opaque
    setGraphicsEffect(effect);

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

  virtual ~OverlayWidget() override = default;

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

  static QColor overlayBackgroundColor() {
    switch (StylesheetManager::StylesheetManager::currentStylesheet()) {
    case StylesheetManager::StylesheetKey::SourcererLight:
      return QColor(255, 244, 224);
    case StylesheetManager::StylesheetKey::SourcererPurple:
      return QColor::fromString("#374659");
    default:
      return QColor::fromString("#384D59");
    }
  }

  static QPixmap overlayButtonIcon(const QString &iconPath) {
    const QColor iconColor =
        StylesheetManager::StylesheetManager::menuBarButtonIconColor();

    if (iconPath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
      return StylesheetManager::StylesheetManager::renderSvgToPixmap(iconPath,
                                                                     iconColor);
    }

    return StylesheetManager::StylesheetManager::recolorPixmap(
        QPixmap(iconPath), iconColor);
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

  virtual void adjustToParent() {
    if (const auto *p = parentWidget()) {
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
