#pragma once

#include <ConfigManager.h>

#include <QHoverEvent>
#include <QMenu>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarMenuButton : public QPushButton {
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
  void settings();
  void about();

private slots:
  void onClicked();
  void onOpen();
  void onClone();
  void onInit();
  void onSettings();
  void onAbout();

private:
  QMenu *createMenu();
  bool hover(QHoverEvent *event, bool entered);

private:
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Widgets
} // namespace GuiManager
