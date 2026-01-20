#include "qvterminal.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QKeyEvent>
#include <QMenu>
#include <QPainter>
#include <QScrollBar>
#include <QStyleHints>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QTextCodec>
#else
#include <QStringDecoder>
#endif

#include <vt/vt100.h>

static const int xMargin = 6;
static const int yMargin = 0;

QVTerminal::QVTerminal(QWidget *parent) : QAbstractScrollArea(parent) {
  _device = Q_NULLPTR;

  setCursorPos(QPoint(0, 0));
  _cursorTimer.start(QGuiApplication::styleHints()->cursorFlashTime() / 2);
  _cvisible = true;
  connect(&_cursorTimer, &QTimer::timeout, this, &QVTerminal::toggleCursor);

  _echo = false;
  _crlf = false;
  _state = QVTerminal::Text;

  QVTCharFormat format;
  QFont font;
  font.setFamily("monospace");
  font.setStyleHint(QFont::Monospace);
  font.setPointSize(13);
  format.setFont(font);
  format.setForeground(QColor(187, 187, 187));
  format.setBackground(QColor(0x23, 0x26, 0x29));
  setFormat(format);

  _layout = new QVTLayout();

  _pasteAction = new QAction("Paste");
  _pasteAction->setShortcut(QKeySequence("Ctrl+V"));
  connect(_pasteAction, &QAction::triggered, this, &QVTerminal::paste);
  addAction(_pasteAction);

  _clearAction = new QAction("Clear all");
  connect(_clearAction, &QAction::triggered, this, &QVTerminal::clear);
  addAction(_clearAction);

  _vt = new VT100(this);
  _useFormaValue_Y = false;
  _viewportRowCountOverride = 0;

  viewport()->setCursor(QCursor(Qt::IBeamCursor));
}

QVTerminal::~QVTerminal() {}

void QVTerminal::setViewportRowCountOverride(int rows) {
  _viewportRowCountOverride = rows > 0 ? rows : 0;
}

void QVTerminal::setIODevice(QIODevice *device) {
  _device = device;
  if (_device) {
    connect(_device, &QIODevice::readyRead, this, &QVTerminal::read);
    read();
  }
}

