#include "widgets/win32/PowerShellWidget.h"

#include <Log.h>

#include <QHideEvent>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QTimer>

#include <tlhelp32.h>
#include <windows.h>

#define THREAD_ID "PowerShellWidget"

namespace GuiManager::Widgets::Win32 {

PowerShellWidget::PowerShellWidget(QWidget *parent)
    : QWidget(parent), _consoleWindow(nullptr), _processRunning(false), 
      _embedRetries(0), _visibilityTimer(nullptr), _inDestructor(false) {
  LOG_PERF(0, "Enter");

  ZeroMemory(&_processInfo, sizeof(_processInfo));

  // Set background to black to match console
  setStyleSheet("background-color: black;");
  
  // Set minimum size to ensure widget gets space in layout/splitter
  setMinimumSize(400, 200);
  
  // Force creation of native window handle
  setAttribute(Qt::WA_NativeWindow);
  
  // Make sure widget updates are enabled
  setAttribute(Qt::WA_OpaquePaintEvent, false);
  setAttribute(Qt::WA_NoSystemBackground, false);
  
  // Create native window now
  create();
  
  // Create visibility timer
  _visibilityTimer = new QTimer(this);
  _visibilityTimer->setInterval(100); // Check every 100ms
  connect(_visibilityTimer, &QTimer::timeout, this, &PowerShellWidget::ensureConsoleVisible);

  // Start PowerShell process immediately
  startPowerShell();

  LOG_PERF(0, "Exit");
}

PowerShellWidget::~PowerShellWidget() {
  LOG_PERF(0, "Enter");

  _inDestructor = true;
  cleanupProcess();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::startPowerShell() {
  LOG_PERF(0, "Enter");

  if (_processRunning) {
    LOG_WARN("PowerShell process is already running");
    LOG_PERF(0, "Exit");
    return;
  }

  STARTUPINFOW si;
  ZeroMemory(&si, sizeof(si));
  si.cb = sizeof(si);
  si.dwFlags = STARTF_USESHOWWINDOW;
  si.wShowWindow = SW_HIDE; // Start hidden, show after embedding
  ZeroMemory(&_processInfo, sizeof(_processInfo));

  // Get path to PowerShell - use older powershell.exe to avoid Windows Terminal
  wchar_t powershellPath[MAX_PATH];
  ExpandEnvironmentStringsW(
      L"%SystemRoot%\\System32\\WindowsPowerShell\\v1.0\\powershell.exe",
      powershellPath, MAX_PATH);

  // Create PowerShell process with -NoExit to keep it open
  wchar_t cmdLine[MAX_PATH * 2];
  swprintf_s(cmdLine, L"\"%s\" -NoExit -NoLogo", powershellPath);

  const BOOL success = CreateProcessW(nullptr, // Application name
                                      cmdLine, // Command line
                                      nullptr, // Process security attributes
                                      nullptr, // Thread security attributes
                                      FALSE,   // Inherit handles
                                      CREATE_NEW_CONSOLE, // Creation flags - creates console
                                      nullptr,            // Environment
                                      nullptr,            // Current directory
                                      &si,                // Startup info
                                      &_processInfo       // Process info
  );

  if (!success) {
    DWORD error = GetLastError();
    LOG_ERROR(Log::ErrorId::Gui,
              "Failed to create PowerShell process. Error code: {}", error);
    emit errorOccurred(
        QString("Failed to create PowerShell process. Error code: %1")
            .arg(error));
    LOG_PERF(0, "Exit");
    return;
  }

  _processRunning = true;
  LOG_INFO("PowerShell process created with PID: {}", _processInfo.dwProcessId);

  // Start embedding attempts with retry mechanism
  _embedRetries = 0;
  embedConsoleWithRetry(0);

  emit processStarted();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::embedConsole() {
  LOG_PERF(0, "Enter");

  if (!_processRunning) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Find the console window associated with the process
  HWND foundWindow = nullptr;
  
  // Structure to pass data to callback
  struct EnumData {
    DWORD processId;
    HWND* windowHandle;
  };
  
  EnumData enumData = { _processInfo.dwProcessId, &foundWindow };

  // Enumerate all windows to find the console window
  EnumWindows(
      [](HWND hwnd, LPARAM lParam) -> BOOL {
        EnumData* data = reinterpret_cast<EnumData*>(lParam);
        DWORD processId = 0;
        GetWindowThreadProcessId(hwnd, &processId);

        if (processId == data->processId) {
          wchar_t className[256];
          GetClassNameW(hwnd, className, 256);

          // Check if it's a console window
          if (wcscmp(className, L"ConsoleWindowClass") == 0) {
            *(data->windowHandle) = hwnd;
            return FALSE; // Stop enumeration
          }
        }
        return TRUE; // Continue enumeration
      },
      reinterpret_cast<LPARAM>(&enumData));

  if (!foundWindow) {
    LOG_ERROR(Log::ErrorId::Gui, "Could not find console window for process {}",
              _processInfo.dwProcessId);
    
    // Try alternative approach - get console window by attaching to the process console
    if (AttachConsole(_processInfo.dwProcessId)) {
      foundWindow = GetConsoleWindow();
      FreeConsole();
    }
    
    if (!foundWindow) {
      if (!_inDestructor) {
        emit errorOccurred("Could not find console window");
      }
      LOG_PERF(0, "Exit");
      return;
    }
  }

  _consoleWindow = foundWindow;
  LOG_INFO("Found console window: {}",
           reinterpret_cast<uintptr_t>(_consoleWindow));

  // Get current window style
  LONG_PTR style = GetWindowLongPtrW(_consoleWindow, GWL_STYLE);
  LONG_PTR exStyle = GetWindowLongPtrW(_consoleWindow, GWL_EXSTYLE);
  
  LOG_INFO("Original console style: 0x{:X}, exStyle: 0x{:X}", style, exStyle);
  
  // Remove window decorations and make it a child window WITH visibility
  style &= ~(WS_CAPTION | WS_THICKFRAME | WS_MINIMIZE | WS_MAXIMIZE | WS_SYSMENU | WS_OVERLAPPED | WS_POPUP | WS_BORDER);
  style |= WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE;
  
  SetWindowLongPtrW(_consoleWindow, GWL_STYLE, style);
  SetWindowLongPtrW(_consoleWindow, GWL_EXSTYLE, exStyle & ~(WS_EX_WINDOWEDGE | WS_EX_DLGMODALFRAME));
  
  LOG_INFO("Updated console style to: 0x{:X} (added WS_CHILD | WS_VISIBLE)", style);

  // Embed the console window into this widget BEFORE showing it
  const HWND parentHwnd = reinterpret_cast<HWND>(winId());
  
  if (!parentHwnd || !IsWindow(parentHwnd)) {
    LOG_ERROR(Log::ErrorId::Gui, "Parent widget does not have a valid window handle!");
    if (!_inDestructor) {
      emit errorOccurred("Parent widget has no valid window handle");
    }
    LOG_PERF(0, "Exit");
    return;
  }
  
  LOG_INFO("Parent widget HWND: 0x{:X}, visible: {}, size: {}x{}", 
           reinterpret_cast<uintptr_t>(parentHwnd), isVisible(), width(), height());
  
  // Set parent first
  HWND oldParent = SetParent(_consoleWindow, parentHwnd);
  LOG_INFO("Changed console parent from 0x{:X} to 0x{:X}", 
           reinterpret_cast<uintptr_t>(oldParent), reinterpret_cast<uintptr_t>(parentHwnd));
  
  // Verify parent was set
  HWND currentParent = GetParent(_consoleWindow);
  if (currentParent != parentHwnd) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to set parent! Expected 0x{:X}, got 0x{:X}",
              reinterpret_cast<uintptr_t>(parentHwnd), reinterpret_cast<uintptr_t>(currentParent));
  }

  // Get the actual client area size
  const QSize widgetSize = size();
  const int w = widgetSize.width() > 0 ? widgetSize.width() : 800;
  const int h = widgetSize.height() > 0 ? widgetSize.height() : 600;
  
  LOG_INFO("Setting console window size to {}x{}", w, h);

  // Position and size the window in client coordinates (0,0 relative to parent)
  // Use SWP_NOACTIVATE to prevent activation issues
  BOOL posResult = SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h,
                                 SWP_FRAMECHANGED | SWP_NOZORDER | SWP_NOACTIVATE);
  
