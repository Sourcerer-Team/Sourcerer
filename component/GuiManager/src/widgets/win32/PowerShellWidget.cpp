#include "widgets/win32/PowerShellWidget.h"
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

// ConPTY API requires Windows 10 1809 (10.0.17763) or later
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE                                    \
  ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)
#endif

#ifndef HPCON
typedef VOID *HPCON;
#endif

// Function pointers for ConPTY API (dynamically loaded from kernel32.dll)
typedef HRESULT(WINAPI *PFN_CreatePseudoConsole)(COORD size, HANDLE hInput,
                                                 HANDLE hOutput, DWORD dwFlags,
                                                 HPCON *phPC);
typedef HRESULT(WINAPI *PFN_ResizePseudoConsole)(HPCON hPC, COORD size);
typedef void(WINAPI *PFN_ClosePseudoConsole)(HPCON hPC);

static PFN_CreatePseudoConsole pfnCreatePseudoConsole = nullptr;
static PFN_ResizePseudoConsole pfnResizePseudoConsole = nullptr;
static PFN_ClosePseudoConsole pfnClosePseudoConsole = nullptr;

static bool LoadConPtyAPI() {
  static bool loaded = false;
  static bool available = false;

  if (loaded) {
    return available;
  }

  loaded = true;

  HMODULE hKernel32 = GetModuleHandleW(L"kernel32.dll");
  if (!hKernel32) {
    return false;
  }

  pfnCreatePseudoConsole = reinterpret_cast<PFN_CreatePseudoConsole>(
      GetProcAddress(hKernel32, "CreatePseudoConsole"));
  pfnResizePseudoConsole = reinterpret_cast<PFN_ResizePseudoConsole>(
      GetProcAddress(hKernel32, "ResizePseudoConsole"));
  pfnClosePseudoConsole = reinterpret_cast<PFN_ClosePseudoConsole>(
      GetProcAddress(hKernel32, "ClosePseudoConsole"));

  available = (pfnCreatePseudoConsole && pfnResizePseudoConsole &&
               pfnClosePseudoConsole);
  return available;
}

#define THREAD_ID "PowerShellWidget"

using namespace GuiManager::Widgets::Win32;

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

// ConPtyIODevice implementation
ConPtyIODevice::ConPtyIODevice(HANDLE hInput, HANDLE hOutput, QObject *parent)
    : QIODevice(parent), _hInput(hInput), _hOutput(hOutput),
      _readerThread(nullptr), _running(true) {

  open(QIODevice::ReadWrite | QIODevice::Unbuffered);

  // Start reader thread for synchronous reads
  _readerThread = QThread::create([this]() {
    char buffer[4096];

    while (_running && _hOutput != INVALID_HANDLE_VALUE) {
      DWORD bytesRead = 0;

      // Synchronous read
      if (ReadFile(_hOutput, buffer, sizeof(buffer), &bytesRead, nullptr)) {
        if (bytesRead > 0) {
          QMutexLocker locker(&_bufferMutex);
          _readBuffer.append(buffer, bytesRead);
          locker.unlock();
          emit readyRead();
        }
      } else {
        DWORD error = GetLastError();
        if (error == ERROR_BROKEN_PIPE || error == ERROR_HANDLE_EOF) {
          break;
        }
        // Other errors - wait a bit to avoid tight loop
        Sleep(10);
      }
    }
  });
  _readerThread->start();
}

ConPtyIODevice::~ConPtyIODevice() {
  _running = false;

  // Cancel any pending I/O operations before closing handles
  if (_hOutput != INVALID_HANDLE_VALUE) {
    CancelIoEx(_hOutput, nullptr);
  }
  if (_hInput != INVALID_HANDLE_VALUE) {
    CancelIoEx(_hInput, nullptr);
  }

  // Wait for reader thread to exit
  if (_readerThread) {
    _readerThread->wait();
    delete _readerThread;
  }

  // Now safe to close handles
  if (_hOutput != INVALID_HANDLE_VALUE) {
    CloseHandle(_hOutput);
    _hOutput = INVALID_HANDLE_VALUE;
  }
  if (_hInput != INVALID_HANDLE_VALUE) {
    CloseHandle(_hInput);
    _hInput = INVALID_HANDLE_VALUE;
  }
}

