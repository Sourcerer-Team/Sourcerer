#pragma once

#include "models/GitHistoryModel.h"
#include "widgets/ContainerWidget.h"
#include "widgets/ShortcutsToolbarWidget.h"
#include "widgets/SpinnerOverlayWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <EventManager.h>
#include <GitRepo.h>
#include <LockGuard.h>
#include <UserProfileManager.h>

#include <QAction>
#include <QEnableSharedFromThis>
#ifdef Q_OS_WIN32
#include <win32/DirectoryWatcher.h>
#else
#include <QFileSystemWatcher>
#endif
#include <QInputDialog>
#include <QItemSelection>
#include <QListView>
#include <QMenu>
#include <QPushButton>
#include <QSemaphore>
#include <QSet>
#include <QString>
#include <QToolButton>
#include <QTreeView>
#include <QWidget>

#include <functional>
#include <memory>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager::Models {
class GitCommitListModel;
class GitCommitTreeModel;
} // namespace GuiManager::Models

namespace GuiManager::Widgets {
class ScrollbarMarksWidget;
}

class QEvent;
class QCheckBox;

namespace GuiManager::Containers {

class GitRepoManagerBridge;
class GitRepoManagerBridgeImpl;

class GitRepoContainer final : public Widgets::ContainerWidget,
                               public QEnableSharedFromThis<GitRepoContainer> {
  Q_OBJECT

  friend class GitRepoManagerBridgeImpl;

  typedef QWeakPointer<GitRepoContainer> WeakPtr;
  typedef QSharedPointer<GitRepoContainer> SharedPtr;

public:
  explicit GitRepoContainer(QString path, QWidget *parent = nullptr);
  ~GitRepoContainer() override;

  void init() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  QString path() const override { return _path; }

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

  // Widgets::ContainerWidget
public:
  void deselected() override;
  void selected() override;

signals:
  void historyViewRightClicked(const QModelIndex &index, const QPoint &pos);
  void openRepoRequested(const QString &path);

protected:
  void hideEvent(QHideEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void changeEvent(QEvent *event) override;

private slots:
  void onAddSubmodule();
  void onAddTarget(int remoteIndex);
  void onApplyPatch();
  void onApplyStash(int stashIndex);
  void onDeleteStash(int stashIndex);
  void onDeleteTarget(int remoteIndex, int targetIndex);
  void onClearAllStashes();
  void onAddRemote();
  void onEditRemote(int remoteIndex);
  void onFetchRemote(int remoteIndex);
  void onEditTarget(int remoteIndex, int targetIndex);
  void onDeleteRemote(int remoteIndex);
  void onSetDefaultRemote(int remoteIndex);
  void onSetDefaultTarget(int remoteIndex, int targetIndex);
  void onSplitterMoved(int pos, int index);
  void onHistoryHeaderContextMenuRequested(const QPoint &pos);
  void onHistoryHeaderSectionResized(int logicalIndex, int oldSize,
                                     int newSize);

  void showTooltipForSummaryColumn(const QModelIndex &index) const;

  void onCheckoutCommit(const QString &commit);
  void onAbortButtonClicked();
  void onCommitButtonClicked();
  void onMergeButtonClicked();
  void onCommitAndMergeIndexTreeUnstageAllButtonClicked();
  void onCommitAndMergeWorkingTreeTreeStageAllButtonClicked();
  void onCreateBranchFromCommit(const QString &commit);
  void onCreatePatchFromCommit(const QString &commit);
  void onCreateTagFromCommit(const QString &commit);
  void onDeleteBranch(const QString &branchName);
  void onDeleteRemoteBranch(const QString &branchName);
  void onDeleteTag(const QString &tagName);
  void onDeleteRemoteTag(const QString &tagName);
  void onPushTag(const QString &tagName);
  void onPushAllTags();
  void onDiffCloseClicked();
  void onBlameCloseClicked();
  void onDiffUnifiedClicked();
  void onDiffSideBySideClicked();
  void onDisplayTreeExpandAllButtonClicked();
  void onDisplayFilesAllToggled(bool checked);
  void onPageDisplayFilesFlatPressed(const QModelIndex &index);
  void onPageDisplayFilesTreePressed(const QModelIndex &index);
  void onPageDisplayFilesFlatListViewContextMenuRequested(const QPoint &pos);
  void onPageDisplayFilesTreeViewContextMenuRequested(const QPoint &pos);
  void onHardResetToCommit(const QString &commit);
  void onHistorySelectionChanged(const QItemSelection &selected,
                                 const QItemSelection &deselected);
  void onHistoryViewRightClicked(const QModelIndex &index, const QPoint &pos);
  void onHistoryViewItemEntered(const QModelIndex &index);
  void onListViewTabWidgetFlatSelected(const QModelIndex &index);
  void onListViewTabWidgetFlatSelectedFromCommit(const QModelIndex &index,
                                                 const QString &commit);
  void onMergeDiffCloseClicked();
  void onMergeDiffDownClicked();
  void onMergeDiffUpClicked();
  void onMergeDiffSaveClicked();
  void onTreeViewTabWidgetTreeSelected(const QModelIndex &index,
                                       const Models::GitCommitTreeModel &model);
  void onTreeViewTabWidgetTreeSelectedFromCommit(
      const QModelIndex &index, const Models::GitCommitTreeModel &model,
      const QString &commit);
  void onLocalBranchMoved(const QString &sourceBranch,
                          const QString &destBranch);
  void onLocalBranchSelected(const QString &branchName);
  void onLocalBranchRightClicked(const QString &branchName, const QPoint &pos);
  void onMixedResetToCommit(const QString &commit);
  void onPop();
  void onPopStash(int stashIndex);
  void onPull(bool rebase = false);
  void onFetchAll();
  void onPush(bool force = false);
  void onStartGitFlowBranch(GitRepo::GitFlowBranchType branchType);
  void onFinishGitFlowBranch(GitRepo::GitFlowBranchType branchType);
  void onRemoteBranchSelected(const QString &branchName);
  void onRemoteBranchRightClicked(const QString &branchName, const QPoint &pos);
  void onRenameBranch(const QString &branchName);
  void onRenameTag(const QString &tagName);
  void onRepoDirChanged(const QString &path) const;
  void onRepoDirChangedTimeout();
  void onRevertCommit(const QString &commit);
  void onCherryPickCommit();
  void onSetUpstream(const QString &branchName);
  void onSoftResetToCommit(const QString &commit);
  void onStash();
  void onStashSelected(int stashIndex);
  void onStashApplyClicked();
  void onStashPopClicked();
  void onStashContentCloseClicked();
  void onStashDiffUnifiedClicked();
  void onStashDiffSideBySideClicked();
  void onSubmitRenameBranch(const QString &oldName, const QString &newName);
  void onSubmitRenameTag(const QString &oldName, const QString &newName);
  void onSubmoduleRightClicked(const QString &submoduleName, const QPoint &pos);
  void onSubmodulePanelRightClicked(const QPoint &pos);
  void onSubmoduleSelected(const QString &submoduleName);
  void onTagRightClicked(const QString &tagName, const QPoint &pos);
  void onTagsEmptyAreaRightClicked(const QPoint &pos);
  void onTagSelected(const QString &tagName);
  void onShowSpinnerOverlay();
  void onDeleteSubmodule(const QString &submoduleName);
  void onGraphColumnExpand(int section);
  void onWorkingTreeFlatListViewContextMenuRequested(const QPoint &pos);
  void onWorkingTreeFlatListViewClicked(const QModelIndex &index);
  void onWorkingTreeFlatListViewStageSelected(bool checked = false);
  void onWorkingTreeFlatListViewIgnoreSelected(bool checked = false);
  void onWorkingTreeFlatListViewIgnoreRecursiveSelected(bool checked = false);
  void onWorkingTreeFlatListViewCreateFilePatchSelected(bool checked = false);
  void onWorkingTreeFlatListViewDeleteSelected(bool checked = false);
  void onWorkingTreeFlatListViewDiscardSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewContextMenuRequested(const QPoint &pos);
  void onWorkingTreeTreeTreeViewClicked(const QModelIndex &index);
  void onWorkingTreeTreeTreeViewStageSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewIgnoreSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewIgnoreRecursiveSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewCreateFilePatchSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewDeleteSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewDiscardSelected(bool checked = false);
  void onIndexFlatListViewContextMenuRequested(const QPoint &pos);
  void onIndexFlatListViewClicked(const QModelIndex &index);
  void onIndexFlatListViewUnstageSelected(bool checked = false);
  void onIndexFlatListViewIgnoreSelected(bool checked = false);
  void onIndexFlatListViewIgnoreRecursiveSelected(bool checked = false);
  void onIndexFlatListViewCreateFilePatchSelected(bool checked = false);
  void onIndexFlatListViewDeleteSelected(bool checked = false);
  void onIndexFlatListViewDiscardSelected(bool checked = false);
  void onIndexTreeTreeViewContextMenuRequested(const QPoint &pos);
  void onIndexTreeTreeViewClicked(const QModelIndex &index);
  void onIndexTreeTreeViewUnstageSelected(bool checked = false);
  void onIndexTreeTreeViewIgnoreSelected(bool checked = false);
  void onIndexTreeTreeViewIgnoreRecursiveSelected(bool checked = false);
  void onIndexTreeTreeViewCreateFilePatchSelected(bool checked = false);
  void onIndexTreeTreeViewDeleteSelected(bool checked = false);
  void onIndexTreeTreeViewDiscardSelected(bool checked = false);
  void onWorkingTreeFlatListViewShowInFileManagerSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewShowInFileManagerSelected(bool checked = false);
  void onIndexFlatListViewShowInFileManagerSelected(bool checked = false);
  void onIndexTreeTreeViewShowInFileManagerSelected(bool checked = false);
  void updateModelActiveBranch(const WeakPtr &guard, const QString &branchName,
                               GitRepo::BranchType branchType);

private:
  void blame(const QStringList &files);
  void clearSummaryAndDescription();
  void closeDiffViewerIfOpen();
  void refreshScintillaTheme();
  void refreshSigningControls();
  void deleteFiles(const WeakPtr &guard, const QStringList &files);
  void copyFilePathsToClipboard(const QStringList &filePaths) const;
  bool getActiveLocalBranch();
  bool getActiveRemoteBranch();
  bool getActiveTag();
  static QStringList getSelectedFiles(QListView *listView);
  QStringList getSelectedFilesWhichExist(QListView *listView);
  QStringList getSelectedFiles(QTreeView *treeView);
  QStringList getSelectedFilesWhichExist(QTreeView *treeView);
  QStringList getSubdirectories(const QString &path,
                                const QStringList &excluded) const;
  QStringList getAllFiles(const QString &path,
                          const QStringList &excluded) const;
  void hideAllWidgets() const;
  void ignoreFiles(const WeakPtr &guard, const QStringList &files);
  void ignoreFilesRecursive(const WeakPtr &guard, const QStringList &files);
  void addFilesToLfs(const WeakPtr &guard, const QStringList &files);
  void removeFilesFromLfs(const WeakPtr &guard, const QStringList &files);
  void popStash(const WeakPtr &guard, int stashIndex);
  void prepareWatcher(const WeakPtr &guard);
  bool refreshHistory(const WeakPtr &guard);
  void resetCommitAndMergeIndexTreeUnstageAllButton() const;
  void resetCommitAndMergeWorkingTreeTreeStageAllButton();
  void resetCommitButton() const;
  void resetDisplayTreeExpandedButton();
  void refreshDisplayFilesModels();
  void releaseBranchHistorySemaphore();
  void releaseHistoryRefreshSemaphore();
  void applyDisplayFilesModels();
  void resizeSectionToViewport(int section);
  void restorePreviousSectionWidth();
  void applyHistoryColumnVisibility();
  void persistHistoryColumnVisibility();
  void saveRepoInRecentList() const;
  void saveThreeWayMergeResultFile(const WeakPtr &guard,
                                   const QString &filePath,
                                   const QString &content);
  void startOverlayTimer() const;
  void stopOverlayTimer() const;
  void stopWatcher(const WeakPtr &guard);
  void stretchBranchHistoryTableColumns();
  void showAllWidgets() const;
  void selectHistoryCommit(const QString &commit);
  void updateHistoryNavigationButtons() const;
  void recordHistorySelection(const QString &commit);
  void navigateHistorySelection(int offset);
  void showCommit(const WeakPtr &guard, const QString &commit);
  void showErrorPage(bool error = true) const;
  void showMainSpinnerPage(bool spinnerEnabled = true) const;
  void showSpinnerOverlay(bool spinnerEnabled = true) const;
  void showToast(const QString &message, const QString &title);
  void showErrorToast(const QString &message, const QString &title);
  QString gitFlowBranchPrefix(GitRepo::GitFlowBranchType branchType) const;
  QString gitFlowBranchDisplayName(GitRepo::GitFlowBranchType branchType) const;
  QString gitFlowBranchNameForInput(GitRepo::GitFlowBranchType branchType,
                                    const QString &name) const;
  QString currentGitFlowBranchName(GitRepo::GitFlowBranchType branchType) const;
  void copyFilePathsToClipboard(const QStringList &filePaths);
  bool showWarningForBranchCheckout() const;
  void simulateClickOnHistoryRow(int row) const;
  void simulateClickOnFirstHistoryRow() const;
  void stageAll(const WeakPtr &guard);
  void unstageAll(const WeakPtr &guard);
  void updateBranchHistory(
      const WeakPtr &guard,
      const GitRepo::GitHistoryWithStatus &historyWithStatus);
  void updateLocalBranchesModel(const WeakPtr &guard,
                                const QStringList &localBranches) const;
  void updateLocalBranchesModelActiveBranch(const WeakPtr &guard,
                                            const QString &activeBranch,
                                            bool updatePages = true);
  void updateRemoteBranchesModel(const WeakPtr &guard,
                                 const QStringList &remoteBranches) const;
  void updateRemoteBranchesModelWithStatus(
      const WeakPtr &guard,
      const QVector<GitRepo::BranchInfo> &branchesWithStatus) const;
  void updateRemoteBranchesModelActiveBranch(const WeakPtr &guard,
                                             const QString &activeBranch,
                                             bool updatePages = true);
  void updateStashesModel(const WeakPtr &guard,
                          const QStringList &stashes) const;
  void updateTagsModelActiveTag(const WeakPtr &guard, const QString &activeTag,
                                bool updatePages = true);
  void updateSubmodulesModel(const WeakPtr &guard,
                             const QStringList &tags) const;
  void updateSubmodulesModelWithStatus(
      const WeakPtr &guard,
      const QVector<GitRepo::SubmoduleInfo> &submodulesWithStatus) const;
  void updateTagsModel(const WeakPtr &guard, const QStringList &tags) const;
  void refreshHistoryFilterButtonIcon();
  void updateHistoryFilterButtonGeometry();

private:
  Ui::GitRepoContainer *ui;
  QString _uuid;
  QString _path;
  QString _activeBranch;
  QString _lastDiffedCommit;
  QString _lastDiffedFilePath;
  QString _currentSignature;
  bool _lastDiffRepoStatusChanged;
  GitRepo::BranchType _activeBranchType;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  std::unique_ptr<GitRepoManagerBridge> _bridge;
  Common::UserProfileManager &_userProfileManager;
  Widgets::ToastHostWidget &_toastHost;
  QSharedPointer<Models::GitHistoryModel> _historyModel;
  QToolButton *_historyFilterButton;
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

  bool _applyingHistoryColumnWidths;

  QToolButton *_commitAmendSignButton;
  QAction *_actionCommitAmend;
  QAction *_actionCommitSignOff;
  QAction *_actionCommitSign;
  QPushButton *_commitAndMergeWorkingDirTreeStageAllButton;
  QPushButton *_commitAndMergeIndexTreeUnstageAllButton;
  QPushButton *_stashApplyButton;
  QPushButton *_stashPopButton;
  QCheckBox *_displayFilesAllCheckBox;
  QPushButton *_displayTreeExpandAllButton;
  bool _displayTreeExpanded;
  bool _displayFilesShowAll;
  bool _mergeInProgress;

  QString _currentDisplayCommit;
  QStringList _historySelectionHistory;
  int _historySelectionIndex = -1;
  bool _suppressHistoryNavigationRecord = false;
  QToolButton *_historyBackButton;
  QToolButton *_historyForwardButton;
  QSharedPointer<Models::GitCommitListModel> _commitListModel;
  QSharedPointer<Models::GitCommitTreeModel> _commitTreeModel;
  QSharedPointer<Models::GitCommitListModel> _displayFilesAllListModel;
  QSharedPointer<Models::GitCommitTreeModel> _displayFilesAllTreeModel;
  QSharedPointer<Models::GitCommitListModel> _commitStagedListModel;
  QSharedPointer<Models::GitCommitTreeModel> _commitStagedTreeModel;
  QSharedPointer<Models::GitCommitListModel> _stashFilesModel;
  QSharedPointer<Models::GitCommitTreeModel> _stashFilesTreeModel;

  int _currentStashIndex = -1;
  QString _lastStashCommit;

#ifdef Q_OS_WIN32
  Util::DirectoryWatcher *_repoDirWatcher;
#else
  QFileSystemWatcher *_repoDirWatcher;
#endif
  QTimer *_repoDirWatcherTimer;

  GuiManager::Widgets::ShortcutsToolbarWidget *_shortcutsWidget;
  GuiManager::Widgets::ScrollbarMarksWidget *_unifiedDiffMarks;

  bool _selected;

  QSemaphore *_historyRefreshSemaphore;
  QSemaphore *_branchHistorySemaphore;
};

} // namespace GuiManager::Containers
