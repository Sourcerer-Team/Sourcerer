#pragma once

#include "MainWindow.h"
#include "WindowFrame.h"

#include <QMainWindow>
#include <QMargins>

namespace GuiManager {

class WindowsFrame : public WindowFrame {
public:
  explicit WindowsFrame(MainWindow *window);
  virtual ~WindowsFrame() {}

  // WindowFrame interface
public:
  void init();
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);

private:
  MainWindow *_window;
  QMargins _margins;
  QMargins _frames;
  bool _justMaximized;
};

} // namespace GuiManager
