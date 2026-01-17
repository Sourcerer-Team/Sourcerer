#include "widgets/win32/PowerShellWidget.h"
#include "Log.h"

#include <QFont>
#include <QVBoxLayout>
#include <qvterminal.h>
#include <winpty.h>

#define THREAD_ID "PowerShellWidget"

using namespace GuiManager::Widgets::Win32;

// WinPtyIODevice implementation
WinPtyIODevice::WinPtyIODevice(HANDLE hInput, HANDLE hOutput, QObject *parent)
    : QIODevice(parent), _hInput(hInput), _hOutput(hOutput),
      _readerThread(nullptr), _running(true) {

  open(QIODevice::ReadWrite | QIODevice::Unbuffered);

  // Start reader thread for async reads
  _readerThread = QThread::create([this]() {
    while (_running && _hOutput != INVALID_HANDLE_VALUE) {
      char buffer[4096];

      std::memset(&buffer[0], 0, sizeof(buffer));

      DWORD bytesRead = 0;
      OVERLAPPED overlapped = {};
      overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

      if (!overlapped.hEvent) {
        break;
      }

      BOOL readResult =
          ReadFile(_hOutput, buffer, sizeof(buffer), &bytesRead, &overlapped);

      if (readResult && bytesRead > 0) {
        // Read completed immediately - buffer and emit readyRead
        CloseHandle(overlapped.hEvent);
        {
          QMutexLocker locker(&_bufferMutex);
          _readBuffer.append(buffer, bytesRead);
        }
        emit readyRead();
      } else {
        DWORD error = GetLastError();
        if (error == ERROR_IO_PENDING) {
          DWORD waitResult = WaitForSingleObject(overlapped.hEvent, INFINITE);
          if (waitResult == WAIT_OBJECT_0) {
            if (GetOverlappedResult(_hOutput, &overlapped, &bytesRead, FALSE)) {
              if (bytesRead > 0) {
                {
                  QMutexLocker locker(&_bufferMutex);
                  _readBuffer.append(buffer, bytesRead);
                }
                emit readyRead();
              }
            } else {
              DWORD overlappedError = GetLastError();
              if (overlappedError == ERROR_BROKEN_PIPE ||
                  overlappedError == ERROR_HANDLE_EOF) {
                CloseHandle(overlapped.hEvent);
                break;
              }
            }
          }
          CloseHandle(overlapped.hEvent);
        } else if (error == ERROR_BROKEN_PIPE) {
          CloseHandle(overlapped.hEvent);
          break;
        } else {
          CloseHandle(overlapped.hEvent);
          break;
        }
      }
    }
  });
  _readerThread->start();
}

WinPtyIODevice::~WinPtyIODevice() {
  _running = false;
  if (_readerThread) {
    _readerThread->wait();
    delete _readerThread;
  }
}

qint64 WinPtyIODevice::bytesAvailable() const {
  QMutexLocker locker(&_bufferMutex);
  return _readBuffer.size() + QIODevice::bytesAvailable();
}

qint64 WinPtyIODevice::readData(char *data, qint64 maxSize) {
  QMutexLocker locker(&_bufferMutex);

  if (_readBuffer.isEmpty()) {
    return 0;
  }

  qint64 bytesToRead = qMin(maxSize, static_cast<qint64>(_readBuffer.size()));
  memcpy(data, _readBuffer.constData(), bytesToRead);
  _readBuffer.remove(0, bytesToRead);

  return bytesToRead;
}

qint64 WinPtyIODevice::writeData(const char *data, qint64 maxSize) {
  if (_hInput == INVALID_HANDLE_VALUE || maxSize <= 0) {
    return -1;
  }

  const QString buffer = QString::fromUtf8(data, static_cast<int>(maxSize));
  ;

  // Write buffered data to PTY
  DWORD bytesWritten = 0;
  OVERLAPPED overlapped = {};
  overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

  if (!overlapped.hEvent) {
    return -1;
  }

  BOOL result = WriteFile(_hInput, buffer.constData(), buffer.length(),
                          &bytesWritten, &overlapped);

  if (result) {
    CloseHandle(overlapped.hEvent);
    return maxSize;
  }

  DWORD error = GetLastError();
  if (error == ERROR_IO_PENDING) {
    if (WaitForSingleObject(overlapped.hEvent, 5000) == WAIT_OBJECT_0) {
      if (GetOverlappedResult(_hInput, &overlapped, &bytesWritten, FALSE)) {
        CloseHandle(overlapped.hEvent);
        return maxSize;
      }
    } else {
      CancelIo(_hInput);
    }
  }

  CloseHandle(overlapped.hEvent);
  return -1;
}

