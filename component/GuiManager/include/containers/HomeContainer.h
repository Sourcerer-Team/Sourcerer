#pragma once

#include "models/IssueTrackersModel.h"
#include "models/RecentlyOpenedReposModel.h"
#include "models/WorkspacesModel.h"
#include "widgets/ContainerWidget.h"
#include <delegates/HomeRecentItemDelegate.h>

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <GitHubRepo.h>

#include <QEnableSharedFromThis>
#include <QModelIndex>
#include <QSharedPointer>
#include <QTableView>
#include <QWeakPointer>
#include <QWidget>

namespace Ui {
class HomeContainer;
}

namespace GuiManager::Containers {

using namespace Delegates;
using namespace Models;
using namespace Widgets;

class HomeContainer final : public ContainerWidget,
                            public QEnableSharedFromThis<HomeContainer> {
  Q_OBJECT

  typedef QSharedPointer<HomeContainer> SharedPtr;
  typedef QWeakPointer<HomeContainer> WeakPtr;

public:
  explicit HomeContainer(QWidget *parent = nullptr);
  ~HomeContainer() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  void reloadIssueTrackers() const;
  void reloadRecentlyOpened() const;
  void reloadWorkspaces() const;

  void selected() override;

signals:
  void clone();
  void init() override;
  void open();
  void issueTrackerClicked(const QString &serviceType, const QString &name);
  void issueTrackerRightClicked(const QString &serviceType, const QString &name,
                                const QPoint &pos);
  void recentClicked(const QString &path);
  void recentRightClicked(const QString &path, const QPoint &pos);
  void workspaceClicked(const QString &name);
  void workspaceRightClicked(const QString &name, const QPoint &pos);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void hoverEnter(const QModelIndex &index);
  void hoverLeave(const QModelIndex &index);
  void onCloneClicked();
  void onGitHubIssueTrackerConnectClicked();
  void onInitClicked();
  void onOpenClicked(bool checked = false);
  void onIssueTrackerRightClicked(const QString &serviceType,
                                  const QString &name, const QPoint &pos);
  void onRecentRightClicked(const QString &path, const QPoint &pos);
  void onWorkspaceRightClicked(const QString &name, const QPoint &pos);

private:
  Ui::HomeContainer *ui;
  RecentlyOpenedReposModel *_recentlyOpenedModel;
  WorkspacesModel *_workspacesModel;
  IssueTrackersModel *_issueTrackersModel;
  HomeRecentItemDelegate *_itemDelegate;
  ::ConfigManager::ConfigManager &_configManager;
  Util::Dispatcher _dispatcher;
};

} // namespace GuiManager::Containers
