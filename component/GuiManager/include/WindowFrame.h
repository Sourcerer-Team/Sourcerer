#pragma once

#include <QWidget>

namespace GuiManager {

class WindowFrame {
public:
  virtual ~WindowFrame() {}

  virtual void init() = 0;

  virtual bool nativeEvent(const QByteArray &eventType, void *message,
                           qintptr *result) = 0;
};

} // namespace GuiManager
