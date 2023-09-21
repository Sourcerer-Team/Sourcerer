#pragma once

#include "WindowFrame.h"
#include "widgets/CentralWidget.h"
#include "widgets/ContainerWidget.h"
#include "widgets/TitlebarWidget.h"
#include "widgets/ToastHostWidget.h"
#include "widgets/TopBarWidget.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitRepo.h>

#include <QMainWindow>
#include <QResizeEvent>
#include <QSet>
#include <QVector>

namespace GuiManager {

using namespace GitRepo;
using namespace Widgets;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

  // QWidget interface
protected:
  void resizeEvent(QResizeEvent *event) override;
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;

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
  void onToastHostSizeChanged(const QSize &newSize);
  void onMinimizeClicked();
  void onMaximizeClicked();
  void onCloseClicked();
  void onTitleBarDrag(int xOffset, int yOffset);

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
  void showCloneProgressToast(const CloneProgress &data);
  void showToast(const QString &text);

private:
  int _current;
  CentralWidget *_centralWidget;
  TopBarWidget *_topBarWidget;
  ToastHostWidget &_toastHost;
  QVector<ContainerWidget *> _containers;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  QSet<QString> _progressing;
  bool _maximized;
  bool _mousePressed;
  WindowFrame *_frame;
};

} // namespace GuiManager
