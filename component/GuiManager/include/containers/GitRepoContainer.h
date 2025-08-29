#pragma once

#include "models/GitHistoryModel.h"
#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <EventManager.h>
#include <GitRepo.h>

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
  ~GitRepoContainer() override;

protected:
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onSplitterMoved(int pos, int index) const;

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
  void refreshViews() const;
  void stretchBranchHistoryTableColumns() const;
  void showToast(const QString &message) const;
  void updateBranchHistory(const GitRepo::GitHistory& history) const;
  void updateLocalBranchesModel(const QStringList &localBranches) const;
  void updateLocalBranchesModelActiveBranch(const QString &activeBranch);
  void updateRemoteBranchesModel(const QStringList &remoteBranches) const;
  void updateSubmodulesModel(const QStringList &tags) const;
  void updateTagsModel(const QStringList &tags) const;

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
