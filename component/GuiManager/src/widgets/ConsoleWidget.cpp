#include "widgets/ConsoleWidget.h"
#ifdef Q_OS_WIN
#include "widgets/win32/PowerShellWidget.h"
#endif

#include <StylesheetManager.h>

#include <QColor>
#include <QEvent>

#ifdef Q_OS_UNIX
#include "widgets/linux/BashShellWidget.h"
#endif

#include <Log.h>

#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

#define THREAD_ID "ConsoleWidget"

namespace {

struct ConsoleThemeColors {
  QColor titleBarBackground;
  QColor titleBarText;
  QColor titleBarButtonHover;
  QColor titleBarButtonPressed;
};

ConsoleThemeColors consoleThemeColors() {
  switch (StylesheetManager::StylesheetManager::currentStylesheet()) {
  case StylesheetManager::StylesheetKey::SourcererLight:
    return ConsoleThemeColors{QColor(245, 235, 220), QColor(32, 32, 32),
                              QColor(230, 218, 198), QColor(214, 198, 176)};
  case StylesheetManager::StylesheetKey::SourcererPurple:
    return ConsoleThemeColors{QColor(43, 39, 53), QColor(232, 230, 238),
                              QColor(62, 58, 72), QColor(55, 70, 89)};
  case StylesheetManager::StylesheetKey::SourcererBlue:
    return ConsoleThemeColors{QColor(26, 36, 53), QColor(230, 236, 245),
                              QColor(42, 55, 80), QColor(32, 84, 114)};
  case StylesheetManager::StylesheetKey::SourcererMonokai:
    return ConsoleThemeColors{QColor(39, 40, 34), QColor(248, 248, 242),
                              QColor(62, 61, 50), QColor(73, 72, 62)};
  case StylesheetManager::StylesheetKey::SourcererGruvbox:
    return ConsoleThemeColors{QColor(40, 40, 40), QColor(235, 219, 178),
                              QColor(60, 56, 54), QColor(80, 73, 69)};
  case StylesheetManager::StylesheetKey::SourcererSolarized:
    return ConsoleThemeColors{QColor(0, 43, 54), QColor(131, 148, 150),
                              QColor(7, 54, 66), QColor(14, 74, 90)};
  case StylesheetManager::StylesheetKey::SourcererDracula:
    return ConsoleThemeColors{QColor(40, 42, 54), QColor(248, 248, 242),
                              QColor(50, 52, 66), QColor(68, 71, 90)};
  case StylesheetManager::StylesheetKey::SourcererAyu:
    return ConsoleThemeColors{QColor(15, 20, 25), QColor(230, 225, 207),
                              QColor(20, 25, 31), QColor(37, 51, 64)};
  default:
    return ConsoleThemeColors{QColor(45, 45, 48), QColor(255, 255, 255),
                              QColor(62, 62, 66), QColor(0, 122, 204)};
  }
}

QColor consoleCloseButtonIconColor(const ConsoleThemeColors &colors) {
  const int backgroundLuma = qRound(0.299 * colors.titleBarBackground.red() +
                                    0.587 * colors.titleBarBackground.green() +
                                    0.114 * colors.titleBarBackground.blue());

  // Use a neutral gray that stays readable on both themes.
  return backgroundLuma < 128 ? QColor(220, 220, 220) : QColor(80, 80, 80);
}

} // namespace

namespace GuiManager::Widgets {

ConsoleWidget::ConsoleWidget(QWidget *parent)
    : QWidget(parent), _titleBar(nullptr), _iconLabel(nullptr),
      _titleLabel(nullptr), _closeButton(nullptr)
#ifdef Q_OS_WIN
      ,
      _powerShellWidget(nullptr)
#endif
#ifdef Q_OS_UNIX
      ,
      _bashShellWidget(nullptr)
#endif
{
  LOG_PERF(0, "Enter");

  // Create main layout
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);

  const ConsoleThemeColors colors = consoleThemeColors();

  // Create title bar
  _titleBar = new QWidget(this);
  _titleBar->setStyleSheet(QString("background-color: %1; padding: 4px;")
                               .arg(colors.titleBarBackground.name()));
  _titleBar->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  _titleBar->setFixedHeight(28); // Fixed height for title bar

  // Add terminal icon in the top left corner of the title bar
  _iconLabel = new QLabel(_titleBar);
  _iconLabel->setFixedSize(16, 16);
  _iconLabel->setScaledContents(true);
  _iconLabel->setStyleSheet("background: transparent; padding: 0px;");
  updateTitleIcon(colors.titleBarText);

