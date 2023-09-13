
#include "widgets/ToastHostWidget.h"

#include <Log.h>

#include <QSizePolicy>

#define THREAD_ID "ToastHostWidget"

#include <QTimer>
#include <QVBoxLayout>

namespace GuiManager {

namespace Widgets {

ToastHostWidget::ToastHostWidget()
    : QWidget{}, _position{ToastPosition::BottomLeft},
      _configManager(ConfigManager::ConfigManager::getInstance()), _yOffset(0) {
  LOG_PERF(0, "Enter");

  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::Widget);

  const int toastSpacing =
      _configManager.getLong(ConfigManager::ConfigKey::ToastSpacing);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(toastSpacing);
  layout->setContentsMargins(16, 16, 16, 16);
  layout->setSizeConstraint(QLayout::SetFixedSize);

  setLayout(layout);

  QTimer::singleShot(0, this, [this]() {
    _cachedGeometry = calculateGeometry();
    setGeometry(_cachedGeometry);
  });

  LOG_PERF(0, "Exit");
}

ToastHostWidget::~ToastHostWidget() {
  LOG_PERF(0, "Enter");

  destroyAllToasts();

  LOG_PERF(0, "Exit");
}

QRect ToastHostWidget::calculateGeometry() const {
  LOG_PERF(0, "Enter");

  const QSize sizeHint = ToastHostWidget::sizeHint();

  int x;
  int y;
  int width = sizeHint.width();
  int height = sizeHint.height();

  if (_position == ToastPosition::BottomLeft) {
    x = _parentGeometry.x();
    y = _parentGeometry.height() - sizeHint.height();
  } else if (_position == ToastPosition::TopLeft) {
    x = _parentGeometry.x();
    y = _parentGeometry.y() + _yOffset;
  } else if (_position == ToastPosition::TopRight) {
    x = _parentGeometry.x() + _parentGeometry.width() - sizeHint.width();
    y = _parentGeometry.y() + _yOffset;
  } else if (_position == ToastPosition::BottomRight) {
    x = _parentGeometry.x() + _parentGeometry.width() - sizeHint.width();
    y = _parentGeometry.height() - sizeHint.height();
  } else if (_position == ToastPosition::Top) {
    x = _parentGeometry.x() + (_parentGeometry.width() - sizeHint.width()) / 2;
    y = _parentGeometry.y() + _yOffset;
  } else {
    x = _parentGeometry.x() + (_parentGeometry.width() - sizeHint.width()) / 2;
    y = _parentGeometry.height() - sizeHint.height();
  }

  const QSize _maximumSize = _parentGeometry.size();

  x = qMax(x, 0);
  y = qMax(y, _yOffset);

  if (x + width > _maximumSize.width())
    width = _maximumSize.width() - x;

  if (y + height > _maximumSize.height())
    height = _maximumSize.height() - y;

  LOG_DEBUG("x = {}, y = {}, width = {}, height = {}.", x, y, width, height);

  LOG_PERF(0, "Exit");

  return QRect(x, y, width, height);
}

void ToastHostWidget::destroyAllToasts() {
  LOG_PERF(0, "Enter");

  for (int i = 0; i < _toasts.length(); ++i) {
    ToastWidget *toast = _toasts[i];
    if (!toast)
      continue;

    toast->stop();
  }

  _toasts.clear();

  while (!_waitingToasts.isEmpty()) {
    ToastWidget *toast = _waitingToasts.dequeue();
    if (!toast)
      continue;

    toast->stop();
  }

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::addToLayout(ToastWidget *toast) {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  QVBoxLayout *layout = qobject_cast<QVBoxLayout *>(ToastHostWidget::layout());
  if (!layout) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (_position == ToastPosition::Bottom ||
      _position == ToastPosition::BottomLeft ||
      _position == ToastPosition::BottomRight) {
    layout->insertWidget(0, toast, 0, Qt::AlignBottom);
  } else if (_position == ToastPosition::Top ||
             _position == ToastPosition::TopLeft ||
             _position == ToastPosition::TopRight) {
    layout->insertWidget(layout->count(), toast, 0, Qt::AlignTop);
  } else
    layout->insertWidget(layout->count(), toast, 0, Qt::AlignCenter);

  LOG_PERF(0, "Exit");
}

ToastHostWidget &ToastHostWidget::getInstance() {
  static ToastHostWidget instance;
  return instance;
}

void ToastHostWidget::addToast(ToastWidget *toast) {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int toastWidth =
      _configManager.getLong(ConfigManager::ConfigKey::ToastWidth);

  toast->setParent(this);
  toast->setMinimumWidth(toastWidth);
  toast->setMaximumWidth(toastWidth);

  if (toastWillFit(toast)) {
    _toasts += toast;
    addToLayout(toast);
  } else
    _waitingToasts.enqueue(toast);

  connect(toast, &ToastWidget::stopped, this, &ToastHostWidget::onToastStopped);

  QTimer::singleShot(0, this, [=]() {
    _cachedGeometry = calculateGeometry();
    setGeometry(_cachedGeometry);
    show();
  });

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::setPosition(ToastPosition position) {
  LOG_PERF(0, "Enter");

  _position = position;

  _cachedGeometry = calculateGeometry();
  setGeometry(_cachedGeometry);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::setYOffset(int offset) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Y offset is {}.", offset);

  _yOffset = offset;

  _cachedGeometry = calculateGeometry();
  setGeometry(_cachedGeometry);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::setParentSize(const QRect &geometry) {
  LOG_PERF(0, "Enter");

  _parentGeometry = geometry;
  _cachedGeometry = calculateGeometry();
  setGeometry(_cachedGeometry);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::onToastStopped(ToastWidget *toast) {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  _toasts.removeAll(toast);
  _waitingToasts.removeAll(toast);

  layout()->removeWidget(toast);

  delete toast;

  if (!_waitingToasts.isEmpty()) {
    ToastWidget *waitingToast = _waitingToasts.first();
    if (waitingToast) {
      if (toastWillFit(waitingToast)) {
        addToLayout(waitingToast);
        _waitingToasts.dequeue();
        _toasts += waitingToast;
      }
    } else
      _waitingToasts.dequeue();
  }

  if (_toasts.length() == 0)
    hide();

  QTimer::singleShot(0, this, [this]() {
    _cachedGeometry = calculateGeometry();
    setGeometry(_cachedGeometry);
  });

  LOG_PERF(0, "Exit");
}

bool ToastHostWidget::toastWillFit(ToastWidget *toast) const {
  LOG_PERF(0, "Enter");

  const int toastSpacing =
      _configManager.getLong(ConfigManager::ConfigKey::ToastSpacing);

  const QLayout *layout = ToastHostWidget::layout();
  const int layoutCount = layout->count();
  int spacing = 0;
  if (layoutCount > 0)
    spacing = toastSpacing;

  /*
  QLayout* toastLayout = toast->layout();
  if(toastLayout) {
    toast->show();
    toastLayout->invalidate();
    toast->hide();
  }
*/

  //toast->adjustSize();

  const QSize toastSizeHint = toast->sizeHint();
  QRect boundingGeometry = _cachedGeometry;

  if (_position == ToastPosition::Bottom ||
      _position == ToastPosition::BottomLeft ||
      _position == ToastPosition::BottomRight)
    boundingGeometry.adjust(0, -(toastSizeHint.height() + spacing) - 100, 0, 0);
  else
    boundingGeometry.adjust(0, 0, 0, toastSizeHint.height() + spacing + 100);

  QRect parentGeometry = _parentGeometry;
  parentGeometry.adjust(0, _yOffset, 0, 0);

  LOG_PERF(0, "Exit");

  return parentGeometry.contains(boundingGeometry);
}

} // namespace Widgets
} // namespace GuiManager
