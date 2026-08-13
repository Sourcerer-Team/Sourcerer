#pragma once

#include "widgets/TitleBarMenuButton.h"
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

  void addTab(const QString &text, const QString &toolTip = QString()) const;
  void removeTab(int index) const;
  void selectTab(int index) const;
  int tabsCount() const;

  Widgets::TitleBarMenuButton *menuButton() const;

signals:
  void open();
  void clone();
  void init();
  void saveWorkspace();
  void connectToGitHubIssueTracker();
  void connectToGitLabIssueTracker();
  void connectToAzureIssueTracker();
  void connectToBitbucketIssueTracker();
  void settings();
  void credentials();
  void sshKeys();
  void terminal();
  void bugReport();
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
  void onCredentials();
  void onSshKeys();
  void onTerminal();
  void onBugReport();
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