// PowerShellWidget implementation

PowerShellWidget::PowerShellWidget(QWidget *parent)
    : QWidget(parent), _pty(nullptr), _ptyIn(INVALID_HANDLE_VALUE),
      _ptyOut(INVALID_HANDLE_VALUE), _childProcess(INVALID_HANDLE_VALUE),
      _ptyDevice(nullptr) {
  LOG_PERF(0, "Enter");

  // Create layout
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create QVTerminal widget for native VT100 emulation
  _textDisplay = new QVTerminal(this);
  _textDisplay->setFocusPolicy(Qt::StrongFocus);

  // Set console-like font
  QFont font("Consolas", 10);
  font.setStyleHint(QFont::Monospace);
  _textDisplay->setFont(font);

  // Set terminal format (colors)
  QVTCharFormat format = _textDisplay->format();
  format.setForeground(QColor("#CCCCCC"));
  format.setBackground(QColor("#012456"));
  _textDisplay->setFormat(format);

  // Dark theme styling
  _textDisplay->setStyleSheet("QVTerminal {"
                              "  background-color: #012456;"
                              "  border: none;"
                              "}");

  layout->addWidget(_textDisplay);

  // Set focus to terminal
  _textDisplay->setFocus();

  _textDisplay->setCrlf(true);

  // Start PowerShell with WinPTY
  startPowerShell();

  LOG_PERF(0, "Exit");
}

