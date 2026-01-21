#include "widgets/ConsoleWidget.h"
#ifdef Q_OS_WIN
#include "widgets/win32/PowerShellWidget.h"
#endif

#include <Log.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#define THREAD_ID "ConsoleWidget"

namespace GuiManager::Widgets {

ConsoleWidget::ConsoleWidget(QWidget *parent)
    : QWidget(parent), _titleBar(nullptr), _closeButton(nullptr)
#ifdef Q_OS_WIN
      ,
      _powerShellWidget(nullptr)
#endif
{
  LOG_PERF(0, "Enter");

  // Create main layout
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  // Create title bar
  _titleBar = new QWidget(this);
  _titleBar->setStyleSheet("background-color: #2d2d30; padding: 4px;");
  _titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  _titleBar->setFixedHeight(28); // Fixed height for title bar

  // Add title label
  auto *titleLabel = new QLabel("PowerShell Console", _titleBar);
  titleLabel->setStyleSheet(
      "color: white; font-weight: bold; font-size: 12px;");
  titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *titleBarLayout = new QHBoxLayout(_titleBar);
  titleBarLayout->setContentsMargins(8, 4, 4, 4);
  titleBarLayout->setSpacing(4);
  titleBarLayout->addWidget(titleLabel);
  titleBarLayout->addStretch();

  // Create minimize button with icon
  _closeButton = new QPushButton(_titleBar);
  _closeButton->setFlat(true);
  _closeButton->setFixedSize(12, 12);
  _closeButton->setIcon(QIcon(":/icons/close_button.png"));
  _closeButton->setIconSize(QSize(12, 12));
  _closeButton->setStyleSheet(
      "QPushButton { background-color: transparent; border: none; "
      "border-radius: 2px; }"
      "QPushButton:hover { background-color: #3e3e42; }"
      "QPushButton:pressed { background-color: #007acc; }");
  _closeButton->setCursor(Qt::ArrowCursor);

  titleBarLayout->addWidget(_closeButton);

  layout->addWidget(_titleBar);

  // Connect minimize button
  connect(_closeButton, &QPushButton::clicked, this,
          &ConsoleWidget::onCloseClicked);

#ifdef Q_OS_WIN
  // Create and add PowerShell widget
  _powerShellWidget = new Win32::PowerShellWidget(this);
  _powerShellWidget->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Expanding);
  layout->addWidget(_powerShellWidget,
                    1); // Stretch factor 1 to take remaining space

  // Connect signals
  connect(_powerShellWidget, &Win32::PowerShellWidget::processStarted, this,
          &ConsoleWidget::consoleStarted);
  connect(_powerShellWidget, &Win32::PowerShellWidget::processStopped, this,
          &ConsoleWidget::consoleStopped);
  connect(_powerShellWidget, &Win32::PowerShellWidget::errorOccurred, this,
          &ConsoleWidget::consoleError);
#else
  // On non-Windows platforms, show a message
  auto *label =
      new QLabel("PowerShell console is only available on Windows", this);
  label->setAlignment(Qt::AlignCenter);
  label->setStyleSheet("color: gray; font-size: 14px;");
  layout->addWidget(label);
#endif

  LOG_PERF(0, "Exit");
}

ConsoleWidget::~ConsoleWidget() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void ConsoleWidget::init() {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    _powerShellWidget->init();
  }
#endif

  LOG_PERF(0, "Exit");
}

bool ConsoleWidget::executeCommand(const QString &command) {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    bool result = _powerShellWidget->executeCommand(command);
    LOG_PERF(0, "Exit");
    return result;
  }
#endif

  LOG_PERF(0, "Exit");
  return false;
}

bool ConsoleWidget::isConsoleRunning() const {
#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    return _powerShellWidget->isRunning();
  }
#endif
  return false;
}

void ConsoleWidget::restartConsole() {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    _powerShellWidget->restart();
  }
#endif

  LOG_PERF(0, "Exit");
}
void ConsoleWidget::setWorkingDirectory(const QString &path) {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    _powerShellWidget->setWorkingDirectory(path);
  }
#endif

  LOG_PERF(0, "Exit");
}
#ifdef Q_OS_WIN
Win32::PowerShellWidget *ConsoleWidget::getPowerShellWidget() const {
  return _powerShellWidget;
}
#endif

void ConsoleWidget::setFocusToConsole() {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    // Use QTimer::singleShot to ensure the widget is fully visible before
    // setting focus
    QTimer::singleShot(0, _powerShellWidget,
                       [this]() { _powerShellWidget->setFocusToInput(); });
  }
#endif

  LOG_PERF(0, "Exit");
}

void ConsoleWidget::onCloseClicked() {
  LOG_PERF(0, "Enter");

  // Hide the console widget
  hide();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
