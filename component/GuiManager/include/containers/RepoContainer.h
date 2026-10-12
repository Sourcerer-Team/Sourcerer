#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/ElidedLabel.h"
#include "widgets/ShortcutsToolbarWidget.h"
#include "widgets/SpinnerOverlayWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <EventManager.h>
#include <GitRepo.h>
#include <GraphiteUtils.h>
#include <LockGuard.h>
#include <UserProfileManager.h>

#include <QAction>
#include <QEnableSharedFromThis>
#include <QHash>
#include <QVector>
#ifdef Q_OS_WIN32
#include <win32/DirectoryWatcher.h>
#else
#include <QFileSystemWatcher>
#endif
#include <QItemSelection>
#include <QListView>
#include <QMenu>
#include <QProcess>
#include <QPushButton>
#include <QSemaphore>
#include <QSet>
#include <QString>
#include <QTimer>
#include <QToolButton>
#include <QTreeView>
#include <QUrl>
#include <QWidget>

#include <atomic>
#include <functional>
#include <memory>

class QActionGroup;

namespace Ui {
class RepoContainer;
}

namespace GuiManager::Models {
class GitCommitListModel;
class GitCommitTreeModel;
} // namespace GuiManager::Models

namespace GuiManager::Widgets {
class RefBadgesOverlay;
class ScrollbarMarksWidget;
} // namespace GuiManager::Widgets

class QEvent;
class QCheckBox;
class QLabel;
class QNetworkAccessManager;

namespace GuiManager::Models {
class GitHistoryModel;
}

