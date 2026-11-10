// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QPushButton>
#include <QWidget>

class QEvent;
class QLabel;

namespace GuiManager::Widgets {

namespace Win32 {
class PowerShellWidget;
}

namespace Linux {
class BashShellWidget;
}

/**
 * @brief A widget that displays an embedded terminal console
 *
 * This widget provides a container for platform-specific terminal widgets:
 * - PowerShellWidget on Windows
 * - BashShellWidget on Linux and macOS
 */
class ConsoleWidget : public QWidget {
  Q_OBJECT

public:
  explicit ConsoleWidget(QWidget *parent = nullptr);
  ~ConsoleWidget() override;

  /**
   * @brief Initializes the console widget and starts the shell
   */
  void init();

  /**
   * @brief Executes a command in the embedded console
   * @param command The command to execute
   * @return true if command was sent successfully, false otherwise
   */
  bool executeCommand(const QString &command);

  /**
   * @brief Checks if the console is running
   * @return true if running, false otherwise
   */
  bool isConsoleRunning() const;

  /**
   * @brief Restarts the console
   */
  void restartConsole();

  /**
   * @brief Sets focus to the console input
   */
  void setFocusToConsole();

  /**
   * @brief Sets the working directory for the console
   * @param path The working directory path
   */
  void setWorkingDirectory(const QString &path);

protected:
  void changeEvent(QEvent *event) override;

private:
  void applyTheme();
  void updateTitleIcon(const QColor &color);

#ifdef Q_OS_WIN
  /**
   * @brief Gets the PowerShell widget instance (Windows only)
   * @return Pointer to the PowerShellWidget, or nullptr if not available
   */
  Win32::PowerShellWidget *getPowerShellWidget() const;
#endif

#ifdef Q_OS_UNIX
  /**
   * @brief Gets the Bash shell widget instance (Linux only)
   * @return Pointer to the BashShellWidget, or nullptr if not available
   */
  Linux::BashShellWidget *getBashShellWidget() const;
#endif

signals:
  void consoleStarted();
  void consoleStopped();
  void consoleError(const QString &error);

private slots:
  void onCloseClicked();

private:
  QWidget *_titleBar;
  QLabel *_iconLabel;
  QLabel *_titleLabel;
  QPushButton *_closeButton;

#ifdef Q_OS_WIN
  Win32::PowerShellWidget *_powerShellWidget;
#endif

#ifdef Q_OS_UNIX
  Linux::BashShellWidget *_bashShellWidget;
#endif
};

} // namespace GuiManager::Widgets
