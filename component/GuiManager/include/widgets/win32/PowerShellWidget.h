#pragma once

#include <QWidget>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace GuiManager::Widgets::Win32 {

/**
 * @brief A QWidget that embeds a PowerShell console on Windows platform
 * 
 * This widget creates a PowerShell process and embeds its console window
 * within the Qt widget hierarchy using Windows API.
 */
class PowerShellWidget : public QWidget {
  Q_OBJECT

public:
  explicit PowerShellWidget(QWidget *parent = nullptr);
  ~PowerShellWidget() override;

  /**
   * @brief Executes a PowerShell command
   * @param command The PowerShell command to execute
   * @return true if command was sent successfully, false otherwise
   */
  bool executeCommand(const QString &command);

  /**
   * @brief Checks if the PowerShell process is running
   * @return true if running, false otherwise
   */
  bool isRunning() const;

  /**
   * @brief Restarts the PowerShell process
   */
  void restart();

  /**
   * @brief Terminates the PowerShell process
   */
  void terminate();

signals:
  void processStarted();
  void processStopped();
  void errorOccurred(const QString &error);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void hideEvent(QHideEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;

private:
  void startPowerShell();
  void embedConsole();
  void embedConsoleWithRetry(int attempt);
  void cleanupProcess();
  void ensureConsoleVisible();
  
#ifdef Q_OS_WIN
  HWND _consoleWindow;
  PROCESS_INFORMATION _processInfo;
  bool _processRunning;
  int _embedRetries;
  QTimer *_visibilityTimer;
  bool _inDestructor;
#endif
};

} // namespace GuiManager::Widgets::Win32
