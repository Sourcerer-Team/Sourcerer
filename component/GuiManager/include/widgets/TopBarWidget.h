#pragma once

#include "widgets/TitlebarWidget.h"

#include <ConfigManager.h>

#include <QMouseEvent>
#include <QPoint>
#include <QPushButton>
#include <QWidget>

namespace GuiManager::Widgets {

class TopBarWidget final : public QWidget {
  Q_OBJECT
public:
  explicit TopBarWidget(QWidget *parent = nullptr);

  TitlebarWidget *titleBarWidget() const { return _titleBarWidget; }

signals:
  void minimized();
  void maximized();
  void closed();
  void doubleClicked();
  void profile(int middleX);

  void open();
  void clone();
  void init();
  void settings();
  void about();

  void closedTab(int index);
  void selectedTab(int index);
  void resizeTabBar();

  void dragged();

private slots:
  void onProfileClicked();

  // QWidget interface
protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;

private:
  ConfigManager::ConfigManager &_configManager;
  TitlebarWidget *_titleBarWidget;
  QPushButton *_profileButton;
};

} // namespace GuiManager::Widgets
