#include "widgets/linux/BashShellWidget.h"
#include "ConfigManager.h"
#include "Log.h"

#include <QDir>
#include <QFont>
#include <QFontInfo>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <qvterminal.h>

#include <fcntl.h>
#include <pty.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

#define THREAD_ID "BashShellWidget"

using namespace GuiManager::Widgets::Linux;

namespace {

TerminalGeometry calculateTerminalGeometry(QVTerminal *terminal) {
  TerminalGeometry geometry;
  if (!terminal) {
    return geometry;
  }

  const QFontMetrics fm(terminal->format().font());
  const int charWidth = qMax(1, fm.horizontalAdvance('M'));
  const int charHeight = qMax(1, fm.height());

  int viewportWidth = terminal->viewport() ? terminal->viewport()->width() : 0;
  int viewportHeight =
      terminal->viewport() ? terminal->viewport()->height() : 0;

  if (viewportWidth <= 0) {
    viewportWidth = terminal->width();
  }
  if (viewportHeight <= 0) {
    viewportHeight = terminal->height();
  }

  if (viewportWidth <= 0 || viewportHeight <= 0) {
    geometry.usedFallback = true;
    viewportWidth = qMax(viewportWidth, charWidth * geometry.cols);
    viewportHeight = qMax(viewportHeight, charHeight * geometry.rows);
  }

  geometry.cols = qMax(1, viewportWidth / charWidth);
  geometry.rows = qMax(1, viewportHeight / charHeight);

  return geometry;
}

} // namespace

// PtyIODevice implementation
PtyIODevice::PtyIODevice(int masterFd, QObject *parent)
    : QIODevice(parent), _masterFd(masterFd), _readNotifier(nullptr) {

  open(QIODevice::ReadWrite | QIODevice::Unbuffered);

  // Set non-blocking mode
  int flags = fcntl(_masterFd, F_GETFL, 0);
  fcntl(_masterFd, F_SETFL, flags | O_NONBLOCK);

  // Create socket notifier for read events
  _readNotifier = new QSocketNotifier(_masterFd, QSocketNotifier::Read, this);
  connect(_readNotifier, &QSocketNotifier::activated, this, [this]() {
    char buffer[4096];
    ssize_t bytesRead = ::read(_masterFd, buffer, sizeof(buffer));

    if (bytesRead > 0) {
      QMutexLocker locker(&_bufferMutex);
      _readBuffer.append(buffer, bytesRead);
      locker.unlock();
      emit readyRead();
    } else if (bytesRead == 0) {
      // EOF - child process closed
      _readNotifier->setEnabled(false);
    } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
      // Error occurred
      _readNotifier->setEnabled(false);
    }
  });
}

PtyIODevice::~PtyIODevice() {
  if (_readNotifier) {
    _readNotifier->setEnabled(false);
    delete _readNotifier;
    _readNotifier = nullptr;
  }

  if (_masterFd >= 0) {
    ::close(_masterFd);
    _masterFd = -1;
  }
}

qint64 PtyIODevice::bytesAvailable() const {
  QMutexLocker locker(&_bufferMutex);
  return _readBuffer.size() + QIODevice::bytesAvailable();
}

static QString escape(const QByteArray &buffer) {
  QString escaped;
  for (int i = 0; i < buffer.length(); ++i) {
    char c = buffer[i];
    if (c == '\r') {
      escaped.append("\\r");
    } else if (c == '\n') {
      escaped.append("\\n");
    } else if (c == 0x1B) { // ESC character
      escaped.append("\\033");
    } else if (c >= 32 && c <= 126) { // printable ASCII
      escaped.append(c);
    } else {
      escaped.append(QString("\\x%1").arg(static_cast<unsigned char>(c), 2, 16,
                                          QChar('0')));
    }
  }

  return escaped;
}

