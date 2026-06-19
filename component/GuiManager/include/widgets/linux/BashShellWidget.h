#pragma once

#include <QIODevice>
#include <QMutex>
#include <QSocketNotifier>
#include <QThread>
#include <QWidget>
#include <qvterminal.h>

namespace GuiManager::Widgets::Linux {

/**
 * @brief QIODevice wrapper for PTY file descriptors
 */
class PtyIODevice : public QIODevice {
  Q_OBJECT

public:
  explicit PtyIODevice(int masterFd, QObject *parent = nullptr);
  ~PtyIODevice() override;

  bool isSequential() const override { return true; }
  qint64 bytesAvailable() const override;

protected:
  qint64 readData(char *data, qint64 maxSize) override;
  qint64 writeData(const char *data, qint64 maxSize) override;

private:
  int _masterFd;
  QSocketNotifier *_readNotifier;
  QByteArray _readBuffer;
  mutable QMutex _bufferMutex;
};

/**
 * @brief Bash terminal widget using PTY and QVTerminal
 *
 * Uses Unix PTY (pseudoterminal) to create a real pseudo-terminal with
 * QVTerminal for native VT100 emulation, making interactive programs like
 * Python, gcc, vim, etc. work properly with full I/O support.
 */
class BashShellWidget : public QWidget {
  Q_OBJECT

public:
  explicit BashShellWidget(QWidget *parent = nullptr);
  ~BashShellWidget() override;

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
  void startBash();
  void cleanupPty();

  QString _workingDirectory;
  bool _initialized;

  // PTY handles
  int _masterFd;
  int _slaveFd;
  pid_t _childPid;

  // UI - QVTerminal handles terminal emulation
  QVTerminal *_textDisplay;
  PtyIODevice *_ptyDevice;
};

} // namespace GuiManager::Widgets::Linux
