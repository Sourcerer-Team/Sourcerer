#pragma once

#include <QPlainTextEdit>
#include <QProcess>
#include <QWidget>

namespace GuiManager::Widgets::Win32 {

/**
 * @brief A QWidget that embeds a PowerShell console using QProcess
 *
 * This widget creates a PowerShell process with redirected I/O,
 * captures its output, and displays it in a Qt text widget.
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
  void closeEvent(QCloseEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();
  void onProcessStarted();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void onProcessError(QProcess::ProcessError error);
  void processCompletionResults();
  void requestPrompt();

private:
  struct CompletionState {
    QStringList matches;
    QList<bool> isDirectory;
    int index;
    QString partial;
    QString base;
    QString pathPrefix;
  };

  void startPowerShell();
  void appendOutput(const QString &text, bool isError = false);
  void requestTabCompletion(const QString &partial);

  QProcess *_process;
  QPlainTextEdit *_textDisplay;
  QString _commandBuffer;
  int _commandCursorPos;
  bool _waitingForPrompt;
  QStringList _commandHistory;
  int _historyIndex;
  QString _currentDirectory;
  QString _lastCommand;
  bool _waitingForCompletion;
  QString _completionPartial;
  QStringList _completionMatches;
  QList<bool> _completionIsDirectory;
  int _completionIndex;
  QString _completionBase;
  QString _completionBuffer;
  QString _completionPathPrefix;
  QTimer *_completionTimer;
  QTimer *_promptTimer;
  QList<CompletionState> _completionStack;
};

} // namespace GuiManager::Widgets::Win32
