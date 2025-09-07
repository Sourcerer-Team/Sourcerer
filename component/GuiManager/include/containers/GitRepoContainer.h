#pragma once

#include "models/GitHistoryModel.h"
#include "widgets/ContainerWidget.h"
#include "widgets/SpinnerOverlayWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <EventManager.h>
#include <GitRepo.h>
#include <LockGuard.h>

#include <QSet>
#include <QString>
#include <QTimer>
#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager::Containers {

class GitRepoContainer final : public Widgets::ContainerWidget {
  Q_OBJECT

public:
  explicit GitRepoContainer(QString path, QWidget *parent = nullptr);
  ~GitRepoContainer() override;

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

protected:
  void hideEvent(QHideEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void onSplitterMoved(int pos, int index) const;
  void onHistoryViewItemEntered(const QModelIndex &index);
  void onLocalBranchSelected(const QString &branchName);
  void onRemoteBranchSelected(const QString &branchName);
  void onShowSpinnerOverlay() const;

private:
  void closeRepo() const;
  void eventCallback(EventManager::Event event);
  void getActiveLocalBranch() const;
  void getActiveRemoteBranch() const;
  void getBranchHistory() const;
  QStringList getCommitBranches(const QString &commit) const;
  void getLocalBranches() const;
  void getRemoteBranches() const;
  void getSubmodules() const;
  void getTags() const;
  void openRepo() const;
  void refreshViews() const;
  void stretchBranchHistoryTableColumns() const;
  void showErrorPage(bool error = true) const;
  void showMainSpinnerPage(bool spinnerEnabled = true) const;
  void showSpinnerOverlay(bool spinnerEnabled = true) const;
  void showToast(const QString &message) const;
  void updateBranchHistory(const GitRepo::GitHistory &history) const;
  void updateLocalBranchesModel(const QStringList &localBranches) const;
  void updateLocalBranchesModelActiveBranch(const QString &activeBranch,
                                            bool updatePages = true);
  void updateRemoteBranchesModel(const QStringList &remoteBranches) const;
  void updateRemoteBranchesModelActiveBranch(const QString &activeBranch,
                                             bool updatePages = true);
  void updateSubmodulesModel(const QStringList &tags) const;
  void updateTagsModel(const QStringList &tags) const;

private:
  Ui::GitRepoContainer *ui;
  QString _uuid;
  QString _path;
  QString _activeBranch;
  GitRepo::BranchType _activeBranchType;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  Widgets::ToastHostWidget &_toastHost;
  Models::GitHistoryModel *_historyModel;
  QPixmap *_errorPixmap;
  Util::Dispatcher _dispatcher;

  Util::Mutex _tooltipsMutex;
  QPoint _mousePos;
  QSet<QString> _tooltipsToShow;

  Widgets::SpinnerOverlayWidget *_spinnerOverlay;
  QTimer *_spinnerOverlayTimer;

  bool _quitGettingHistory;
};

} // namespace GuiManager::Containers