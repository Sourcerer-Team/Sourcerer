#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/TitlebarWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>

#include <QGoodCentralWidget>
#include <QGoodWindow>

#include <QVector>

namespace GuiManager {

using namespace Widgets;

class MainWindow : public QGoodWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  // QWidget interface
protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onHome();
  void onOpen();
  void onClone();
  void onInit();
  void onSettings();
  void onAbout();
  void onClosedTab(int index);
  void onSelectedTab(int index);
  void onTabBarResize();
  void onToastHostSizeChanged(const QSize& newSize);

private:
  void addGitRepoContainer(const QString &path);
  void addHomeContainer();
  void addSettingsContainer();
  void selectContainer(int index);

private:
  int _current;
  TitlebarWidget *_titleBarWidget;
  ToastHostWidget &_toastHost;
  QGoodCentralWidget *_centralWidget;
  QVector<ContainerWidget *> _containers;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager
