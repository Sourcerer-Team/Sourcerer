
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

#ifdef Q_OS_LINUX
#include <QWindow>
#endif

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

  LOG_DEBUG("ToastHostWidget::addToLayout - Toast size: {}x{}, TopLevel global pos: ({}, {}), TopLevel size: {}x{}", 
            toast->width(), toast->height(), topLevelGlobalPos.x(), topLevelGlobalPos.y(), topLevel->width(), topLevel->height());

  // Adjust based on position preference
  if (_position == ToastPosition::BottomRight ||
      _position == ToastPosition::TopRight) {
    xPos += topLevel->width() - toast->width() - 16;  // 16px margin from right
  } else if (_position == ToastPosition::BottomLeft ||
             _position == ToastPosition::TopLeft) {
    xPos += 16;  // 16px margin from left
  } else {
    // Center horizontally
    xPos += (topLevel->width() - toast->width()) / 2;
  }

  if (_position == ToastPosition::Bottom ||
      _position == ToastPosition::BottomLeft ||
      _position == ToastPosition::BottomRight) {
    yPos += topLevel->height() - yOffset - toast->height() - 16;  // 16px margin from bottom
  } else {
    yPos += _yOffset + yOffset;
  }

  LOG_DEBUG("ToastHostWidget::addToLayout - Final toast position: ({}, {})", xPos, yPos);

#ifdef Q_OS_LINUX
  // Linux-specific: Direct window positioning to work around X11 window manager issues
  // Show the window first to ensure it has a window handle
  toast->show();
  
  // Position both the window handle and widget for redundancy
  QWindow *window = toast->windowHandle();
  if (window) {
    window->setPosition(xPos, yPos);
  }
  toast->move(xPos, yPos);
  
  toast->raise();
  toast->activateWindow();
  
  // Verify position after event loop processes (X11 WM may interfere)
  QTimer::singleShot(100, [toast, xPos, yPos]() {
    QWindow *window = toast->windowHandle();
    if (toast->x() != xPos || toast->y() != yPos) {
      toast->move(xPos, yPos);
      if (window) {
        window->setPosition(xPos, yPos);
      }
    }
  });
#else
  // Windows and macOS: Simple positioning works reliably
  toast->move(xPos, yPos);
  toast->show();
  toast->raise();
