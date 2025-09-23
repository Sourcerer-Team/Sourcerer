#pragma once

#include "HitDetector.h"
#include "widgets/CentralWidget.h"
#include "widgets/ContainerWidget.h"
#include "widgets/GitCredentialsOverlayWidget.h"
#include "widgets/ProfileBoardWidget.h"
#include "widgets/ToastHostWidget.h"
#include "widgets/TopBarWidget.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitRepo.h>

#include <QEnableSharedFromThis>
#include <QMainWindow>
#include <QMap>
#include <QResizeEvent>
#include <QSet>

namespace GuiManager {

using namespace GitRepo;
using namespace Widgets;

class MainWindow final : public QMainWindow,
                         public QEnableSharedFromThis<MainWindow> {
  Q_OBJECT

  typedef QSharedPointer<MainWindow> SharedPtr;
  typedef QWeakPointer<MainWindow> WeakPtr;

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  QStringList containerPaths() const;

  // QWidget interface
protected:
  void mouseMoveEvent(QMouseEvent *event) override;

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onOpen();
  void onClone();
  void onCloneFinished(ContainerWidget *sender);
  void onInit();
  void onInitFinished(ContainerWidget *sender);
  void onRecentClicked(const QString &path);
  void onSaveWorkspace();
  void onSettings();
  void onSubmitWorkspace(const QString &name);
  void onWorkspaceClicked(const QString &name);
  void onAbout() const;
  void onClosedTab(int index);
  void onSelectedTab(int index);
  static void onTabBarResize();
  void onToastHostSizeChanged(const QSize &newSize);
  void onMinimizeClicked();
  void onMaximizeClicked();
  void onCloseClicked();
  void onTitleBarDrag() const;
  void onEdgeHit(Qt::Edges edges) const;
  void onProfile(int middleX);

private:
  void addCloneContainer();
  void addGitRepoContainer(const QString &path);
  void addHomeContainer();
  void addInitGitRepoContainer();
  void addSettingsContainer();
  void closeContainer(const ContainerWidget *container);
  void eventCallback(EventManager::Event event);
  void saveNewWorkspace();
  void selectContainer(int index);
  static void hideCredentialsOverlay(GitCredentialsOverlayWidget *widget);
  static void showCredentialsOverlay(const GitCredentialsRequest &request,
                                     GitCredentialsOverlayWidget *widget);
  void showCloneProgressToast(const CloneProgress &data);
  void showToast(const QString &text, const QString &title);
  void showErrorToast(const QString &text, const QString &title);

private:
  int _current;
  CentralWidget *_centralWidget;
  TopBarWidget *_topBarWidget;
  ToastHostWidget &_toastHost;
  QVector<QSharedPointer<ContainerWidget>> _containers;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  QSet<QString> _progressing;
  bool _maximized;
  bool _mousePressed;
  HitDetector *_hitDetector;
  ProfileBoardWidget *_profileBoardWidget;
  bool _profileBoardActive;
  QMap<uint, GitCredentialsOverlayWidget *> _credentialsWidgets;
};

} // namespace GuiManager