  // Add title label
  _titleLabel = new QLabel("Console", _titleBar);
  _titleLabel->setStyleSheet(
      QString("color: %1; font-weight: bold; font-size: 12px;")
          .arg(colors.titleBarText.name()));
  _titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  auto *titleBarLayout = new QHBoxLayout(_titleBar);
  titleBarLayout->setContentsMargins(8, 4, 4, 4);
  titleBarLayout->setSpacing(4);
  titleBarLayout->addWidget(_iconLabel);
  titleBarLayout->addWidget(_titleLabel);
  titleBarLayout->addStretch();

  // Create minimize button with icon
  _closeButton = new QPushButton(_titleBar);
  _closeButton->setFlat(true);
  _closeButton->setFixedSize(12, 12);
  _closeButton->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":/icons/close_button.png"),
      consoleCloseButtonIconColor(colors)));
  _closeButton->setIconSize(QSize(12, 12));
  _closeButton->setStyleSheet(
      QString("QPushButton { background-color: transparent; border: none; "
              "border-radius: 2px; }"
              "QPushButton:hover { background-color: %1; }"
              "QPushButton:pressed { background-color: %2; }")
          .arg(colors.titleBarButtonHover.name(),
               colors.titleBarButtonPressed.name()));
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
#elif defined(Q_OS_UNIX)
  // Create and add Bash shell widget
  _bashShellWidget = new Linux::BashShellWidget(this);
  _bashShellWidget->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Expanding);
  layout->addWidget(_bashShellWidget,
                    1); // Stretch factor 1 to take remaining space

  // Connect signals
  connect(_bashShellWidget, &Linux::BashShellWidget::processStarted, this,
          &ConsoleWidget::consoleStarted);
  connect(_bashShellWidget, &Linux::BashShellWidget::processStopped, this,
          &ConsoleWidget::consoleStopped);
  connect(_bashShellWidget, &Linux::BashShellWidget::errorOccurred, this,
          &ConsoleWidget::consoleError);
#else
  // On unsupported platforms, show a message
  auto *label =
      new QLabel("Terminal console is not available on this platform", this);
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
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    _bashShellWidget->init();
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
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    bool result = _bashShellWidget->executeCommand(command);
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
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    return _bashShellWidget->isRunning();
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
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    _bashShellWidget->restart();
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
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    _bashShellWidget->setWorkingDirectory(path);
  }
#endif

  LOG_PERF(0, "Exit");
}

void ConsoleWidget::updateTitleIcon(const QColor &color) {
  if (!_iconLabel) {
    return;
  }

  _iconLabel->setPixmap(StylesheetManager::StylesheetManager::renderSvgToPixmap(
      QStringLiteral(":/icons/terminal.svg"), color));
}

void ConsoleWidget::applyTheme() {
  const ConsoleThemeColors colors = consoleThemeColors();

  if (_titleBar) {
    _titleBar->setStyleSheet(QString("background-color: %1; padding: 4px;")
                                 .arg(colors.titleBarBackground.name()));
  }

  if (_titleLabel) {
    _titleLabel->setStyleSheet(
        QString("color: %1; font-weight: bold; font-size: 12px;")
            .arg(colors.titleBarText.name()));
  }

  updateTitleIcon(colors.titleBarText);

  if (_closeButton) {
    _closeButton->setStyleSheet(
        QString("QPushButton { background-color: transparent; border: none; "
                "border-radius: 2px; }"
                "QPushButton:hover { background-color: %1; }"
                "QPushButton:pressed { background-color: %2; }")
            .arg(colors.titleBarButtonHover.name(),
                 colors.titleBarButtonPressed.name()));
  }

#ifdef Q_OS_WIN
  if (_powerShellWidget) {
    _powerShellWidget->refreshTheme();
  }
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    _bashShellWidget->refreshTheme();
  }
#endif
}

void ConsoleWidget::changeEvent(QEvent *event) {
  if (!event) {
    QWidget::changeEvent(event);
    return;
  }

  switch (event->type()) {
  case QEvent::StyleChange:
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
    applyTheme();
    break;
  default:
    break;
  }

  QWidget::changeEvent(event);
}
#ifdef Q_OS_WIN
Win32::PowerShellWidget *ConsoleWidget::getPowerShellWidget() const {
  return _powerShellWidget;
}
#endif

#ifdef Q_OS_UNIX
Linux::BashShellWidget *ConsoleWidget::getBashShellWidget() const {
  return _bashShellWidget;
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
#elif defined(Q_OS_UNIX)
  if (_bashShellWidget) {
    // Use QTimer::singleShot to ensure the widget is fully visible before
    // setting focus
    QTimer::singleShot(0, _bashShellWidget,
                       [this]() { _bashShellWidget->setFocusToInput(); });
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
