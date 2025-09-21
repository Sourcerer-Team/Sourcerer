#pragma once

#include <MainWindow.h>

#include <ConfigManager.h>

#include <QHoverEvent>
#include <QMenu>
#include <QToolButton>
#include <QWidget>

namespace GuiManager::Widgets {

class TitleBarMenuButton final : public QToolButton {
  Q_OBJECT
public:
  explicit TitleBarMenuButton(QWidget *parent = nullptr);

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void open();
  void clone();
  void init();
  void saveWorkspace();
  void settings();
  void about();

private slots:
  void onClicked();
  void onOpen();
  void onClone();
  void onInit();
  void onSaveWorkspace();
  void onSettings();
  void onAbout();

private:
  QMenu *createMenu();
  static MainWindow* mainWindow() ;
  bool hover(QHoverEvent *event, bool entered);

private:
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets