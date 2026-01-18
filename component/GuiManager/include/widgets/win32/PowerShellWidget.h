#pragma once

#include <QBuffer>
#include <QIODevice>
#include <QMutex>
#include <QThread>
#include <QWidget>
#include <qvterminal.h>
#include <windows.h>

// Forward declare winpty types
struct winpty_s;
typedef struct winpty_s winpty_t;

namespace GuiManager::Widgets::Win32 {

/**
 * @brief QIODevice wrapper for WinPTY pipes using overlapped I/O
 */
class WinPtyIODevice : public QIODevice {
  Q_OBJECT

public:
  explicit WinPtyIODevice(HANDLE hInput, HANDLE hOutput,
                          QObject *parent = nullptr);
  ~WinPtyIODevice() override;

  bool isSequential() const override { return true; }
  qint64 bytesAvailable() const override;

protected:
  qint64 readData(char *data, qint64 maxSize) override;
  qint64 writeData(const char *data, qint64 maxSize) override;

private:
  HANDLE _hInput;
  HANDLE _hOutput;
  QThread *_readerThread;
  bool _running;
  QByteArray _readBuffer;
  mutable QMutex _bufferMutex;
};

/**
 * @brief PowerShell terminal widget using WinPTY and QVTerminal
 *
 * Uses WinPTY to create a real pseudo-terminal with QVTerminal for
 * native VT100 emulation, making interactive programs like Python,
 * gcc, etc. work properly with full I/O support.
 */
class PowerShellWidget : public QWidget {
  Q_OBJECT

public:
  explicit PowerShellWidget(QWidget *parent = nullptr);
  ~PowerShellWidget() override;

  void init();
  bool executeCommand(const QString &command);
  bool isRunning() const;
  void restart();
  void terminate();
  void setFocusToInput();
  void setWorkingDirectory(const QString &path);

signals:
  void processStarted();
  void processStopped();
  void errorOccurred(const QString &error);

protected:
  void closeEvent(QCloseEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;

private:
  void startPowerShell();
  void cleanupPty();

  QString _workingDirectory;
  bool _initialized;

  // WinPTY handles
  winpty_t *_pty;
  HANDLE _ptyIn;  // Handle to write to PTY
  HANDLE _ptyOut; // Handle to read from PTY
  HANDLE _childProcess;

  // UI - QVTerminal handles terminal emulation
  QVTerminal *_textDisplay;
  WinPtyIODevice *_ptyDevice;
};

} // namespace GuiManager::Widgets::Win32
