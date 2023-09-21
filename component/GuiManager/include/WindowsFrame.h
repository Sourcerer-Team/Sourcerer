#pragma once

#include "WindowFrame.h"

#include <QMainWindow>
#include <QMargins>

namespace GuiManager {

class WindowsFrame : public WindowFrame {
public:
  explicit WindowsFrame(QMainWindow *window);
  virtual ~WindowsFrame() {}

  // WindowFrame interface
public:
  void init();
  bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result);

private:
  QMainWindow *_window;
  QMargins _margins;
  QMargins _frames;
  bool _justMaximized;
};

} // namespace GuiManager
