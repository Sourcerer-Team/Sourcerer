#pragma once

#include "models/GitHistoryModel.h"
#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitRepo.h>
#include <recyclerview.h>

#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager {

namespace Containers {

class GitRepoContainer : public Widgets::ContainerWidget {
  Q_OBJECT

public:
  explicit GitRepoContainer(const QString &path, QWidget *parent = nullptr);
  virtual ~GitRepoContainer();

private slots:
  void onSplitterMoved(int pos, int index);

private:
  void closeRepo() const;
  void eventCallback(EventManager::Event event);
  void getActiveBranch() const;
  void getBranchHistory() const;
  void getLocalBranches() const;
  void getRemoteBranches() const;
  void getSubmodules() const;
  void getTags() const;
  void openRepo() const;
  void refreshViews();
  void showToast(const QString &message);
  void updateBranchHistory(const GitRepo::GitHistory& history);
  void updateLocalBranchesModel(const QStringList &localBranches);
  void updateLocalBranchesModelActiveBranch(const QString &activeBranch);
  void updateRemoteBranchesModel(const QStringList &remoteBranches);
  void updateSubmodulesModel(const QStringList &tags);
  void updateTagsModel(const QStringList &tags);

private:
  Ui::GitRepoContainer *ui;
  QString _uuid;
  QString _path;
  QString _activeBranch;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  Widgets::ToastHostWidget &_toastHost;
  Models::GitHistoryModel *_historyModel;
};

} // namespace Containers
} // namespace GuiManager