#endif

  //animateWidget(toast);

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

  // Walk up to find the actual top-level window (MainWindow)
  const QWidget *parent = parentWidget();
  if (!parent) {
    widget->show();
    LOG_PERF(0, "Exit");
    return;
  }
  
  const QWidget *topLevel = parent;
  while (topLevel->parentWidget() != nullptr) {
    topLevel = topLevel->parentWidget();
  }

  QPoint topLevelGlobalPos = topLevel->mapToGlobal(QPoint(0, 0));
  int startX = targetX;

  // Start position: fully outside based on position preference
  if (_position == ToastPosition::TopLeft ||
      _position == ToastPosition::BottomLeft) {
    // Slide in from the left
    startX = topLevelGlobalPos.x() - targetWidth;
  } else if (_position == ToastPosition::TopRight ||
             _position == ToastPosition::BottomRight) {
    // Slide in from the right
    startX = topLevelGlobalPos.x() + topLevel->width();
  } else if (_position == ToastPosition::Top) {
    // Slide in from the top (use targetX for horizontal position)
    widget->setGeometry(targetX, topLevelGlobalPos.y() - targetHeight, targetWidth, targetHeight);
  } else {
    // Slide in from the bottom
    widget->setGeometry(targetX, topLevelGlobalPos.y() + topLevel->height(), targetWidth, targetHeight);
  }

  // Set initial geometry for left/right animations
  if (_position == ToastPosition::TopLeft ||
      _position == ToastPosition::BottomLeft ||
      _position == ToastPosition::TopRight ||
      _position == ToastPosition::BottomRight) {
    widget->setGeometry(startX, targetY, targetWidth, targetHeight);
  }

  const qlonglong toastAnimationDuration =
      _configManager.getLong(ConfigManager::ConfigKey::ToastAnimationDuration);

  const auto animation = new QPropertyAnimation(widget, "geometry");
  animation->setDuration(toastAnimationDuration); // ms

  if (_position == ToastPosition::TopLeft ||
      _position == ToastPosition::BottomLeft) {
    animation->setStartValue(
        QRect(startX, targetY, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else if (_position == ToastPosition::TopRight ||
             _position == ToastPosition::BottomRight) {
    animation->setStartValue(
        QRect(startX, targetY, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else if (_position == ToastPosition::Top) {
    animation->setStartValue(
        QRect(targetX, topLevelGlobalPos.y() - targetHeight, targetWidth, targetHeight));
    animation->setEndValue(QRect(targetX, targetY, targetWidth, targetHeight));
  } else {
    animation->setStartValue(
        QRect(targetX, topLevelGlobalPos.y() + topLevel->height(), targetWidth, targetHeight));
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

  // Platform-specific window configuration
  toast->setParent(nullptr);
  
#ifdef Q_OS_LINUX
  // Linux-specific: Use Qt::ToolTip to bypass X11 window manager positioning
  // Qt::ToolTip allows multiple windows to coexist (unlike Qt::Popup which is exclusive)
  // and bypasses window manager positioning like Qt::Popup does.
  toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
#else
  // Windows and macOS: Use standard Qt::Tool flag
  toast->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
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
  const int toastHeight =
      _configManager.getLong(ConfigManager::ConfigKey::ToastHeight);

  // Platform-specific window configuration
  toast->setParent(nullptr);
  
#ifdef Q_OS_LINUX
  // Linux-specific: Use Qt::ToolTip to bypass X11 window manager positioning
  // Qt::ToolTip allows multiple windows to coexist (unlike Qt::Popup which is exclusive)
  // and bypasses window manager positioning like Qt::Popup does.
  toast->setWindowFlags(Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
#else
  // Windows and macOS: Use standard Qt::Tool flag
  toast->setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
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

  // Calculate the size needed for the toast host widget
  // It should be sized to fit the toasts, positioned in the corner
  const int toastWidth =
      _configManager.getLong(ConfigManager::ConfigKey::ToastWidth);
  const int hostWidth = toastWidth + 24;  // Add padding
  const int hostHeight = p->height() - _yOffset;

  // Position based on the configured position
  int xPos = 0;
  int yPos = _yOffset;

  switch (_position) {
  case ToastPosition::BottomRight:
  case ToastPosition::TopRight: {
    xPos = p->width() - hostWidth;
    break;
  }
  case ToastPosition::BottomLeft:
  case ToastPosition::TopLeft: {
    xPos = 0;
    break;
  }
  case ToastPosition::Top:
  case ToastPosition::Bottom: {
    xPos = (p->width() - hostWidth) / 2;
    break;
  }
  default: {
    xPos = p->width() - hostWidth;
    break;
  }
  }

  LOG_DEBUG("ToastHostWidget::adjustToParent - Parent: {}x{}, Position: {}, Setting geometry: x={}, y={}, w={}, h={}", 
            p->width(), p->height(), static_cast<int>(_position), xPos, yPos, hostWidth, hostHeight);

  // Set geometry to position in corner instead of filling parent
  setGeometry(xPos, yPos, hostWidth, hostHeight);
  raise();

  LOG_PERF(0, "Exit");
}

void ToastHostWidget::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");
  
  // Don't call the base class (OverlayWidget) implementation which resizes to match parent
  // Skip directly to QWidget::resizeEvent
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
      // For each toast, calculate yOffset as the sum of all toasts BELOW it
      for (int i = _toasts.size() - 1; i >= 0; --i) {
        ToastWidget *currentToast = _toasts[i];
        if (!currentToast || !currentToast->isVisible()) {
          continue;
        }
        
        // Calculate yOffset: sum of heights of all toasts below this one
        int yOffset = _yOffset;
        for (int j = _toasts.size() - 1; j > i; --j) {
          ToastWidget *belowToast = _toasts[j];
          if (belowToast && belowToast->isVisible()) {
            yOffset += belowToast->height() + toastSpacing;
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
        
        // Calculate Y position: same formula as addToLayout
        int yPos = topLevelGlobalPos.y() + topLevel->height() - yOffset - currentToast->height() - 16;
        
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
