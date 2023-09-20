#pragma once

#include "widgets/TitlebarWidget.h"

#include <ConfigManager.h>

#include <QMouseEvent>
#include <QPoint>
#include <QWidget>

namespace GuiManager {
namespace Widgets {

class TopBarWidget : public QWidget {
  Q_OBJECT
public:
  explicit TopBarWidget(QWidget *parent = nullptr);

  TitlebarWidget *titleBarWidget() const { return _titleBarWidget; }

signals:
  void minimized();
  void maximized();
  void closed();
  void doubleClicked();

  void home();
  void open();
  void clone();
  void init();
  void settings();
  void about();

  void closedTab(int index);
  void selectedTab(int index);
  void resizeTabBar();

  void dragged(int xOffset, int yOffset);

  // QWidget interface
protected:
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;

private:
  ConfigManager::ConfigManager &_configManager;
  TitlebarWidget *_titleBarWidget;
  QPoint _previousPos;
  bool _drag;
};

} // namespace Widgets
} // namespace GuiManager
