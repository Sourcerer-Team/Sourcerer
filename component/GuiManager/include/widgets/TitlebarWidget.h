#pragma once

#include <ConfigManager.h>

#include <QWidget>

namespace Ui {
class TitlebarWidget;
}

namespace GuiManager {

namespace Widgets {

class TitlebarWidget final : public QWidget {
  Q_OBJECT

public:
  explicit TitlebarWidget(QWidget *parent = nullptr);
  ~TitlebarWidget() override;

  void addTab(const QString &text) const;
  void removeTab(int index) const;
  void selectTab(int index) const;
  int tabsCount() const;

signals:
  void open();
  void clone();
  void init();
  void saveWorkspace();
  void settings();
  void terminal();
  void about();

  void closedTab(int index);
  void selectedTab(int index);
  void resizeTabBar();

private slots:
  void onOpen();
  void onClone();
  void onInit();
  void onSaveWorkspace();
  void onSettings();
  void onTerminal();
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
