#pragma once

#include <QPushButton>
#include <QWidget>

namespace GuiManager::Widgets {

namespace Win32 {
class PowerShellWidget;
}

/**
 * @brief A widget that displays an embedded PowerShell console
 *
 * This widget provides a container for the PowerShellWidget,
 * making it easy to integrate a PowerShell console into the application.
 */
class ConsoleWidget : public QWidget {
  Q_OBJECT

public:
  explicit ConsoleWidget(QWidget *parent = nullptr);
  ~ConsoleWidget() override;

  /**
   * @brief Executes a PowerShell command in the embedded console
   * @param command The PowerShell command to execute
   * @return true if command was sent successfully, false otherwise
   */
  bool executeCommand(const QString &command);

  /**
   * @brief Checks if the PowerShell console is running
   * @return true if running, false otherwise
   */
  bool isConsoleRunning() const;

  /**
   * @brief Restarts the PowerShell console
   */
  void restartConsole();

#ifdef Q_OS_WIN
  /**
   * @brief Gets the PowerShell widget instance
   * @return Pointer to the PowerShellWidget, or nullptr if not available
   */
  Win32::PowerShellWidget *getPowerShellWidget() const;
#endif

signals:
  void consoleStarted();
  void consoleStopped();
  void consoleError(const QString &error);

private slots:
  void onCloseClicked();

private:
  QWidget *_titleBar;
  QPushButton *_closeButton;

#ifdef Q_OS_WIN
  Win32::PowerShellWidget *_powerShellWidget;
#endif
};

} // namespace GuiManager::Widgets
