#include "widgets/win32/PowerShellWidget.h"
#include "Log.h"

#include <QFont>
#include <QKeyEvent>
#include <QScrollBar>
#include <QTimer>
#include <QVBoxLayout>

#define THREAD_ID "PowerShellWidget"

using namespace GuiManager::Widgets::Win32;

PowerShellWidget::PowerShellWidget(QWidget *parent)
    : QWidget(parent), _process(nullptr), _commandBuffer(),
      _commandCursorPos(0), _waitingForPrompt(false), _historyIndex(-1),
      _waitingForCompletion(false), _completionIndex(-1) {
  LOG_PERF(0, "Enter");

  // Create completion timer
  _completionTimer = new QTimer(this);
  _completionTimer->setSingleShot(true);
  _completionTimer->setInterval(150);
  connect(_completionTimer, &QTimer::timeout, this,
          &PowerShellWidget::processCompletionResults);

  // Create prompt request timer
  _promptTimer = new QTimer(this);
  _promptTimer->setSingleShot(true);
  _promptTimer->setInterval(
      1000); // 1 second delay to handle interactive programs
  connect(_promptTimer, &QTimer::timeout, this,
          &PowerShellWidget::requestPrompt);

  // Create layout
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create text display widget
  _textDisplay = new QPlainTextEdit(this);
  _textDisplay->setReadOnly(false); // Allow cursor to be visible
  _textDisplay->setWordWrapMode(QTextOption::WrapAnywhere);
  _textDisplay->setFocusPolicy(Qt::StrongFocus);
  _textDisplay->setTabChangesFocus(false); // Disable tab focus navigation

  // Install event filter to capture keyboard input
  _textDisplay->installEventFilter(this);

  // Set console-like font
  QFont font("Consolas", 10);
  font.setStyleHint(QFont::Monospace);
  _textDisplay->setFont(font);

  // Dark console theme with visible cursor
  _textDisplay->setStyleSheet("QPlainTextEdit {"
                              "  background-color: #012456;"
                              "  color: #CCCCCC;"
                              "  border: none;"
                              "  padding: 4px;"
                              "}");

  layout->addWidget(_textDisplay);

  // Set focus to text display
  _textDisplay->setFocus();

  // Start PowerShell
  startPowerShell();

  LOG_PERF(0, "Exit");
}

