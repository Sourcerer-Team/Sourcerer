#ifndef QVTERMINAL_H
#define QVTERMINAL_H

#include <QAbstractScrollArea>
#include <QAction>
#include <QTimer>
#include <QtMath>

#include <vt/vt.h>

#include "qvtlayout.h"

#include <limits.h>

struct TerminalGeometry {
  int cols = 80;
  int rows = 25;
  bool usedFallback = false;
};

class QVTerminal : public QAbstractScrollArea {
  Q_OBJECT

public:
  explicit QVTerminal(QWidget *parent = nullptr);
  ~QVTerminal() override;

  enum Direction {
    RIGHT_DIRECT = 1,
    LEFT_DIRECT = -1,
  };

  void setIODevice(QIODevice *device);

  // style
  const QVTCharFormat &format() const;
  void setFormat(const QVTCharFormat &format);

  // viewport overrides (useful for external PTY integrations)
  void setViewportRowCountOverride(int rows);

  // mode
  bool echo() const;
  void setEcho(bool echo);

  bool crlf() const;
  void setCrlf(bool crlf);

  QPoint cursorPos() const;
  QPoint posToCursor(const QPoint &cursorPos) const;

signals:
  void cursorMoved(QPoint cursorPos);
  void cleared();

public slots:
  void writeData(const QByteArray &data);

  void paste();
  void copy();
  void clear();

protected slots:
  void read();
  void appendData(const QByteArray &data);
  void appendString(const QString &str);
  void removeStringFromCursor(Direction direction = RIGHT_DIRECT,
                              int len = INT_MAX);

  void toggleCursor();

private:
  QIODevice *_device;

  // parser
  enum State {
    Text,
    Escape,
    Format,
    ResetFont,
    OSC, // Operating System Command (ESC ] ... BEL/ST)
  };
  State _state;
  int _formatValue;
  int _formatValue_Y;
  bool _useFormaValue_Y;

  // cursor
  void setCursorPos(int x, int y);
  void setCursorPos(QPoint cursorPos);
  QVTCharFormat _format;
  QVTCharFormat _curentFormat;
  int _cw;
  int _ch;
  QPoint _cursorPos;
  QPoint _savedCursorPos; // For ESC 7 / ESC 8 (save/restore cursor)
  QTimer _cursorTimer;
  bool _cvisible;
  int _viewportRowCountOverride;
  int _cachedViewportOrigin;

  // select
  QPoint _startCursorSelectPos;
  QPoint _startSelectPos;
  QPoint _endSelectPos;

  // data
  QVTLayout *_layout;

  // mode
  bool _echo;
  bool _crlf;

  QAction *_pasteAction;
  QAction *_copyAction;
  QAction *_clearAction;
  VT *_vt;

  // QWidget interface
protected:
  bool event(QEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
#ifndef QT_NO_CONTEXTMENU
  void contextMenuEvent(QContextMenuEvent *event) override;
#endif // QT_NO_CONTEXTMENU

  // QAbstractScrollArea interface
protected:
  bool viewportEvent(QEvent *event) override;
  QColor vt100color(char c, bool bright = false);
};

#endif // QVTERMINAL_H
