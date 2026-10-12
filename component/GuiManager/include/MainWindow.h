#pragma once

#include "HitDetector.h"

#include "widgets/CentralWidget.h"
#include "widgets/ConsoleWidget.h"
#include "widgets/ContainerWidget.h"
#include "widgets/GitCredentialsOverlayWidget.h"
#include "widgets/ProfileBoardWidget.h"
#include "widgets/SshPassphraseOverlayWidget.h"
#include "widgets/ToastHostWidget.h"
#include "widgets/TopBarWidget.h"

#include "containers/CloneRepoContainer.h"
#include "containers/InitRepoContainer.h"
#include "containers/RepoContainer.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitRepo.h>

#include <QCloseEvent>
#include <QEnableSharedFromThis>
#include <QMainWindow>
#include <QMap>
#include <QResizeEvent>
#include <QSet>
#include <QUrl>

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

  // Brings the window to the front when another Sourcerer process was started.
  void activateFromAnotherInstance(const QStringList &arguments);

  // QWidget interface
protected:
  void mouseMoveEvent(QMouseEvent *event) override;
  void changeEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void showEvent(QShowEvent *event) override;

#ifdef Q_OS_WIN
  bool nativeEvent(const QByteArray &eventType, void *message,
                   qintptr *result) override;
#endif

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void terminal();

private slots:
  void onOpen();
  void onClone();
  void onCloneSubversion();
  void onCloneFinished(ContainerWidget *sender);
  void onInit();
  void onInitSubversion();
  void onInitFinished(ContainerWidget *sender);
  void onOpenRepo(const QString &path);
  void onRecentClicked(const QString &path);
  void onSaveWorkspace();
  void onConnectToGitHubIssueTracker();
  void onConnectToGitLabIssueTracker();
  void onConnectToAzureIssueTracker();
  void onConnectToBitbucketIssueTracker();
  void onConnectToCustomIssueTracker();
  void onIssueTrackerClicked(const QString &serviceType,
                             const QString &repositoryName);
  void onSettings();
  void onCredentials();
  void onSshKeys();
  void onSubmitWorkspace(const QString &name);
  void onTerminal();
  void onWorkspaceClicked(const QString &name);
  void onBugReport();
  void onReleaseNotes();
  void onAbout() const;
  void onThemeChanged();
  void onClosedTab(int index);
  void onSelectedTab(int index);
  void onMovedTab(int from, int to);
  static void onTabBarResize();
  void onToastHostSizeChanged(const QSize &newSize);
  void onMinimizeClicked();
  void onMaximizeClicked();
  void onCloseClicked();
  void onTitleBarDrag() const;
  void onEdgeHit(Qt::Edges edges) const;
  void onProfile(int middleX);

private:
  void
  addCloneContainer(Containers::CloneType type = Containers::CloneType::Git);
  void addGitRepoContainer(const QString &path);
  void addSvnRepoContainer(const QString &path);
  void openRepositoryByType(const QString &path, const QString &sourceLabel);
  void addGitCredentialsContainer();
  void addIssueTrackerContainer(const QString &serviceType,
                                const QString &repositoryName,
                                const QUrl &startUrl = QUrl(),
                                const QString &title = QString());
  void addGitKeysContainer();
  void addHomeContainer();
  void addInitRepoContainer(
      Containers::InitType type = Containers::InitType::Git);
  void addSettingsContainer();
  void addBugReportContainer();
  void addReleaseNotesContainer(const QString &title, const QString &markdown);
  void showReleaseNotesIfUpdated();
  void closeContainer(const ContainerWidget *container);
  void eventCallback(EventManager::Event event);
  void saveNewWorkspace();
  void selectContainer(int index);
  static void hideCredentialsOverlay(GitCredentialsOverlayWidget *widget);
  static void showCredentialsOverlay(const GitCredentialsRequest &request,
                                     GitCredentialsOverlayWidget *widget);
  static void hidePassphraseOverlay(SshPassphraseOverlayWidget *widget);
  static void showPassphraseOverlay(const GitCredentialsRequest &request,
                                    SshPassphraseOverlayWidget *widget);
  void showCloneProgressToast(const CloneProgress &data,
                              const QString &title = QString());
  void showToast(const QString &text, const QString &title);
  void showErrorToast(const QString &text, const QString &title);

private:
  int _current;
  CentralWidget *_centralWidget;
  TopBarWidget *_topBarWidget;
  ConsoleWidget *_consoleWidget;
  ToastHostWidget &_toastHost;
  QVector<QSharedPointer<ContainerWidget>> _containers;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  QSet<QString> _progressing;
  bool _maximized;
  bool _mousePressed;
  HitDetector *_hitDetector;
  QWidget *_bottomResizeGrip;
  QSharedPointer<ProfileBoardWidget> _profileBoardWidget;
  bool _profileBoardActive;
  QMap<size_t, GitCredentialsOverlayWidget *> _credentialsWidgets;
  QMap<size_t, SshPassphraseOverlayWidget *> _passphraseWidgets;
};

} // namespace GuiManager
