// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <ConfigManager.h>

#include <QHoverEvent>
#include <QMenu>
#include <QToolButton>
#include <QWidget>

namespace GuiManager {
class MainWindow;
}

namespace GuiManager::Widgets {

class TitleBarMenuButton final : public QToolButton {
  Q_OBJECT
public:
  explicit TitleBarMenuButton(QWidget *parent = nullptr);

  QAction *terminalAction() const { return _terminalAction; }

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void open();
  void clone();
  void init();
  void saveWorkspace();
  void connectToGitHubIssueTracker();
  void connectToGitLabIssueTracker();
  void connectToAzureIssueTracker();
  void connectToBitbucketIssueTracker();
  void connectToCustomIssueTracker();
  void settings();
  void credentials();
  void sshKeys();
  void terminal();
  void bugReport();
  void releaseNotes();
  void about();
  void themeChanged();

private slots:
  void onClicked();
  void onOpen();
  void onClone();
  void onInit();
  void onSaveWorkspace();
  void onConnectToGitHubIssueTracker();
  void onConnectToGitLabIssueTracker();
  void onConnectToAzureIssueTracker();
  void onConnectToBitbucketIssueTracker();
  void onConnectToCustomIssueTracker();
  void onSettings();
  void onCredentials();
  void onSshKeys();
  void onTerminal();
  void onBugReport();
  void onAbout();

private:
  QMenu *createMenu();
  void createActions();
  static MainWindow *mainWindow();
  bool hover(QHoverEvent *event, bool entered);

private:
  ConfigManager::ConfigManager &_configManager;
  QAction *_openAction;
  QAction *_cloneAction;
  QAction *_initAction;
  QAction *_saveWorkspaceAction;
  QAction *_connectToGitHubIssueTrackerAction;
  QAction *_connectToGitLabIssueTrackerAction;
  QAction *_connectToAzureIssueTrackerAction;
  QAction *_connectToBitbucketIssueTrackerAction;
  QAction *_connectToCustomIssueTrackerAction;
  QAction *_settingsAction;
  QAction *_credentialsAction;
  QAction *_sshKeysAction;
  QAction *_terminalAction;
  QAction *_bugReportAction;
  QAction *_aboutAction;
  QAction *_releaseNotesAction;
};

} // namespace GuiManager::Widgets
