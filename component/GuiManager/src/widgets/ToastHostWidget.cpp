
#include "widgets/ToastHostWidget.h"

#include "Dispatcher.h"
#include "MainWindow.h"

#include <Log.h>

#include <QApplication>
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
    : OverlayWidget{nullptr, 0}, _yOffset(0),
      _position{ToastPosition::BottomRight},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::Widget);
  // Make host transparent to mouse events - individual toasts will be top-level
  // windows
  setAttribute(Qt::WA_TransparentForMouseEvents, true);

  // Set transparent background for the host widget
  setAttribute(Qt::WA_StyledBackground, true);
  setStyleSheet("background-color: transparent;");

  // Remove the opacity effect from OverlayWidget
  setGraphicsEffect(nullptr);

  setMouseTracking(true);

  // Hide toasts when application loses focus, show them again when regaining
  // focus
  connect(qApp, &QApplication::applicationStateChanged, this,
          [this](Qt::ApplicationState state) {
            if (state == Qt::ApplicationActive) {
              showAllToasts();
            } else {
              hideAllToasts();
            }
          });

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
  const int toastSpacing = 8; // 8 pixels spacing between toasts

  int yOffset = _yOffset; // Start with base offset, no extra padding

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
  toast->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                        Qt::Tool);
  toast->setAttribute(Qt::WA_TransparentForMouseEvents, false);

  toast->setFixedWidth(toastWidth);
  toast->setMinimumHeight(0);
  toast->setMaximumHeight(QWIDGETSIZE_MAX);
  // Don't set fixed height - let it size to content
  toast->layout()->activate();
  toast->updateGeometry();
  toast->adjustSize();

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
  toast->setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                        Qt::Tool);
  toast->setAttribute(Qt::WA_TransparentForMouseEvents, false);

  toast->setFixedWidth(toastWidth);
  toast->setMinimumHeight(0);
  toast->setMaximumHeight(QWIDGETSIZE_MAX);
  // Don't set fixed height - let it size to content
  toast->layout()->activate();
  toast->updateGeometry();
  toast->adjustSize();

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

  // Reposition all remaining toasts
  for (ToastWidget *existingToast : _toasts) {
    if (existingToast && existingToast->isVisible()) {
      addToLayout(existingToast);
    }
  }

  // Try to display waiting toasts that now have space
  while (!_waitingToasts.isEmpty()) {
    ToastWidget *waitingToast = _waitingToasts.first();
    if (!waitingToast) {
      _waitingToasts.dequeue();
      continue;
    }

    if (toastWillFit(waitingToast)) {
      _waitingToasts.dequeue();
      _toasts += waitingToast;
      addToLayout(waitingToast);
    } else {
      // If this toast doesn't fit, no subsequent toasts will fit either
      break;
    }
  }

  LOG_PERF(0, "Exit");
}

bool ToastHostWidget::toastWillFit(const ToastWidget *toast) const {
  LOG_PERF(0, "Enter");

  if (!toast) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const int toastSpacing =
      _configManager.getLong(ConfigManager::ConfigKey::ToastSpacing);

  // Calculate total height of currently displayed toasts
  int totalToastHeight = 0;
  for (const ToastWidget *existingToast : _toasts) {
    if (existingToast && existingToast->isVisible()) {
      totalToastHeight += existingToast->height();
      if (totalToastHeight > 0) {
        totalToastHeight += toastSpacing;
      }
    }
  }

  // Add height of the new toast being checked
  totalToastHeight += toast->height();

  // Check if total height fits within the host widget
  const bool willFit = height() >= totalToastHeight;

  LOG_PERF(0, "Exit");

  return willFit;
}

void ToastHostWidget::hideAllToasts() {
  LOG_PERF(0, "Enter");

  for (ToastWidget *toast : _toasts) {
    if (toast && toast->isVisible()) {
      toast->hide();
    }
  }

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::showAllToasts() {
  LOG_PERF(0, "Enter");

  for (ToastWidget *toast : _toasts) {
    if (toast && !toast->isVisible()) {
      toast->show();
    }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
