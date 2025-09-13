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
#include <QCheckBox>

#include <QEnableSharedFromThis>
#include <QItemSelection>
#include <QSet>
#include <QString>
#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager::Models {
class GitCommitListModel;
class GitCommitTreeModel;
}

namespace GuiManager::Containers {

class GitRepoContainer final : public Widgets::ContainerWidget,
                               public QEnableSharedFromThis<GitRepoContainer> {
  Q_OBJECT

  typedef QWeakPointer<GitRepoContainer> WeakPtr;
  typedef QSharedPointer<GitRepoContainer> SharedPtr;

public:
  explicit GitRepoContainer(QString path, QWidget *parent = nullptr);
  ~GitRepoContainer() override;

  void init() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

protected:
  void hideEvent(QHideEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void onSplitterMoved(int pos, int index) const;
  void showTooltipForColumnSlice(const QModelIndex &index);
  void showTooltipForSummaryColumn(const QModelIndex &index) const;
  void onHistorySelectionChanged(const QItemSelection &selected, const QItemSelection &deselected);
  void onHistoryViewItemEntered(const QModelIndex &index);
  void onLocalBranchSelected(const QString &branchName);
  void onRemoteBranchSelected(const QString &branchName);
  void onTagSelected(const QString &tagName);
  void onShowSpinnerOverlay() const;
  void onGraphColumnExpand(int section);
  void updateModelActiveBranch(const WeakPtr &guard, const QString &branchName,
                               GitRepo::BranchType branchType);

private:
  void checkoutBranch(const WeakPtr &guard, const QString &branchName,
                      GitRepo::BranchType branchType);
  void closeRepo() const;
  bool getActiveBranchOrTag(const WeakPtr &guard,
                            const GitRepo::BranchType branchType);
  bool getActiveLocalBranch();
  bool getActiveRemoteBranch();
  bool getActiveTag();
  bool getBranchHistory(const WeakPtr &guard);
  [[nodiscard]] QStringList getCommitBranches(const QString &commit) const;
  [[nodiscard]] GitRepo::GitCommitStatus getCommiStatus(const QString &commit) const;
  bool getLocalBranches();
  bool getRemoteBranches();
  bool getSubmodules();
  bool getTags();
  bool openRepo();
  void refreshViews();
  void resizeSectionToViewport(int section);
  void restorePreviousSectionWidth();
  void stretchBranchHistoryTableColumns() const;
  void showCommit(const WeakPtr &guard, const QString &commit);
  void showCommitAndMerge(WeakPtr guard, const Models::StatusFieldsMap &statusMap);
  void showErrorPage(bool error = true) const;
  void showMainSpinnerPage(bool spinnerEnabled = true) const;
  void showSpinnerOverlay(bool spinnerEnabled = true) const;
  void showToast(const QString &message);
  void showErrorToast(const QString &message, const QString &title);
  void updateBranchHistory(
      const WeakPtr &guard,
      const GitRepo::GitHistoryWithStatus &historyWithStatus) const;
  void updateLocalBranchesModel(const WeakPtr &guard,
                                const QStringList &localBranches) const;
  void updateLocalBranchesModelActiveBranch(const WeakPtr &guard,
                                            const QString &activeBranch,
                                            bool updatePages = true);
  void updateRemoteBranchesModel(const WeakPtr &guard,
                                 const QStringList &remoteBranches) const;
  void updateRemoteBranchesModelActiveBranch(const WeakPtr &guard,
                                             const QString &activeBranch,
                                             bool updatePages = true);
  void updateTagsModelActiveTag(const WeakPtr &guard, const QString &activeTag,
                                bool updatePages = true);
  void updateSubmodulesModel(const WeakPtr &guard,
                             const QStringList &tags) const;
  void updateTagsModel(const WeakPtr &guard, const QStringList &tags) const;

private:
  Ui::GitRepoContainer *ui;
  QString _uuid;
  QString _path;
  QString _activeBranch;
  GitRepo::BranchType _activeBranchType;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  Widgets::ToastHostWidget &_toastHost;
  QSharedPointer<Models::GitHistoryModel> _historyModel;
  QPixmap *_errorPixmap;
  Util::Dispatcher _dispatcher;

  Util::Mutex _tooltipsMutex;
  QPoint _mousePos;
  QSet<QString> _tooltipsToShow;

  Widgets::SpinnerOverlayWidget *_spinnerOverlay;
  QTimer *_spinnerOverlayTimer;

  bool _quitGettingHistory;

  int _resizedHeaderSection;
  int _resizedHeaderSectionPreviousWidth;

  QCheckBox *_checkBoxAmend;

  Models::GitCommitListModel *_commitListModel;
  Models::GitCommitTreeModel *_commitTreeModel;
};

} // namespace GuiManager::Containers