void QVTerminal::appendData(const QByteArray &data) {
  QString text;

  setUpdatesEnabled(false);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
  QTextCodec *textCodec = QTextCodec::codecForName("UTF-8");
  QString dataString = textCodec->toUnicode(data);
#else
  QString dataString = QStringDecoder(QStringDecoder::Utf8)(data);
#endif

  QString::const_iterator it = dataString.cbegin();
  while (it != dataString.cend()) {
    QChar c = *it;
    switch (_state) {
    case QVTerminal::Text:
      if (c.toLatin1() == 0x1B) {
        appendString(text);
        text.clear();
        _state = QVTerminal::Escape;
      } else if (c == '\n') {
        appendString(text);
        text.clear();
        _layout->appendLine();

        setCursorPos(0, _cursorPos.y() + 1);
      } else if (c == '\r') {
        appendString(text);
        text.clear();
        setCursorPos(0, _cursorPos.y());
      } else if (c == '\b') {
        if (_cursorPos.x()) {
          appendString(text);
          text.clear();
          setCursorPos(_cursorPos.x() - 1, _cursorPos.y());
        }
      } else if (c.isPrint()) {
        text.append(c);
      }
      break;

    case QVTerminal::Escape:
      _formatValue = 0;
      _formatValue_Y = 0;
      _useFormaValue_Y = false;
      if (c == '[') {
        _state = QVTerminal::Format;
      } else if (c == '(') {
        _state = QVTerminal::ResetFont;
      }
      break;

    case QVTerminal::Format:
      if (c >= '0' && c <= '9') {
        if (_useFormaValue_Y) {
          _formatValue_Y = _formatValue_Y * 10 + (c.cell() - '0');
        } else {
          _formatValue = _formatValue * 10 + (c.cell() - '0');
        }
      } else {
        _useFormaValue_Y = false;
        if (c == ';' || c == '?') // Format continuation
        {
          _formatValue_Y = 0;
          _useFormaValue_Y = true;
          _state = QVTerminal::Format;
        } else if (c == 'h') {
          // DEC Private Mode Set - typically cursor visibility
          // ESC[?25h - show cursor
          if (_formatValue == 25) {
            _cursorTimer.start();
            _cvisible = true;
          }
          _state = QVTerminal::Text;
        } else if (c == 'l') {
          // DEC Private Mode Reset - typically cursor visibility
          // ESC[?25l - hide cursor
          if (_formatValue == 25) {
            _cursorTimer.stop();
            _cvisible = false;
          }
          _state = QVTerminal::Text;
        } else if (c == 'm') {
          if (_formatValue == 0) // reset format
          {
            _curentFormat = _format;
          } else if (_formatValue == 4) // underline
          {
            _curentFormat.font().setUnderline(true);
          } else if (_formatValue == 7) // reverse
          {
            QColor foreground = _curentFormat.foreground();
            _curentFormat.setForeground(_curentFormat.background());
            _curentFormat.setBackground(foreground);
          } else if ((_formatValue >= 30 && _formatValue <= 37) ||
                     (_formatValue >= 90 && _formatValue <= 97)) // foreground
          {
            _curentFormat.setForeground(
                vt100color(static_cast<char>(_formatValue % 10) + '0'));
          } else if ((_formatValue >= 30 && _formatValue <= 37) ||
                     (_formatValue >= 100 && _formatValue <= 107)) // background
          {
            _curentFormat.setBackground(
                vt100color(static_cast<char>(_formatValue % 10) + '0'));
          }

          _state = QVTerminal::Text;
        } else if (c >= 'A' && c <= 'D') // Cursor command
        {
          // move at least one char
          if (!_formatValue) {
            _formatValue++;
          }

          switch (c.toLatin1()) {
          case 'A': // up
            setCursorPos(_cursorPos.x(),
                         qMax(_cursorPos.y() - _formatValue, 0));
            break;

          case 'B': // down
            setCursorPos(_cursorPos.x(), qMin(_cursorPos.y() + _formatValue,
                                              _layout->lineCount() - 1));
            break;

          case 'C': // right
            setCursorPos(_cursorPos.x() + _formatValue, _cursorPos.y());
            break;

          case 'D': // left
            setCursorPos(qMax(_cursorPos.x() - _formatValue, 0),
                         _cursorPos.y());
            break;

          default:
            break;
          }
          _state = QVTerminal::Text;
        } else if (c == 'H') {
          // Cursor positioning uses 1-based indexing in VT100
          // Convert to 0-based for internal use
          // VT100 coordinates are viewport-relative, not buffer-absolute
          int row = _formatValue > 0 ? _formatValue - 1 : 0;
          int col = _formatValue_Y > 0 ? _formatValue_Y - 1 : 0;

          // Calculate viewport size in rows based on widget dimensions
          int viewportRows = _viewportRowCountOverride;
          if (viewportRows <= 0) {
            viewportRows = viewport()->size().height() / _ch;
          }
          if (viewportRows < 1) {
            viewportRows = 1;
          }

          // The viewport origin is based on the current scroll position
          // Row 1 in the terminal is the first visible row in the viewport
          int viewportOrigin = verticalScrollBar()->value() / _ch;
          row += viewportOrigin;

          // Ensure row is not negative
          row = qMax(0, row);
          row = qMin(row, _layout->lineCount() - 1);

          setCursorPos(col, row);
          _state = QVTerminal::Text;
        } else if (c == 'G') {
          int row = _cursorPos.y();
          int col = _formatValue > 0 ? _formatValue - 1 : 0;
          setCursorPos(col, row);
          _state = QVTerminal::Text;
        } else if (c == 'J') {
          switch (_formatValue) {
          case 0:
            // Clear from cursor to end of display
            removeStringFromCursor(RIGHT_DIRECT);
            // Clear all lines below current line
            for (int i = _cursorPos.y() + 1; i < _layout->lineCount(); ++i) {
              QVTLine &line = _layout->lineAt(i);
              QVTChar termChar('\x20', _curentFormat);
              for (int j = 0; j < line.size(); ++j) {
                line.replace(termChar, j);
              }
            }
            break;

          case 1:
            // Clear from beginning of display to cursor
            // Clear all lines above current line
            for (int i = 0; i < _cursorPos.y(); ++i) {
              QVTLine &line = _layout->lineAt(i);
              QVTChar termChar('\x20', _curentFormat);
              for (int j = 0; j < line.size(); ++j) {
                line.replace(termChar, j);
              }
            }
            // Clear from start of current line to cursor
            removeStringFromCursor(LEFT_DIRECT);
            break;

          case 2:
            // Clear entire display
            clear();
            break;

          default:
            qDebug() << __FUNCTION__ << "unknown J function:" << _formatValue;
            break;
          }
          _state = QVTerminal::Text;
        } else if (c == 'K') {
          switch (_formatValue) {
          case 0:
            // Erase from cursor to end of line
            removeStringFromCursor(RIGHT_DIRECT);
            break;

          case 1:
            // Erase from beginning of line to cursor
            removeStringFromCursor(LEFT_DIRECT);
            break;

          case 2:
            // Erase entire line
            removeStringFromCursor(LEFT_DIRECT);
            removeStringFromCursor(RIGHT_DIRECT);
            break;

          default:
            qDebug() << __FUNCTION__ << "unknown K function:" << _formatValue;
            break;
          }
          _state = QVTerminal::Text;
        } else if (c == 'P') {
          removeStringFromCursor(LEFT_DIRECT, _formatValue);
          removeStringFromCursor(RIGHT_DIRECT);
          _state = QVTerminal::Text;
        } else if (c == 'X') {
          // ECH - Erase Character
          // Erase n characters from cursor position (replace with spaces)
          const int count = _formatValue > 0 ? _formatValue : 1;
          removeStringFromCursor(RIGHT_DIRECT, count);
          _state = QVTerminal::Text;
        } else {
          _state = QVTerminal::Text;
        }
      }
      break;

    case QVTerminal::ResetFont:
      _curentFormat = _format;
      _state = QVTerminal::Text;
      break;
    }
    it++;
  }
  appendString(text);

  if (viewport()->height() > 0) {
    int viewportHeight = _viewportRowCountOverride * _ch;
    int cursorHeight = (_cursorPos.y() + 1) * _ch;
    int newMax = qMax(0, cursorHeight - viewportHeight) + yMargin * 2;
    verticalScrollBar()->setRange(0, newMax);
    verticalScrollBar()->setValue(newMax);
  }

  setUpdatesEnabled(true);
  update();
}

