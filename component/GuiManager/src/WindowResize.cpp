#include "WindowResize.h"

#include <QWidget>
#include <QWindow>

#ifdef Q_OS_MACOS
#include <QCursor>
#include <QGuiApplication>
#include <QPointer>
#include <QRect>
#include <QTimer>
#endif

namespace GuiManager {

namespace {

#ifdef Q_OS_MACOS
class PollingResizer final : public QObject {
public:
  PollingResizer(QWidget *window, Qt::Edges edges)
      : QObject(window), _window(window), _edges(edges),
        _startCursor(QCursor::pos()), _startGeometry(window->geometry()) {
    _timer.setInterval(8);
    connect(&_timer, &QTimer::timeout, this, [this]() { tick(); });
    _timer.start();
  }

private:
  void tick() {
    if (!_window || !(QGuiApplication::mouseButtons() & Qt::LeftButton)) {
      deleteLater();
      _timer.stop();
      return;
    }

    const QPoint delta = QCursor::pos() - _startCursor;
    const QSize minimum = _window->minimumSize().expandedTo(QSize(1, 1));
    QRect geometry = _startGeometry;

    if (_edges.testFlag(Qt::LeftEdge)) {
      const int left = qMin(geometry.left() + delta.x(),
                            geometry.right() - minimum.width() + 1);
      geometry.setLeft(left);
    }
    if (_edges.testFlag(Qt::RightEdge)) {
      geometry.setRight(qMax(geometry.right() + delta.x(),
                             geometry.left() + minimum.width() - 1));
    }
    if (_edges.testFlag(Qt::TopEdge)) {
      const int top = qMin(geometry.top() + delta.y(),
                           geometry.bottom() - minimum.height() + 1);
      geometry.setTop(top);
    }
    if (_edges.testFlag(Qt::BottomEdge)) {
      geometry.setBottom(qMax(geometry.bottom() + delta.y(),
                              geometry.top() + minimum.height() - 1));
    }

    if (geometry != _window->geometry()) {
      _window->setGeometry(geometry);
    }
  }

  QPointer<QWidget> _window;
  Qt::Edges _edges;
  QPoint _startCursor;
  QRect _startGeometry;
  QTimer _timer;
};
#endif

} // namespace

bool beginInteractiveResize(QWidget *window, Qt::Edges edges) {
  if (!window || edges == Qt::Edges()) {
    return false;
  }

  if (QWindow *handle = window->windowHandle()) {
    if (handle->startSystemResize(edges)) {
      return true;
    }
  }

#ifdef Q_OS_MACOS
  if (!window->isMaximized() && !window->isFullScreen()) {
    static QPointer<PollingResizer> active;
    if (!active) {
      active = new PollingResizer(window, edges);
    }
    return true;
  }
#endif

  return false;
}

} // namespace GuiManager