qint64 ConPtyIODevice::bytesAvailable() const {
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

qint64 ConPtyIODevice::readData(char *data, qint64 maxSize) {
  QMutexLocker locker(&_bufferMutex);

  if (_readBuffer.isEmpty()) {
    return 0;
  }

  qint64 bytesToRead = qMin(maxSize, static_cast<qint64>(_readBuffer.size()));

  // Log the buffer data with escaped control characters
  LOG_DEBUG("ConPTY readData: {}", escape(_readBuffer.left(bytesToRead)));

  memcpy(data, _readBuffer.constData(), bytesToRead);
  _readBuffer.remove(0, bytesToRead);

  return bytesToRead;
}

qint64 ConPtyIODevice::writeData(const char *data, qint64 maxSize) {
  if (_hInput == INVALID_HANDLE_VALUE || maxSize <= 0) {
    return -1;
  }

  QByteArray buffer(data, maxSize);
  buffer = buffer.replace("\r\n", "\r"); // This is a workaround for
                                         // PowerShell handling of CRLF

  LOG_DEBUG("ConPTY writeData: {}", escape(buffer));

  // Write data synchronously
  DWORD bytesWritten = 0;
  if (WriteFile(_hInput, buffer.constData(), buffer.length(), &bytesWritten,
                nullptr)) {
    return bytesWritten;
  }

  return -1;
}

// PowerShellWidget implementation

PowerShellWidget::PowerShellWidget(QWidget *parent)
    : QWidget(parent), _hPseudoConsole(nullptr), _ptyIn(INVALID_HANDLE_VALUE),
      _ptyOut(INVALID_HANDLE_VALUE), _childProcess(INVALID_HANDLE_VALUE),
      _childThread(INVALID_HANDLE_VALUE), _ptyDevice(nullptr),
      _initialized(false) {
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

PowerShellWidget::~PowerShellWidget() {
  LOG_PERF(0, "Enter");

  cleanupPty();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::init() {
  LOG_PERF(0, "Enter");

  if (!_initialized) {
    _initialized = true;
    startPowerShell();
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::startPowerShell() {
  LOG_PERF(0, "Enter");

  // Clean up any existing PTY
  cleanupPty();

  // Check if ConPTY API is available
  if (!LoadConPtyAPI()) {
    LOG_ERROR(Log::ErrorId::Gui,
              "ConPTY API not available (requires Windows 10 1809 or later)");
    emit errorOccurred(
        "ConPTY API not available. Requires Windows 10 version 1809 or later.");
    return;
  }

  // Create pipes for ConPTY I/O
  HANDLE hPipeIn = INVALID_HANDLE_VALUE;
  HANDLE hPipeOut = INVALID_HANDLE_VALUE;

  SECURITY_ATTRIBUTES sa = {sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};

  // Create input pipe (we write to _ptyIn, console reads from hPipeIn)
  if (!CreatePipe(&hPipeIn, &_ptyIn, &sa, 0)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to create input pipe: {}",
              GetLastError());
    emit errorOccurred("Failed to create input pipe");
    return;
  }

  // Create output pipe (console writes to hPipeOut, we read from _ptyOut)
  if (!CreatePipe(&_ptyOut, &hPipeOut, &sa, 0)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to create output pipe: {}",
              GetLastError());
    CloseHandle(hPipeIn);
    CloseHandle(_ptyIn);
    _ptyIn = INVALID_HANDLE_VALUE;
    emit errorOccurred("Failed to create output pipe");
    return;
  }

  // Make sure child process doesn't inherit our ends of the pipes
  SetHandleInformation(_ptyIn, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(_ptyOut, HANDLE_FLAG_INHERIT, 0);

  constexpr int cols = 80;
  constexpr int rows = 25;

  // Create the pseudoconsole
  COORD consoleSize = {static_cast<SHORT>(cols), static_cast<SHORT>(rows)};
  HRESULT hr = pfnCreatePseudoConsole(consoleSize, hPipeIn, hPipeOut, 0,
                                      &_hPseudoConsole);

  // Close the console ends of the pipes (ConPTY owns them now)
  CloseHandle(hPipeIn);
  CloseHandle(hPipeOut);

  if (FAILED(hr)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to create pseudoconsole: {:x}", hr);
    CloseHandle(_ptyIn);
    CloseHandle(_ptyOut);
    _ptyIn = INVALID_HANDLE_VALUE;
    _ptyOut = INVALID_HANDLE_VALUE;
    emit errorOccurred("Failed to create pseudoconsole");
    return;
  }

  // Prepare startup info with pseudoconsole
  STARTUPINFOEXW siEx = {};
  siEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
  siEx.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  siEx.StartupInfo.hStdInput = nullptr;
  siEx.StartupInfo.hStdOutput = nullptr;
  siEx.StartupInfo.hStdError = nullptr;

  // Initialize attribute list
  SIZE_T attrListSize = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attrListSize);
  siEx.lpAttributeList =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(malloc(attrListSize));

  if (!InitializeProcThreadAttributeList(siEx.lpAttributeList, 1, 0,
                                         &attrListSize)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to initialize attribute list: {}",
              GetLastError());
    free(siEx.lpAttributeList);
    cleanupPty();
    emit errorOccurred("Failed to initialize attribute list");
    return;
  }

  // Attach the pseudoconsole to the attribute list
  if (!UpdateProcThreadAttribute(
          siEx.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE,
          _hPseudoConsole, sizeof(_hPseudoConsole), nullptr, nullptr)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to update proc thread attribute: {}",
              GetLastError());
    DeleteProcThreadAttributeList(siEx.lpAttributeList);
    free(siEx.lpAttributeList);
    cleanupPty();
    emit errorOccurred("Failed to update proc thread attribute");
    return;
  }

  // Build command line
  std::wstring cmdLine;
  if (GetFileAttributesW(L"C:\\Program Files\\PowerShell\\7\\pwsh.exe") !=
      INVALID_FILE_ATTRIBUTES) {
    cmdLine = L"C:\\Program Files\\PowerShell\\7\\pwsh.exe -NoLogo";
  } else {
    cmdLine = L"C:\\Windows\\System32\\WindowsPowerShell\\v1.0\\powershell.exe "
              L"-NoLogo -NoExit";
  }

  // CreateProcessW modifies the command line, so make a mutable copy
  std::vector<wchar_t> cmdLineBuf(cmdLine.begin(), cmdLine.end());
  cmdLineBuf.push_back(L'\0');

  // Set working directory
  std::wstring workingDir = _workingDirectory.toStdWString();
  LPCWSTR cwd = workingDir.empty() ? nullptr : workingDir.c_str();

  // Create the process
  PROCESS_INFORMATION pi = {};
  BOOL success = CreateProcessW(nullptr,           // application name
                                cmdLineBuf.data(), // command line (mutable)
                                nullptr,           // process attributes
                                nullptr,           // thread attributes
                                FALSE,             // inherit handles
                                EXTENDED_STARTUPINFO_PRESENT, // creation flags
                                nullptr,                      // environment
                                cwd,               // current directory
                                &siEx.StartupInfo, // startup info
                                &pi                // process information
  );

  // Clean up attribute list
  DeleteProcThreadAttributeList(siEx.lpAttributeList);
  free(siEx.lpAttributeList);

  if (!success) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to create PowerShell process: {}",
              GetLastError());
    cleanupPty();
    emit errorOccurred("Failed to create PowerShell process");
    return;
  }

  _childProcess = pi.hProcess;
  _childThread = pi.hThread;

  // Create QIODevice wrapper for the PTY pipes
  _ptyDevice = new ConPtyIODevice(_ptyIn, _ptyOut, this);

  // Connect the IO device to QVTerminal
  _textDisplay->setIODevice(_ptyDevice);

  QTimer::singleShot(0, this, [this]() {
    if (!_hPseudoConsole || !_textDisplay) {
      return;
    }
    const auto refreshedGeometry = calculateTerminalGeometry(_textDisplay);
    _textDisplay->setViewportRowCountOverride(refreshedGeometry.rows);
    COORD newSize = {static_cast<SHORT>(refreshedGeometry.cols),
                     static_cast<SHORT>(refreshedGeometry.rows)};
    HRESULT hr = pfnResizePseudoConsole(_hPseudoConsole, newSize);
    if (FAILED(hr)) {
      LOG_ERROR(Log::ErrorId::Gui,
                "Failed to apply deferred pseudoconsole resize: {:x}", hr);
    }
  });

  LOG_INFO("PowerShell started with ConPTY, child process handle: {}",
           (void *)_childProcess);

  // Send initial command to trigger output
  emit processStarted();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::cleanupPty() {
  LOG_PERF(0, "Enter");

  // Delete the IO device first (which will stop its reader thread and close
  // pipe handles)
  if (_ptyDevice) {
    _textDisplay->setIODevice(nullptr);
    delete _ptyDevice;
    _ptyDevice = nullptr;
  }

  // Handles are closed by ConPtyIODevice destructor, just reset them
  _ptyIn = INVALID_HANDLE_VALUE;
  _ptyOut = INVALID_HANDLE_VALUE;

  if (_childThread != INVALID_HANDLE_VALUE) {
    CloseHandle(_childThread);
    _childThread = INVALID_HANDLE_VALUE;
  }

  if (_childProcess != INVALID_HANDLE_VALUE) {
    TerminateProcess(_childProcess, 0);
    CloseHandle(_childProcess);
    _childProcess = INVALID_HANDLE_VALUE;
  }

  if (_hPseudoConsole) {
    pfnClosePseudoConsole(_hPseudoConsole);
    _hPseudoConsole = nullptr;
  }

  LOG_PERF(0, "Exit");
}