PowerShellWidget::~PowerShellWidget() {
  LOG_PERF(0, "Enter");

  cleanupPty();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::startPowerShell() {
  LOG_PERF(0, "Enter");

  // Clean up any existing PTY
  cleanupPty();

  // Create WinPTY configuration
  winpty_error_ptr_t err = nullptr;
  winpty_config_t *config = winpty_config_new(0, &err);

  if (!config) {
    QString errorMsg = "Failed to create WinPTY config";
    if (err) {
      errorMsg +=
          QString(": %1").arg(QString::fromWCharArray(winpty_error_msg(err)));
      winpty_error_free(err);
    }
    LOG_ERROR(Log::ErrorId::Gui, errorMsg);
    emit errorOccurred(errorMsg);
    return;
  }

  // Set PTY size (80x24 is standard)
  winpty_config_set_initial_size(config, 120, 30);

  // Create the PTY
  _pty = winpty_open(config, &err);
  winpty_config_free(config);

  if (!_pty) {
    QString errorMsg = "Failed to open WinPTY";
    if (err) {
      errorMsg +=
          QString(": %1").arg(QString::fromWCharArray(winpty_error_msg(err)));
      winpty_error_free(err);
    }
    LOG_ERROR(Log::ErrorId::Gui, errorMsg);
    emit errorOccurred(errorMsg);
    return;
  }

  // Get pipe names for I/O
  LPCWSTR coninName = winpty_conin_name(_pty);
  LPCWSTR conoutName = winpty_conout_name(_pty);

  // Open input pipe (write end) with overlapped I/O
  _ptyIn = CreateFileW(coninName, GENERIC_WRITE, 0, nullptr, OPEN_EXISTING,
                       FILE_FLAG_OVERLAPPED, nullptr);

  if (_ptyIn == INVALID_HANDLE_VALUE) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to open PTY input pipe: {}",
              GetLastError());
    cleanupPty();
    emit errorOccurred("Failed to open PTY input pipe");
    return;
  }

  // Open output pipe (read end) with overlapped I/O
  _ptyOut = CreateFileW(conoutName, GENERIC_READ, 0, nullptr, OPEN_EXISTING,
                        FILE_FLAG_OVERLAPPED, nullptr);

  if (_ptyOut == INVALID_HANDLE_VALUE) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to open PTY output pipe: {}",
              GetLastError());
    cleanupPty();
    emit errorOccurred("Failed to open PTY output pipe");
    return;
  }

  // Spawn PowerShell process with full path
  winpty_spawn_config_t *spawnConfig =
      winpty_spawn_config_new(WINPTY_SPAWN_FLAG_AUTO_SHUTDOWN,
                              nullptr, // appname (use cmdline)
                              L"C:\\Windows\\System32\\WindowsPowerShell\\v1."
                              L"0\\powershell.exe -NoProfile -NoLogo -NoExit",
                              nullptr, // cwd
                              nullptr, // env
                              &err);

  if (!spawnConfig) {
    QString errorMsg = "Failed to create spawn config";
    if (err) {
      errorMsg +=
          QString(": %1").arg(QString::fromWCharArray(winpty_error_msg(err)));
      winpty_error_free(err);
    }
    LOG_ERROR(Log::ErrorId::Gui, errorMsg);
    cleanupPty();
    emit errorOccurred(errorMsg);
    return;
  }

  DWORD exitCode = 0;
  BOOL spawnSuccess = winpty_spawn(_pty, spawnConfig, &_childProcess,
                                   nullptr, // thread handle
                                   &exitCode, &err);

  winpty_spawn_config_free(spawnConfig);

  if (!spawnSuccess) {
    QString errorMsg = "Failed to spawn PowerShell";
    if (err) {
      errorMsg +=
          QString(": %1").arg(QString::fromWCharArray(winpty_error_msg(err)));
      winpty_error_free(err);
    }
    LOG_ERROR(Log::ErrorId::Gui, errorMsg);
    cleanupPty();
    emit errorOccurred(errorMsg);
    return;
  }

  // Create QIODevice wrapper for the PTY pipes
  _ptyDevice = new WinPtyIODevice(_ptyIn, _ptyOut, this);

  // Connect the IO device to QVTerminal
  _textDisplay->setIODevice(_ptyDevice);

  LOG_INFO("PowerShell started with WinPTY, child process handle: {}",
           (void *)_childProcess);

  // Send initial command to trigger output
  emit processStarted();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::cleanupPty() {
  LOG_PERF(0, "Enter");

  if (_ptyIn != INVALID_HANDLE_VALUE) {
    CloseHandle(_ptyIn);
    _ptyIn = INVALID_HANDLE_VALUE;
  }

  if (_ptyOut != INVALID_HANDLE_VALUE) {
    CloseHandle(_ptyOut);
    _ptyOut = INVALID_HANDLE_VALUE;
  }

  // Delete the IO device first (which will stop its reader thread)
  if (_ptyDevice) {
    _textDisplay->setIODevice(nullptr);
    delete _ptyDevice;
    _ptyDevice = nullptr;
  }

  if (_childProcess != INVALID_HANDLE_VALUE) {
    TerminateProcess(_childProcess, 0);
    CloseHandle(_childProcess);
    _childProcess = INVALID_HANDLE_VALUE;
  }

  if (_pty) {
    winpty_free(_pty);
    _pty = nullptr;
  }

  LOG_PERF(0, "Exit");
}

bool PowerShellWidget::executeCommand(const QString &command) {
  LOG_PERF(0, "Enter");

  if (!_ptyDevice || !_pty) {
    LOG_PERF(0, "Exit - not running");
    return false;
  }

  // Write command to the PTY device
  _ptyDevice->write((command + "\r\n").toUtf8());

  LOG_PERF(0, "Exit");
  return true;
}

bool PowerShellWidget::isRunning() const {
  LOG_PERF(0, "Enter/Exit");
  return _ptyDevice != nullptr && _childProcess != INVALID_HANDLE_VALUE;
}

void PowerShellWidget::restart() {
  LOG_PERF(0, "Enter");

  _textDisplay->clear();

  startPowerShell();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::terminate() {
  LOG_PERF(0, "Enter");

  cleanupPty();
  emit processStopped();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::setFocusToInput() {
  LOG_PERF(0, "Enter");

  if (_textDisplay) {
    _textDisplay->setFocus();
    _textDisplay->activateWindow();
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::closeEvent(QCloseEvent *event) {
  terminate();
  QWidget::closeEvent(event);
}

// QVTerminal handles all terminal emulation, ANSI escape sequences,
// keyboard input, and rendering natively via WinPtyIODevice wrapper
