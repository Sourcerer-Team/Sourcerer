#pragma once

#include <QBuffer>
#include <QIODevice>
#include <QMutex>
#include <QThread>
#include <QWidget>
#include <qvterminal.h>

#include <windows.h>

#ifndef HPCON
typedef void *HPCON;
#endif

namespace GuiManager::Widgets::Win32 {

/**
 * @brief QIODevice wrapper for ConPTY pipes using overlapped I/O
 */
class ConPtyIODevice : public QIODevice {
  Q_OBJECT

public:
  explicit ConPtyIODevice(HANDLE hInput, HANDLE hOutput,
                          QObject *parent = nullptr);
  ~ConPtyIODevice() override;

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
 * @brief PowerShell terminal widget using ConPTY and QVTerminal
 *
 * Uses Windows ConPTY (Console Pseudoconsole) API to create a real
 * pseudo-terminal with QVTerminal for native VT100 emulation, making
 * interactive programs like Python, gcc, etc. work properly with full I/O
 * support.
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
  void refreshTheme();

private:
  void applyTheme();

signals:
  void processStarted();
  void processStopped();
  void errorOccurred(const QString &error);

protected:
  void closeEvent(QCloseEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;
  void focusOutEvent(QFocusEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private:
  void startPowerShell();
  void cleanupPty();

  QString _workingDirectory;
  bool _initialized;

  // ConPTY handles
  HPCON _hPseudoConsole;
  HANDLE _ptyIn;  // Handle to write to PTY
  HANDLE _ptyOut; // Handle to read from PTY
  HANDLE _childProcess;
  HANDLE _childThread;

  // UI - QVTerminal handles terminal emulation
  QVTerminal *_textDisplay;
  ConPtyIODevice *_ptyDevice;
};

} // namespace GuiManager::Widgets::Win32