  if (!posResult) {
    LOG_WARN("SetWindowPos failed with error: {}", GetLastError());
  }
  
  // Verify position
  RECT consoleRect;
  if (GetWindowRect(_consoleWindow, &consoleRect)) {
    POINT topLeft = {consoleRect.left, consoleRect.top};
    ScreenToClient(parentHwnd, &topLeft);
    LOG_INFO("Console window positioned at ({}, {}) with size {}x{}", 
             topLeft.x, topLeft.y, 
             consoleRect.right - consoleRect.left, 
             consoleRect.bottom - consoleRect.top);
  }
  
  // Check window state before showing
  WINDOWPLACEMENT placement;
  placement.length = sizeof(WINDOWPLACEMENT);
  GetWindowPlacement(_consoleWindow, &placement);
  LOG_INFO("Console window placement: showCmd={}, flags={}", placement.showCmd, placement.flags);
  
  // Get window style
  style = GetWindowLongPtrW(_consoleWindow, GWL_STYLE);
  exStyle = GetWindowLongPtrW(_consoleWindow, GWL_EXSTYLE);
  LOG_INFO("Console window styles: style=0x{:X}, exStyle=0x{:X}", style, exStyle);
  LOG_INFO("Has WS_VISIBLE: {}, Has WS_CHILD: {}", (style & WS_VISIBLE) ? "YES" : "NO", (style & WS_CHILD) ? "YES" : "NO");
  
