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

#include <QCheckBox>
#include <QEnableSharedFromThis>
#ifdef Q_OS_WIN32
#include <win32/DirectoryWatcher.h>
#else
#include <QFileSystemWatcher>
#endif
#include <QItemSelection>
#include <QListView>
#include <QPushButton>
#include <QSet>
#include <QString>
#include <QTreeView>
#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager::Models {
class GitCommitListModel;
class GitCommitTreeModel;
} // namespace GuiManager::Models

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

  QString path() const override { return _path; }

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
  void onCommitButtonClicked();
  void onCommitAndMergeIndexTreeUnstageAllButtonClicked();
  void onCommitAndMergeWorkingTreeTreeStageAllButtonClicked();
  void onDisplayTreeExpandAllButtonClicked();
  void onHistorySelectionChanged(const QItemSelection &selected,
                                 const QItemSelection &deselected);
  void onHistoryViewItemEntered(const QModelIndex &index);
  void onLocalBranchSelected(const QString &branchName);
  void onPop();
  void onPull();
  void onPush();
  void onRemoteBranchSelected(const QString &branchName);
  void onRepoDirChanged(const QString &path);
  void onRepoDirChangedTimeout();
  void onStash();
  void onTagSelected(const QString &tagName);
  void onShowSpinnerOverlay();
  void onGraphColumnExpand(int section);
  void onWorkingTreeFlatListViewContextMenuRequested(const QPoint &pos);
  void onWorkingTreeFlatListViewStageSelected(bool checked = false);
  void onWorkingTreeFlatListViewIgnoreSelected(bool checked = false);
  void onWorkingTreeFlatListViewCreateFilePatchSelected(bool checked = false);
  void onWorkingTreeFlatListViewDeleteSelected(bool checked = false);
  void onWorkingTreeFlatListViewDiscardSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewContextMenuRequested(const QPoint &pos);
  void onWorkingTreeTreeTreeViewStageSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewIgnoreSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewCreateFilePatchSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewDeleteSelected(bool checked = false);
  void onWorkingTreeTreeTreeViewDiscardSelected(bool checked = false);
  void onIndexFlatListViewContextMenuRequested(const QPoint &pos);
  void onIndexFlatListViewUnstageSelected(bool checked = false);
  void onIndexFlatListViewIgnoreSelected(bool checked = false);
  void onIndexFlatListViewCreateFilePatchSelected(bool checked = false);
  void onIndexFlatListViewDeleteSelected(bool checked = false);
  void onIndexFlatListViewDiscardSelected(bool checked = false);
  void onIndexTreeTreeViewContextMenuRequested(const QPoint &pos);
  void onIndexTreeTreeViewUnstageSelected(bool checked = false);
  void onIndexTreeTreeViewIgnoreSelected(bool checked = false);
  void onIndexTreeTreeViewCreateFilePatchSelected(bool checked = false);
  void onIndexTreeTreeViewDeleteSelected(bool checked = false);
  void onIndexTreeTreeViewDiscardSelected(bool checked = false);
  void updateModelActiveBranch(const WeakPtr &guard, const QString &branchName,
                               GitRepo::BranchType branchType);

private:
  void checkoutBranch(const WeakPtr &guard, const QString &branchName,
                      GitRepo::BranchType branchType);
  void clearSummaryAndDescription();
  void closeRepo() const;
  void commit(const WeakPtr &guard, const QString &summary,
              const QString &description, const QString &author,
              const QString &email);
  void deleteFiles(const WeakPtr &guard, const QStringList &files);
  void diffFiles(const WeakPtr &guard, const QStringList &files);
  void discardFiles(const WeakPtr &guard, const QStringList &files);
  bool getActiveBranchOrTag(const WeakPtr &guard,
                            const GitRepo::BranchType branchType);
  bool getActiveLocalBranch();
  bool getActiveRemoteBranch();
  bool getActiveTag();
  bool getBranchHistory(const WeakPtr &guard);
  [[nodiscard]] QStringList getCommitBranches(const WeakPtr &guard,
                                              const QString &commit) const;
  [[nodiscard]] GitRepo::GitCommitStatus
  getCommitStatus(const WeakPtr &guard, const QString &commit) const;
  QStringList getSelectedFiles(QListView *listView);
  QStringList getSelectedFilesWhichExist(QListView *listView);
  QStringList getSelectedFiles(QTreeView *treeView);
  QStringList getSelectedFilesWhichExist(QTreeView *treeView);
  bool getLocalBranches();
  bool getRemoteBranches();
  bool getSubmodules();
  bool getTags();
  QStringList getSubdirectories(const QString &path,
                                const QStringList &excluded) const;
  void hideAllWidgets() const;
  void ignoreFiles(const WeakPtr &guard, const QStringList &files);
  bool openRepo();
  void pull(const WeakPtr &guard);
  void push(const WeakPtr &guard);
  void prepareWatcher(const WeakPtr &guard);
  bool refreshHistory(const WeakPtr &guard);
  void refreshStatus(const WeakPtr &guard);
  void resetCommitAndMergeIndexTreeUnstageAllButton();
  void resetCommitAndMergeWorkingTreeTreeStageAllButton();
  void resetCommitButton();
  void resetDisplayTreeExpandedButton();
  void resizeSectionToViewport(int section);
  void restorePreviousSectionWidth();
  void saveRepoInRecentList() const;
  void startOverlayTimer() const;
  void stopOverlayTimer() const;
  void stopWatcher(const WeakPtr &guard);
  void stretchBranchHistoryTableColumns() const;
  void showAllWidgets() const;
  void showCommit(const WeakPtr &guard, const QString &commit);
  void showCommitAndMerge(WeakPtr guard);
  void showErrorPage(bool error = true) const;
  void showMainSpinnerPage(bool spinnerEnabled = true) const;
  void showSpinnerOverlay(bool spinnerEnabled = true) const;
  void showToast(const QString &message, const QString &title);
  void showErrorToast(const QString &message, const QString &title);
  void simulateClickOnCurrentHistoryRow() const;
  void simulateClickOnFirstHistoryRow() const;
  void stageAll(const WeakPtr &guard);
  void stageFiles(const WeakPtr &guard, const QStringList &files);
  void unstageAll(const WeakPtr &guard);
  void unstageFiles(const WeakPtr &guard, const QStringList &files);
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
  Common::UserProfileManager &_userProfileManager;
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
  QPushButton *_commitAndMergeWorkingDirTreeStageAllButton;
  QPushButton *_commitAndMergeIndexTreeUnstageAllButton;
  QPushButton *_displayTreeExpandAllButton;
  bool _displayTreeExpanded;

  QSharedPointer<Models::GitCommitListModel> _commitListModel;
  QSharedPointer<Models::GitCommitTreeModel> _commitTreeModel;
  QSharedPointer<Models::GitCommitListModel> _commitStagedListModel;
  QSharedPointer<Models::GitCommitTreeModel> _commitStagedTreeModel;

#ifdef Q_OS_WIN32
  Util::DirectoryWatcher *_repoDirWatcher;
#else
  QFileSystemWatcher *_repoDirWatcher;
#endif
  QTimer *_repoDirWatcherTimer;

  GuiManager::Widgets::ShortcutsToolbarWidget *_shortcutsWidget;
};

} // namespace GuiManager::Containers