PowerShellWidget::~PowerShellWidget() {
  LOG_PERF(0, "Enter");

  if (_process) {
    // Disconnect signals to avoid error notifications during shutdown
    _process->disconnect();

    // Try graceful termination first
    _process->write("exit\n");
    if (!_process->waitForFinished(1000)) {
      _process->kill();
      _process->waitForFinished(1000);
    }
    delete _process;
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::startPowerShell() {
  LOG_PERF(0, "Enter");

  _process = new QProcess(this);

  // Connect signals
  connect(_process, &QProcess::readyReadStandardOutput, this,
          &PowerShellWidget::onReadyReadStandardOutput);
  connect(_process, &QProcess::readyReadStandardError, this,
          &PowerShellWidget::onReadyReadStandardError);
  connect(_process, &QProcess::started, this,
          &PowerShellWidget::onProcessStarted);
  connect(_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &PowerShellWidget::onProcessFinished);
  connect(_process, &QProcess::errorOccurred, this,
          &PowerShellWidget::onProcessError);

  // Set environment variables for unbuffered output
  const QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  _process->setProcessEnvironment(env);

  // Start PowerShell in interactive mode with no logo
  _process->setProgram("powershell.exe");
  _process->setArguments({"-NoLogo", "-NoExit"});
  _process->setProcessChannelMode(
      QProcess::MergedChannels); // Merge stdout and stderr

  _process->start();

  if (!_process->waitForStarted(3000)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to start PowerShell");
    emit errorOccurred("Failed to start PowerShell");
  }

  LOG_PERF(0, "Exit");
}

bool PowerShellWidget::executeCommand(const QString &command) {
  LOG_PERF(0, "Enter");

  if (!_process || _process->state() != QProcess::Running) {
    LOG_PERF(0, "Exit");
    return false;
  }

  QString cmd = command;
  if (!cmd.endsWith('\n')) {
    cmd += "\n";
  }

  _process->write(cmd.toLocal8Bit());

  LOG_PERF(0, "Exit");

  return true;
}

bool PowerShellWidget::isRunning() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return _process && _process->state() == QProcess::Running;
}

void PowerShellWidget::restart() {
  LOG_PERF(0, "Enter");

  if (_process) {
    _process->kill();
    _process->waitForFinished(1000);
  }

  _textDisplay->clear();
  _commandBuffer.clear();

  startPowerShell();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::terminate() {
  LOG_PERF(0, "Enter");

  if (_process) {
    _process->terminate();

    if (!_process->waitForFinished(2000)) {
      _process->kill();
    }
  }

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

bool PowerShellWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == _textDisplay && event->type() == QEvent::KeyPress) {
    const QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);

    if (_process && _process->state() == QProcess::Running) {
      // Handle special keys
      if (keyEvent->key() == Qt::Key_Return ||
          keyEvent->key() == Qt::Key_Enter) {
        const QString cmd = _commandBuffer;

        // Save to history if not empty
        if (!cmd.trimmed().isEmpty()) {
          _commandHistory.append(cmd);
          _historyIndex = _commandHistory.size();
        }

        LOG_INFO("Executing command: '{}'", cmd);

        appendOutput("\n", false); // Add newline after command

        _lastCommand = cmd; // Store for echo filtering
        _process->write((cmd + "\n").toUtf8());
        _commandBuffer.clear();
        _commandCursorPos = 0;

        // Start timer to request prompt after output stops
        _promptTimer->start();

        return true;
      } else if (keyEvent->key() == Qt::Key_Up) {
        // Navigate to previous command in history
        if (_historyIndex > 0) {
          _historyIndex--;

          // Clear current input from display
          for (int i = 0; i < _commandBuffer.length(); i++) {
            QTextCursor cursor = _textDisplay->textCursor();
            cursor.deletePreviousChar();
          }

          // Set command from history
          _commandBuffer = _commandHistory[_historyIndex];
          _commandCursorPos = _commandBuffer.length();

          appendOutput(_commandBuffer, false);
        }

        return true;
      } else if (keyEvent->key() == Qt::Key_Down) {
        // Navigate to next command in history
        if (_historyIndex < _commandHistory.size() - 1) {
          _historyIndex++;

          // Clear current input from display
          for (int i = 0; i < _commandBuffer.length(); i++) {
            QTextCursor cursor = _textDisplay->textCursor();
            cursor.deletePreviousChar();
          }

          // Set command from history
          _commandBuffer = _commandHistory[_historyIndex];
          _commandCursorPos = _commandBuffer.length();

          appendOutput(_commandBuffer, false);
        } else if (_historyIndex == _commandHistory.size() - 1) {
          // At the end of history, clear to empty
          _historyIndex = _commandHistory.size();

          // Clear current input from display
          for (int i = 0; i < _commandBuffer.length(); i++) {
            QTextCursor cursor = _textDisplay->textCursor();
            cursor.deletePreviousChar();
          }

          _commandBuffer.clear();
          _commandCursorPos = 0;
        }

        return true;
      } else if (keyEvent->key() == Qt::Key_Left) {
        // Move cursor left
        if (_commandCursorPos > 0) {
          _commandCursorPos--;

          QTextCursor cursor = _textDisplay->textCursor();
          cursor.movePosition(QTextCursor::Left);

          _textDisplay->setTextCursor(cursor);
        }

        return true;
      } else if (keyEvent->key() == Qt::Key_Right) {
        // Move cursor right
        if (_commandCursorPos < _commandBuffer.length()) {
          _commandCursorPos++;

          QTextCursor cursor = _textDisplay->textCursor();
          cursor.movePosition(QTextCursor::Right);

          _textDisplay->setTextCursor(cursor);
        }
        return true;
      } else if (keyEvent->key() == Qt::Key_Home) {
        // Move cursor to start of command
        _commandCursorPos = 0;

        QTextCursor cursor = _textDisplay->textCursor();
        // Move to end first, then back by command length to get to start of
        // command
        cursor.movePosition(QTextCursor::End);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor,
                            _commandBuffer.length());

        _textDisplay->setTextCursor(cursor);

        return true;
      } else if (keyEvent->key() == Qt::Key_End) {
        // Move cursor to end of command
        _commandCursorPos = _commandBuffer.length();

        QTextCursor cursor = _textDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);

        _textDisplay->setTextCursor(cursor);

        return true;
      } else if (keyEvent->key() == Qt::Key_Tab) {
        // Tab completion - cycle through matches
        LOG_INFO("Tab key pressed, commandBuffer: '{}'", _commandBuffer);

        // Extract the base command (everything before the last word)
        const int lastSpaceIdx = _commandBuffer.lastIndexOf(' ');
        const QString base = (lastSpaceIdx >= 0)
                                 ? _commandBuffer.left(lastSpaceIdx + 1)
                                 : QString();

        // Check if current buffer ends with a directory (has backslash at end)
        const bool endsWithBackslash =
            _commandBuffer.endsWith('\\') || _commandBuffer.endsWith('/');

        LOG_INFO(
            "Tab handler - base: '{}', _completionBase: '{}', "
            "endsWithBackslash: {}, stack size: {}, matches: {}, index: {}",
            base.toStdString(), _completionBase.toStdString(),
            endsWithBackslash, _completionStack.size(),
            _completionMatches.size(), _completionIndex);

        // If we already have completions for this base, cycle to next
        if (!_completionMatches.isEmpty() && _completionBase == base &&
            !endsWithBackslash) {
          // Calculate next index
          const int nextIndex =
              (_completionIndex + 1) % _completionMatches.size();

          // Check if we've wrapped around (reached end of current level)
          // Only pop if we actually have multiple items or if we've cycled at
          // least once
          if (nextIndex == 0 && _completionIndex > 0 &&
              !_completionStack.isEmpty()) {
            // Pop back to parent directory
            LOG_INFO("Reached end of subdirectory, returning to parent");

            // Stop any pending completion operations
            _completionTimer->stop();
            _waitingForCompletion = false;
            _completionBuffer.clear();

            CompletionState parentState = _completionStack.takeLast();

            // Clear current path from display
            const QString currentWord =
                (lastSpaceIdx >= 0) ? _commandBuffer.mid(lastSpaceIdx + 1)
                                    : _commandBuffer;
            for (int i = 0; i < currentWord.length(); i++) {
              QTextCursor cursor = _textDisplay->textCursor();
              cursor.deletePreviousChar();
            }

            // Restore parent state and move to next sibling
            _completionMatches = parentState.matches;
            _completionIsDirectory = parentState.isDirectory;
            _completionPartial = parentState.partial;
            _completionBase = parentState.base;
            _completionPathPrefix = parentState.pathPrefix;
            _completionIndex =
                (parentState.index + 1) % parentState.matches.size();

            // Check if we wrapped back to 0 at parent level - need to pop again
            if (_completionIndex == 0 && !_completionStack.isEmpty()) {
              LOG_INFO("Wrapped at parent level, popping again");
              // Recursively pop until we find a level with more siblings or
              // stack is empty
              while (_completionIndex == 0 && !_completionStack.isEmpty()) {
                parentState = _completionStack.takeLast();

                _completionMatches = parentState.matches;
                _completionIsDirectory = parentState.isDirectory;
                _completionPartial = parentState.partial;
                _completionBase = parentState.base;
                _completionPathPrefix = parentState.pathPrefix;
                _completionIndex =
                    (parentState.index + 1) % parentState.matches.size();

                LOG_INFO(
                    "Popped to level with stack size: {}, new index: {}/{}",
                    _completionStack.size(), _completionIndex,
                    _completionMatches.size());
              }

              // If we exhausted the entire stack, we're back at root - just
              // start over
              if (_completionStack.isEmpty() && _completionIndex == 0) {
                LOG_INFO("Exhausted all levels, cycling from start");
                _completionIndex = 0;
              }
            }

            const QString match = _completionMatches[_completionIndex];
            const bool isDir =
                _completionIndex < _completionIsDirectory.size() &&
                _completionIsDirectory[_completionIndex];

            LOG_INFO("Back to parent, cycling to match {}/{}: '{}' (isDir: {})",
                     _completionIndex + 1, _completionMatches.size(), match,
                     isDir);

            // Build full match with path prefix
            const QString fullMatch = _completionPathPrefix + match;

            _commandBuffer = _completionBase + fullMatch;
            _commandCursorPos = _commandBuffer.length();

            // If it's a directory, add backslash
            if (isDir && !match.endsWith('\\') && !match.endsWith('/')) {
              _commandBuffer += "\\";
              _commandCursorPos++;

              appendOutput(fullMatch + "\\", false);
            } else {
              appendOutput(fullMatch, false);
            }
            return true;
          }

          // Normal cycling within current level
          _completionIndex = nextIndex;

          const QString match = _completionMatches[_completionIndex];
          const bool isDir = _completionIndex < _completionIsDirectory.size() &&
                             _completionIsDirectory[_completionIndex];

          LOG_INFO("Cycling to match {}/{}: '{}' (isDir: {})",
                   _completionIndex + 1, _completionMatches.size(), match,
                   isDir);

          // Clear the current word from display
          const QString currentWord = (lastSpaceIdx >= 0)
                                          ? _commandBuffer.mid(lastSpaceIdx + 1)
                                          : _commandBuffer;
          for (int i = 0; i < currentWord.length(); i++) {
            QTextCursor cursor = _textDisplay->textCursor();
            cursor.deletePreviousChar();
          }

          // Build full match with path prefix
          const QString fullMatch = _completionPathPrefix + match;

          _commandBuffer = base + fullMatch;
          _commandCursorPos = _commandBuffer.length();

          // If it's a directory, add backslash and prepare for automatic
          // descent
          if (isDir && !match.endsWith('\\') && !match.endsWith('/')) {
            _commandBuffer += "\\";
            _commandCursorPos++;

            appendOutput(fullMatch + "\\", false);

            // For recursive traversal: automatically descend into directory on
            // next Tab by not returning here - the next Tab will detect
            // endsWithBackslash and enter
          } else {
            appendOutput(fullMatch, false);
          }
        } else if (endsWithBackslash) {
          // Entering a subdirectory - save parent state for recursive traversal
          const QString partial = (lastSpaceIdx >= 0)
                                      ? _commandBuffer.mid(lastSpaceIdx + 1)
                                      : _commandBuffer;

          if (!_completionMatches.isEmpty()) {
            CompletionState parentState;
            parentState.matches = _completionMatches;
            parentState.isDirectory = _completionIsDirectory;
            parentState.index = _completionIndex;
            parentState.partial = _completionPartial;
            parentState.base = _completionBase;
            parentState.pathPrefix = _completionPathPrefix;
            _completionStack.append(parentState);

            LOG_INFO(
                "Entering subdirectory, saved parent state (stack size: {})",
                _completionStack.size());
          }

          // Check if we're completing inside a subdirectory
          QString searchPath;
          QString searchPartial;
          int lastBackslash = partial.lastIndexOf('\\');
          if (lastBackslash < 0) {
            lastBackslash = partial.lastIndexOf('/');
          }

          if (lastBackslash >= 0) {
            // We have a path component
            searchPath = partial.left(lastBackslash + 1);
            searchPartial = partial.mid(lastBackslash + 1);
          } else {
            searchPath = "";
            searchPartial = partial;
          }

          _completionPartial = partial;
          _completionBase = base;
          _completionMatches.clear();
          _completionIsDirectory.clear();
          _completionIndex = -1;
          _completionBuffer.clear();
          _waitingForCompletion = true;

          // Use PowerShell to find matching files/folders with directory flag
          QString completionCmd;
          if (searchPath.isEmpty()) {
            // Search in current directory
            if (searchPartial.isEmpty()) {
              // No partial - show all items
              completionCmd = "Get-ChildItem -Force -ErrorAction "
                              "SilentlyContinue | Select-Object -First 50 | "
                              "ForEach-Object { if ($_.PSIsContainer) { "
                              "$_.Name + '|DIR' } else { $_.Name } }\n";
            } else {
              // With partial - search matching items
              completionCmd =
                  QString(
                      "Get-ChildItem -Force -ErrorAction SilentlyContinue | "
                      "Where-Object { $_.Name -like '%1*' } | "
                      "Select-Object -First 50 | "
                      "ForEach-Object { if ($_.PSIsContainer) { $_.Name + "
                      "'|DIR' } else { $_.Name } }\n")
                      .arg(searchPartial);
            }
          } else {
            // Search in subdirectory
            if (searchPartial.isEmpty()) {
              completionCmd =
                  QString("Get-ChildItem -Path '%1' -Force -ErrorAction "
                          "SilentlyContinue | "
                          "Select-Object -First 50 | "
                          "ForEach-Object { if ($_.PSIsContainer) { $_.Name + "
                          "'|DIR' } else { $_.Name } }\n")
                      .arg(searchPath);
            } else {
              completionCmd =
                  QString("Get-ChildItem -Path '%1' -Force -ErrorAction "
                          "SilentlyContinue | "
                          "Where-Object { $_.Name -like '%2*' } | "
                          "Select-Object -First 50 | "
                          "ForEach-Object { if ($_.PSIsContainer) { $_.Name + "
                          "'|DIR' } else { $_.Name } }\n")
                      .arg(searchPath, searchPartial);
            }
          }

          LOG_INFO("Sending completion command: '{}'", completionCmd.left(100));

          _process->write(completionCmd.toLocal8Bit());

          // Start timer to process accumulated results
          _completionTimer->start();
        } else {
          // Starting fresh completion (not cycling, not entering subdirectory),
          // clear stack
          _completionStack.clear();

          QString partial = (lastSpaceIdx >= 0)
                                ? _commandBuffer.mid(lastSpaceIdx + 1)
                                : _commandBuffer;
          _completionPartial = partial;
          _completionBase = base;
          _completionMatches.clear();
          _completionIsDirectory.clear();
          _completionIndex = -1;
          _completionBuffer.clear();
          _completionPathPrefix.clear();
          _waitingForCompletion = true;

          // Extract just the filename part for matching (strip .\ or ./ prefix)
          QString searchPattern = partial;
          if (searchPattern.startsWith(".\\") ||
              searchPattern.startsWith("./")) {
            searchPattern = searchPattern.mid(2);
          }

          // Request completions for current directory
          QString completionCmd;
          if (searchPattern.isEmpty()) {
            completionCmd =
                "Get-ChildItem -Force -ErrorAction SilentlyContinue | "
                "Select-Object -First 50 | ForEach-Object { if "
                "($_.PSIsContainer) { $_.Name + '|DIR' } else { $_.Name } }\n";
          } else {
            completionCmd =
                QString("Get-ChildItem -Force -ErrorAction SilentlyContinue | "
                        "Where-Object { $_.Name -like '%1*' } | "
                        "Select-Object -First 50 | "
                        "ForEach-Object { if ($_.PSIsContainer) { $_.Name + "
                        "'|DIR' } else { $_.Name } }\n")
                    .arg(searchPattern);
          }

          LOG_INFO("Sending completion command: '{}'", completionCmd.left(100));
          _process->write(completionCmd.toLocal8Bit());

          // Start timer to process accumulated results
          _completionTimer->start();
        }

        return true;
      } else if (keyEvent->key() == Qt::Key_Backspace) {
        if (!_commandBuffer.isEmpty()) {
          // Get current cursor position in display
          QTextCursor cursor = _textDisplay->textCursor();
          const int cursorPosInDisplay = cursor.position();
          const int docLength = _textDisplay->document()->characterCount() - 1;

          // Calculate position within command buffer
          _commandCursorPos =
              _commandBuffer.length() - (docLength - cursorPosInDisplay);
          if (_commandCursorPos < 0)
            _commandCursorPos = 0;
          if (_commandCursorPos > _commandBuffer.length())
            _commandCursorPos = _commandBuffer.length();

          if (_commandCursorPos > 0) {
            _commandBuffer.remove(_commandCursorPos - 1, 1);
            _commandCursorPos--;
            // Remove character from display
            cursor.deletePreviousChar();
          }

          // Clear completion state
          _completionMatches.clear();
          _completionIsDirectory.clear();
          _completionIndex = -1;
          _completionStack.clear();
        }

        return true;
      } else if (keyEvent->key() == Qt::Key_C &&
                 (keyEvent->modifiers() & Qt::ControlModifier)) {
        // Ctrl+C - clear current command and show new prompt
        if (!_commandBuffer.isEmpty()) {
          // Clear the command buffer visually by deleting all characters
          QTextCursor cursor = _textDisplay->textCursor();
          for (int i = 0; i < _commandBuffer.length(); i++) {
            cursor.deletePreviousChar();
          }
        }

        _commandBuffer.clear();
        _commandCursorPos = 0;

        // Display ^C indicator and new prompt
        appendOutput("^C\n", false);
        appendOutput("PS " + _currentDirectory + "> ", false);

        // Clear completion state
        _completionMatches.clear();
        _completionIsDirectory.clear();
        _completionIndex = -1;
        _completionStack.clear();

        return true;
      } else if (!keyEvent->text().isEmpty() && keyEvent->text()[0].isPrint()) {
        // Printable character - insert at cursor position
        const QString text = keyEvent->text();

        // Get current cursor position in display
        QTextCursor cursor = _textDisplay->textCursor();
        const int cursorPosInDisplay = cursor.position();
        const int docLength = _textDisplay->document()->characterCount() -
                              1; // -1 for trailing newline

        // Calculate position within command buffer (from end of document)
        _commandCursorPos =
            _commandBuffer.length() - (docLength - cursorPosInDisplay);
        if (_commandCursorPos < 0)
          _commandCursorPos = 0;
        if (_commandCursorPos > _commandBuffer.length())
          _commandCursorPos = _commandBuffer.length();

        // Insert into command buffer
        _commandBuffer.insert(_commandCursorPos, text);
        _commandCursorPos += text.length();

        // Insert into display at cursor
        cursor.insertText(text);

        // Clear completion state
        _completionMatches.clear();
        _completionIsDirectory.clear();
        _completionIndex = -1;
        _completionStack.clear();

        return true;
      }
    }
  }

  return QWidget::eventFilter(obj, event);
}

