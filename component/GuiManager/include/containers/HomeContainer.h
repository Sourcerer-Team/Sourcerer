#pragma once

#include "models/RecentlyOpenedReposModel.h"
#include "models/WorkspacesModel.h"
#include "widgets/ContainerWidget.h"
#include <delegates/HomeRecentItemDelegate.h>

#include <ConfigManager.h>

#include <QEnableSharedFromThis>
#include <QModelIndex>
#include <QWidget>

namespace Ui {
class HomeContainer;
}

class QStandardItemModel;

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

  void reloadRecentlyOpened() const;
  void reloadWorkspaces() const;

  void selected() override;

signals:
  void clone();
  void init() override;
  void open();
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
  void onInitClicked();
  void onOpenClicked(bool checked = false);
  void onRecentRightClicked(const QString &path, const QPoint &pos);
  void onWorkspaceRightClicked(const QString &name, const QPoint &pos);

private:
  Ui::HomeContainer *ui;
  RecentlyOpenedReposModel *_recentlyOpenedModel;
  WorkspacesModel *_workspacesModel;
  QStandardItemModel *_issueTrackerConnectionsModel;
  HomeRecentItemDelegate *_itemDelegate;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Containers