bool PowerShellWidget::executeCommand(const QString &command) {
  LOG_PERF(0, "Enter");

  if (!_ptyDevice || !_hPseudoConsole) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write command to the PTY device
  for (const auto &c : command.toUtf8()) {
    QByteArray part;
    part.append(c);
    _ptyDevice->write(part);
  }

  _ptyDevice->write(QString("\r\n").toUtf8());

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

void PowerShellWidget::setWorkingDirectory(const QString &path) {
  _workingDirectory = path;
}

void PowerShellWidget::closeEvent(QCloseEvent *event) {
  terminate();
  QWidget::closeEvent(event);
}

void PowerShellWidget::focusInEvent(QFocusEvent *event) {
  QWidget::focusInEvent(event);
  if (_textDisplay) {
    _textDisplay->setFocus();
  }
}

void PowerShellWidget::focusOutEvent(QFocusEvent *event) {
  QWidget::focusOutEvent(event);
  // QVTerminal will lose focus and stop showing cursor automatically
}

void PowerShellWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  // Update PTY size when widget is resized
  if (_hPseudoConsole && _textDisplay &&
      _textDisplay->viewport()->height() > 0 &&
      _textDisplay->viewport()->width() > 0) {
    const auto geometry = calculateTerminalGeometry(_textDisplay);
    _textDisplay->setViewportRowCountOverride(geometry.rows);

    // Update pseudoconsole size
    COORD newSize = {static_cast<SHORT>(geometry.cols),
                     static_cast<SHORT>(geometry.rows)};
    HRESULT hr = pfnResizePseudoConsole(_hPseudoConsole, newSize);
    if (FAILED(hr)) {
      LOG_ERROR(Log::ErrorId::Gui, "Failed to resize pseudoconsole: {:x}", hr);
    }
  }
}