namespace GuiManager::Containers {

class GitRepoManagerBridge;
class GitRepoManagerBridgeImpl;

class RepoContainer final : public Widgets::ContainerWidget,
                            public QEnableSharedFromThis<RepoContainer> {
  Q_OBJECT

  friend class GitRepoManagerBridgeImpl;
  friend class SvnRepoManagerBridgeImpl;
  friend class Models::GitHistoryModel;

  typedef QWeakPointer<RepoContainer> WeakPtr;
  typedef QSharedPointer<RepoContainer> SharedPtr;

public:
  explicit RepoContainer(Widgets::ContainerType type, QString path,
                         QWidget *parent = nullptr);
  ~RepoContainer() override;

  void init() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  QString path() const override { return _path; }

  // Line background markers of the unified diff viewers.
  static constexpr int UnifiedDiffHunkMarker = 1;
  static constexpr int UnifiedDiffDeletedMarker = 2;
  static constexpr int UnifiedDiffAddedMarker = 3;

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

  // Widgets::ContainerWidget
public:
  void deselected() override;
  void selected() override;
  void themeChanged() override;

signals:
  void historyViewRightClicked(const QModelIndex &index, const QPoint &pos);
  void openRepoRequested(const QString &path);
  void openPullRequestRequested(const QString &repositoryName, const QUrl &url,
                                const QString &title);

protected:
  void hideEvent(QHideEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void changeEvent(QEvent *event) override;

private slots:
  void onAddSubmodule();
  void onAddWorktree();
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
  void onStashSplitterMoved(int pos, int index);
  void onHistoryHeaderContextMenuRequested(const QPoint &pos);
  void onHistoryHeaderSectionResized(int logicalIndex, int oldSize,
                                     int newSize);

  void onCheckoutCommit(const QString &commit);
  void onAbortButtonClicked();
  void onCommitButtonClicked();
  void onMergeButtonClicked();
  void onRebaseButtonClicked();
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
  void onWorkflowSelected(GitRepo::WorkflowType workflow);
  void onStartGitLabFlowFeature();
  void onFinishGitLabFlowFeature();
  void onPromoteGitLabFlow();
  void onStartGitLabFlowRelease();
  void onCherryPickToGitLabFlowRelease(const QString &releaseBranchName);
  void onStartGitHubFlowBranch();
  void onUpdateGitHubFlowBranch();
  void onFinishGitHubFlowBranch();
  void onOpenGitHubPullRequest();
  void onInitGitFlowNext();
  void onStartGitFlowNextBranch(const QString &typeName);
  void onFinishGitFlowNextBranch();
  void onUpdateGitFlowNextBranch();
  void onInitGraphite();
  void onCreateGraphiteBranch();
  void onModifyGraphiteBranch();
  void onModifyGraphiteBranchWithCommit();
  void onNavigateGraphiteStack(const QString &command);
  void chooseGraphiteBranch(const QString &command,
                            const QStringList &branches);
  void onTrackGraphiteBranch(const QString &branchName);
  void onCustomWorkflowSelected(const QString &id);
  void onNewCustomWorkflow();
  void onSaveRepositoryWorkflow();
  void onEditCustomWorkflow(const QString &id);
  void onRemoveCustomWorkflow(const QString &id);
  void onRestackGraphite();
  void onSyncGraphite();
  void onSubmitGraphiteStack();
  void onRemoteBranchSelected(const QString &branchName);
  void onRemoteBranchRightClicked(const QString &branchName, const QPoint &pos);
  void onRenameBranch(const QString &branchName);
  void onRenameTag(const QString &tagName);
  void onRepoDirChanged(const QString &path) const;
  void onRepoDirChangedTimeout();
  void onRevertCommit(const QString &commit);
  void onCherryPickCommit();
  void onBisect(GitRepo::BisectAction action, const QString &commit);
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
  void onWorktreeRightClicked(const GitRepo::WorktreeInfo &worktree,
                              const QPoint &pos);
  void onWorktreePanelRightClicked(const QPoint &pos);
  void onWorktreeSelected(const GitRepo::WorktreeInfo &worktree);
  void onRemoveWorktree(const GitRepo::WorktreeInfo &worktree);
  void onLockWorktree(const GitRepo::WorktreeInfo &worktree);
  void onUnlockWorktree(const GitRepo::WorktreeInfo &worktree);
  void onShowSpinnerOverlay();
  void onSpinnerBreakTimer();
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
  void applyToolBoxIcons();
  void blame(const QStringList &files);
  void clearSummaryAndDescription();
  void closeDiffViewerIfOpen();
  void refreshScintillaTheme();
  void refreshSigningControls();
  void refreshUnifiedDiffMarkerColors();
  void updateAuthorAvatar(const QVector<QLabel *> &labels,
                          const QString &authorName, const QString &authorEmail,
                          QString &currentAuthorAvatarKey,
                          quint64 &authorAvatarRequestSerial);
  void updateCommitAuthorAvatar(const QString &authorName,
                                const QString &authorEmail);
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
  void updateAllBranchesActions();
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
  void promptToStartBranch(
      const QString &placeholderText,
      const std::function<QString(const QString &)> &branchNameForInput,
      const std::function<void(const WeakPtr &, const QString &)> &startBranch);
  void runFlowOperation(const std::function<void(const WeakPtr &)> &operation);
  void applyWorkflow(GitRepo::WorkflowType workflow);
  void applyWorkflowState(const GitRepo::WorkflowState &state);
  // The git-flow-next entries depend on the repository configuration and the
  // current branch, so they are recreated whenever the menu opens.
  void rebuildGitFlowNextMenu();
  // The custom GitFlow-Next workflows of the Workflow submenu.
  void rebuildCustomWorkflowMenu();
  void showCustomWorkflowEditor(const GitRepo::GitFlowNextWorkflow &workflow);
  // Shows how to install the git-flow-next command line tool when it is not
  // in PATH. Returns true if it was found.
  bool ensureGitFlowNextCli();
  bool ensureGraphiteCli();
  bool ensureCliInstalled(const QString &executable, const QString &toolName,
                          const QString &workflowName, const QString &url);
  bool graphiteInitialized() const;
  void updateGraphiteMenu();
  // The local branches panel shows the Graphite stacks read by gt state.
  void requestGraphiteStacksReload() const;
  void reloadGraphiteStacks();
  void setGraphiteStacks(const GitRepo::GraphiteStacks &stacks);
  void addGraphiteBranchActions(QMenu &menu, const QString &branchName);
  // Run on the dispatcher thread and return an error message, or an empty
  // string on success.
  QString runGraphiteCli(const QStringList &arguments,
                         QString *output = nullptr) const;
  QString runGitCli(const QStringList &arguments,
                    int *exitCode = nullptr) const;
  QString commitAllChanges(const QString &message, const QString &author,
                           const QString &email) const;
  void runGraphiteOperation(const std::function<QString()> &operation,
                            const QString &successMessage);
  void updateGitLabFlowMenu();
  void updateGitLabFlowCherryPickMenu();
  QString gitLabFlowMainBranchName() const;
  QStringList gitLabFlowEnvironmentChain() const;
  QString gitLabFlowReleaseBranchSuffix() const;
  QString gitHubFlowMainBranchName() const;
  QString selectedHistoryCommit() const;
  void copyFilePathsToClipboard(const QStringList &filePaths);
  bool showWarningForUncommittedChanges(const QString &title,
                                        const QString &message) const;
  bool showWarningForBranchCheckout() const;
  void simulateClickOnHistoryRow(int row) const;
  void simulateClickOnFirstHistoryRow() const;
  void stageAll(const WeakPtr &guard);
  void unstageAll(const WeakPtr &guard);
  void
  updateBranchHistory(const WeakPtr &guard,
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
  void
  updateWorktreesModel(const WeakPtr &guard,
                       const QVector<GitRepo::WorktreeInfo> &worktrees) const;
  void updateTagsModel(const WeakPtr &guard, const QStringList &tags) const;
  void updateRemoteTagsModel(const WeakPtr &guard,
                             const QStringList &tags) const;
  void refreshHistoryFilterButtonIcon();
  void updateHistoryFilterButtonGeometry();
  void updateHistorySearchRepositoryButton();
  void updateStashContentInfo(int stashIndex,
                              const GitRepo::GitCommitStatus &stashStatus);

private:
  Ui::RepoContainer *ui;
  QString _uuid;
  QString _path;
  QString _activeBranch;
  // Local branches as of the last refresh. Used to populate the branch
  // dropdown of the add worktree dialog.
  mutable QStringList _localBranches;
  mutable QStringList _remoteBranches;
  // Branches checked out in other worktrees as of the last refresh (branch ->
  // worktree path). Git refuses to checkout or delete such branches.
  mutable QHash<QString, QString> _worktreeBranches;
  // True while the history of all branches ('*' item) is shown. The checked
  // out branch stays in _activeBranch, the history is requested for all
  // branches instead of it.
  std::atomic_bool _historyViewAll{false};
  // Shortcuts bar items that need a single branch, so they are disabled
  // while the history of all branches is shown.
  QToolButton *_pullFetchButton{nullptr};
  QToolButton *_pushButton{nullptr};
  QVector<QAction *> _singleBranchShortcutActions;
  QToolButton *_gitFlowButton{nullptr};
  QAction *_gitFlowWorkflowAction{nullptr};
  QAction *_gitLabFlowWorkflowAction{nullptr};
  QAction *_gitHubFlowWorkflowAction{nullptr};
  QAction *_gitFlowNextWorkflowAction{nullptr};
  QAction *_graphiteWorkflowAction{nullptr};
  QMenu *_workflowMenu{nullptr};
  QActionGroup *_workflowGroup{nullptr};
  QVector<QAction *> _customWorkflowMenuActions;
  QVector<QMenu *> _customWorkflowSubmenus;
  QString _customWorkflowId;
  int _customWorkflowRevision{0};
  QAction *_graphiteInitAction{nullptr};
  QMenu *_gitFlowMenu{nullptr};
  QAction *_gitLabFlowPromoteAction{nullptr};
  QMenu *_gitLabFlowCherryPickMenu{nullptr};
  // Menu entries, separators included, shown for each workflow.
  QVector<QAction *> _gitFlowActions;
  QVector<QAction *> _gitLabFlowActions;
  QVector<QAction *> _gitHubFlowActions;
  QVector<QAction *> _gitFlowNextActions;
  QVector<QAction *> _graphiteActions;
  QTimer *_graphiteStacksTimer{nullptr};
  QProcess *_graphiteStacksProcess{nullptr};
  bool _graphiteStacksReloadPending{false};
  GitRepo::GraphiteStacks _graphiteStacks;
  GitRepo::GitFlowNextConfig _gitFlowNextConfig;
  GitRepo::WorkflowType _workflow{GitRepo::WorkflowType::GitFlow};
  // Commit the user asked for that is not loaded yet, selected once the
  // history page containing it arrives.
  QString _pendingSelectCommit;
  // Full commit SHA of each line of the blame viewer, empty for lines that
  // are not blamed to a commit.
  QStringList _blameCommits;
  static constexpr int BlameCommitMargin = 1;
  static constexpr int BlameAuthorMargin = 2;
  static constexpr int BlameShortShaLength = 6;
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
  // Animates the icon of the search repository button while searching.
  QTimer *_historySearchSpinnerTimer;
  int _historySearchSpinnerAngle;
  Widgets::RefBadgesOverlay *_refBadgesOverlay;
  QPixmap *_errorPixmap;
  QNetworkAccessManager *_authorAvatarNetworkManager;
  QHash<QString, QPixmap> _authorAvatarCache;
  QSet<QString> _authorAvatarInFlight;
  QString _currentAuthorAvatarKey;
  quint64 _authorAvatarRequestSerial = 0;
  QString _currentStashAuthorAvatarKey;
  quint64 _stashAuthorAvatarRequestSerial = 0;
  Widgets::ElidedLabel *_authorNameValueLabel;
  Widgets::ElidedLabel *_authorEmailValueLabel;
  Util::Dispatcher _dispatcher;

  Widgets::SpinnerOverlayWidget *_spinnerOverlay;
  QTimer *_spinnerOverlayTimer;
  QTimer *_spinnerBreakTimer;

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
  bool _rebaseInProgress;

  QString _currentDisplayCommit;
  QStringList _historySelectionHistory;
  int _historySelectionIndex = -1;
  bool _suppressHistoryNavigationRecord = false;
  QToolButton *_historyBackButton;
  QToolButton *_historyForwardButton;
  QVector<QLabel *> _stashContentAuthorAvatarLabels;
  QVector<QLabel *> _stashContentAuthorNameValueLabels;
  QVector<QLabel *> _stashContentAuthorEmailValueLabels;
  QVector<QLabel *> _stashContentCommitterDateValueLabels;
  QVector<QLabel *> _stashContentShaValueLabels;
  QVector<QLabel *> _stashContentParentShaValueLabels;
  QVector<QLabel *> _stashContentSummaryValueLabels;
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

  ::GuiManager::Widgets::ShortcutsToolbarWidget *_shortcutsWidget;
  ::GuiManager::Widgets::ScrollbarMarksWidget *_unifiedDiffMarks;

  bool _selected;

  QSemaphore *_historyRefreshSemaphore;
  QSemaphore *_branchHistorySemaphore;

  QString _rebaseSourceBranch;
  QString _rebaseDestinationBranch;
};

} // namespace GuiManager::Containers
