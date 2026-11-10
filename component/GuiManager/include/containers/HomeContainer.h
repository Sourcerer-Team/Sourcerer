// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "delegates/HomeRecentItemDelegate.h"
#include "models/IssueTrackersModel.h"
#include "models/RecentlyOpenedReposModel.h"
#include "models/WorkspacesModel.h"
#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <GitHubRepo.h>
#include <GitLabRepo.h>

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

  void deselected() override;
  void selected() override;

signals:
  void clone();
  void cloneSubversion();
  void cloneMercurial();
  void init() override;
  void initSubversion();
  void initMercurial();
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
  void onCloneSubversionClicked();
  void onCloneMercurialClicked();
  void onGitHubIssueTrackerConnectClicked();
  void onGitLabIssueTrackerConnectClicked();
  void onAzureIssueTrackerConnectClicked();
  void onBitbucketOAuthConnectClicked();
  void onCustomIssueTrackerConnectClicked();
  void onInitClicked();
  void onInitSubversionClicked();
  void onInitMercurialClicked();
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
  Widgets::ToastHostWidget &_toastHost;
  Util::Dispatcher _dispatcher;
  bool _selected;
};

} // namespace GuiManager::Containers
