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
  _savedCursorPos = QPoint(0, 0);
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

  _copyAction = new QAction("Copy");
  _copyAction->setShortcut(QKeySequence("Ctrl+C"));
  connect(_copyAction, &QAction::triggered, this, &QVTerminal::copy);
  addAction(_copyAction);

  _clearAction = new QAction("Clear all");
  connect(_clearAction, &QAction::triggered, this, [this]() {
      clear();
      emit cleared();
  });
  addAction(_clearAction);

  _vt = new VT100(this);
  _useFormaValue_Y = false;
  _viewportRowCountOverride = 0;

  viewport()->setCursor(QCursor(Qt::IBeamCursor));

  // Enable keyboard input and prevent Tab from switching focus
  setFocusPolicy(Qt::StrongFocus);
  viewport()->setFocusPolicy(Qt::StrongFocus);
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

  // Calculate and cache viewport origin once for this entire batch
  // This ensures consistent coordinate mapping throughout the batch
  int viewportRows = _viewportRowCountOverride;
  if (viewportRows <= 0) {
    viewportRows = viewport()->size().height() / _ch;
  }
  if (viewportRows < 1) {
    viewportRows = 1;
  }
  int currentLineCount = _layout->lineCount();
  if (currentLineCount <= viewportRows) {
    _cachedViewportOrigin = 0; // Small buffer: direct absolute positioning
  } else {
    _cachedViewportOrigin =
        currentLineCount - viewportRows; // Large buffer: bottom-aligned
  }

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

        // Only append a new line if we're at the last line
        // Otherwise, just move to the existing next line
        if (_cursorPos.y() >= _layout->lineCount() - 1) {
          _layout->appendLine();
        }

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
      } else if (c == ']') {
        // OSC - Operating System Command (e.g., window title)
        _state = QVTerminal::OSC;
      } else if (c == '7') {
        // ESC 7 = Save cursor position
        _savedCursorPos = _cursorPos;
        _state = QVTerminal::Text;
      } else if (c == '8') {
        // ESC 8 = Restore cursor position
        setCursorPos(_savedCursorPos);
        _state = QVTerminal::Text;
      } else {
        // Unknown escape sequence, return to Text state
        _state = QVTerminal::Text;
      }
      break;

    case QVTerminal::OSC:
      // OSC sequences are terminated by BEL (0x07) or ST (ESC \)
      if (c == '\x07') {
        // BEL terminates OSC
        _state = QVTerminal::Text;
      } else if (c == '\\' && _formatValue == 0x1B) {
        // ST (ESC \) terminates OSC
        _state = QVTerminal::Text;
      }
      // Store last character to detect ST sequence
      _formatValue = c.unicode();
      break;

    case QVTerminal::Format:
      if (c >= '0' && c <= '9') {
        if (_useFormaValue_Y) {
          _formatValue_Y = _formatValue_Y * 10 + (c.cell() - '0');
        } else {
          _formatValue = _formatValue * 10 + (c.cell() - '0');
        }
      } else {
        if (c == ';' || c == '?') // Format continuation
        {
          _formatValue_Y = 0;
          _useFormaValue_Y = true;
          _state = QVTerminal::Format;
        } else if (c == 'h') {
          // DEC Private Mode Set - typically cursor visibility
          // ESC[?25h - show cursor
          if ((!_useFormaValue_Y && _formatValue == 25) ||
              (_useFormaValue_Y && _formatValue_Y == 25)) {
            _cursorTimer.start();
            _cvisible = true;
          }
          _state = QVTerminal::Text;
        } else if (c == 'l') {
          // DEC Private Mode Reset - typically cursor visibility
          // ESC[?25l - hide cursor
          if ((!_useFormaValue_Y && _formatValue == 25) ||
              (_useFormaValue_Y && _formatValue_Y == 25)) {
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
          } else if (_formatValue >= 30 &&
                     _formatValue <= 37) // standard foreground
          {
            _curentFormat.setForeground(
                vt100color(static_cast<char>(_formatValue % 10) + '0', false));
          } else if (_formatValue >= 90 &&
                     _formatValue <= 97) // bright foreground
          {
            _curentFormat.setForeground(
                vt100color(static_cast<char>(_formatValue % 10) + '0', true));
          } else if (_formatValue >= 40 &&
                     _formatValue <= 47) // standard background
          {
            _curentFormat.setBackground(
                vt100color(static_cast<char>(_formatValue % 10) + '0', false));
          } else if (_formatValue >= 100 &&
                     _formatValue <= 107) // bright background
          {
            _curentFormat.setBackground(
                vt100color(static_cast<char>(_formatValue % 10) + '0', true));
          }

          _state = QVTerminal::Text;
        } else if (c >= 'A' && c <= 'D') // Cursor command
        {
          // move at least one char
          if (!_formatValue) {
            _formatValue++;
          }

          switch (c.toLatin1()) {
          case 'A': { // up
            int newY = qMax(_cursorPos.y() - _formatValue, 0);
            setCursorPos(_cursorPos.x(), newY);
            break;
          }
          case 'B': { // down
            int newY =
                qMin(_cursorPos.y() + _formatValue, _layout->lineCount() - 1);
            setCursorPos(_cursorPos.x(), newY);
            break;
          }
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

          // Calculate viewport origin dynamically based on CURRENT buffer size
          // PowerShell's positioning commands expect origin to reflect buffer
          // state at command time
          int viewportRows = _viewportRowCountOverride;
          if (viewportRows <= 0) {
            viewportRows = viewport()->size().height() / _ch;
          }
          if (viewportRows < 1) {
            viewportRows = 1;
          }
          int currentLineCount = _layout->lineCount();
          int viewportOrigin = 0;
          if (currentLineCount > viewportRows) {
            viewportOrigin = currentLineCount - viewportRows;
          }

          row += viewportOrigin;

          // Ensure row is valid
          row = qMax(0, row);

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

          case 3:
            // Clear entire display and scrollback buffer (xterm extension)
            // For QVTerminal, treat this the same as case 2 (clear entire
            // display)
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
        if (c != ';' && c != '?') {
          _useFormaValue_Y = false;
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
    // Calculate viewport parameters
    int viewportRows = _viewportRowCountOverride;
    if (viewportRows <= 0) {
      viewportRows = viewport()->size().height() / _ch;
    }
    if (viewportRows < 1) {
      viewportRows = 1;
    }

    int viewportHeight = viewportRows * _ch;
    // Use buffer size instead of cursor position to ensure all content is
    // scrollable This fixes the issue where text written above cursor position
    // gets scrolled off
    int bufferHeight = _layout->lineCount() * _ch;
    int newMaxWithoutMargin = qMax(0, bufferHeight - viewportHeight);
    int newMax = qMax(0, newMaxWithoutMargin + yMargin * 2);
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

void QVTerminal::copy() {
  // Check if there's a selection
  if (_startSelectPos.isNull() || _endSelectPos.isNull() || 
      _startSelectPos == _endSelectPos) {
    return;
  }

  QString selectedText;
  
  // If selection is on a single line
  if (_startSelectPos.y() == _endSelectPos.y()) {
    if (_startSelectPos.y() < _layout->lineCount()) {
      const QVTLine &line = _layout->lineAt(_startSelectPos.y());
      selectedText = line.text(_startSelectPos.x(), 
                              _endSelectPos.x() - _startSelectPos.x());
    }
  } else {
    // Multi-line selection
    // Add text from the first line (from start position to end of line)
    if (_startSelectPos.y() < _layout->lineCount()) {
      const QVTLine &startLine = _layout->lineAt(_startSelectPos.y());
      selectedText += startLine.text(_startSelectPos.x());
      selectedText += "\n";
    }

    // Add text from middle lines
    for (int y = _startSelectPos.y() + 1; y < _endSelectPos.y(); ++y) {
      if (y < _layout->lineCount()) {
        selectedText += _layout->lineAt(y).text();
        selectedText += "\n";
      }
    }

    // Add text from the last line (from start to end position)
    if (_endSelectPos.y() < _layout->lineCount()) {
      const QVTLine &endLine = _layout->lineAt(_endSelectPos.y());
      selectedText += endLine.text(0, _endSelectPos.x());
    }
  }

  if (!selectedText.isEmpty()) {
    QApplication::clipboard()->setText(selectedText);
  }
}

void QVTerminal::clear() {
  setUpdatesEnabled(false);
  _layout->clear();
  setCursorPos(0, 0);

  // Reset scrollbar since buffer is now empty
  verticalScrollBar()->setRange(0, 0);
  verticalScrollBar()->setValue(0);

  setUpdatesEnabled(true);
  update();
}

QColor QVTerminal::vt100color(char c, bool bright) {
  QColor color;
  switch (c) {
  case '0':
    color = bright ? QColor(128, 128, 128) : QColor(Qt::black);
    break;
  case '1':
    color = QColor(Qt::red);
    break;
  case '2':
    color = QColor(Qt::green);
    break;
  case '3':
    color = QColor(Qt::yellow);
    break;
  case '4':
    color = QColor(Qt::blue);
    break;
  case '5':
    color = QColor(Qt::magenta);
    break;
  case '6':
    color = QColor(Qt::cyan);
    break;
  case '7':
    color = QColor(Qt::white);
    break;
  default:
    color = QColor(Qt::black);
    break;
  }

  // Make bright colors lighter (except black which is handled specially)
  if (bright && c != '0') {
    color = color.lighter(150);
  }

  return color;
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
    // Ensure the line exists and has enough capacity for cursor position
    // Don't truncate - only expand if needed
    QVTLine &line = _layout->lineAt(_cursorPos.y());
    if (line.size() < _cursorPos.x()) {
      line.resize(_cursorPos.x(), _curentFormat);
    }
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
  event->accept(); // Prevent Qt from processing this event further

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

bool QVTerminal::event(QEvent *e) {
  // Intercept Tab and Backtab before Qt's focus system handles them
  if (e->type() == QEvent::KeyPress) {
    QKeyEvent *keyEvent = static_cast<QKeyEvent *>(e);
    if (keyEvent->key() == Qt::Key_Tab || keyEvent->key() == Qt::Key_Backtab) {
      keyPressEvent(keyEvent);
      return true;
    }
  }
  return QAbstractScrollArea::event(e);
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
  
  // Enable paste if clipboard has content
  _pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());
  menu.addAction(_pasteAction);
  
  // Enable copy if there's selected text
  bool hasSelection = !_startSelectPos.isNull() && !_endSelectPos.isNull() && 
                      _startSelectPos != _endSelectPos;
  _copyAction->setEnabled(hasSelection);
  menu.addAction(_copyAction);
  
  menu.addAction(_clearAction);
  menu.exec(event->globalPos());
}
#endif // QT_NO_CONTEXTMENU

bool QVTerminal::viewportEvent(QEvent *event) {
  return QAbstractScrollArea::viewportEvent(event);
}
