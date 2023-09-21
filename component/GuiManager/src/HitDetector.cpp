#include "HitDetector.h"
#include "MainWindow.h"

#include <Log.h>

#define THREAD_ID "HitDetector"

#define FIXED_WIDTH(gw) (gw->minimumWidth() >= gw->maximumWidth())
#define FIXED_HEIGHT(gw) (gw->minimumHeight() >= gw->maximumHeight())
#define FIXED_SIZE(gw) (FIXED_WIDTH(gw) && FIXED_HEIGHT(gw))

namespace GuiManager {

HitDetector::HitDetector(MainWindow *window)
    : _window(window), _pixelRatio(1.0),
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

  LOG_PERF(0, "Exit");
}

bool HitDetector::eventFilter(QObject *watched, QEvent *event) {
  if (watched != _window)
    return false;

  switch (event->type()) {
  case QEvent::MouseButtonPress: {
    QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
    if (mouseEvent->button() == Qt::LeftButton)
      emit edgeHit(_lastEdges);

    break;
  }
  case QEvent::MouseMove: {
    const QPoint currentPosition = QCursor::pos();
    const HitDetector::Margin currentMargin = ncHitTest(
        _window->frameGeometry(), qFloor(currentPosition.x() * _pixelRatio),
        qFloor(currentPosition.y() * _pixelRatio));

    switch (currentMargin) {
    case HitDetector::Margin::HTBOTTOM: {
      _window->setCursor(Qt::SizeVerCursor);
      _lastEdges = Qt::BottomEdge;
      break;
    }
    case HitDetector::Margin::HTRIGHT: {
      _window->setCursor(Qt::SizeHorCursor);
      _lastEdges = Qt::RightEdge;
      break;
    }
    case HitDetector::Margin::HTBOTTOMRIGHT: {
      _window->setCursor(Qt::SizeFDiagCursor);
      _lastEdges = Qt::BottomEdge | Qt::RightEdge;
      break;
    }
    case HitDetector::Margin::HTBOTTOMLEFT: {
      _window->setCursor(Qt::SizeBDiagCursor);
      _lastEdges = Qt::BottomEdge | Qt::LeftEdge;
      break;
    }
    case HitDetector::Margin::HTLEFT: {
      _window->setCursor(Qt::SizeHorCursor);
      _lastEdges = Qt::LeftEdge;
      break;
    }
    case HitDetector::Margin::HTTOPLEFT: {
      _window->setCursor(Qt::SizeFDiagCursor);
      _lastEdges = Qt::TopEdge | Qt::LeftEdge;
      break;
    }
    case HitDetector::Margin::HTTOP: {
      _window->setCursor(Qt::SizeVerCursor);
      _lastEdges = Qt::TopEdge;
      break;
    }
    case HitDetector::Margin::HTTOPRIGHT: {
      _window->setCursor(Qt::SizeBDiagCursor);
      _lastEdges = Qt::TopEdge | Qt::RightEdge;
      break;
    }
    case HitDetector::Margin::HTNOWHERE: {
      _window->unsetCursor();
      break;
    }
    default: {
      break;
    }
    }

    break;
  }
  default: {
    break;
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

  LOG_DEBUG("Border width is {} pixels.", borderWidth);

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
