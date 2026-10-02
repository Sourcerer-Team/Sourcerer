#pragma once

#include <GitRepoManagerState.h>
#include <GitRepoManagerStructs.h>

#include <EventManager.h>
#include <LockGuard.h>

#include <QMap>
#include <QString>

namespace GitRepoManager {

class ProcessingState final : public GitRepoManagerState {
  Q_OBJECT

public:
  explicit ProcessingState(GitRepoManagerFsm &fsm);
  ~ProcessingState() override;

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  void abortMergeWorker(const Structs::GitRepoAbortMergeCommand &command);
  void abortRebaseWorker(const Structs::GitRepoAbortRebaseCommand &command);
  void addSubmoduleWorker(const Structs::GitRepoAddSubmoduleCommand &command);
  void amendWorker(const Structs::GitRepoAmendCommand &command);
  void blameWorker(const Structs::GitRepoBlameCommand &command);
  void applyPatchWorker(const Structs::GitRepoApplyPatchCommand &command);
  void applyStashWorker(const Structs::GitRepoApplyStashCommand &command);
  void
  checkoutBranchWorker(const Structs::GitRepoCheckoutBranchCommand &command);
  void
  checkoutCommitWorker(const Structs::GitRepoCheckoutCommitCommand &command);
  void cloneRepoWorker(const Structs::GitRepoCloneCommand &command);
  void closeRepoWorker(const Structs::GitRepoCloseCommand &command);
  void commitStagedWorker(const Structs::GitRepoCommitStagedCommand &command);
  void commitRebaseWorker(const Structs::GitRepoCommitRebaseCommand &command);
  void createBranchFromCommitWorker(
      const Structs::GitRepoCreateBranchFromCommitCommand &command);
  void createPatchFromCommitWorker(
      const Structs::GitRepoCreatePatchFromCommitCommand &command);
  void createTagFromCommitWorker(
      const Structs::GitRepoCreateTagFromCommitCommand &command);
  Structs::GitRepoContext *context(const QString &uuid);
  void destroyContext(const QString &uuid);
  void destroyContextsMap();
  void deleteBranchWorker(const Structs::GitRepoDeleteBranchCommand &command);
  void deleteRemoteBranchWorker(
      const Structs::GitRepoDeleteRemoteBranchCommand &command);
  void deleteStashWorker(const Structs::GitRepoDeleteStashCommand &command);
  void deleteAllStashesWorker(
      const Structs::GitRepoDeleteAllStashesCommand &command);
  void
  deleteSubmoduleWorker(const Structs::GitRepoDeleteSubmoduleCommand &command);
  void deleteTagWorker(const Structs::GitRepoDeleteTagCommand &command);
  void
  deleteRemoteTagWorker(const Structs::GitRepoDeleteRemoteTagCommand &command);
  void diffFilesWorker(const Structs::GitRepoDiffFilesCommand &command);
  void diffFilesFromCommitWorker(
      const Structs::GitRepoDiffFilesFromCommitCommand &command);
  void discardFilesWorker(const Structs::GitRepoDiscardFilesCommand &command);
  void eventCallback(const EventManager::Event event);
  void fetchAllWorker(const Structs::GitRepoFetchAllCommand &command);
  void fetchRemoteWorker(const Structs::GitRepoFetchRemoteCommand &command);
  void rebaseWorker(const Structs::GitRepoRebaseCommand &command);
  void squashWorker(const Structs::GitRepoSquashCommand &command);
  void bisectWorker(const Structs::GitRepoBisectCommand &command);
  void gitFlowWorker(const Structs::GitRepoGitFlowCommand &command);
  void
  getActiveBranchWorker(const Structs::GitRepoGetActiveBranchCommand &command);
  void getFileSizeFromCommitWorker(
      const Structs::GitRepoGetFileSizeFromCommitCommand &command);
  void getCommitBranchesWorker(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void getCommitTagsWorker(const Structs::GitRepoGetCommitTagsCommand &command);
  void
  getCommitHeadsWorker(const Structs::GitRepoGetCommitHeadsCommand &command);
  void
  getCommitIsHeadWorker(const Structs::GitRepoGetCommitIsHeadCommand &command);
  void
  getCommitStatusWorker(const Structs::GitRepoGetCommitStatusCommand &command);
  void getLfsStorageStatusWorker(
      const Structs::GitRepoGetLfsStorageStatusCommand &command);
  void
  getStashStatusWorker(const Structs::GitRepoGetStashStatusCommand &command);
  void getIsMergeInProgressWorker(
      const Structs::GitRepoGetIsMergeInProgressCommand &command);
  void getIsRebaseInProgressWorker(
      const Structs::GitRepoGetIsRebaseInProgressCommand &command);
  void getBranchHistoryWorker(
      const Structs::GitRepoGetBranchHistoryCommand &command);
  void getSubmodulePathWorker(
      const Structs::GitRepoGetSubmodulePathCommand &command);
  void getUpstreamWorker(const Structs::GitRepoGetUpstreamCommand &command);
  void initRepoWorker(const Structs::GitRepoInitRepoCommand &command);
  void listConflictsWorker(const Structs::GitRepoListConflictsCommand &command);
  void listLocalBranchesWorker(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void listRemoteBranchesWorker(
      const Structs::GitRepoListRemoteBranchesCommand &command);
  void listRemoteBranchesWithStatusWorker(
      const Structs::GitRepoListRemoteBranchesWithStatusCommand &command);
  void listStashesWorker(const Structs::GitRepoListStashesCommand &command);
  void
  listSubmodulesWorker(const Structs::GitRepoListSubmodulesCommand &command);
  void listSubmodulesWithStatusWorker(
      const Structs::GitRepoListSubmodulesWithStatusCommand &command);
  void scheduleWorkerForListSubmodulesWithStatus(
      const Structs::GitRepoListSubmodulesWithStatusCommand &command);
  void listTagsWorker(const Structs::GitRepoListTagsCommand &command);
  void mergeWorker(const Structs::GitRepoMergeCommand &command);
  void openRepoWorker(const Structs::GitRepoOpenCommand &command);
  void pullWorker(const Structs::GitRepoPullCommand &command);
  void pushWorker(const Structs::GitRepoPushCommand &command);
  void pushTagWorker(const Structs::GitRepoPushTagCommand &command);
  void pushAllTagsWorker(const Structs::GitRepoPushAllTagsCommand &command);
  void renameBranchWorker(const Structs::GitRepoRenameBranchCommand &command);
  void renameTagWorker(const Structs::GitRepoRenameTagCommand &command);
  void resetWorker(const Structs::GitRepoResetCommand &command);
  void revertWorker(const Structs::GitRepoRevertCommand &command);
  void cherryPickWorker(const Structs::GitRepoCherryPickCommand &command);
  void
  scheduleWorkerForCherryPick(const Structs::GitRepoCherryPickCommand &command);
  void setUpstreamWorker(const Structs::GitRepoSetUpstreamCommand &command);
  void stageFilesWorker(const Structs::GitRepoStageFilesCommand &command);
  void stashWorker(const Structs::GitRepoStashCommand &command);
  void statusWorker(const Structs::GitRepoStatusCommand &command);
  void
  submoduleUpdateWorker(const Structs::GitRepoSubmoduleUpdateCommand &command);
  void unstageFilesWorker(const Structs::GitRepoUnstageFilesCommand &command);
  void listRemotesWorker(const Structs::GitRepoListRemotesCommand &command);
  void addRemoteWorker(const Structs::GitRepoAddRemoteCommand &command);
  void removeRemoteWorker(const Structs::GitRepoRemoveRemoteCommand &command);
  void updateRemoteWorker(const Structs::GitRepoUpdateRemoteCommand &command);
  void setDefaultRemoteWorker(
      const Structs::GitRepoSetDefaultRemoteCommand &command);
  void addTargetWorker(const Structs::GitRepoAddTargetCommand &command);
  void editTargetWorker(const Structs::GitRepoEditTargetCommand &command);
  void deleteTargetWorker(const Structs::GitRepoDeleteTargetCommand &command);
  void setDefaultTargetWorker(
      const Structs::GitRepoSetDefaultTargetCommand &command);
  void quitRepoWorker(const Structs::GitRepoQuitCommand &command);
  void reportError(EventManager::EventType eventType, const QString &message,
                   const Structs::GitRepoCommand &command) const;
  void
  scheduleWorkerForAbortMerge(const Structs::GitRepoAbortMergeCommand &command);
  void scheduleWorkerForAbortRebase(
      const Structs::GitRepoAbortRebaseCommand &command);
  void scheduleWorkerForAddSubmodule(
      const Structs::GitRepoAddSubmoduleCommand &command);
  void scheduleWorkerForAmend(const Structs::GitRepoAmendCommand &command);
  void scheduleWorkerForBlame(const Structs::GitRepoBlameCommand &command);
  void
  scheduleWorkerForApplyPatch(const Structs::GitRepoApplyPatchCommand &command);
  void
  scheduleWorkerForApplyStash(const Structs::GitRepoApplyStashCommand &command);
  void scheduleWorkerForCheckoutBranch(
      const Structs::GitRepoCheckoutBranchCommand &command);
  void scheduleWorkerForCheckoutCommit(
      const Structs::GitRepoCheckoutCommitCommand &command);
  void scheduleWorkerForCloneRepo(const Structs::GitRepoCloneCommand &command);
  void scheduleWorkerForCloseRepo(const Structs::GitRepoCloseCommand &command);
  void scheduleWorkerForCommitStagedRepo(
      const Structs::GitRepoCommitStagedCommand &command);
  void scheduleWorkerForCommitRebaseRepo(
      const Structs::GitRepoCommitRebaseCommand &command);
  void scheduleWorkerForCreateBranchFromCommit(
      const Structs::GitRepoCreateBranchFromCommitCommand &command);
  void scheduleWorkerForCreatePatchFromCommit(
      const Structs::GitRepoCreatePatchFromCommitCommand &command);
  void scheduleWorkerForCreateTagFromCommit(
      const Structs::GitRepoCreateTagFromCommitCommand &command);
  void scheduleWorkerForGetActiveBranch(
      const Structs::GitRepoGetActiveBranchCommand &command);
  void
  scheduleWorkerForFetchAll(const Structs::GitRepoFetchAllCommand &command);
  void scheduleWorkerForFetchRemote(
      const Structs::GitRepoFetchRemoteCommand &command);
  void scheduleWorkerForRebase(const Structs::GitRepoRebaseCommand &command);
  void scheduleWorkerForSquash(const Structs::GitRepoSquashCommand &command);
  void scheduleWorkerForBisect(const Structs::GitRepoBisectCommand &command);
  void scheduleWorkerForGitFlow(const Structs::GitRepoGitFlowCommand &command);
  void scheduleWorkerForGetCommitBranches(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void scheduleWorkerForGetCommitTags(
      const Structs::GitRepoGetCommitTagsCommand &command);
  void scheduleWorkerForGetCommitHeads(
      const Structs::GitRepoGetCommitHeadsCommand &command);
  void scheduleWorkerForGetCommitIsHead(
      const Structs::GitRepoGetCommitIsHeadCommand &command);
  void scheduleWorkerForGetCommitStatus(
      const Structs::GitRepoGetCommitStatusCommand &command);
  void scheduleWorkerForGetLfsStorageStatus(
      const Structs::GitRepoGetLfsStorageStatusCommand &command);
  void scheduleWorkerForGetStashStatus(
      const Structs::GitRepoGetStashStatusCommand &command);
  void scheduleWorkerForGetIsMergeInProgress(
      const Structs::GitRepoGetIsMergeInProgressCommand &command);
  void scheduleWorkerForGetIsRebaseInProgress(
      const Structs::GitRepoGetIsRebaseInProgressCommand &command);
  void scheduleWorkerForGetSubmodulePath(
      const Structs::GitRepoGetSubmodulePathCommand &command);
  void scheduleWorkerForGetUpstream(
      const Structs::GitRepoGetUpstreamCommand &command);
  void scheduleWorkerForDeleteAllStashes(
      const Structs::GitRepoDeleteAllStashesCommand &command);
  void scheduleWorkerForDeleteBranch(
      const Structs::GitRepoDeleteBranchCommand &command);
  void scheduleWorkerForDeleteRemoteBranch(
      const Structs::GitRepoDeleteRemoteBranchCommand &command);
  void scheduleWorkerForDeleteStash(
      const Structs::GitRepoDeleteStashCommand &command);
  void scheduleWorkerForDeleteSubmodule(
      const Structs::GitRepoDeleteSubmoduleCommand &command);
  void
  scheduleWorkerForDeleteTag(const Structs::GitRepoDeleteTagCommand &command);
  void scheduleWorkerForDeleteRemoteTag(
      const Structs::GitRepoDeleteRemoteTagCommand &command);
  void
  scheduleWorkerForDiffFiles(const Structs::GitRepoDiffFilesCommand &command);
  void scheduleWorkerForDiffFilesFromCommit(
      const Structs::GitRepoDiffFilesFromCommitCommand &command);
  void scheduleWorkerForDiscardFiles(
      const Structs::GitRepoDiscardFilesCommand &command);
  void scheduleWorkerForGetFileSizeFromCommit(
      const Structs::GitRepoGetFileSizeFromCommitCommand &command);
  void scheduleWorkerForGetBranchHistory(
      const Structs::GitRepoGetBranchHistoryCommand &command);
  void
  scheduleWorkerForInitRepo(const Structs::GitRepoInitRepoCommand &command);
  void scheduleWorkerForListConflicts(
      const Structs::GitRepoListConflictsCommand &command);
  void scheduleWorkerForListLocalBranches(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void scheduleWorkerForListRemoteBranches(
      const Structs::GitRepoListRemoteBranchesCommand &command);
  void scheduleWorkerForListRemoteBranchesWithStatus(
      const Structs::GitRepoListRemoteBranchesWithStatusCommand &command);
  void scheduleWorkerForListStashes(
      const Structs::GitRepoListStashesCommand &command);
  void scheduleWorkerForListSubmodules(
      const Structs::GitRepoListSubmodulesCommand &command);
  void
  scheduleWorkerForListTags(const Structs::GitRepoListTagsCommand &command);
  void scheduleWorkerForMerge(const Structs::GitRepoMergeCommand &command);
  void scheduleWorkerForOpenRepo(const Structs::GitRepoOpenCommand &command);
  void scheduleWorkerForPull(const Structs::GitRepoPullCommand &command);
  void scheduleWorkerForPush(const Structs::GitRepoPushCommand &command);
  void scheduleWorkerForPushTag(const Structs::GitRepoPushTagCommand &command);
  void scheduleWorkerForPushAllTags(
      const Structs::GitRepoPushAllTagsCommand &command);
  void scheduleWorkerForRenameBranch(
      const Structs::GitRepoRenameBranchCommand &command);
  void
  scheduleWorkerForRenameTag(const Structs::GitRepoRenameTagCommand &command);
  void scheduleWorkerForReset(const Structs::GitRepoResetCommand &command);
  void scheduleWorkerForRevert(const Structs::GitRepoRevertCommand &command);
  void scheduleWorkerForSetUpstream(
      const Structs::GitRepoSetUpstreamCommand &command);
  void
  scheduleWorkerForStageFiles(const Structs::GitRepoStageFilesCommand &command);
  void scheduleWorkerForStash(const Structs::GitRepoStashCommand &command);
  void scheduleWorkerForStatus(const Structs::GitRepoStatusCommand &command);
  void scheduleWorkerForSubmoduleUpdate(
      const Structs::GitRepoSubmoduleUpdateCommand &command);
  void scheduleWorkerForUnstageFiles(
      const Structs::GitRepoUnstageFilesCommand &command);
  void scheduleWorkerForListRemotes(
      const Structs::GitRepoListRemotesCommand &command);
  void
  scheduleWorkerForAddRemote(const Structs::GitRepoAddRemoteCommand &command);
  void scheduleWorkerForRemoveRemote(
      const Structs::GitRepoRemoveRemoteCommand &command);
  void scheduleWorkerForUpdateRemote(
      const Structs::GitRepoUpdateRemoteCommand &command);
  void scheduleWorkerForSetDefaultRemote(
      const Structs::GitRepoSetDefaultRemoteCommand &command);
  void
  scheduleWorkerForAddTarget(const Structs::GitRepoAddTargetCommand &command);
  void
  scheduleWorkerForEditTarget(const Structs::GitRepoEditTargetCommand &command);
  void scheduleWorkerForDeleteTarget(
      const Structs::GitRepoDeleteTargetCommand &command);
  void scheduleWorkerForSetDefaultTarget(
      const Structs::GitRepoSetDefaultTargetCommand &command);
  void scheduleWorkerForQuit(const Structs::GitRepoQuitCommand &command);
  void workersThreadFunc(const QString &uuid);

private:
  std::atomic<bool> _quitAll;
  Util::Mutex _contextsMutex;
  QMap<QString, Structs::GitRepoContext *> _contexts;
  EventManager::EventManager &_eventManager;
};

} // namespace GitRepoManager
