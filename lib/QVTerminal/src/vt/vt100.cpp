#include "vt100.h"

VT100::VT100(QVTerminal *terminal) : VT(terminal) {}

QByteArray VT100::dataFromKey(const QString &text, int key,
                              Qt::KeyboardModifiers modifiers) {
  Q_UNUSED(modifiers)

  QByteArray data;

  switch (key) {
  case Qt::Key_Backspace:
    data.append('\x7f');
    break;
  case Qt::Key_Tab:
    data.append('\t');
    break;
  case Qt::Key_Return:
  case Qt::Key_Enter:
    data.append('\r');
    break;
  case Qt::Key_Escape:
    data.append('\x1b');
    break;
  case Qt::Key_Up:
    data.append("\x1b[A");
    break;
  case Qt::Key_Down:
    data.append("\x1b[B");
    break;
  case Qt::Key_Right:
    data.append("\x1b[C");
    break;
  case Qt::Key_Left:
    data.append("\x1b[D");
    break;
  case Qt::Key_Home:
    data.append("\x1b[H");
    break;
  case Qt::Key_End:
    data.append("\x1b[F");
    break;
  case Qt::Key_PageUp:
    data.append("\x1b[5~");
    break;
  case Qt::Key_PageDown:
    data.append("\x1b[6~");
    break;
  case Qt::Key_Insert:
    data.append("\x1b[2~");
    break;
  case Qt::Key_Delete:
    data.append("\x1b[3~");
    break;

  default:
    data.append(text.toLatin1());
    break;
  }

  return data;
}
