#include "widgets/BlurredWidget.h"
#include <QDebug>
#include <QEvent>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#pragma comment(lib, "dwmapi.lib")

struct ACCENT_POLICY {
  int nAccentState;
  int nFlags;
  int nColor;
  int nAnimationId;
};

struct WINCOMPATTRDATA {
  int attribute;
  PVOID pData;
  ULONG dataSize;
};

enum ACCENT_STATE {
  ACCENT_DISABLED = 0,
  ACCENT_ENABLE_BLURBEHIND = 3,
  ACCENT_ENABLE_ACRYLICBLURBEHIND = 4
};

BOOL setWindowCompositionAttribute(const HWND hwnd, WINCOMPATTRDATA *data) {
  using SetWindowCompositionAttributeFunc =
      BOOL(WINAPI *)(HWND, WINCOMPATTRDATA *);
  static auto func =
      reinterpret_cast<SetWindowCompositionAttributeFunc>(GetProcAddress(
          GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute"));
  return func && func(hwnd, data);
}
#endif

#ifdef Q_OS_MAC
#include <objc/objc-runtime.h>
#endif

#ifdef Q_OS_LINUX
#include <QX11Info>
#include <X11/Xatom.h>
#endif

namespace GuiManager::Widgets {

BlurredWidget::BlurredWidget(QWidget *parent) : QWidget(parent) {
  //setWindowFlags(Qt::FramelessWindowHint | Qt::Window);
  setAttribute(Qt::WA_TranslucentBackground);
  setAttribute(Qt::WA_StyledBackground, false);

  if (parent) {
    parent->installEventFilter(this);
  }
}

void BlurredWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  enablePlatformBlur();
}

bool BlurredWidget::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() &&
      (event->type() == QEvent::Resize || event->type() == QEvent::Move)) {
    setGeometry(parentWidget()->rect());
  }

  return QWidget::eventFilter(watched, event);
}

void BlurredWidget::enablePlatformBlur() {
#ifdef Q_OS_WIN
  enableWindowsBlur();
#elif defined(Q_OS_MAC)
  enableMacBlur();
#elif defined(Q_OS_LINUX)
  enableKDEBlur();
#else
  qDebug() << "Blur not supported on this platform.";
#endif
}

#ifdef Q_OS_WIN
void BlurredWidget::enableWindowsBlur() const {
  const auto hwnd = reinterpret_cast<HWND>(winId());

  ACCENT_POLICY policy = {};
  policy.nAccentState = ACCENT_ENABLE_ACRYLICBLURBEHIND;
  policy.nFlags = 2;
  policy.nColor = 0x99FFFFFF; // ARGB (alpha=0x99 for ~60% opacity)

  WINCOMPATTRDATA data = {19, &policy, sizeof(policy)};
  setWindowCompositionAttribute(hwnd, &data);
}
#endif

#ifdef Q_OS_MAC
void BlurredWidget::enableMacBlur() const {
  id nsview = (id)winId();
  Class NSVisualEffectViewClass = (Class)objc_getClass("NSVisualEffectView");
  id blurView = ((id(*)(id, SEL))objc_msgSend)((id)NSVisualEffectViewClass,
                                               sel_registerName("alloc"));
  blurView = ((id(*)(id, SEL))objc_msgSend)(
      blurView, sel_registerName("initWithFrame:"),
      ((id(*)(id, SEL))objc_msgSend)(nsview, sel_registerName("bounds")));
  ((void (*)(id, SEL, NSUInteger))objc_msgSend)(
      blurView, sel_registerName("setMaterial:"),
      0); // NSVisualEffectMaterialAppearanceBased
  ((void (*)(id, SEL, NSUInteger))objc_msgSend)(
      blurView, sel_registerName("setBlendingMode:"), 0); // BehindWindow
  ((void (*)(id, SEL, NSUInteger))objc_msgSend)(
      blurView, sel_registerName("setState:"), 1); // Active
  ((void (*)(id, SEL, id, NSInteger, id))objc_msgSend)(
      nsview, sel_registerName("addSubview:positioned:relativeTo:"), blurView,
      -1, nil);
}
#endif

#ifdef Q_OS_LINUX
void BlurredWidget::enableKDEBlur() const {
  Atom atom =
      XInternAtom(QX11Info::display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
  if (atom == None)
    return;

  XRectangle rect;
  rect.x = 0;
  rect.y = 0;
  rect.width = width();
  rect.height = height();

  XChangeProperty(QX11Info::display(), winId(), atom, XA_CARDINAL, 32,
                  PropModeReplace, reinterpret_cast<unsigned char *>(&rect), 4);
}
#endif
} // namespace GuiManager::Widgets