void QVTerminal::paste() {
  QByteArray data;
  data.append(QApplication::clipboard()->text().toUtf8());
  writeData(data);
}

void QVTerminal::clear() {
  setUpdatesEnabled(false);
  _layout->clear();
  setCursorPos(0, 0);

  // Reset scrollbar since buffer is now empty
  verticalScrollBar()->setRange(0, 0);
  verticalScrollBar()->setValue(0);

  setUpdatesEnabled(true);
}

QColor QVTerminal::vt100color(char c) {
  switch (c) {
  case '1':
    return QColor(Qt::red);

  case '2':
    return QColor(Qt::green);

  case '3':
    return QColor(Qt::yellow);

  case '4':
    return QColor(Qt::blue);

  case '5':
    return QColor(Qt::magenta);

  case '6':
    return QColor(Qt::cyan);

  case '7':
    return QColor(Qt::white);

  default:
    return QColor(Qt::black);
  }
}

void QVTerminal::read() {
  if (!_device) {
    return;
  }
  if (_device->isReadable()) {
    appendData(_device->readAll());
  }
}

void QVTerminal::appendString(const QString &str) {
  foreach (QChar c, str) {
    QVTChar termChar(c, _curentFormat);
    _layout->lineAt(_cursorPos.y()).replace(termChar, _cursorPos.x());
    setCursorPos(_cursorPos.x() + 1, _cursorPos.y());
  }
}