qint64 PtyIODevice::readData(char *data, qint64 maxSize) {
  QMutexLocker locker(&_bufferMutex);

  if (_readBuffer.isEmpty()) {
    return 0;
  }

  qint64 bytesToRead = qMin(maxSize, static_cast<qint64>(_readBuffer.size()));

  // Log the buffer data with escaped control characters
  LOG_DEBUG("PTY readData: {}", escape(_readBuffer.left(bytesToRead)));

  memcpy(data, _readBuffer.constData(), bytesToRead);
  _readBuffer.remove(0, bytesToRead);

  return bytesToRead;
}

qint64 PtyIODevice::writeData(const char *data, qint64 maxSize) {
  if (_masterFd < 0 || maxSize <= 0) {
    return -1;
  }

  QByteArray buffer(data, maxSize);
  LOG_DEBUG("PTY writeData: {}", escape(buffer));

  // Write data to PTY
  ssize_t bytesWritten =
      ::write(_masterFd, buffer.constData(), buffer.length());

  if (bytesWritten < 0) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0; // Try again later
    }
    return -1;
  }

  return bytesWritten;
}

// BashShellWidget implementation

BashShellWidget::BashShellWidget(QWidget *parent)
    : QWidget(parent), _masterFd(-1), _slaveFd(-1), _childPid(-1),
      _ptyDevice(nullptr), _initialized(false) {
  LOG_PERF(0, "Enter");

  // Create layout
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create QVTerminal widget for native VT100 emulation
  _textDisplay = new QVTerminal(this);
  _textDisplay->setFocusPolicy(Qt::StrongFocus);

  // Get font from ConfigManager (same as diff editor)
  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const QString fontName = configManager.getString(
      ConfigManager::ConfigKey::GitRepoMonospaceFontName);
  const int fontSize = static_cast<int>(configManager.getLong(
      ConfigManager::ConfigKey::GitRepoMonospaceFontSize));

  // Set console font with fallback
  QFont font(fontName, fontSize);
  QFontInfo fontInfo(font);
  if (fontInfo.family() != font.family()) {
    // Fallback if requested font not available
    font = QFont("Courier New", 10);
  }

  font.setStyleHint(QFont::Monospace);
  font.setFixedPitch(true);

  QVTCharFormat format;
  format.setFont(font);
  format.setForeground(QColor(187, 187, 187));
  format.setBackground(QColor(0x23, 0x26, 0x29));

  _textDisplay->setFormat(format);

  layout->addWidget(_textDisplay);

  // Set focus to terminal
  _textDisplay->setFocus();

  _textDisplay->setCrlf(true);

  connect(_textDisplay, &QVTerminal::cleared, this, [this]() {
    executeCommand("\x03");
    executeCommand("clear");
  });

  LOG_PERF(0, "Exit");
}

BashShellWidget::~BashShellWidget() {
  LOG_PERF(0, "Enter");

  cleanupPty();

  LOG_PERF(0, "Exit");
}

void BashShellWidget::init() {
  LOG_PERF(0, "Enter");

  if (!_initialized) {
    _initialized = true;
    startBash();
  }

  LOG_PERF(0, "Exit");
}

