
#include "widgets/ToastHostWidget.h"

#include "Dispatcher.h"
#include "MainWindow.h"

#include <Log.h>

#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QRegion>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#define THREAD_ID "ToastHostWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

ToastHostWidget::ToastHostWidget()
    : OverlayWidget{nullptr, ALPHA}, _yOffset(0),
      _position{ToastPosition::BottomRight},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::Widget);
  // Make host transparent to mouse events - individual toasts will be top-level windows
  setAttribute(Qt::WA_TransparentForMouseEvents, true);

  setMouseTracking(true);

  setPosition(_position);

  const int toastSpacing =
      _configManager.getLong(ConfigManager::ConfigKey::ToastSpacing);

  const auto layout_ = qobject_cast<QVBoxLayout *>(layout());
  if (!layout_) {
    LOG_PERF(0, "Exit");
    return;
  }

  layout_->setSpacing(toastSpacing);
  layout_->setContentsMargins(16, 8, 8, 16);

  LOG_PERF(0, "Exit");
}

ToastHostWidget::~ToastHostWidget() {
  LOG_PERF(0, "Enter");

  destroyAllToasts();

  LOG_PERF(0, "Exit");
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

void ToastHostWidget::addToLayout(ToastWidget *toast) const {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Since toasts are now top-level windows, position them manually
  // Calculate position based on existing toasts and position preference
  const int toastSpacing =
      _configManager.getLong(ConfigManager::ConfigKey::ToastSpacing);
  
  int yOffset = _yOffset + 16; // Start with base offset
  
  // Calculate y position based on existing toasts
  for (const ToastWidget *existingToast : _toasts) {
    if (existingToast && existingToast != toast && existingToast->isVisible()) {
      yOffset += existingToast->height() + toastSpacing;
    }
  }
  
  // Get the host widget's global position
  QPoint hostPos = mapToGlobal(QPoint(0, 0));
  int xPos = hostPos.x();
  int yPos = hostPos.y();
  
  // Adjust based on position preference
  if (_position == ToastPosition::BottomRight || 
      _position == ToastPosition::TopRight) {
    xPos += width() - toast->width() - 8;
  } else if (_position == ToastPosition::BottomLeft ||
             _position == ToastPosition::TopLeft) {
    xPos += 16;
  }
  
  if (_position == ToastPosition::Bottom ||
      _position == ToastPosition::BottomLeft ||
      _position == ToastPosition::BottomRight) {
    yPos += height() - yOffset - toast->height();
  } else {
    yPos += yOffset;
  }
  
  // Set the position for the top-level window
  toast->move(xPos, yPos);
  
  animateWidget(toast);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::animateWidget(QWidget *widget) const {
  LOG_PERF(0, "Enter");

  widget->show();

  const int targetX = widget->x();
  const int targetY = widget->y();
  const int targetWidth = widget->width();
  const int targetHeight = widget->height();

  widget->hide();

  // Start position: fully outside to the left
  if (_position == ToastPosition::TopLeft ||
      _position == ToastPosition::BottomLeft) {
    widget->setGeometry(-targetWidth, targetY, targetWidth, targetHeight);
  } else if (_position == ToastPosition::TopRight ||
             _position == ToastPosition::BottomRight) {
    widget->setGeometry(width(), targetY, targetWidth, targetHeight);
  } else if (_position == ToastPosition::Top) {
    widget->setGeometry(targetX, -targetHeight, targetWidth, targetHeight);
  } else {
    widget->setGeometry(targetX, height(), targetWidth, targetHeight);
  }

  const qlonglong toastAnimationDuration =
      _configManager.getLong(ConfigManager::ConfigKey::ToastAnimationDuration);

  const auto animation = new QPropertyAnimation(widget, "geometry");
  animation->setDuration(toastAnimationDuration); // ms

  if (_position == ToastPosition::TopLeft ||
      _position == ToastPosition::BottomLeft) {
    animation->setStartValue(
        QRect(-targetWidth, targetY, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else if (_position == ToastPosition::TopRight ||
             _position == ToastPosition::BottomRight) {
    animation->setStartValue(
        QRect(width(), targetY, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else if (_position == ToastPosition::Top) {
    animation->setStartValue(
        QRect(targetX, -targetHeight, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else {
    animation->setStartValue(
        QRect(targetX, height(), targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  }

  animation->setEasingCurve(QEasingCurve::OutCubic);

  widget->show();

  animation->start(QAbstractAnimation::DeleteWhenStopped);

  LOG_PERF(0, "Exit");
}

ToastHostWidget &ToastHostWidget::getInstance() {
  static SharedPtr instance = SharedPtr::create();
  return *instance;
}

void ToastHostWidget::addToast(ToastWidget *toast) {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int toastWidth =
      _configManager.getLong(ConfigManager::ConfigKey::ToastWidth);
  const int toastHeight =
      _configManager.getLong(ConfigManager::ConfigKey::ToastHeight);

  // Don't setParent - keep toast as independent top-level window
  // Just ensure window flags are set correctly
  toast->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  toast->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  
  toast->setMinimumWidth(toastWidth);
  toast->setMaximumWidth(toastWidth);
  toast->setMinimumHeight(toastHeight);
  toast->setMaximumHeight(toastHeight);
  toast->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

  if (toastWillFit(toast)) {
    _toasts += toast;
    addToLayout(toast);
  } else
    _waitingToasts.enqueue(toast);

  connect(toast, &ToastWidget::stopped, this, &ToastHostWidget::onToastStopped);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::addToast(const QSharedPointer<ToastWidget> &toast) {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int toastWidth =
      _configManager.getLong(ConfigManager::ConfigKey::ToastWidth);
  const int toastHeight =
      _configManager.getLong(ConfigManager::ConfigKey::ToastHeight);

  // Don't setParent - keep toast as independent top-level window
  // Just ensure window flags are set correctly
  toast->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  toast->setAttribute(Qt::WA_TransparentForMouseEvents, false);
  
  toast->setMinimumWidth(toastWidth);
  toast->setMaximumWidth(toastWidth);
  toast->setMinimumHeight(toastHeight);
  toast->setMaximumHeight(toastHeight);

  if (toastWillFit(toast.get())) {
    _sharedToasts += toast;
    addToLayout(toast.get());
  } else
    _waitingToasts.enqueue(toast.get());

  connect(toast.get(), &ToastWidget::stopped, this,
          &ToastHostWidget::onToastStopped);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::setPosition(ToastPosition position) {
  LOG_PERF(0, "Enter");

  _position = position;

  Qt::Alignment alignment = Qt::AlignHCenter | Qt::AlignVCenter;

  switch (_position) {
  case ToastPosition::BottomLeft: {
    alignment = Qt::AlignLeft | Qt::AlignBottom;
    break;
  }
  case ToastPosition::BottomRight: {
    alignment = Qt::AlignRight | Qt::AlignBottom;
    break;
  }
  case ToastPosition::TopLeft: {
    alignment = Qt::AlignTop | Qt::AlignLeft;
    break;
  }
  case ToastPosition::TopRight: {
    alignment = Qt::AlignRight | Qt::AlignTop;
    break;
  }
  case ToastPosition::Top: {
    alignment = Qt::AlignHCenter | Qt::AlignTop;
    break;
  }
  case ToastPosition::Bottom: {
    alignment = Qt::AlignHCenter | Qt::AlignBottom;
    break;
  }
  default: {
    LOG_ERROR(Log::ErrorId::Gui, "Internal error! Unsupported position type {}",
              static_cast<int>(_position));
  }
  }

  const auto layout_ = qobject_cast<QVBoxLayout *>(layout());
  if (!layout_) {
    LOG_PERF(0, "Exit");
    return;
  }

  layout_->setAlignment(alignment);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::setYOffset(const int offset) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Y offset is {}.", offset);

  _yOffset = offset;

  move(x(), _yOffset);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::onToastStopped(ToastWidget *toast) {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return;
  }

  const bool toDelete = _toasts.removeAll(toast) > 0;
  _sharedToasts.removeIf([&](const QSharedPointer<ToastWidget> &sharedToast) {
    return sharedToast.get() == toast;
  });

  _waitingToasts.removeAll(toast);

  layout()->removeWidget(toast);

  if (toDelete) {
    delete toast;
  }

  if (!_waitingToasts.isEmpty()) {
    if (ToastWidget *waitingToast = _waitingToasts.first()) {
      if (toastWillFit(waitingToast)) {
        addToLayout(waitingToast);
        _waitingToasts.dequeue();
        _toasts += waitingToast;
      }
    } else
      _waitingToasts.dequeue();
  }

  LOG_PERF(0, "Exit");
}

bool ToastHostWidget::toastWillFit(const ToastWidget *toast) const {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const auto layout_ = layout();
  const int toastSpacing =
      _configManager.getLong(ConfigManager::ConfigKey::ToastSpacing);

  int totalToastHeight = 0;
  for (int i = 0; i < layout_->count(); i++) {
    const QLayoutItem *layoutitem = layout_->itemAt(i);
    Q_ASSERT(layoutitem != nullptr);
    if (!layoutitem) {
      continue;
    }

    const QWidget *widget = layoutitem->widget();
    Q_ASSERT(widget != nullptr);
    if (!widget) {
      continue;
    }

    totalToastHeight += widget->rect().height();
    totalToastHeight += toastSpacing;
  }

  totalToastHeight += toast->rect().height();

  LOG_PERF(0, "Exit");

  return rect().height() >= totalToastHeight + toast->rect().height();
}

} // namespace GuiManager::Widgets