void QVTerminal::removeStringFromCursor(Direction direction, int len) {
  // size limitation
  int remove_size = 0;

  if (len < 0) {
    len = INT_MAX;
  }

  if (direction > 0) {
    // right direction
    remove_size = qMin(static_cast<long long>(len),
                       _layout->lineAt(_cursorPos.y()).size() - _cursorPos.x());
  } else {
    // left direction
    remove_size = qMin(len, _cursorPos.x());
  }

  // remove operation
  QVTChar termChar('\x20', _curentFormat);
  int offset = 0;
  for (int i = 0; i < remove_size; ++i) {
    if (direction < 0) {
      offset = -i;
    } else {
      offset = i;
    }
    _layout->lineAt(_cursorPos.y()).replace(termChar, _cursorPos.x() + offset);
  }
}

void QVTerminal::toggleCursor() {
  _cvisible = !_cvisible;
  viewport()->update();
}

void QVTerminal::setCursorPos(int x, int y) {
  setCursorPos(QPoint(x, y));
}

void QVTerminal::setCursorPos(QPoint cursorPos) {
  if (cursorPos != _cursorPos) {
    _cursorPos = cursorPos;
    _layout->lineAt(_cursorPos.y())
        .resize(_cursorPos.x(), _format); // ensure line exists
    emit cursorMoved(cursorPos);
  }
}

QPoint QVTerminal::cursorPos() const {
  return _cursorPos;
}

QPoint QVTerminal::posToCursor(const QPoint &cursorPos) const {
  return QPoint((cursorPos.x() - xMargin) / _cw,
                (cursorPos.y() - yMargin + verticalScrollBar()->value()) / _ch);
}

bool QVTerminal::crlf() const {
  return _crlf;
}

void QVTerminal::setCrlf(bool crlf) {
  _crlf = crlf;
}

void QVTerminal::writeData(const QByteArray &data) {
  if (_device && _device->isWritable()) {
    _device->write(data);
  }
  if (_echo) {
    appendData(data);
  }
}

bool QVTerminal::echo() const {
  return _echo;
}

void QVTerminal::setEcho(bool echo) {
  _echo = echo;
}

const QVTCharFormat &QVTerminal::format() const {
  return _format;
}

void QVTerminal::setFormat(const QVTCharFormat &format) {
  _format = format;
  _curentFormat = format;

  QFontMetrics fm(_format.font());
  _cw = fm.boundingRect('M').width();
  _ch = fm.height();
}

void QVTerminal::keyPressEvent(QKeyEvent *event) {
  QByteArray data;
  QString text = event->text();

  if (text == "\r") {
    if (_crlf) {
      data.append("\r");
    }
    data.append("\n");
  } else {
    data.append(
        _vt->dataFromKey(event->text(), event->key(), event->modifiers()));
  }

  writeData(data);

  // QAbstractScrollArea::keyPressEvent(event);
}

void QVTerminal::paintEvent(QPaintEvent *paintEvent) {
  Q_UNUSED(paintEvent);

  QPainter p(viewport());

  p.setPen(QPen());
  p.fillRect(viewport()->rect(), QColor(0x23, 0x26, 0x29));

  p.translate(QPoint(xMargin, -verticalScrollBar()->value() + yMargin));
  p.setBrush(_format.background());
  p.setFont(_format.font());

  int firstLine = verticalScrollBar()->value() / _ch;
  int lastLine = viewport()->size().height() / _ch + firstLine + 1;
  if (lastLine > _layout->lineCount()) {
    lastLine = _layout->lineCount();
  }

  QPoint pos(0, firstLine * _ch);
  for (int l = firstLine; l < lastLine; l++) // render only visible lines
  {
    const QVTLine &line = _layout->lineAt(l);

    pos.setY(pos.y() + _ch);
    pos.setX(0);
    for (int c = 0; c < line.size(); c++) {
      const QVTChar &vtChar = line.chars()[c];
      QColor foreground = vtChar.format().foreground();
      QColor background = vtChar.format().background();
      bool inverted = false;

      // select range
      if (!_endSelectPos.isNull()) {
        if (l > _startSelectPos.y() && l < _endSelectPos.y()) {
          inverted = true;
        }
        if (l == _startSelectPos.y()) {
          if (l == _endSelectPos.y()) {
            inverted = (c >= _startSelectPos.x() && c <= _endSelectPos.x());
          } else {
            inverted = (c >= _startSelectPos.x());
          }
        } else if (l == _endSelectPos.y()) {
          inverted = (c <= _endSelectPos.x());
        }
      }

      if (inverted) {
        qSwap(foreground, background);
      }

      // draw background
      if (background != _format.background()) {
        p.fillRect(QRect(pos, QSize(_cw, -_ch)), background);
      }

      // draw foreground
      p.setPen(foreground);
      p.drawText(QRect(pos, QSize(_cw, -_ch)).normalized(), Qt::AlignCenter,
                 QString(vtChar.c()));

      pos.setX(pos.x() + _cw);
    }
  }

  // Draw cursor with proper coordinate system (already in translated space)
  if (_cvisible) {
    p.fillRect(QRect(_cursorPos.x() * _cw, _cursorPos.y() * _ch, _cw, _ch),
               QColor(187, 187, 187, 187));
  }
}

