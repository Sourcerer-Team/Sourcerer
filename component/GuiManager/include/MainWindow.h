#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/TitlebarWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitRepo.h>

#include <QGoodCentralWidget>
#include <QGoodWindow>

#include <QSet>
#include <QVector>

namespace GuiManager {

using namespace GitRepo;
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
  void onCloneFinished(ContainerWidget *sender);
  void onInit();
  void onInitFinished(ContainerWidget *sender);
  void onSettings();
  void onAbout();
  void onClosedTab(int index);
  void onSelectedTab(int index);
  void onTabBarResize();
  void onToastHostSizeChanged(const QSize& newSize);

private:
  void addCloneContainer();
  void addGitRepoContainer(const QString &path);
  void addHomeContainer();
  void addInitGitRepoContainer();
  void addSettingsContainer();
  void closeContainer(ContainerWidget *container);
  void eventCallback(EventManager::Event event);
  void selectContainer(int index);
  void showCredentialsToast(const GitCredentialsRequest &request);
  void showCloneProgressToast(const CloneProgress& data);
  void showToast(const QString &text);

private:
  int _current;
  TitlebarWidget *_titleBarWidget;
  ToastHostWidget &_toastHost;
  QGoodCentralWidget *_centralWidget;
  QVector<ContainerWidget *> _containers;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  QSet<QString> _progressing;
};

} // namespace GuiManager