  // Try multiple methods to show the window
  LOG_INFO("Attempting to show console window with multiple methods");
  ShowWindow(_consoleWindow, SW_SHOWNOACTIVATE);
  ShowWindow(_consoleWindow, SW_SHOW);
  SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h,
               SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_FRAMECHANGED);
  
  // Force window to repaint and update
  InvalidateRect(_consoleWindow, nullptr, TRUE);
  UpdateWindow(_consoleWindow);
  RedrawWindow(_consoleWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW | RDW_ALLCHILDREN | RDW_ERASE);
  
  // Send explicit paint message
  SendMessageW(_consoleWindow, WM_PAINT, 0, 0);
  
  // Check if window region is set properly
  HRGN region = CreateRectRgn(0, 0, w, h);
  SetWindowRgn(_consoleWindow, region, TRUE);
  
  // Get client rect to verify it's not zero
  RECT clientRect;
  GetClientRect(_consoleWindow, &clientRect);
  LOG_INFO("Console client rect: ({},{}) to ({},{})", clientRect.left, clientRect.top, clientRect.right, clientRect.bottom);
  
  // Check if it's actually visible now
  BOOL nowVisible = IsWindowVisible(_consoleWindow);
  LOG_INFO("After show attempts, IsWindowVisible returns: {}", nowVisible);
  
  // Check for child windows - the actual rendering might happen in a child
  LOG_INFO("Checking for child windows of console...");
  struct EnumChildData {
    int count;
  };
  EnumChildData childData = {0};
  
  EnumChildWindows(_consoleWindow, [](HWND hwnd, LPARAM lParam) -> BOOL {
    auto* data = reinterpret_cast<EnumChildData*>(lParam);
    data->count++;
    
    RECT childRect;
    GetWindowRect(hwnd, &childRect);
    POINT topLeft = {childRect.left, childRect.top};
    ScreenToClient(GetParent(hwnd), &topLeft);
    
    char className[256];
    GetClassNameA(hwnd, className, sizeof(className));
    
    LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_STYLE);
    BOOL visible = IsWindowVisible(hwnd);
    
    LOG_INFO("  Child window {}: class='{}', pos=({},{}), size={}x{}, visible={}, style=0x{:X}",
             data->count, className, topLeft.x, topLeft.y,
             childRect.right - childRect.left, childRect.bottom - childRect.top,
             visible, style);
    
    // Try to show and resize this child window too
    ShowWindow(hwnd, SW_SHOW);
    
    return TRUE;
  }, reinterpret_cast<LPARAM>(&childData));
  
  LOG_INFO("Found {} child windows", childData.count);
  
  // Try to attach to the console and write directly to trigger rendering
  if (AttachConsole(_processInfo.dwProcessId)) {
    LOG_INFO("Successfully attached to console, writing initial output...");
    
    // Get console output handle and write to it
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole != INVALID_HANDLE_VALUE) {
      const char* msg = "\nPowerShell Console Ready\n";
      DWORD written;
      WriteConsoleA(hConsole, msg, strlen(msg), &written, nullptr);
      FlushConsoleInputBuffer(GetStdHandle(STD_INPUT_HANDLE));
    }
    
    FreeConsole();
    LOG_INFO("Detached from console after writing");
  } else {
    LOG_WARN("Failed to attach to console for writing, error: {}", GetLastError());
  }
  
  // Send focus message to console window
  SetFocus(_consoleWindow);
  SetForegroundWindow(_consoleWindow);
  
  // Force console to think it's active
  SendMessageW(_consoleWindow, WM_SETFOCUS, 0, 0);
  SendMessageW(_consoleWindow, WM_ACTIVATE, WA_ACTIVE, 0);
  
  // Force a repaint of the parent widget
  update();
  repaint();
  
  // Send an initial command to activate the console
  QTimer::singleShot(100, this, [this]() {
    if (_consoleWindow && IsWindow(_consoleWindow)) {
      LOG_INFO("Sending initial command to console to activate it");
      executeCommand("Write-Host 'Console initialized' -ForegroundColor Green");
    }
  });

  LOG_INFO("Console window embedded successfully");

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::embedConsoleWithRetry(int attempt) {
  LOG_PERF(0, "Enter");
  
  if (!_processRunning) {
    LOG_PERF(0, "Exit");
    return;
  }
  
  if (attempt > 10) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to embed console after 10 attempts");
    if (!_inDestructor) {
      emit errorOccurred("Failed to embed console window after multiple attempts");
    }
    LOG_PERF(0, "Exit");
    return;
  }
  
  // Wait a bit more on first few attempts for console to initialize
  const int delay = (attempt == 0) ? 200 : (attempt < 3) ? 150 : 100;
  
  QTimer::singleShot(delay, this, [this, attempt]() {
    embedConsole();
    
    // If embedding didn't succeed (no console window found), retry
    if (!_consoleWindow) {
      LOG_WARN("Console window not found yet, retrying... (attempt {})", attempt + 1);
      embedConsoleWithRetry(attempt + 1);
    } else {
      LOG_INFO("Console successfully embedded after {} attempts", attempt + 1);
      // Start the visibility timer once console is embedded
      if (_visibilityTimer) {
        _visibilityTimer->start();
      }
      
      // Force immediate visibility check
      QTimer::singleShot(50, this, [this]() {
        if (_consoleWindow && IsWindow(_consoleWindow)) {
          LOG_INFO("Post-embedding visibility check");
          ShowWindow(_consoleWindow, SW_SHOW);
          ensureConsoleVisible();
          update();
        }
      });
    }
  });
  
  LOG_PERF(0, "Exit");
}

