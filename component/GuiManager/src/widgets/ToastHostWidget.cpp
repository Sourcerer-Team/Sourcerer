// MIT License
// Copyright (c) 2026 Sourcerer


#include "widgets/ToastHostWidget.h"

#include <Log.h>

#include <QApplication>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QRegion>
#include <QSizePolicy>
#include <QTimer>
#include <QVBoxLayout>

#ifdef Q_OS_LINUX
#include <QWindow>
#endif

#define THREAD_ID "ToastHostWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

ToastHostWidget::ToastHostWidget()
    : OverlayWidget{nullptr, ALPHA}, _yOffset(0),
      _position{ToastPosition::BottomLeft},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint |
                 Qt::Widget);

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

  // Calculate accumulated height of existing toasts (not including title bar
  // offset)
  int existingToastsHeight = 0;
  for (const ToastWidget *existingToast : _toasts) {
    if (existingToast && existingToast != toast && existingToast->isVisible()) {
      existingToastsHeight += existingToast->height() + toastSpacing;
    }
  }

  // Calculate global position based on parent widget, not ToastHostWidget
  const QWidget *parent = parentWidget();
  if (!parent) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Walk up to find the actual top-level window (MainWindow)
  const QWidget *topLevel = parent;
  while (topLevel->parentWidget() != nullptr) {
    topLevel = topLevel->parentWidget();
  }

  QPoint topLevelGlobalPos = topLevel->mapToGlobal(QPoint(0, 0));

  int xPos = topLevelGlobalPos.x();
  int yPos = topLevelGlobalPos.y();

  LOG_DEBUG("ToastHostWidget::addToLayout - Toast size: {}x{}, TopLevel global "
            "pos: ({}, {}), TopLevel size: {}x{}",
            toast->width(), toast->height(), topLevelGlobalPos.x(),
            topLevelGlobalPos.y(), topLevel->width(), topLevel->height());

  if (_position == ToastPosition::Top) {
    xPos += (topLevel->width() - toast->width()) / 2;
    yPos += -toast->height() + existingToastsHeight;
  } else if (_position == ToastPosition::Bottom) {
    xPos += (topLevel->width() - toast->width()) / 2;
    yPos += topLevel->height() - existingToastsHeight;
  } else if (_position == ToastPosition::BottomLeft) {
    xPos += -toast->width();
    yPos += topLevel->height() - toast->height() - existingToastsHeight - 16;
  } else if (_position == ToastPosition::TopLeft) {
    xPos += -toast->width();
    yPos += existingToastsHeight + 16;
  } else if (_position == ToastPosition::BottomRight) {
    xPos += topLevel->width();
    yPos += topLevel->height() - toast->height() - existingToastsHeight - 16;
  } else if (_position == ToastPosition::TopRight) {
    xPos += toast->width();
    yPos += 16;
  }

  LOG_DEBUG("ToastHostWidget::addToLayout - Final toast position: ({}, {})",
            xPos, yPos);

  // Set target position before animation
  toast->move(xPos, yPos);

  // Animate the toast widget appearance
  animateWidget(toast);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::animateWidget(QWidget *widget) const {
  LOG_PERF(0, "Enter");

  // Walk up to find the actual top-level window (MainWindow)
  const QWidget *parent = parentWidget();
  if (!parent) {
    widget->show();
    LOG_PERF(0, "Exit");
    return;
  }

  // Since toasts are now top-level windows, position them manually
  // Calculate position based on existing toasts and position preference
  const int toastSpacing = 8; // 8 pixels spacing between toasts

  // Calculate accumulated height of existing toasts (not including title bar
  // offset)
  int existingToastsHeight = 0;
  for (const ToastWidget *existingToast : _toasts) {
    if (existingToast && existingToast != widget &&
        existingToast->isVisible()) {
      existingToastsHeight += existingToast->height() + toastSpacing;
    }
  }

  const QWidget *topLevel = parent;
  while (topLevel->parentWidget() != nullptr) {
    topLevel = topLevel->parentWidget();
  }

  QPoint topLevelGlobalPos = topLevel->mapToGlobal(QPoint(0, 0));

  int targetX = 0;
  int targetY = 0;

  if (_position == ToastPosition::Top) {
    targetX = topLevelGlobalPos.x() + widget->pos().x();
    targetY = existingToastsHeight + 16;
  } else if (_position == ToastPosition::Bottom) {
    targetX = topLevelGlobalPos.x() + widget->pos().x();
    targetY = topLevelGlobalPos.y() + topLevel->height() - widget->height() -
              existingToastsHeight - 16;
  } else if (_position == ToastPosition::BottomLeft) {
    targetX = topLevelGlobalPos.x() + 16;
    targetY = topLevelGlobalPos.y() + topLevel->height() - widget->height() -
              existingToastsHeight - 16;
  } else if (_position == ToastPosition::TopLeft) {
    targetX = topLevelGlobalPos.x() + 16;
    targetY = topLevelGlobalPos.y() + existingToastsHeight + 16;
  } else if (_position == ToastPosition::BottomRight) {
    targetX = topLevelGlobalPos.x() + topLevel->width() - widget->width() - 16;
    targetY = topLevelGlobalPos.y() + topLevel->height() - widget->height() -
              existingToastsHeight - 16;
  } else if (_position == ToastPosition::TopRight) {
    targetX = topLevelGlobalPos.x() + topLevel->width() - widget->width() - 16;
    targetY = topLevelGlobalPos.y() + existingToastsHeight + 16;
  }

#ifdef Q_OS_LINUX
  widget->move(targetX, targetY);
#else
  const int targetWidth = widget->width();
  const int targetHeight = widget->height();

  const int startX = topLevelGlobalPos.x() + widget->x();
  const int startY = topLevelGlobalPos.y() + widget->y();

  widget->move(startX, startY);

  const qlonglong toastAnimationDuration =
      _configManager.getLong(ConfigManager::ConfigKey::ToastAnimationDuration);

  const auto animation = new QPropertyAnimation(widget, "geometry");
  animation->setDuration(toastAnimationDuration); // ms

  if (_position == ToastPosition::TopLeft ||
      _position == ToastPosition::BottomLeft) {
    animation->setStartValue(QRect(startX, targetY, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else if (_position == ToastPosition::TopRight ||
             _position == ToastPosition::BottomRight) {
    animation->setStartValue(QRect(startX, targetY, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else if (_position == ToastPosition::Top) {
    animation->setStartValue(QRect(targetX,
                                   topLevelGlobalPos.y() - targetHeight,
                                   targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else {
    animation->setStartValue(QRect(targetX,
                                   topLevelGlobalPos.y() + topLevel->height(),
                                   targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  }

  animation->setEasingCurve(QEasingCurve::OutCubic);

  animation->start(QAbstractAnimation::DeleteWhenStopped);
#endif

  widget->show();

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

  // Platform-specific window configuration
  toast->setParent(nullptr);

#ifdef Q_OS_LINUX
  // Linux-specific: Use Qt::ToolTip to bypass X11 window manager positioning
  // Qt::ToolTip allows multiple windows to coexist (unlike Qt::Popup which is
  // exclusive) and bypasses window manager positioning like Qt::Popup does.
  toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                        Qt::WindowStaysOnTopHint |
                        Qt::X11BypassWindowManagerHint);
#else
  // Windows and macOS: Use standard Qt::Tool flag
  toast->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                        Qt::WindowStaysOnTopHint);
#endif

  toast->setWindowModality(Qt::NonModal);
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

  // Platform-specific window configuration
  toast->setParent(nullptr);

#ifdef Q_OS_LINUX
  // Linux-specific: Use Qt::ToolTip to bypass X11 window manager positioning
  // Qt::ToolTip allows multiple windows to coexist (unlike Qt::Popup which is
  // exclusive) and bypasses window manager positioning like Qt::Popup does.
  toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint |
                        Qt::WindowStaysOnTopHint |
                        Qt::X11BypassWindowManagerHint);
#else
  // Windows and macOS: Use standard Qt::Tool flag
  toast->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                        Qt::WindowStaysOnTopHint |
                        Qt::X11BypassWindowManagerHint);
#endif

  toast->setWindowModality(Qt::NonModal);
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

bool ToastHostWidget::showing() const {
  return false;
}

void ToastHostWidget::setYOffset(const int offset) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Y offset is {}.", offset);

  _yOffset = offset;

  move(x(), _yOffset);

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::adjustToParent() {
  LOG_PERF(0, "Enter");

  const auto *p = parentWidget();
  if (!p) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Keep the host invisible; toast windows are positioned independently.
  setGeometry(0, 0, 1, 1);
  hide();

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");

  // Don't call the base class (OverlayWidget) implementation which resizes to
  // match parent Skip directly to QWidget::resizeEvent
  QWidget::resizeEvent(event);

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

#ifdef Q_OS_LINUX
  // Linux-specific: For bottom positions, recalculate positions from bottom up
  if (_position == ToastPosition::Bottom ||
      _position == ToastPosition::BottomLeft ||
      _position == ToastPosition::BottomRight) {

    // Get parent and top-level window for position calculation
    const QWidget *parent = parentWidget();
    if (parent) {
      const QWidget *topLevel = parent;
      while (topLevel->parentWidget() != nullptr) {
        topLevel = topLevel->parentWidget();
      }

      QPoint topLevelGlobalPos = topLevel->mapToGlobal(QPoint(0, 0));
      const int toastSpacing = 8;

      // Position each toast from the newest (bottom) to oldest (top)
      // For each toast, calculate existingToastsHeight as the sum of all toasts
      // BELOW it
      for (int i = _toasts.size() - 1; i >= 0; --i) {
        ToastWidget *currentToast = _toasts[i];
        if (!currentToast || !currentToast->isVisible()) {
          continue;
        }

        // Calculate existingToastsHeight: sum of heights of all toasts below
        // this one
        int existingToastsHeight = 0;
        for (int j = _toasts.size() - 1; j > i; --j) {
          ToastWidget *belowToast = _toasts[j];
          if (belowToast && belowToast->isVisible()) {
            existingToastsHeight += belowToast->height() + toastSpacing;
          }
        }

        // Calculate positions using the exact same formula as addToLayout
        int xPos = topLevelGlobalPos.x();

        // Calculate X position based on alignment
        if (_position == ToastPosition::BottomRight) {
          xPos += topLevel->width() - currentToast->width() - 16;
        } else if (_position == ToastPosition::BottomLeft) {
          xPos += 16;
        } else {
          xPos += (topLevel->width() - currentToast->width()) / 2;
        }

        // Calculate Y position: same formula as addToLayout (for bottom
        // positioning)
        int yPos = topLevelGlobalPos.y() + topLevel->height() -
                   existingToastsHeight - currentToast->height() - 16;

        QWindow *window = currentToast->windowHandle();
        if (window) {
          window->setPosition(xPos, yPos);
        }
        currentToast->move(xPos, yPos);
        currentToast->raise();
      }
    }
  } else {
    // Top positions: Recalculate all positions
    for (ToastWidget *existingToast : _toasts) {
      if (existingToast && existingToast->isVisible()) {
        addToLayout(existingToast);
      }
    }
  }
#else
  // Windows/macOS: Reposition all remaining toasts using original logic
  for (ToastWidget *existingToast : _toasts) {
    if (existingToast && existingToast->isVisible()) {
      addToLayout(existingToast);
    }
  }
#endif

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

  const QWidget *parent = parentWidget();
  if (!parent) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool willFit = (parent->height() - _yOffset) >= totalToastHeight;

  LOG_PERF(0, "Exit");

  return willFit;
}

void ToastHostWidget::hideAllToasts() {
  LOG_PERF(0, "Enter");

  // Hide toasts from _toasts vector
  for (ToastWidget *toast : _toasts) {
    if (toast && toast->isVisible()) {
      toast->hide();
    }
  }

  // Hide toasts from _sharedToasts vector
  for (const QSharedPointer<ToastWidget> &toast : _sharedToasts) {
    if (toast && toast->isVisible()) {
      toast->hide();
    }
  }

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::showAllToasts() {
  LOG_PERF(0, "Enter");

  // Show toasts from _toasts vector
  for (ToastWidget *toast : _toasts) {
    if (toast && !toast->isVisible()) {
      toast->show();
    }
  }

  // Show toasts from _sharedToasts vector
  for (const QSharedPointer<ToastWidget> &toast : _sharedToasts) {
    if (toast && !toast->isVisible()) {
      toast->show();
    }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
