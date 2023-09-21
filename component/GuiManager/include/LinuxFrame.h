#pragma once

#include "MainWindow.h"
#include "WindowFrame.h"

#include <ConfigManager.h>

#include <QObject>
#include <QPoint>
#include <QRect>

namespace GuiManager {

class LinuxFrame : public WindowFrame {
  Q_OBJECT
public:
  explicit LinuxFrame(MainWindow *window);
  virtual ~LinuxFrame() {}

  // WindowFrame interface
public:
  void init() override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  enum class Margin : int {
    Unknown = 0,
    HTNOWHERE,
    HTBOTTOMRIGHT,
    HTBOTTOM,
    HTBOTTOMLEFT,
    HTLEFT,
    HTTOPLEFT,
    HTTOP,
    HTTOPRIGHT,
    HTRIGHT
  };

  Margin ncHitTest(const QRect &frameGeometry, int x, int y);

private:
  MainWindow *_window;
  qreal _pixelRatio;
  Margin _lastClickMargin;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager
