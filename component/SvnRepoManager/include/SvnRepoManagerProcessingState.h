#pragma once

#include "SvnRepoManagerState.h"
#include "SvnRepoManagerStructs.h"

#include <EventManager.h>
#include <LockGuard.h>

#include <QMap>
#include <QString>

namespace SvnRepoManager {

class SvnRepoManagerProcessingState final : public SvnRepoManagerState {
  Q_OBJECT

public:
  explicit SvnRepoManagerProcessingState(SvnRepoManagerFsm &fsm);
  ~SvnRepoManagerProcessingState() override;

  // FsmState interface
public:
  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  void abortMergeWorker(const Structs::SvnRepoAbortMergeCommand &command);
  void addSubmoduleWorker(const Structs::SvnRepoAddSubmoduleCommand &command);
  void amendWorker(const Structs::SvnRepoAmendCommand &command);
  void applyPatchWorker(const Structs::SvnRepoApplyPatchCommand &command);
  void applyStashWorker(const Structs::SvnRepoApplyStashCommand &command);
  void
  checkoutBranchWorker(const Structs::SvnRepoCheckoutBranchCommand &command);
  void
  checkoutCommitWorker(const Structs::SvnRepoCheckoutCommitCommand &command);
  void cloneRepoWorker(const Structs::SvnRepoCloneCommand &command);
  void closeRepoWorker(const Structs::SvnRepoCloseCommand &command);
  void commitStagedWorker(const Structs::SvnRepoCommitStagedCommand &command);
  void createBranchFromCommitWorker(
      const Structs::SvnRepoCreateBranchFromCommitCommand &command);
  void createPatchFromCommitWorker(
      const Structs::SvnRepoCreatePatchFromCommitCommand &command);
  void createTagFromCommitWorker(
      const Structs::SvnRepoCreateTagFromCommitCommand &command);
  Structs::SvnRepoContext *context(const QString &uuid);
  void destroyContext(const QString &uuid);
  void destroyContextsMap();
  void deleteBranchWorker(const Structs::SvnRepoDeleteBranchCommand &command);
  void deleteRemoteBranchWorker(
      const Structs::SvnRepoDeleteRemoteBranchCommand &command);
  void deleteStashWorker(const Structs::SvnRepoDeleteStashCommand &command);
  void deleteAllStashesWorker(
      const Structs::SvnRepoDeleteAllStashesCommand &command);
  void
  deleteSubmoduleWorker(const Structs::SvnRepoDeleteSubmoduleCommand &command);
  void deleteTagWorker(const Structs::SvnRepoDeleteTagCommand &command);
  void
  deleteRemoteTagWorker(const Structs::SvnRepoDeleteRemoteTagCommand &command);
  void diffFilesWorker(const Structs::SvnRepoDiffFilesCommand &command);
  void diffFilesFromCommitWorker(
      const Structs::SvnRepoDiffFilesFromCommitCommand &command);
  void discardFilesWorker(const Structs::SvnRepoDiscardFilesCommand &command);
  void eventCallback(const EventManager::Event event);
  void fetchAllWorker(const Structs::SvnRepoFetchAllCommand &command);
  void fetchRemoteWorker(const Structs::SvnRepoFetchRemoteCommand &command);
  void rebaseWorker(const Structs::SvnRepoRebaseCommand &command);
  void
  getActiveBranchWorker(const Structs::SvnRepoGetActiveBranchCommand &command);
  void getFileSizeFromCommitWorker(
      const Structs::SvnRepoGetFileSizeFromCommitCommand &command);
  void getCommitBranchesWorker(
      const Structs::SvnRepoGetCommitBranchesCommand &command);
  void getCommitTagsWorker(const Structs::SvnRepoGetCommitTagsCommand &command);
  void
  getCommitHeadsWorker(const Structs::SvnRepoGetCommitHeadsCommand &command);
  void
  getCommitIsHeadWorker(const Structs::SvnRepoGetCommitIsHeadCommand &command);
  void
  getCommitStatusWorker(const Structs::SvnRepoGetCommitStatusCommand &command);
  void
  getStashStatusWorker(const Structs::SvnRepoGetStashStatusCommand &command);
  void getIsMergeInProgressWorker(
      const Structs::SvnRepoGetIsMergeInProgressCommand &command);
  void getBranchHistoryWorker(
      const Structs::SvnRepoGetBranchHistoryCommand &command);
  void getSubmodulePathWorker(
      const Structs::SvnRepoGetSubmodulePathCommand &command);
  void getUpstreamWorker(const Structs::SvnRepoGetUpstreamCommand &command);
  void initRepoWorker(const Structs::SvnRepoInitRepoCommand &command);
  void listConflictsWorker(const Structs::SvnRepoListConflictsCommand &command);
  void listLocalBranchesWorker(
      const Structs::SvnRepoListLocalBranchesCommand &command);
  void listRemoteBranchesWorker(
      const Structs::SvnRepoListRemoteBranchesCommand &command);
  void listRemoteBranchesWithStatusWorker(
      const Structs::SvnRepoListRemoteBranchesWithStatusCommand &command);
  void listStashesWorker(const Structs::SvnRepoListStashesCommand &command);
  void
  listSubmodulesWorker(const Structs::SvnRepoListSubmodulesCommand &command);
  void listSubmodulesWithStatusWorker(
      const Structs::SvnRepoListSubmodulesWithStatusCommand &command);
  void scheduleWorkerForListSubmodulesWithStatus(
      const Structs::SvnRepoListSubmodulesWithStatusCommand &command);
  void listTagsWorker(const Structs::SvnRepoListTagsCommand &command);
  void mergeWorker(const Structs::SvnRepoMergeCommand &command);
  void openRepoWorker(const Structs::SvnRepoOpenCommand &command);
  void pullWorker(const Structs::SvnRepoPullCommand &command);
  void pushWorker(const Structs::SvnRepoPushCommand &command);
  void pushTagWorker(const Structs::SvnRepoPushTagCommand &command);
  void pushAllTagsWorker(const Structs::SvnRepoPushAllTagsCommand &command);
  void renameBranchWorker(const Structs::SvnRepoRenameBranchCommand &command);
  void renameTagWorker(const Structs::SvnRepoRenameTagCommand &command);
  void resetWorker(const Structs::SvnRepoResetCommand &command);
  void revertWorker(const Structs::SvnRepoRevertCommand &command);
  void cherryPickWorker(const Structs::SvnRepoCherryPickCommand &command);
  void
  scheduleWorkerForCherryPick(const Structs::SvnRepoCherryPickCommand &command);
  void setUpstreamWorker(const Structs::SvnRepoSetUpstreamCommand &command);
  void stageFilesWorker(const Structs::SvnRepoStageFilesCommand &command);
  void stashWorker(const Structs::SvnRepoStashCommand &command);
  void statusWorker(const Structs::SvnRepoStatusCommand &command);
  void
  submoduleUpdateWorker(const Structs::SvnRepoSubmoduleUpdateCommand &command);
  void unstageFilesWorker(const Structs::SvnRepoUnstageFilesCommand &command);
  void listRemotesWorker(const Structs::SvnRepoListRemotesCommand &command);
  void addRemoteWorker(const Structs::SvnRepoAddRemoteCommand &command);
  void removeRemoteWorker(const Structs::SvnRepoRemoveRemoteCommand &command);
  void updateRemoteWorker(const Structs::SvnRepoUpdateRemoteCommand &command);
  void setDefaultRemoteWorker(
      const Structs::SvnRepoSetDefaultRemoteCommand &command);
  void addTargetWorker(const Structs::SvnRepoAddTargetCommand &command);
  void editTargetWorker(const Structs::SvnRepoEditTargetCommand &command);
  void deleteTargetWorker(const Structs::SvnRepoDeleteTargetCommand &command);
  void setDefaultTargetWorker(
      const Structs::SvnRepoSetDefaultTargetCommand &command);
  void quitRepoWorker(const Structs::SvnRepoQuitCommand &command);
  void reportError(EventManager::EventType eventType, const QString &message,
                   const Structs::SvnRepoCommand &command) const;
  void
  scheduleWorkerForAbortMerge(const Structs::SvnRepoAbortMergeCommand &command);
  void scheduleWorkerForAddSubmodule(
      const Structs::SvnRepoAddSubmoduleCommand &command);
  void scheduleWorkerForAmend(const Structs::SvnRepoAmendCommand &command);
  void
  scheduleWorkerForApplyPatch(const Structs::SvnRepoApplyPatchCommand &command);
  void
  scheduleWorkerForApplyStash(const Structs::SvnRepoApplyStashCommand &command);
  void scheduleWorkerForCheckoutBranch(
      const Structs::SvnRepoCheckoutBranchCommand &command);
  void scheduleWorkerForCheckoutCommit(
      const Structs::SvnRepoCheckoutCommitCommand &command);
  void scheduleWorkerForCloneRepo(const Structs::SvnRepoCloneCommand &command);
  void scheduleWorkerForCloseRepo(const Structs::SvnRepoCloseCommand &command);
  void scheduleWorkerForCommitStagedRepo(
      const Structs::SvnRepoCommitStagedCommand &command);
  void scheduleWorkerForCreateBranchFromCommit(
      const Structs::SvnRepoCreateBranchFromCommitCommand &command);
  void scheduleWorkerForCreatePatchFromCommit(
      const Structs::SvnRepoCreatePatchFromCommitCommand &command);
  void scheduleWorkerForCreateTagFromCommit(
      const Structs::SvnRepoCreateTagFromCommitCommand &command);
  void scheduleWorkerForGetActiveBranch(
      const Structs::SvnRepoGetActiveBranchCommand &command);
  void
  scheduleWorkerForFetchAll(const Structs::SvnRepoFetchAllCommand &command);
  void scheduleWorkerForFetchRemote(
      const Structs::SvnRepoFetchRemoteCommand &command);
  void scheduleWorkerForRebase(const Structs::SvnRepoRebaseCommand &command);
  void scheduleWorkerForGetCommitBranches(
      const Structs::SvnRepoGetCommitBranchesCommand &command);
  void scheduleWorkerForGetCommitTags(
      const Structs::SvnRepoGetCommitTagsCommand &command);
  void scheduleWorkerForGetCommitHeads(
      const Structs::SvnRepoGetCommitHeadsCommand &command);
  void scheduleWorkerForGetCommitIsHead(
      const Structs::SvnRepoGetCommitIsHeadCommand &command);
  void scheduleWorkerForGetCommitStatus(
      const Structs::SvnRepoGetCommitStatusCommand &command);
  void scheduleWorkerForGetStashStatus(
      const Structs::SvnRepoGetStashStatusCommand &command);
  void scheduleWorkerForGetIsMergeInProgress(
      const Structs::SvnRepoGetIsMergeInProgressCommand &command);
  void scheduleWorkerForGetSubmodulePath(
      const Structs::SvnRepoGetSubmodulePathCommand &command);
  void scheduleWorkerForGetUpstream(
      const Structs::SvnRepoGetUpstreamCommand &command);
  void scheduleWorkerForDeleteAllStashes(
      const Structs::SvnRepoDeleteAllStashesCommand &command);
  void scheduleWorkerForDeleteBranch(
      const Structs::SvnRepoDeleteBranchCommand &command);
  void scheduleWorkerForDeleteRemoteBranch(
      const Structs::SvnRepoDeleteRemoteBranchCommand &command);
  void scheduleWorkerForDeleteStash(
      const Structs::SvnRepoDeleteStashCommand &command);
  void scheduleWorkerForDeleteSubmodule(
      const Structs::SvnRepoDeleteSubmoduleCommand &command);
  void
  scheduleWorkerForDeleteTag(const Structs::SvnRepoDeleteTagCommand &command);
  void scheduleWorkerForDeleteRemoteTag(
      const Structs::SvnRepoDeleteRemoteTagCommand &command);
  void
  scheduleWorkerForDiffFiles(const Structs::SvnRepoDiffFilesCommand &command);
  void scheduleWorkerForDiffFilesFromCommit(
      const Structs::SvnRepoDiffFilesFromCommitCommand &command);
  void scheduleWorkerForDiscardFiles(
      const Structs::SvnRepoDiscardFilesCommand &command);
  void scheduleWorkerForGetFileSizeFromCommit(
      const Structs::SvnRepoGetFileSizeFromCommitCommand &command);
  void scheduleWorkerForGetBranchHistory(
      const Structs::SvnRepoGetBranchHistoryCommand &command);
  void
  scheduleWorkerForInitRepo(const Structs::SvnRepoInitRepoCommand &command);
  void scheduleWorkerForListConflicts(
      const Structs::SvnRepoListConflictsCommand &command);
  void scheduleWorkerForListLocalBranches(
      const Structs::SvnRepoListLocalBranchesCommand &command);
  void scheduleWorkerForListRemoteBranches(
      const Structs::SvnRepoListRemoteBranchesCommand &command);
  void scheduleWorkerForListRemoteBranchesWithStatus(
      const Structs::SvnRepoListRemoteBranchesWithStatusCommand &command);
  void scheduleWorkerForListStashes(
      const Structs::SvnRepoListStashesCommand &command);
  void scheduleWorkerForListSubmodules(
      const Structs::SvnRepoListSubmodulesCommand &command);
  void
  scheduleWorkerForListTags(const Structs::SvnRepoListTagsCommand &command);
  void scheduleWorkerForMerge(const Structs::SvnRepoMergeCommand &command);
  void scheduleWorkerForOpenRepo(const Structs::SvnRepoOpenCommand &command);
  void scheduleWorkerForPull(const Structs::SvnRepoPullCommand &command);
  void scheduleWorkerForPush(const Structs::SvnRepoPushCommand &command);
  void scheduleWorkerForPushTag(const Structs::SvnRepoPushTagCommand &command);
  void scheduleWorkerForPushAllTags(
      const Structs::SvnRepoPushAllTagsCommand &command);
  void scheduleWorkerForRenameBranch(
      const Structs::SvnRepoRenameBranchCommand &command);
  void
  scheduleWorkerForRenameTag(const Structs::SvnRepoRenameTagCommand &command);
  void scheduleWorkerForReset(const Structs::SvnRepoResetCommand &command);
  void scheduleWorkerForRevert(const Structs::SvnRepoRevertCommand &command);
  void scheduleWorkerForSetUpstream(
      const Structs::SvnRepoSetUpstreamCommand &command);
  void
  scheduleWorkerForStageFiles(const Structs::SvnRepoStageFilesCommand &command);
  void scheduleWorkerForStash(const Structs::SvnRepoStashCommand &command);
  void scheduleWorkerForStatus(const Structs::SvnRepoStatusCommand &command);
  void scheduleWorkerForSubmoduleUpdate(
      const Structs::SvnRepoSubmoduleUpdateCommand &command);
  void scheduleWorkerForUnstageFiles(
      const Structs::SvnRepoUnstageFilesCommand &command);
  void scheduleWorkerForListRemotes(
      const Structs::SvnRepoListRemotesCommand &command);
  void
  scheduleWorkerForAddRemote(const Structs::SvnRepoAddRemoteCommand &command);
  void scheduleWorkerForRemoveRemote(
      const Structs::SvnRepoRemoveRemoteCommand &command);
  void scheduleWorkerForUpdateRemote(
      const Structs::SvnRepoUpdateRemoteCommand &command);
  void scheduleWorkerForSetDefaultRemote(
      const Structs::SvnRepoSetDefaultRemoteCommand &command);
  void
  scheduleWorkerForAddTarget(const Structs::SvnRepoAddTargetCommand &command);
  void
  scheduleWorkerForEditTarget(const Structs::SvnRepoEditTargetCommand &command);
  void scheduleWorkerForDeleteTarget(
      const Structs::SvnRepoDeleteTargetCommand &command);
  void scheduleWorkerForSetDefaultTarget(
      const Structs::SvnRepoSetDefaultTargetCommand &command);
  void scheduleWorkerForQuit(const Structs::SvnRepoQuitCommand &command);
  void workersThreadFunc(const QString &uuid);

private:
  std::atomic<bool> _quitAll;
  Util::Mutex _contextsMutex;
  QMap<QString, Structs::SvnRepoContext *> _contexts;
  EventManager::EventManager &_eventManager;
};

} // namespace SvnRepoManager