void QVTerminal::resizeEvent(QResizeEvent *event) {
  Q_UNUSED(event)
  verticalScrollBar()->setPageStep(_ch * 10);
  verticalScrollBar()->setSingleStep(_ch);

  // Recalculate scrollbar range based on new viewport size
  int viewportHeight = _viewportRowCountOverride * _ch;
  int cursorHeight = (_cursorPos.y() + 1) * _ch;
  int newMax = qMax(0, cursorHeight - viewportHeight) + yMargin * 2;

  // Maintain scroll position relative to content
  int oldValue = verticalScrollBar()->value();
  verticalScrollBar()->setRange(0, newMax);
  verticalScrollBar()->setValue(qMin(oldValue, newMax));
}

void QVTerminal::focusInEvent(QFocusEvent *event) {
  QAbstractScrollArea::focusInEvent(event);
  _cursorTimer.start();
}

void QVTerminal::focusOutEvent(QFocusEvent *event) {
  QAbstractScrollArea::focusOutEvent(event);
  _cursorTimer.stop();
  _cvisible = false;
  viewport()->update();
}

void QVTerminal::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    _endSelectPos = QPoint();
    _startCursorSelectPos = posToCursor(event->pos());
    setMouseTracking(true);
  }
  if (event->button() == Qt::MiddleButton) {
    if (QApplication::clipboard()->supportsSelection()) {
      QByteArray data;
      data.append(
          QApplication::clipboard()->text(QClipboard::Selection).toUtf8());
      writeData(data);
    }
  }
  QAbstractScrollArea::mousePressEvent(event);
}

void QVTerminal::mouseMoveEvent(QMouseEvent *event) {
  if (!_startCursorSelectPos.isNull()) {
    _endSelectPos = posToCursor(event->pos());
    if ((_startCursorSelectPos.y() > _endSelectPos.y()) ||
        (_startCursorSelectPos.y() == _endSelectPos.y() &&
         _startCursorSelectPos.x() > _endSelectPos.x())) {
      _startSelectPos = posToCursor(event->pos());
      _endSelectPos = _startCursorSelectPos;
    } else {
      _startSelectPos = _startCursorSelectPos;
      _endSelectPos = posToCursor(event->pos());
    }
    viewport()->update();
  }
}

void QVTerminal::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    if (_startCursorSelectPos == _endSelectPos) {
      _startSelectPos = QPoint();
      _endSelectPos = QPoint();
    }
    _startCursorSelectPos = QPoint();
    viewport()->update();
    setMouseTracking(false);
  }
  QAbstractScrollArea::mouseReleaseEvent(event);
}

#ifndef QT_NO_CONTEXTMENU
void QVTerminal::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);
  _pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
  menu.addAction(_pasteAction);
  menu.addAction(_clearAction);
  menu.exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU

bool QVTerminal::viewportEvent(QEvent *event) {
  return QAbstractScrollArea::viewportEvent(event);
}
