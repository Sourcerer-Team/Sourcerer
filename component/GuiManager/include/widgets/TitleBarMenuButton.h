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
  void settings();
  void credentials();
  void sshKeys();
  void terminal();
  void about();

private slots:
  void onClicked();
  void onOpen();
  void onClone();
  void onInit();
  void onSaveWorkspace();
  void onSettings();
  void onCredentials();
  void onSshKeys();
  void onTerminal();
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
  QAction *_settingsAction;
  QAction *_credentialsAction;
  QAction *_sshKeysAction;
  QAction *_terminalAction;
  QAction *_aboutAction;
};

} // namespace GuiManager::Widgets