void PowerShellWidget::onReadyReadStandardOutput() {
  LOG_PERF(0, "Enter");

  if (_process) {
    const QByteArray data = _process->readAllStandardOutput();
    const QString text = QString::fromLocal8Bit(data);

    LOG_INFO("Received {} bytes from stdout: '{}', waitingForCompletion: {}",
             data.size(), text.left(100), _waitingForCompletion);

    // Check if this is the working directory output (for prompt)
    if (_waitingForPrompt) {
      const QString trimmed = text.trimmed();
      // Check if it's a path - should contain drive letter and colon
      if (trimmed.contains(":\\") && trimmed.length() < 200 &&
          !trimmed.contains('\n') && !trimmed.contains('>') &&
          !trimmed.startsWith("PS ")) {
        // This is the path for the prompt
        _waitingForPrompt = false;
        _currentDirectory = trimmed;

        appendOutput("\nPS " + _currentDirectory + "> ", false);

        LOG_PERF(0, "Exit");
        return;
      }
      // If we receive output that's clearly not a path (like Python banner),
      // cancel prompt wait only if it contains multiple lines or lots of text
      if (trimmed.length() > 250 || trimmed.contains('\n') ||
          trimmed.toLower().contains("python")) {
        _waitingForPrompt = false;
        _promptTimer->stop();
      } else {
        // For short output that doesn't match, just display it and keep waiting
        // This handles cases where path comes in chunks
      }
    }

    if (_waitingForCompletion) {
      // Accumulate completion results - DO NOT display
      _completionBuffer += text;
      // Restart timer to wait for more data
      _completionTimer->start();
    } else if (!text.trimmed().isEmpty()) {
      // Only display non-empty output (skip initial PowerShell prompt)
      // Skip if it looks like a PowerShell prompt (starts with PS and contains
      // path)
      const bool isPrompt = text.trimmed().startsWith("PS ") &&
                            text.contains(":\\") && text.contains(">");
      // Skip our internal command echo
      const bool isInternalCommand = text.contains("Write-Host -NoNewLine") ||
                                     text.contains("(Get-Location).Path");
      // Skip command echo - PowerShell echoes the command back
      const bool isCommandEcho =
          !_lastCommand.isEmpty() && text.trimmed() == _lastCommand.trimmed();
      if (isCommandEcho) {
        _lastCommand.clear(); // Clear after filtering
      }

      if (!isPrompt && !isInternalCommand && !isCommandEcho) {
        appendOutput(text, false);

        // Restart prompt timer since we're still receiving output
        if (_promptTimer->isActive()) {
          _promptTimer->start();
        }
      }
    }
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::onReadyReadStandardError() {
  LOG_PERF(0, "Enter");

  if (_process) {
    const QByteArray data = _process->readAllStandardError();
    const QString text = QString::fromLocal8Bit(data);

    LOG_INFO("Received {} bytes from stderr: '{}'", data.size(),
             text.left(100).toStdString());

    appendOutput(text, true);

    // Restart prompt timer since we're still receiving output
    if (_promptTimer->isActive()) {
      _promptTimer->start();
    }
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::requestPrompt() {
  LOG_PERF(0, "Enter");

  if (_process && _process->state() == QProcess::Running &&
      !_waitingForPrompt) {
    _waitingForPrompt = true;
    _process->write("Write-Host -NoNewLine ''; (Get-Location).Path\n");
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::onProcessStarted() {
  LOG_PERF(0, "Enter");
  LOG_INFO("PowerShell process started");

  // Display initial prompt
  QTimer::singleShot(100, this, [this]() {
    if (_process && _process->state() == QProcess::Running) {
      // Get current directory and display prompt
      // Use Write-Host with -NoNewLine to suppress command echo
      _waitingForPrompt = true;
      _process->write("Write-Host -NoNewLine ''; (Get-Location).Path\n");
    }
  });

  emit processStarted();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::onProcessFinished(int exitCode,
                                         QProcess::ExitStatus exitStatus) {
  LOG_PERF(0, "Enter");
  LOG_INFO("PowerShell process finished with exit code: {}", exitCode);

  appendOutput(QString("\n[Process exited with code %1]\n").arg(exitCode),
               true);

  emit processStopped();

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::onProcessError(QProcess::ProcessError error) {
  LOG_PERF(0, "Enter");

  QString errorMsg;
  switch (error) {
  case QProcess::FailedToStart:
    errorMsg = "Failed to start PowerShell";
    break;
  case QProcess::Crashed:
    errorMsg = "PowerShell crashed";
    break;
  case QProcess::Timedout:
    errorMsg = "PowerShell timed out";
    break;
  case QProcess::WriteError:
    errorMsg = "Write error";
    break;
  case QProcess::ReadError:
    errorMsg = "Read error";
    break;
  default:
    errorMsg = "Unknown error";
  }

  LOG_ERROR(Log::ErrorId::Gui, "PowerShell error: {}", errorMsg.toStdString());

  appendOutput(QString("\n[Error: %1]\n").arg(errorMsg), true);

  emit errorOccurred(errorMsg);

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::processCompletionResults() {
  LOG_PERF(0, "Enter");

  _waitingForCompletion = false;

  if (_completionBuffer.isEmpty()) {
    LOG_INFO("Completion: No results");
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_INFO("Processing completion buffer: '{}'",
           _completionBuffer.left(200).toStdString());

  const QStringList matches =
      _completionBuffer.trimmed().split("\n", Qt::SkipEmptyParts);

  // Clean up matches and filter out command echoes and prompt
  QStringList cleanMatches;
  QList<bool> isDirectory;

  for (const QString &match : matches) {
    const QString trimmed = match.trimmed();
    // Skip empty lines, command echoes, and PowerShell prompt (contains "PS "
    // and ends with ">")
    const bool isPrompt = trimmed.startsWith("PS ") && trimmed.contains(">") &&
                          trimmed.contains(":\\");

    if (!trimmed.isEmpty() && !trimmed.startsWith("Get-ChildItem") &&
        !trimmed.startsWith("Where-Object") &&
        !trimmed.startsWith("Select-Object") &&
        !trimmed.startsWith("ForEach-Object") && !isPrompt) {

      // Check if it's a directory (marked with |DIR)
      if (trimmed.endsWith("|DIR")) {
        cleanMatches.append(trimmed.left(trimmed.length() - 4));
        isDirectory.append(true);
      } else {
        cleanMatches.append(trimmed);
        isDirectory.append(false);
      }
    }
  }

  LOG_INFO("Completion: {} matches found", cleanMatches.size());

  if (cleanMatches.isEmpty()) {
    _completionMatches.clear();
    _completionIsDirectory.clear();
    _completionIndex = -1;

    LOG_PERF(0, "Exit");
    return;
  }

  // Store matches and apply first one
  _completionMatches = cleanMatches;
  _completionIsDirectory = isDirectory;
  _completionIndex = 0;

  const QString match = _completionMatches[0];
  const bool isDir = _completionIsDirectory[0];

  LOG_INFO("Applying first match: '{}' (isDir: {})", match.toStdString(),
           isDir);

  // Extract the path component from partial
  int lastBackslash = _completionPartial.lastIndexOf('\\');
  if (lastBackslash < 0) {
    lastBackslash = _completionPartial.lastIndexOf('/');
  }

  _completionPathPrefix = (lastBackslash >= 0)
                              ? _completionPartial.left(lastBackslash + 1)
                              : QString();

  // Clear the partial from display
  for (int i = 0; i < _completionPartial.length(); i++) {
    QTextCursor cursor = _textDisplay->textCursor();
    cursor.deletePreviousChar();
  }

  // Update command buffer with first completion
  const QString fullMatch = _completionPathPrefix + match;

  _commandBuffer = _completionBase + fullMatch;
  _commandCursorPos = _commandBuffer.length();

  // If it's a directory, add backslash
  if (isDir) {
    _commandBuffer += "\\";
    _commandCursorPos++;

    appendOutput(fullMatch + "\\", false);
  } else {
    appendOutput(fullMatch, false);
  }

  LOG_PERF(0, "Exit");
}

void PowerShellWidget::appendOutput(const QString &text, bool isError) {
  if (text.isEmpty()) {
    return;
  }

  QTextCursor cursor = _textDisplay->textCursor();
  cursor.movePosition(QTextCursor::End);

  // Set color based on error status
  QTextCharFormat format;
  if (isError) {
    format.setForeground(QColor(0xFF6B6B));
  } else {
    format.setForeground(QColor(0xCCCCCC));
  }

  cursor.insertText(text, format);

  _textDisplay->setTextCursor(cursor);
  _textDisplay->verticalScrollBar()->setValue(
      _textDisplay->verticalScrollBar()->maximum());
}