void PowerShellWidget::ensureConsoleVisible() {
  if (!_consoleWindow || !IsWindow(_consoleWindow)) {
    return;
  }
  
  const int w = width();
  const int h = height();
  
  if (w <= 0 || h <= 0) {
    return;
  }
  
  // Check if console is visible
  const BOOL consoleVisible = IsWindowVisible(_consoleWindow);
  if (!consoleVisible) {
    LOG_WARN("Console window was hidden, making it visible again");
    ShowWindow(_consoleWindow, SW_SHOWNA);
    SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h, SWP_SHOWWINDOW | SWP_NOMOVE | SWP_NOSIZE);
    InvalidateRect(_consoleWindow, nullptr, TRUE);
    UpdateWindow(_consoleWindow);
    update();
  }
  
  // Ensure console is child of this widget
  HWND currentParent = GetParent(_consoleWindow);
  HWND expectedParent = reinterpret_cast<HWND>(winId());
  if (currentParent != expectedParent) {
    LOG_WARN("Console window parent changed from {} to {}, re-parenting", 
             reinterpret_cast<uintptr_t>(currentParent),
             reinterpret_cast<uintptr_t>(expectedParent));
    SetParent(_consoleWindow, expectedParent);
    ShowWindow(_consoleWindow, SW_SHOW);
    SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h, SWP_SHOWWINDOW);
  }
  
  // Ensure proper size and position
  RECT consoleRect;
  if (GetWindowRect(_consoleWindow, &consoleRect)) {
    POINT topLeft = {consoleRect.left, consoleRect.top};
    ScreenToClient(expectedParent, &topLeft);
    
    const int consoleW = consoleRect.right - consoleRect.left;
    const int consoleH = consoleRect.bottom - consoleRect.top;
    
    // Check if position or size is wrong
    if (topLeft.x != 0 || topLeft.y != 0 || abs(consoleW - w) > 5 || abs(consoleH - h) > 5) {
      SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h, 
                   SWP_SHOWWINDOW);
      InvalidateRect(_consoleWindow, nullptr, TRUE);
    }
  }
  
  // Ensure window style still has WS_VISIBLE
  LONG_PTR style = GetWindowLongPtrW(_consoleWindow, GWL_STYLE);
  if (!(style & WS_VISIBLE)) {
    LOG_WARN("Console window lost WS_VISIBLE style, restoring");
    style |= WS_VISIBLE;
    SetWindowLongPtrW(_consoleWindow, GWL_STYLE, style);
    ShowWindow(_consoleWindow, SW_SHOW);
  }
}