void BashShellWidget::startBash() {
  LOG_PERF(0, "Enter");

  // Clean up any existing PTY
  cleanupPty();

  // Create PTY with default size (will be updated after IODevice connection)
  if (openpty(&_masterFd, &_slaveFd, nullptr, nullptr, nullptr) < 0) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to create PTY: {}", strerror(errno));
    emit errorOccurred(
        QString("Failed to create PTY: %1").arg(strerror(errno)));
    return;
  }

  // Fork process
  _childPid = fork();

  if (_childPid < 0) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to fork process: {}", strerror(errno));
    ::close(_masterFd);
    ::close(_slaveFd);
    _masterFd = -1;
    _slaveFd = -1;
    emit errorOccurred(
        QString("Failed to fork process: %1").arg(strerror(errno)));
    return;
  }

  if (_childPid == 0) {
    // Child process

    // Close master fd in child
    ::close(_masterFd);

    // Create new session and make slave the controlling terminal
    setsid();
    ioctl(_slaveFd, TIOCSCTTY, 0);

    // Redirect stdin, stdout, stderr to slave
    dup2(_slaveFd, STDIN_FILENO);
    dup2(_slaveFd, STDOUT_FILENO);
    dup2(_slaveFd, STDERR_FILENO);

    // Close slave fd after duplication
    if (_slaveFd > STDERR_FILENO) {
      ::close(_slaveFd);
    }

    // Set working directory if specified
    if (!_workingDirectory.isEmpty()) {
      if (chdir(_workingDirectory.toLocal8Bit().constData()) < 0) {
        LOG_ERROR(Log::ErrorId::Gui, "Failed to change directory: {}",
                  strerror(errno));
      }
    }

    // Set environment variables
    setenv("TERM", "xterm-256color", 1);
    setenv("COLORTERM", "truecolor", 1);

    // Disable window title escape sequences that QVTerminal doesn't support
    setenv("PROMPT_COMMAND", "", 1);

    // Execute bash
    const char *shell = getenv("SHELL");
    if (!shell || !*shell) {
      shell = "/bin/bash";
    }

    execl(shell, shell, "-i", nullptr);

    // If execl fails
    LOG_ERROR(Log::ErrorId::Gui, "Failed to execute shell: {}",
              strerror(errno));
    _exit(1);
  }

  // Parent process

  // Close slave fd in parent
  ::close(_slaveFd);
  _slaveFd = -1;

  LOG_INFO("Bash started with PTY, child PID: {}", _childPid);

  // Create QIODevice wrapper for the PTY
  _ptyDevice = new PtyIODevice(_masterFd, this);

  // Calculate initial terminal geometry
  const auto initialGeometry = calculateTerminalGeometry(_textDisplay);
  _textDisplay->setViewportRowCountOverride(initialGeometry.rows);

  // Set initial PTY size before connecting IODevice
  struct winsize ws;
  ws.ws_col = initialGeometry.cols;
  ws.ws_row = initialGeometry.rows;
  ws.ws_xpixel = 0;
  ws.ws_ypixel = 0;

  if (ioctl(_masterFd, TIOCSWINSZ, &ws) < 0) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to set initial PTY size: {}",
              strerror(errno));
  }

  LOG_DEBUG("Set initial PTY size to {}x{}", initialGeometry.cols,
            initialGeometry.rows);

  // Connect the IO device to QVTerminal
  _textDisplay->setIODevice(_ptyDevice);

  LOG_DEBUG("Connected PtyIODevice to QVTerminal, bytesAvailable: {}",
            _ptyDevice->bytesAvailable());

  // Force widget to be shown and updated
  _textDisplay->show();
  _textDisplay->setVisible(true);
  _textDisplay->update();

  // Verify terminal size again after a short delay (widget might resize)
  QTimer::singleShot(150, this, [this]() {
    if (_masterFd < 0 || !_textDisplay) {
      return;
    }

    const auto refreshedGeometry = calculateTerminalGeometry(_textDisplay);
    _textDisplay->setViewportRowCountOverride(refreshedGeometry.rows);

    struct winsize ws2;
    ws2.ws_col = refreshedGeometry.cols;
    ws2.ws_row = refreshedGeometry.rows;
    ws2.ws_xpixel = 0;
    ws2.ws_ypixel = 0;

    if (ioctl(_masterFd, TIOCSWINSZ, &ws2) < 0) {
      LOG_ERROR(Log::ErrorId::Gui, "Failed to set PTY size: {}",
                strerror(errno));
    }

    LOG_DEBUG("PTY resized to {}x{}, bytesAvailable: {}",
              refreshedGeometry.cols, refreshedGeometry.rows,
              _ptyDevice ? _ptyDevice->bytesAvailable() : 0);
  });

  emit processStarted();

  LOG_PERF(0, "Exit");
}

