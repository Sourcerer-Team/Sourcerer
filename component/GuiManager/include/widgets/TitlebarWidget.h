#pragma once

#include <ConfigManager.h>

#include <QWidget>

namespace Ui {
class TitlebarWidget;
}

namespace GuiManager {

namespace Widgets {

class TitlebarWidget : public QWidget {
  Q_OBJECT

public:
  explicit TitlebarWidget(QWidget *parent = nullptr);
  ~TitlebarWidget();

  void addTab(const QString &text);
  void removeTab(int index);
  void selectTab(int index);
  int tabsCount() const;

signals:
  void home();
  void open();
  void clone();
  void init();
  void settings();
  void about();

  void closedTab(int index);
  void selectedTab(int index);
  void resizeTabBar();

private slots:
  void onHome();
  void onOpen();
  void onClone();
  void onInit();
  void onSettings();
  void onAbout();

  void onClosedTab(int index);
  void onSelectedTab(int index);
  void onTabBarResized();

private:
  Ui::TitlebarWidget *ui;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Widgets
} // namespace GuiManager