bool PowerShellWidget::executeCommand(const QString &command) {
  LOG_PERF(0, "Enter");

  if (!_processRunning || !_consoleWindow) {
    LOG_WARN("PowerShell process is not running or console not embedded");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Send the command to the console window by simulating keyboard input
  const QString fullCommand = command + "\n";

  for (const QChar &ch : fullCommand) {
    if (ch == '\n') {
      // Send Enter key
      PostMessageW(_consoleWindow, WM_CHAR, VK_RETURN, 0);
    } else {
      // Send character
      PostMessageW(_consoleWindow, WM_CHAR, ch.unicode(), 0);
    }
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool PowerShellWidget::isRunning() const {
  LOG_PERF(0, "Enter");

  if (!_processRunning) {
    LOG_PERF(0, "Exit");
    return false;
  }

  DWORD exitCode;
  if (GetExitCodeProcess(_processInfo.hProcess, &exitCode)) {
    LOG_PERF(0, "Exit");
    return exitCode == STILL_ACTIVE;
  }

  LOG_PERF(0, "Exit");

  return false;
}

void PowerShellWidget::restart() {
  LOG_PERF(0, "Enter");

  terminate();

  QTimer::singleShot(500, this, [this]() { startPowerShell(); });

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::terminate() {
  LOG_PERF(0, "Enter");

  cleanupProcess();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::cleanupProcess() {
  LOG_PERF(0, "Enter");

  // Stop visibility timer
  if (_visibilityTimer) {
    _visibilityTimer->stop();
  }

  if (_processRunning) {
    if (_consoleWindow) {
      // Unparent the console window before terminating
      SetParent(_consoleWindow, nullptr);
      _consoleWindow = nullptr;
    }

    if (_processInfo.hProcess) {
      // Try to terminate gracefully first
      if (WaitForSingleObject(_processInfo.hProcess, 1000) == WAIT_TIMEOUT) {
        // Force terminate if it doesn't close gracefully
        TerminateProcess(_processInfo.hProcess, 0);
      }
      CloseHandle(_processInfo.hProcess);
    }

    if (_processInfo.hThread) {
      CloseHandle(_processInfo.hThread);
    }

    ZeroMemory(&_processInfo, sizeof(_processInfo));
    _processRunning = false;

    // Only emit signal if not in destructor
    if (!_inDestructor) {
      emit processStopped();
    }
    LOG_INFO("PowerShell process terminated");
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");

  QWidget::resizeEvent(event);

  if (_consoleWindow && IsWindow(_consoleWindow)) {
    const int w = event->size().width();
    const int h = event->size().height();
    
    LOG_INFO("resizeEvent: New size={}x{}", w, h);
    
    if (w > 0 && h > 0) {
      SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h,
                   SWP_SHOWWINDOW | SWP_FRAMECHANGED);
      UpdateWindow(_consoleWindow);
      RedrawWindow(_consoleWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
      
      // Ensure it's visible
      if (!IsWindowVisible(_consoleWindow)) {
        LOG_WARN("resizeEvent: Console not visible, forcing show");
        ShowWindow(_consoleWindow, SW_SHOW);
        BringWindowToTop(_consoleWindow);
      }
    }
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::showEvent(QShowEvent *event) {
  LOG_PERF(0, "Enter");

  QWidget::showEvent(event);

  if (_consoleWindow && IsWindow(_consoleWindow)) {
    const int w = width() > 0 ? width() : 800;
    const int h = height() > 0 ? height() : 600;
    
    LOG_INFO("showEvent: Widget size={}x{}, making console visible", w, h);
    BOOL wasVisible = IsWindowVisible(_consoleWindow);
    LOG_INFO("showEvent: Console was visible: {}", wasVisible);
    
    // Multiple show attempts
    ShowWindow(_consoleWindow, SW_SHOWNA);
    ShowWindow(_consoleWindow, SW_SHOW);
    SetWindowPos(_consoleWindow, HWND_TOP, 0, 0, w, h,
                 SWP_SHOWWINDOW | SWP_FRAMECHANGED);
    UpdateWindow(_consoleWindow);
    RedrawWindow(_consoleWindow, nullptr, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
    BringWindowToTop(_consoleWindow);
    
    BOOL nowVisible = IsWindowVisible(_consoleWindow);
    LOG_INFO("showEvent: Console now visible: {}", nowVisible);
    
    // Trigger a repaint
    update();
    
    // Start visibility timer
    if (_visibilityTimer) {
      _visibilityTimer->start();
    }
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::hideEvent(QHideEvent *event) {
  LOG_PERF(0, "Enter");

  QWidget::hideEvent(event);

  // Stop visibility timer
  if (_visibilityTimer) {
    _visibilityTimer->stop();
  }

  // Hide the console window when the widget is hidden
  if (_consoleWindow && IsWindow(_consoleWindow)) {
    ShowWindow(_consoleWindow, SW_HIDE);
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::closeEvent(QCloseEvent *event) {
  LOG_PERF(0, "Enter");

  cleanupProcess();
  QWidget::closeEvent(event);

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::paintEvent(QPaintEvent *event) {
  QWidget::paintEvent(event);
  
  // The visibility timer will handle ensuring console is visible
  // Just trigger a manual check here
  if (_consoleWindow && IsWindow(_consoleWindow) && isVisible()) {
    ensureConsoleVisible();
  }
}

} // namespace GuiManager::Widgets::Win32
