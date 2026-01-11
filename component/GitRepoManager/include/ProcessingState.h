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
  void addSubmoduleWorker(const Structs::GitRepoAddSubmoduleCommand &command);
  void applyPatchWorker(const Structs::GitRepoApplyPatchCommand &command);
  void applyStashWorker(const Structs::GitRepoApplyStashCommand &command);
  void
  checkoutBranchWorker(const Structs::GitRepoCheckoutBranchCommand &command);
  void
  checkoutCommitWorker(const Structs::GitRepoCheckoutCommitCommand &command);
  void cloneRepoWorker(const Structs::GitRepoCloneCommand &command);
  void closeRepoWorker(const Structs::GitRepoCloseCommand &command);
  void commitStagedWorker(const Structs::GitRepoCommitStagedCommand &command);
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
  void deleteStashWorker(const Structs::GitRepoDeleteStashCommand &command);
  void
  deleteSubmoduleWorker(const Structs::GitRepoDeleteSubmoduleCommand &command);
  void deleteTagWorker(const Structs::GitRepoDeleteTagCommand &command);
  void diffFilesWorker(const Structs::GitRepoDiffFilesCommand &command);
  void diffFilesFromCommitWorker(
      const Structs::GitRepoDiffFilesFromCommitCommand &command);
  void discardFilesWorker(const Structs::GitRepoDiscardFilesCommand &command);
  void eventCallback(const EventManager::Event event);
  void
  getActiveBranchWorker(const Structs::GitRepoGetActiveBranchCommand &command);
  void getCommitBranchesWorker(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void
  getCommitHeadsWorker(const Structs::GitRepoGetCommitHeadsCommand &command);
  void
  getCommitIsHeadWorker(const Structs::GitRepoGetCommitIsHeadCommand &command);
  void
  getCommitStatusWorker(const Structs::GitRepoGetCommitStatusCommand &command);
  void getBranchHistoryWorker(
      const Structs::GitRepoGetBranchHistoryCommand &command);
  void getSubmodulePathWorker(
      const Structs::GitRepoGetSubmodulePathCommand &command);
  void getUpstreamWorker(const Structs::GitRepoGetUpstreamCommand &command);
  void initRepoWorker(const Structs::GitRepoInitRepoCommand &command);
  void listLocalBranchesWorker(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void listRemoteBranchesWorker(
      const Structs::GitRepoListRemoteBranchesCommand &command);
  void listStashesWorker(const Structs::GitRepoListStashesCommand &command);
  void
  listSubmodulesWorker(const Structs::GitRepoListSubmodulesCommand &command);
  void listTagsWorker(const Structs::GitRepoListTagsCommand &command);
  void mergeWorker(const Structs::GitRepoMergeCommand &command);
  void openRepoWorker(const Structs::GitRepoOpenCommand &command);
  void pullWorker(const Structs::GitRepoPullCommand &command);
  void pushWorker(const Structs::GitRepoPushCommand &command);
  void renameBranchWorker(const Structs::GitRepoRenameBranchCommand &command);
  void renameTagWorker(const Structs::GitRepoRenameTagCommand &command);
  void resetWorker(const Structs::GitRepoResetCommand &command);
  void revertWorker(const Structs::GitRepoRevertCommand &command);
  void setUpstreamWorker(const Structs::GitRepoSetUpstreamCommand &command);
  void stageFilesWorker(const Structs::GitRepoStageFilesCommand &command);
  void stashWorker(const Structs::GitRepoStashCommand &command);
  void statusWorker(const Structs::GitRepoStatusCommand &command);
  void
  submoduleUpdateWorker(const Structs::GitRepoSubmoduleUpdateCommand &command);
  void unstageFilesWorker(const Structs::GitRepoUnstageFilesCommand &command);
  void quitRepoWorker(const Structs::GitRepoQuitCommand &command);
  void reportError(EventManager::EventType eventType, const QString &message,
                   const Structs::GitRepoCommand &command) const;
  void scheduleWorkerForAddSubmodule(
      const Structs::GitRepoAddSubmoduleCommand &command);
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
  void scheduleWorkerForCreateBranchFromCommit(
      const Structs::GitRepoCreateBranchFromCommitCommand &command);
  void scheduleWorkerForCreatePatchFromCommit(
      const Structs::GitRepoCreatePatchFromCommitCommand &command);
  void scheduleWorkerForCreateTagFromCommit(
      const Structs::GitRepoCreateTagFromCommitCommand &command);
  void scheduleWorkerForGetActiveBranch(
      const Structs::GitRepoGetActiveBranchCommand &command);
  void scheduleWorkerForGetCommitBranches(
      const Structs::GitRepoGetCommitBranchesCommand &command);
  void scheduleWorkerForGetCommitHeads(
      const Structs::GitRepoGetCommitHeadsCommand &command);
  void scheduleWorkerForGetCommitIsHead(
      const Structs::GitRepoGetCommitIsHeadCommand &command);
  void scheduleWorkerForGetCommitStatus(
      const Structs::GitRepoGetCommitStatusCommand &command);
  void scheduleWorkerForGetSubmodulePath(
      const Structs::GitRepoGetSubmodulePathCommand &command);
  void scheduleWorkerForGetUpstream(
      const Structs::GitRepoGetUpstreamCommand &command);
  void scheduleWorkerForDeleteBranch(
      const Structs::GitRepoDeleteBranchCommand &command);
  void scheduleWorkerForDeleteStash(
      const Structs::GitRepoDeleteStashCommand &command);
  void scheduleWorkerForDeleteSubmodule(
      const Structs::GitRepoDeleteSubmoduleCommand &command);
  void
  scheduleWorkerForDeleteTag(const Structs::GitRepoDeleteTagCommand &command);
  void
  scheduleWorkerForDiffFiles(const Structs::GitRepoDiffFilesCommand &command);
  void scheduleWorkerForDiffFilesFromCommit(
      const Structs::GitRepoDiffFilesFromCommitCommand &command);
  void scheduleWorkerForDiscardFiles(
      const Structs::GitRepoDiscardFilesCommand &command);
  void scheduleWorkerForGetBranchHistory(
      const Structs::GitRepoGetBranchHistoryCommand &command);
  void
  scheduleWorkerForInitRepo(const Structs::GitRepoInitRepoCommand &command);
  void scheduleWorkerForListLocalBranches(
      const Structs::GitRepoListLocalBranchesCommand &command);
  void scheduleWorkerForListRemoteBranches(
      const Structs::GitRepoListRemoteBranchesCommand &command);
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
  void scheduleWorkerForQuit(const Structs::GitRepoQuitCommand &command);
  void workersThreadFunc(const QString &uuid);

private:
  std::atomic<bool> _quitAll;
  Util::Mutex _contextsMutex;
  QMap<QString, Structs::GitRepoContext *> _contexts;
  EventManager::EventManager &_eventManager;
};

} // namespace GitRepoManager