void BashShellWidget::cleanupPty() {
  LOG_PERF(0, "Enter");

  // Delete the IO device first (which will close master fd)
  if (_ptyDevice) {
    _textDisplay->setIODevice(nullptr);
    delete _ptyDevice;
    _ptyDevice = nullptr;
  }

  // Master fd is closed by PtyIODevice destructor
  _masterFd = -1;

  // Close slave fd if still open
  if (_slaveFd >= 0) {
    ::close(_slaveFd);
    _slaveFd = -1;
  }

  // Terminate child process if running
  if (_childPid > 0) {
    // Send SIGHUP first (graceful)
    kill(_childPid, SIGHUP);

    // Wait briefly for graceful shutdown
    for (int i = 0; i < 10; ++i) {
      int status;
      if (waitpid(_childPid, &status, WNOHANG) == _childPid) {
        _childPid = -1;
        break;
      }
      usleep(10000); // 10ms
    }

    // If still running, send SIGKILL
    if (_childPid > 0) {
      kill(_childPid, SIGKILL);
      waitpid(_childPid, nullptr, 0);
      _childPid = -1;
    }
  }

  LOG_PERF(0, "Exit");
}

bool BashShellWidget::executeCommand(const QString &command) {
  LOG_PERF(0, "Enter");

  if (!_ptyDevice || _masterFd < 0) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write command to the PTY device
  QByteArray cmdData = command.toUtf8() + "\n";
  _ptyDevice->write(cmdData);

  LOG_PERF(0, "Exit");
  return true;
}

bool BashShellWidget::isRunning() const {
  LOG_PERF(0, "Enter/Exit");

  if (_childPid <= 0) {
    return false;
  }

  // Check if process is still alive
  int status;
  pid_t result = waitpid(_childPid, &status, WNOHANG);

  return result == 0; // 0 means still running
}

void BashShellWidget::restart() {
  LOG_PERF(0, "Enter");

  _textDisplay->clear();

  startBash();

  LOG_PERF(0, "Exit");
}

void BashShellWidget::terminate() {
  LOG_PERF(0, "Enter");

  cleanupPty();
  emit processStopped();

  LOG_PERF(0, "Exit");
}

void BashShellWidget::setFocusToInput() {
  LOG_PERF(0, "Enter");

  if (_textDisplay) {
    _textDisplay->setFocus();
    _textDisplay->activateWindow();
  }

  LOG_PERF(0, "Exit");
}

void BashShellWidget::setWorkingDirectory(const QString &path) {
  _workingDirectory = path;
}

void BashShellWidget::closeEvent(QCloseEvent *event) {
  terminate();
  QWidget::closeEvent(event);
}

void BashShellWidget::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);
  if (_textDisplay) {
    _textDisplay->setFocus();
  }
}

void BashShellWidget::focusOutEvent(QFocusEvent *event) {
  QWidget::focusOutEvent(event);
  // QVTerminal will lose focus and stop showing cursor automatically
}

void BashShellWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  // Update PTY size when widget is resized
  if (_masterFd >= 0 && _textDisplay &&
      _textDisplay->viewport()->height() > 0 &&
      _textDisplay->viewport()->width() > 0) {

    const auto geometry = calculateTerminalGeometry(_textDisplay);
    _textDisplay->setViewportRowCountOverride(geometry.rows);

    // Update PTY window size
    struct winsize ws;
    ws.ws_col = geometry.cols;
    ws.ws_row = geometry.rows;
    ws.ws_xpixel = 0;
    ws.ws_ypixel = 0;

    if (ioctl(_masterFd, TIOCSWINSZ, &ws) < 0) {
      LOG_ERROR(Log::ErrorId::Gui, "Failed to resize PTY: {}", strerror(errno));
    }
  }
}
