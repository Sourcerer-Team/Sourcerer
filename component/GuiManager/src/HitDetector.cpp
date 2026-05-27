#include "HitDetector.h"
#include "MainWindow.h"

#include <Log.h>
#include <QApplication>
#include <QCursor>
#include <QTimer>

#define THREAD_ID "HitDetector"

#define FIXED_WIDTH(gw) (gw->minimumWidth() >= gw->maximumWidth())
#define FIXED_HEIGHT(gw) (gw->minimumHeight() >= gw->maximumHeight())
#define FIXED_SIZE(gw) (FIXED_WIDTH(gw) && FIXED_HEIGHT(gw))

namespace GuiManager {

HitDetector::HitDetector(QWidget *window)
    : _window(window), _pixelRatio(1.0), _lastMargin(Margin::HTNOWHERE),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void HitDetector::init() {
  LOG_PERF(0, "Enter");

  if (!_window) {
    LOG_PERF(0, "Exit");
    return;
  }

  _pixelRatio = _window->devicePixelRatioF();

  // Use a timer to continuously poll cursor position instead of relying on
  // events This bypasses event filter chains and works even when overlays are
  // present
  auto *timer = new QTimer(_window);
  connect(timer, &QTimer::timeout, this, [this]() {
    if (!_window || !_window->isVisible()) {
      return;
    }

    // Check cursor position against window edges
    const QPoint globalPos = QCursor::pos();
    const HitDetector::Margin margin =
        ncHitTest(_window->frameGeometry(), qFloor(globalPos.x() * _pixelRatio),
                  qFloor(globalPos.y() * _pixelRatio));

    // Update cursor based on margin
    if (margin == HitDetector::Margin::HTNOWHERE) {
      _window->unsetCursor();
    } else {
      switch (margin) {
      case HitDetector::Margin::HTBOTTOM:
      case HitDetector::Margin::HTTOP:
        _window->setCursor(Qt::SizeVerCursor);
        break;
      case HitDetector::Margin::HTLEFT:
      case HitDetector::Margin::HTRIGHT:
        _window->setCursor(Qt::SizeHorCursor);
        break;
      case HitDetector::Margin::HTTOPLEFT:
      case HitDetector::Margin::HTBOTTOMRIGHT:
        _window->setCursor(Qt::SizeFDiagCursor);
        break;
      case HitDetector::Margin::HTTOPRIGHT:
      case HitDetector::Margin::HTBOTTOMLEFT:
        _window->setCursor(Qt::SizeBDiagCursor);
        break;
      default:
        break;
      }
    }
    _lastMargin = margin;
  });

  // Poll cursor position 20 times per second (50ms intervals)
  timer->start(50);
  qDebug() << "HitDetector: Started cursor polling timer";

  LOG_PERF(0, "Exit");
}

bool HitDetector::eventFilter(QObject *watched, QEvent *event) {
  // Only handle mouse press events from MainWindow
  if (watched != _window)
    return false;

  if (event->type() == QEvent::MouseButtonPress) {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton &&
        _lastMargin != Margin::HTNOWHERE) {
      // Convert margin to edges
      Qt::Edges edges;
      switch (_lastMargin) {
      case Margin::HTTOP:
        edges = Qt::TopEdge;
        break;
      case Margin::HTTOPRIGHT:
        edges = Qt::TopEdge | Qt::RightEdge;
        break;
      case Margin::HTRIGHT:
        edges = Qt::RightEdge;
        break;
      case Margin::HTBOTTOMRIGHT:
        edges = Qt::BottomEdge | Qt::RightEdge;
        break;
      case Margin::HTBOTTOM:
        edges = Qt::BottomEdge;
        break;
      case Margin::HTBOTTOMLEFT:
        edges = Qt::BottomEdge | Qt::LeftEdge;
        break;
      case Margin::HTLEFT:
        edges = Qt::LeftEdge;
        break;
      case Margin::HTTOPLEFT:
        edges = Qt::TopEdge | Qt::LeftEdge;
        break;
      default:
        edges = Qt::Edges();
      }

      if (edges != Qt::Edges()) {
        qDebug() << "HitDetector: MouseButtonPress with edges:" << edges;
        emit edgeHit(edges);
      }
    }
  }

  return false;
}

HitDetector::Margin HitDetector::ncHitTest(const QRect &frameGeometry, int x,
                                           int y) {
  if (!_window)
    return Margin::HTNOWHERE;

  const qlonglong borderWidth =
      _configManager.getLong(ConfigManager::ConfigKey::MainWindowBorderWidth);

  // Get the point coordinates for the hit test.
  const QPoint cursorPos =
      QPoint(qFloor(x / _pixelRatio), qFloor(y / _pixelRatio));

  // Determine if the hit test is for resizing. Default middle (1,1).
  int row = 1;
  int col = 1;

  // Determine if the point is at the top or bottom of the window.
  if (cursorPos.y() < frameGeometry.top() + borderWidth) {
    row = 0; // top border.
  } else if (cursorPos.y() > frameGeometry.bottom() - borderWidth)
    row = 2; // bottom border.

  // Determine if the point is at the left or right of the window.
  if (cursorPos.x() < frameGeometry.left() + borderWidth)
    col = 0; // left border.
  else if (cursorPos.x() > frameGeometry.right() - borderWidth)
    col = 2; // right border.

  // Hit test (HTTOPLEFT, ... HTBOTTOMRIGHT)
  const HitDetector::Margin hitTests[3][3] = {
      {HitDetector::Margin::HTTOPLEFT, HitDetector::Margin::HTTOP,
       HitDetector::Margin::HTTOPRIGHT},
      {HitDetector::Margin::HTLEFT, HitDetector::Margin::HTNOWHERE,
       HitDetector::Margin::HTRIGHT},
      {HitDetector::Margin::HTBOTTOMLEFT, HitDetector::Margin::HTBOTTOM,
       HitDetector::Margin::HTBOTTOMRIGHT},
  };

  return hitTests[row][col];
}
} // namespace GuiManager
