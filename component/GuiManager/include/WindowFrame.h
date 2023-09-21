#pragma once

#include <QByteArray>
#include <QWidget>

namespace GuiManager {

class WindowFrame {
public:
  virtual ~WindowFrame() {}

  virtual void init(){};

  virtual bool nativeEvent(const QByteArray &eventType, void *message,
                           qintptr *result){ return false; }

  virtual bool eventFilter(QObject *watched, QEvent *event) { return false; }
};

} // namespace GuiManager
