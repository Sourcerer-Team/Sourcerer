#include "GitRepoManagerProcessingState.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerStructs.h"

#include <GitLfs.h>
#include <GitRepo.h>
#include <Log.h>

#include <git2/errors.h>

#include <QVariantMap>
#include <functional>

#define THREAD_ID "GitRepoManagerProcessingState"

using namespace GitRepoManager::Structs;

namespace GitRepoManager {

GitRepoManagerProcessingState::GitRepoManagerProcessingState(
    GitRepoManagerFsm &fsm)
    : GitRepoManagerState{StateType::Processing, fsm}, _quitAll{false},
      _eventManager{EventManager::EventManager::getInstance()} {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&GitRepoManagerProcessingState::eventCallback, this,
                std::placeholders::_1);

  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerCloneRepoError,
                       eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerCloneRepoResult,
                       eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerInitRepoResult,
                       eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerCloseRepoResult,
                       eventCallbackFunc));

  LOG_PERF(0, "Exit");
}

GitRepoManagerProcessingState::~GitRepoManagerProcessingState() {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&GitRepoManagerProcessingState::eventCallback, this,
                std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerInitRepoResult,
                       eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerCloneRepoError,
                       eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerCloneRepoResult,
                       eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerCloseRepoResult,
                       eventCallbackFunc));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::enter() {
  LOG_PERF(0, "Enter");

  _quitAll = false;

  LOG_PERF(0, "Exit");
}

bool GitRepoManagerProcessingState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  switch (const auto type = static_cast<EventType>(event.type())) {
  case EventType::Setup: {
    break;
  }
  case EventType::AbortMerge: {
    const auto abortMergeCommand = event.nextParam<GitRepoAbortMergeCommand>();
    scheduleWorkerForAbortMerge(abortMergeCommand);
    break;
  }
  case EventType::AbortRebase: {
    const auto abortRebaseCommand =
        event.nextParam<GitRepoAbortRebaseCommand>();
    scheduleWorkerForAbortRebase(abortRebaseCommand);
    break;
  }
  case EventType::AddSubmodule: {
    const auto addSubmoduleCommand =
        event.nextParam<GitRepoAddSubmoduleCommand>();
    scheduleWorkerForAddSubmodule(addSubmoduleCommand);
    break;
  }
  case EventType::AddRemote: {
    const auto addRemoteCommand = event.nextParam<GitRepoAddRemoteCommand>();
    scheduleWorkerForAddRemote(addRemoteCommand);
    break;
  }
  case EventType::Amend: {
    const auto amendCommand = event.nextParam<GitRepoAmendCommand>();
    scheduleWorkerForAmend(amendCommand);
    break;
  }
  case EventType::Blame: {
    const auto blameCommand = event.nextParam<GitRepoBlameCommand>();
    scheduleWorkerForBlame(blameCommand);
    break;
  }
  case EventType::ApplyPatch: {
    const auto applyPatchCommand = event.nextParam<GitRepoApplyPatchCommand>();
    scheduleWorkerForApplyPatch(applyPatchCommand);
    break;
  }
  case EventType::ApplyStash: {
    const auto applyStashCommand = event.nextParam<GitRepoApplyStashCommand>();
    scheduleWorkerForApplyStash(applyStashCommand);
    break;
  }
  case EventType::CheckoutBranch: {
    const auto checkoutCommand =
        event.nextParam<GitRepoCheckoutBranchCommand>();
    scheduleWorkerForCheckoutBranch(checkoutCommand);
    break;
  }
  case EventType::CheckoutCommit: {
    const auto checkoutCommand =
        event.nextParam<GitRepoCheckoutCommitCommand>();
    scheduleWorkerForCheckoutCommit(checkoutCommand);
    break;
  }
  case EventType::CloneRepo: {
    const auto cloneCommand = event.nextParam<GitRepoCloneCommand>();
    scheduleWorkerForCloneRepo(cloneCommand);
    break;
  }
  case EventType::CloseRepo: {
    const auto closeCommand = event.nextParam<GitRepoCloseCommand>();
    scheduleWorkerForCloseRepo(closeCommand);
    break;
  }
  case EventType::CommitStaged: {
    const auto commitStagedCommand =
        event.nextParam<GitRepoCommitStagedCommand>();
    scheduleWorkerForCommitStagedRepo(commitStagedCommand);
    break;
  }
  case EventType::CommitRebase: {
    const auto commitRebaseCommand =
        event.nextParam<GitRepoCommitRebaseCommand>();
    scheduleWorkerForCommitRebaseRepo(commitRebaseCommand);
    break;
  }
  case EventType::CreateBranchFromCommit: {
    const auto createBranchFromCommitCommand =
        event.nextParam<GitRepoCreateBranchFromCommitCommand>();
    scheduleWorkerForCreateBranchFromCommit(createBranchFromCommitCommand);
    break;
  }
  case EventType::CreatePatchFromCommit: {
    const auto createPatchFromCommitCommand =
        event.nextParam<GitRepoCreatePatchFromCommitCommand>();
    scheduleWorkerForCreatePatchFromCommit(createPatchFromCommitCommand);
    break;
  }
  case EventType::CreateTagFromCommit: {
    const auto createTagFromCommitCommand =
        event.nextParam<GitRepoCreateTagFromCommitCommand>();
    scheduleWorkerForCreateTagFromCommit(createTagFromCommitCommand);
    break;
  }
  case EventType::DeleteAllStashes: {
    const auto deleteAllStashesCommand =
        event.nextParam<GitRepoDeleteAllStashesCommand>();
    scheduleWorkerForDeleteAllStashes(deleteAllStashesCommand);
    break;
  }
  case EventType::DeleteBranch: {
    const auto deleteBranchCommand =
        event.nextParam<GitRepoDeleteBranchCommand>();
    scheduleWorkerForDeleteBranch(deleteBranchCommand);
    break;
  }
  case EventType::DeleteRemoteBranch: {
    const auto deleteRemoteBranchCommand =
        event.nextParam<GitRepoDeleteRemoteBranchCommand>();
    scheduleWorkerForDeleteRemoteBranch(deleteRemoteBranchCommand);
    break;
  }
  case EventType::DeleteStash: {
    const auto deleteStashCommand =
        event.nextParam<GitRepoDeleteStashCommand>();
    scheduleWorkerForDeleteStash(deleteStashCommand);
    break;
  }
  case EventType::DeleteSubmodule: {
    const auto deleteSubmoduleCommand =
        event.nextParam<GitRepoDeleteSubmoduleCommand>();
    scheduleWorkerForDeleteSubmodule(deleteSubmoduleCommand);
    break;
  }
  case EventType::DeleteTag: {
    const auto deleteTagCommand = event.nextParam<GitRepoDeleteTagCommand>();
    scheduleWorkerForDeleteTag(deleteTagCommand);
    break;
  }
  case EventType::DeleteRemoteTag: {
    const auto deleteRemoteTagCommand =
        event.nextParam<GitRepoDeleteRemoteTagCommand>();
    scheduleWorkerForDeleteRemoteTag(deleteRemoteTagCommand);
    break;
  }
  case EventType::DiffFiles: {
    const auto diffFilesCommand = event.nextParam<GitRepoDiffFilesCommand>();
    scheduleWorkerForDiffFiles(diffFilesCommand);
    break;
  }
  case EventType::DiffFilesFromCommit: {
    const auto diffFilesFromCommitCommand =
        event.nextParam<GitRepoDiffFilesFromCommitCommand>();
    scheduleWorkerForDiffFilesFromCommit(diffFilesFromCommitCommand);
    break;
  }
  case EventType::DiscardFiles: {
    const auto discardFilesCommand =
        event.nextParam<GitRepoDiscardFilesCommand>();
    scheduleWorkerForDiscardFiles(discardFilesCommand);
    break;
  }
  case EventType::FetchAll: {
    const auto fetchAllCommand = event.nextParam<GitRepoFetchAllCommand>();
    scheduleWorkerForFetchAll(fetchAllCommand);
    break;
  }
  case EventType::FetchRemote: {
    const auto fetchRemoteCommand =
        event.nextParam<GitRepoFetchRemoteCommand>();
    scheduleWorkerForFetchRemote(fetchRemoteCommand);
    break;
  }
  case EventType::GetFileSizeFromCommit: {
    const auto getFileSizeFromCommitCommand =
        event.nextParam<GitRepoGetFileSizeFromCommitCommand>();
    scheduleWorkerForGetFileSizeFromCommit(getFileSizeFromCommitCommand);
    break;
  }
  case EventType::GetActiveBranch: {
    const auto getActiveBranchCommand =
        event.nextParam<GitRepoGetActiveBranchCommand>();
    scheduleWorkerForGetActiveBranch(getActiveBranchCommand);
    break;
  }
  case EventType::GetBranchHistory: {
    const auto branchHistoryCommand =
        event.nextParam<GitRepoGetBranchHistoryCommand>();
    scheduleWorkerForGetBranchHistory(branchHistoryCommand);
    break;
  }
  case EventType::GetCommitBranches: {
    const auto getCommitBranchesCommand =
        event.nextParam<GitRepoGetCommitBranchesCommand>();
    scheduleWorkerForGetCommitBranches(getCommitBranchesCommand);
    break;
  }
  case EventType::GetCommitTags: {
    const auto getCommitTagsCommand =
        event.nextParam<GitRepoGetCommitTagsCommand>();
    scheduleWorkerForGetCommitTags(getCommitTagsCommand);
    break;
  }
  case EventType::GetCommitHeads: {
    const auto getCommitHeadsCommand =
        event.nextParam<GitRepoGetCommitHeadsCommand>();
    scheduleWorkerForGetCommitHeads(getCommitHeadsCommand);
    break;
  }
  case EventType::GetCommitIsHead: {
    const auto getCommitIsHeadCommand =
        event.nextParam<GitRepoGetCommitIsHeadCommand>();
    scheduleWorkerForGetCommitIsHead(getCommitIsHeadCommand);
    break;
  }
  case EventType::GetCommitStatus: {
    const auto commitStatusCommand =
        event.nextParam<GitRepoGetCommitStatusCommand>();
    scheduleWorkerForGetCommitStatus(commitStatusCommand);
    break;
  }
  case EventType::GetLfsStorageStatus: {
    const auto lfsStorageStatusCommand =
        event.nextParam<GitRepoGetLfsStorageStatusCommand>();
    scheduleWorkerForGetLfsStorageStatus(lfsStorageStatusCommand);
    break;
  }
  case EventType::GetStashStatus: {
    const auto stashStatusCommand =
        event.nextParam<GitRepoGetStashStatusCommand>();
    scheduleWorkerForGetStashStatus(stashStatusCommand);
    break;
  }
  case EventType::GetIsMergeInProgress: {
    const auto getIsMergeInProgressCommand =
        event.nextParam<GitRepoGetIsMergeInProgressCommand>();
    scheduleWorkerForGetIsMergeInProgress(getIsMergeInProgressCommand);
    break;
  }
  case EventType::GetIsRebaseInProgress: {
    const auto getIsRebaseInProgressCommand =
        event.nextParam<GitRepoGetIsRebaseInProgressCommand>();
    scheduleWorkerForGetIsRebaseInProgress(getIsRebaseInProgressCommand);
    break;
  }
  case EventType::GetSubmodulePath: {
    const auto getSubmodulePathCommand =
        event.nextParam<GitRepoGetSubmodulePathCommand>();
    scheduleWorkerForGetSubmodulePath(getSubmodulePathCommand);
    break;
  }
  case EventType::GetUpstream: {
    const auto getUpstreamCommand =
        event.nextParam<GitRepoGetUpstreamCommand>();
    scheduleWorkerForGetUpstream(getUpstreamCommand);
    break;
  }
  case EventType::Init: {
    const auto initCommand = event.nextParam<GitRepoInitRepoCommand>();
    scheduleWorkerForInitRepo(initCommand);
    break;
  }
  case EventType::ListConflicts: {
    const auto listConflictsCommand =
        event.nextParam<GitRepoListConflictsCommand>();
    scheduleWorkerForListConflicts(listConflictsCommand);
    break;
  }
  case EventType::ListLocalBranches: {
    const auto listLocalCommand =
        event.nextParam<GitRepoListLocalBranchesCommand>();
    scheduleWorkerForListLocalBranches(listLocalCommand);
    break;
  }
  case EventType::ListRemoteBranches: {
    const auto listRemoteCommand =
        event.nextParam<GitRepoListRemoteBranchesCommand>();
    scheduleWorkerForListRemoteBranches(listRemoteCommand);
    break;
  }
  case EventType::ListRemoteBranchesWithStatus: {
    const auto listRemoteStatusCommand =
        event.nextParam<GitRepoListRemoteBranchesWithStatusCommand>();
    scheduleWorkerForListRemoteBranchesWithStatus(listRemoteStatusCommand);
    break;
  }
  case EventType::ListRemotes: {
    const auto listRemotesCommand =
        event.nextParam<GitRepoListRemotesCommand>();
    scheduleWorkerForListRemotes(listRemotesCommand);
    break;
  }
  case EventType::ListStashes: {
    const auto listStashesCommand =
        event.nextParam<GitRepoListStashesCommand>();
    scheduleWorkerForListStashes(listStashesCommand);
    break;
  }
  case EventType::ListSubmodules: {
    const auto listSubmodulesCommand =
        event.nextParam<GitRepoListSubmodulesCommand>();
    scheduleWorkerForListSubmodules(listSubmodulesCommand);
    break;
  }
  case EventType::ListSubmodulesWithStatus: {
    const auto listSubmodulesWithStatusCommand =
        event.nextParam<GitRepoListSubmodulesWithStatusCommand>();
    scheduleWorkerForListSubmodulesWithStatus(listSubmodulesWithStatusCommand);
    break;
  }
  case EventType::ListTags: {
    const auto listTagsCommand = event.nextParam<GitRepoListTagsCommand>();
    scheduleWorkerForListTags(listTagsCommand);
    break;
  }
  case EventType::ListWorktrees: {
    const auto listWorktreesCommand =
        event.nextParam<GitRepoListWorktreesCommand>();
    scheduleWorkerForListWorktrees(listWorktreesCommand);
    break;
  }
  case EventType::AddWorktree: {
    const auto addWorktreeCommand =
        event.nextParam<GitRepoAddWorktreeCommand>();
    scheduleWorkerForAddWorktree(addWorktreeCommand);
    break;
  }
  case EventType::RemoveWorktree: {
    const auto removeWorktreeCommand =
        event.nextParam<GitRepoRemoveWorktreeCommand>();
    scheduleWorkerForRemoveWorktree(removeWorktreeCommand);
    break;
  }
  case EventType::LockWorktree: {
    const auto lockWorktreeCommand =
        event.nextParam<GitRepoLockWorktreeCommand>();
    scheduleWorkerForLockWorktree(lockWorktreeCommand);
    break;
  }
  case EventType::UnlockWorktree: {
    const auto unlockWorktreeCommand =
        event.nextParam<GitRepoUnlockWorktreeCommand>();
    scheduleWorkerForUnlockWorktree(unlockWorktreeCommand);
    break;
  }
  case EventType::Merge: {
    const auto mergeCommand = event.nextParam<GitRepoMergeCommand>();
    scheduleWorkerForMerge(mergeCommand);
    break;
  }
  case EventType::Rebase: {
    const auto rebaseCommand = event.nextParam<GitRepoRebaseCommand>();
    scheduleWorkerForRebase(rebaseCommand);
    break;
  }
  case EventType::Squash: {
    const auto squashCommand = event.nextParam<GitRepoSquashCommand>();
    scheduleWorkerForSquash(squashCommand);
    break;
  }
  case EventType::Bisect: {
    const auto bisectCommand = event.nextParam<GitRepoBisectCommand>();
    scheduleWorkerForBisect(bisectCommand);
    break;
  }
  case EventType::GitFlow: {
    const auto gitFlowCommand = event.nextParam<GitRepoGitFlowCommand>();
    scheduleWorkerForGitFlow(gitFlowCommand);
    break;
  }
  case EventType::OpenRepo: {
    const auto openCommand = event.nextParam<GitRepoOpenCommand>();
    scheduleWorkerForOpenRepo(openCommand);
    break;
  }
  case EventType::Pull: {
    const auto pullCommand = event.nextParam<GitRepoPullCommand>();
    scheduleWorkerForPull(pullCommand);
    break;
  }
  case EventType::Push: {
    const auto pushCommand = event.nextParam<GitRepoPushCommand>();
    scheduleWorkerForPush(pushCommand);
    break;
  }
  case EventType::PushTag: {
    const auto pushTagCommand = event.nextParam<GitRepoPushTagCommand>();
    scheduleWorkerForPushTag(pushTagCommand);
    break;
  }
  case EventType::PushAllTags: {
    const auto pushAllTagsCommand =
        event.nextParam<GitRepoPushAllTagsCommand>();
    scheduleWorkerForPushAllTags(pushAllTagsCommand);
    break;
  }
  case EventType::RemoveRemote: {
    const auto removeRemoteCommand =
        event.nextParam<GitRepoRemoveRemoteCommand>();
    scheduleWorkerForRemoveRemote(removeRemoteCommand);
    break;
  }
  case EventType::RenameBranch: {
    const auto renameBranchCommand =
        event.nextParam<GitRepoRenameBranchCommand>();
    scheduleWorkerForRenameBranch(renameBranchCommand);
    break;
  }
  case EventType::RenameTag: {
    const auto renameTagCommand = event.nextParam<GitRepoRenameTagCommand>();
    scheduleWorkerForRenameTag(renameTagCommand);
    break;
  }
  case EventType::Reset: {
    const auto resetCommand = event.nextParam<GitRepoResetCommand>();
    scheduleWorkerForReset(resetCommand);
    break;
  }
  case EventType::Revert: {
    const auto revertCommand = event.nextParam<GitRepoRevertCommand>();
    scheduleWorkerForRevert(revertCommand);
    break;
  }
  case EventType::CherryPick: {
    const auto cherryPickCommand = event.nextParam<GitRepoCherryPickCommand>();
    scheduleWorkerForCherryPick(cherryPickCommand);
    break;
  }
  case EventType::SetDefaultRemote: {
    const auto setDefaultRemoteCommand =
        event.nextParam<GitRepoSetDefaultRemoteCommand>();
    scheduleWorkerForSetDefaultRemote(setDefaultRemoteCommand);
    break;
  }
  case EventType::Workflow: {
    const auto workflowCommand = event.nextParam<GitRepoWorkflowCommand>();
    scheduleWorkerForWorkflow(workflowCommand);
    break;
  }
  case EventType::AddTarget: {
    const auto addTargetCommand = event.nextParam<GitRepoAddTargetCommand>();
    scheduleWorkerForAddTarget(addTargetCommand);
    break;
  }
  case EventType::EditTarget: {
    const auto editTargetCommand = event.nextParam<GitRepoEditTargetCommand>();
    scheduleWorkerForEditTarget(editTargetCommand);
    break;
  }
  case EventType::DeleteTarget: {
    const auto deleteTargetCommand =
        event.nextParam<GitRepoDeleteTargetCommand>();
    scheduleWorkerForDeleteTarget(deleteTargetCommand);
    break;
  }
  case EventType::SetDefaultTarget: {
    const auto setDefaultTargetCommand =
        event.nextParam<GitRepoSetDefaultTargetCommand>();
    scheduleWorkerForSetDefaultTarget(setDefaultTargetCommand);
    break;
  }
  case EventType::SetUpstream: {
    const auto setUpstreamCommand =
        event.nextParam<GitRepoSetUpstreamCommand>();
    scheduleWorkerForSetUpstream(setUpstreamCommand);
    break;
  }
  case EventType::StageFiles: {
    const auto stageFilesCommand = event.nextParam<GitRepoStageFilesCommand>();
    scheduleWorkerForStageFiles(stageFilesCommand);
    break;
  }
  case EventType::Stash: {
    const auto stashCommand = event.nextParam<GitRepoStashCommand>();
    scheduleWorkerForStash(stashCommand);
    break;
  }
  case EventType::Status: {
    const auto statusCommand = event.nextParam<GitRepoStatusCommand>();
    scheduleWorkerForStatus(statusCommand);
    break;
  }
  case EventType::SubmoduleUpdate: {
    const auto submoduleUpdateCommand =
        event.nextParam<GitRepoSubmoduleUpdateCommand>();
    scheduleWorkerForSubmoduleUpdate(submoduleUpdateCommand);
    break;
  }
  case EventType::UnstageFiles: {
    const auto unstageFilesCommand =
        event.nextParam<GitRepoUnstageFilesCommand>();
    scheduleWorkerForUnstageFiles(unstageFilesCommand);
    break;
  }
  case EventType::UpdateRemote: {
    const auto updateRemoteCommand =
        event.nextParam<GitRepoUpdateRemoteCommand>();
    scheduleWorkerForUpdateRemote(updateRemoteCommand);
    break;
  }
  default: {
    LOG_WARN("Unhandled event {}", static_cast<int>(type));
    return false;
  }
  }

  return true;
}

void GitRepoManagerProcessingState::exit() {
  LOG_PERF(0, "Enter");

  destroyContextsMap();

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::abortMergeWorker(
    const Structs::GitRepoAbortMergeCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAbortMergeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::abortMerge(repo);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAbortMergeError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAbortMergeResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::abortRebaseWorker(
    const Structs::GitRepoAbortRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAbortRebaseError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::abortRebase(repo);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAbortRebaseError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAbortRebaseResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::addSubmoduleWorker(
    const Structs::GitRepoAddSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAddSubmoduleError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::addSubmodule(repo, command.url, command.name,
                                             command.branch, command.uuid);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAddSubmoduleError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAddSubmoduleResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::amendWorker(
    const Structs::GitRepoAmendCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAmendError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::amend(repo, command.summary, command.message, command.author,
                     command.email, command.signoff, command.signCommit);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAmendError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAmendResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, true});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::blameWorker(
    const Structs::GitRepoBlameCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerBlameError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (!repo) {
    reportError(EventManager::EventType::GitRepoManagerBlameError,
                tr("Git repository is not available."), command);
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    return;
  }

  const GitRepo::BlameReport blameReport = GitRepo::blame(repo, command.files);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerBlameResult, command.syncUuid);
  resultEvent.addData(GitRepoResult{
      command.uuid, command.type,
      QVariant::fromValue(GitRepoBlameCommandResult{blameReport})});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::applyPatchWorker(
    const Structs::GitRepoApplyPatchCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerApplyPatchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::applyPatch(repo, command.patchContent);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerApplyPatchError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerApplyPatchResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::applyStashWorker(
    const Structs::GitRepoApplyStashCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerApplyStashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::applyStash(repo, command.index); !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerApplyStashError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    // Check if there are conflicts after stash apply
    const auto status = GitRepo::getStatus(repo, false);
    const bool hasConflicts = !status.conflictedFiles.isEmpty();
    QVariantMap conflictedFiles;

    for (const auto &filePath : status.conflictedFiles) {
      const QString fileContentWithMarkers =
          GitRepo::listConflictsForStashFile(repo, command.index, filePath);
      if (!fileContentWithMarkers.isEmpty()) {
        conflictedFiles.insert(filePath, fileContentWithMarkers);
      }
    }

    QMap<QString, QVariant> resultData;
    resultData["success"] = true;
    resultData["hasConflicts"] = hasConflicts;
    resultData["conflictedFiles"] = conflictedFiles;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerApplyStashResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(resultData)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::checkoutBranchWorker(
    const Structs::GitRepoCheckoutBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::checkoutBranch(repo, command.branchName, command.branchType);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCheckoutBranchError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    LOG_DEBUG("Sending checkout error event for branch: '{}'",
              command.branchName.toStdString());
    _eventManager.send(errorEvent);
  } else {
    GitRepoCheckoutBranchCommandResult result;
    result.branchName = command.branchName;
    result.branchType = command.branchType;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCheckoutBranchResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    LOG_DEBUG("Sending checkout success event for branch: '{}'",
              command.branchName.toStdString());
    _eventManager.send(resultEvent);
    LOG_DEBUG("Checkout success event sent for branch: '{}'",
              command.branchName.toStdString());
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::checkoutCommitWorker(
    const Structs::GitRepoCheckoutCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutCommitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::checkoutCommit(repo, command.commit);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCheckoutCommitError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCheckoutCommitResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::cloneRepoWorker(
    const GitRepoCloneCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  const bool success = GitRepo::cloneRepository(
      command.url, command.destinationDir, command.uuid);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCloneRepoError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCloneRepoResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, command.destinationDir});
    _eventManager.send(resultEvent);
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = nullptr;
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::closeRepoWorker(
    const Structs::GitRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCloseRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (repo) {
    GitRepo::closeRepository(repo);

    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = nullptr;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCloseRepoResult, command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, QVariant()});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::commitStagedWorker(
    const Structs::GitRepoCommitStagedCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCommitStagedError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::commitStaged(
      repo, command.summary, command.message, command.author, command.email,
      command.cleanup, command.signoff, command.signCommit);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCommitStagedError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCommitStagedResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::commitRebaseWorker(
    const Structs::GitRepoCommitRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCommitRebaseError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::commitRebase(
      repo, command.summary, command.message, command.author, command.email,
      command.cleanup, command.signoff, command.signCommit);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCommitRebaseError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCommitRebaseResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::createBranchFromCommitWorker(
    const Structs::GitRepoCreateBranchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerCreateBranchFromCommitError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::createBranchFromCommit(repo, command.commit, command.branchName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCreateBranchFromCommitError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCreateBranchFromCommitResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::createPatchFromCommitWorker(
    const Structs::GitRepoCreatePatchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerCreatePatchFromCommitError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const QString patchContent =
      GitRepo::createPatchFromCommit(repo, command.commit);
  if (patchContent.isEmpty()) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCreatePatchFromCommitError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    GitRepoCreatePatchFromCommitCommandResult result;
    result.patchContent = patchContent;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCreatePatchFromCommitResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::createTagFromCommitWorker(
    const Structs::GitRepoCreateTagFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::createTagFromCommit(
      repo, command.commit, command.tagName, command.author, command.email,
      command.message);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCreateTagFromCommitResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::addTargetWorker(
    const Structs::GitRepoAddTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAddTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::addTarget(repo, command.remoteName, command.targetName,
                         command.fetchUrl, command.pushUrl);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAddTargetError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAddTargetResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::editTargetWorker(
    const Structs::GitRepoEditTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerEditTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::editTarget(repo, command.remoteName, command.targetName,
                          command.newFetchUrl, command.newPushUrl);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerEditTargetError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerEditTargetResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteTargetWorker(
    const Structs::GitRepoDeleteTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::deleteTarget(repo, command.remoteName, command.targetName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteTargetError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteTargetResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::setDefaultTargetWorker(
    const Structs::GitRepoSetDefaultTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerSetDefaultTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::setDefaultTarget(repo, command.remoteName, command.targetName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerSetDefaultTargetError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerSetDefaultTargetResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::quitRepoWorker(
    const Structs::GitRepoQuitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerQuitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerQuitError, command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerQuitResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, QVariant()});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

Structs::GitRepoContext *
GitRepoManagerProcessingState::context(const QString &uuid) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  LOG_PERF(0, "Exit");

  return _contexts.value(uuid, nullptr);
}

void GitRepoManagerProcessingState::destroyContext(const QString &uuid) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(uuid);
  if (!ctx) {
    LOG_ERROR(Log::ErrorId::Git, "Internal error. Context is not initialized.");
    LOG_PERF(0, "Exit");
    return;
  }

  GitRepoQuitCommand quitCommand;
  quitCommand.uuid = uuid;
  scheduleWorkerForQuit(quitCommand);
  ctx->workersThread->wait();

  LOCKGUARD(_contextsMutex);

  while (!ctx->commandsQueue.empty()) {
    const GitRepoCommand *commandToDelete = ctx->commandsQueue.pop();
    delete commandToDelete;
  }

  delete ctx;
  _contexts.remove(uuid);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::destroyContextsMap() {
  LOG_PERF(0, "Enter");

  QStringList uuids;

  {
    LOCKGUARD(_contextsMutex);
    uuids = _contexts.keys();
  }

  _quitAll = true;

  for (const auto &uuid : uuids)
    destroyContext(uuid);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteBranchWorker(
    const Structs::GitRepoDeleteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success =
          GitRepo::deleteBranch(repo, command.branchName, command.force);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteBranchError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteBranchResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteRemoteBranchWorker(
    const Structs::GitRepoDeleteRemoteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteRemoteBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success =
          GitRepo::deleteRemoteBranch(repo, command.branchName);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteRemoteBranchError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteRemoteBranchResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteStashWorker(
    const Structs::GitRepoDeleteStashCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteStashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::deleteStash(repo, command.index);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteStashError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteStashResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteAllStashesWorker(
    const Structs::GitRepoDeleteAllStashesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteAllStashesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::deleteAllStashes(repo); !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteAllStashesError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteAllStashesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteSubmoduleWorker(
    const Structs::GitRepoDeleteSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteSubmoduleError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::deleteSubmodule(repo, command.submoduleName,
                                                command.submodulePath);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteSubmoduleError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteSubmoduleResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteTagWorker(
    const Structs::GitRepoDeleteTagCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::deleteTag(repo, command.tagName);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteTagError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteTagResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::deleteRemoteTagWorker(
    const Structs::GitRepoDeleteRemoteTagCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDeleteRemoteTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::deleteRemoteTag(repo, command.tagName);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDeleteRemoteTagError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteRemoteTagResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::diffFilesWorker(
    const Structs::GitRepoDiffFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDiffFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const GitRepo::FilesDiff filesDiff = GitRepo::diffFiles(repo, command.files);

  if (!filesDiff.valid) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDiffFilesError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    GitRepoDiffFilesCommandResult commandResult;
    commandResult.diff = filesDiff.diff;
    commandResult.filesContent = filesDiff.filesContent;
    commandResult.repoContent = filesDiff.repoContent;
    commandResult.deltas = filesDiff.deltas;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDiffFilesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commandResult)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::diffFilesFromCommitWorker(
    const Structs::GitRepoDiffFilesFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDiffFilesFromCommitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const GitRepo::FilesDiff filesDiff =
      GitRepo::diffFilesFromCommit(repo, command.files, command.commit);

  if (!filesDiff.valid) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDiffFilesFromCommitError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    GitRepoDiffFilesFromCommitCommandResult commandResult;
    commandResult.diff = filesDiff.diff;
    commandResult.filesContent = filesDiff.filesContent;
    commandResult.repoContent = filesDiff.repoContent;
    commandResult.deltas = filesDiff.deltas;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commandResult)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::discardFilesWorker(
    const Structs::GitRepoDiscardFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerDiscardFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const bool success = GitRepo::discardFiles(repo, command.files);
      !success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerDiscardFilesError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDiscardFilesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::eventCallback(
    const EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Processing event {}.", (int)eventType);

  // A context ends with the answer to the request that made it (a clone, an
  // init or a failed open) or to its close. A refused request (the container
  // is in use) is answered with the same error event types, but must leave
  // the context alone: it is recognised by its sync uuid.
  const auto destroyIfAnswerOf = [this, &event](const QString &uuid,
                                                const bool close) {
    bool matches = false;
    {
      LOCKGUARD(_contextsMutex);
      const GitRepoContext *ctx = _contexts.value(uuid, nullptr);
      matches = ctx && !event.uuid().isEmpty() &&
                event.uuid() == (close ? ctx->closeSyncUuid : ctx->ownerSyncUuid);
    }
    if (matches) {
      destroyContext(uuid);
    }
  };

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerCloneRepoError:
  case EventManager::EventType::GitRepoManagerInitRepoError:
  case EventManager::EventType::GitRepoManagerOpenRepoError: {
    destroyIfAnswerOf(event.data<GitRepoError>().uuid, false);
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoResult:
  case EventManager::EventType::GitRepoManagerInitRepoResult: {
    destroyIfAnswerOf(event.data<GitRepoResult>().uuid, false);
    break;
  }
  case EventManager::EventType::GitRepoManagerCloseRepoResult: {
    destroyIfAnswerOf(event.data<GitRepoResult>().uuid, true);
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::fetchAllWorker(
    const Structs::GitRepoFetchAllCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerFetchAllError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::fetchAll(repo);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerFetchAllError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerFetchAllResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::fetchRemoteWorker(
    const Structs::GitRepoFetchRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerFetchRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::fetchRemote(repo, command.remoteName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerFetchRemoteError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerFetchRemoteResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::rebaseWorker(
    const Structs::GitRepoRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerRebaseError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::rebaseBranches(
      repo, command.sourceBranch, command.destinationBranch, command.author,
      command.email, command.signCommit);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerRebaseError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerRebaseResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::squashWorker(
    const Structs::GitRepoSquashCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerSquashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::squashBranches(
      repo, command.sourceBranch, command.destinationBranch, command.author,
      command.email, command.signCommit);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerSquashError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerSquashResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::bisectWorker(
    const Structs::GitRepoBisectCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerBisectError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  GitRepo::BisectStatus status;
  const bool success =
      GitRepo::bisect(repo, command.action, command.commit, status);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerBisectError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerBisectResult, command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(status)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

static bool runGitLabFlowAction(git_repository *repo,
                                const Structs::GitRepoGitFlowCommand &command,
                                bool *conflicted) {
  switch (command.gitLabFlowAction) {
  case GitRepo::GitLabFlowAction::StartFeature:
    return GitRepo::startGitLabFlowFeature(repo, command.branchName,
                                           command.mainBranchName);
  case GitRepo::GitLabFlowAction::FinishFeature:
    return GitRepo::finishGitLabFlowFeature(repo, command.branchName,
                                            command.mainBranchName,
                                            command.author, command.email);
  case GitRepo::GitLabFlowAction::Promote:
    return GitRepo::promoteGitLabFlowEnvironment(repo, command.environmentChain,
                                                 command.branchName,
                                                 command.author, command.email);
  case GitRepo::GitLabFlowAction::StartRelease:
    return GitRepo::startGitLabFlowRelease(repo, command.branchName,
                                           command.mainBranchName);
  case GitRepo::GitLabFlowAction::CherryPickToRelease:
    return GitRepo::cherryPickToGitLabFlowRelease(
        repo, command.commit, command.branchName, command.author, command.email,
        conflicted);
  case GitRepo::GitLabFlowAction::Unknown:
  case GitRepo::GitLabFlowAction::last:
  default:
    LOG_ERROR(Log::ErrorId::Git, "Unknown GitLab Flow action {}",
              static_cast<int>(command.gitLabFlowAction));
    return false;
  }
}

static bool
runGitFlowNextAction(git_repository *repo,
                     const Structs::GitRepoGitFlowCommand &command) {
  switch (command.gitFlowNextAction) {
  case GitRepo::GitFlowNextAction::Init:
    return GitRepo::initGitFlowNext(repo, command.mainBranchName,
                                    command.developmentBranchName);
  case GitRepo::GitFlowNextAction::Start:
    return GitRepo::startGitFlowNextBranch(repo, command.topicType,
                                           command.branchName);
  case GitRepo::GitFlowNextAction::Finish:
    return GitRepo::finishGitFlowNextBranch(repo, command.branchName,
                                            command.author, command.email);
  case GitRepo::GitFlowNextAction::Update:
    return GitRepo::updateGitFlowNextBranch(repo, command.branchName,
                                            command.author, command.email);
  case GitRepo::GitFlowNextAction::Apply:
    return GitRepo::applyGitFlowNextWorkflow(repo, command.customWorkflow);
  case GitRepo::GitFlowNextAction::Unknown:
  case GitRepo::GitFlowNextAction::last:
  default:
    LOG_ERROR(Log::ErrorId::Git, "Unknown git-flow-next action {}",
              static_cast<int>(command.gitFlowNextAction));
    return false;
  }
}

static bool runGitHubFlowAction(git_repository *repo,
                                const Structs::GitRepoGitFlowCommand &command) {
  switch (command.gitHubFlowAction) {
  case GitRepo::GitHubFlowAction::StartBranch:
    return GitRepo::startGitHubFlowBranch(repo, command.branchName,
                                          command.mainBranchName);
  case GitRepo::GitHubFlowAction::UpdateBranch:
    return GitRepo::updateGitHubFlowBranch(repo, command.branchName,
                                           command.mainBranchName,
                                           command.author, command.email);
  case GitRepo::GitHubFlowAction::FinishBranch:
    return GitRepo::finishGitHubFlowBranch(repo, command.branchName,
                                           command.mainBranchName,
                                           command.author, command.email);
  case GitRepo::GitHubFlowAction::Unknown:
  case GitRepo::GitHubFlowAction::last:
  default:
    LOG_ERROR(Log::ErrorId::Git, "Unknown GitHub Flow action {}",
              static_cast<int>(command.gitHubFlowAction));
    return false;
  }
}

void GitRepoManagerProcessingState::gitFlowWorker(
    const Structs::GitRepoGitFlowCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGitFlowError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  bool conflicted = false;
  bool success = false;
  if (command.workflow == GitRepo::WorkflowType::GitLabFlow) {
    success = runGitLabFlowAction(repo, command, &conflicted);
  } else if (command.workflow == GitRepo::WorkflowType::GitHubFlow) {
    success = runGitHubFlowAction(repo, command);
  } else if (command.workflow == GitRepo::WorkflowType::GitFlowNext) {
    success = runGitFlowNextAction(repo, command);
  } else {
    success = command.start
                  ? GitRepo::startGitFlowBranch(repo, command.branchType,
                                                command.branchName,
                                                command.developmentBranchName)
                  : GitRepo::finishGitFlowBranch(repo, command.branchType,
                                                 command.branchName,
                                                 command.author, command.email,
                                                 command.developmentBranchName);
  }
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGitFlowError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGitFlowResult, command.syncUuid);
    // The result value tells whether a cherry-pick left conflicts behind.
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(conflicted)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForFetchAll(
    const Structs::GitRepoFetchAllCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerFetchAllError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoFetchAllCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForFetchRemote(
    const Structs::GitRepoFetchRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerFetchRemoteError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoFetchRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForRebase(
    const Structs::GitRepoRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerRebaseError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRebaseCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForSquash(
    const Structs::GitRepoSquashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerSquashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoSquashCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForBisect(
    const Structs::GitRepoBisectCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerBisectError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoBisectCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGitFlow(
    const Structs::GitRepoGitFlowCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGitFlowError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGitFlowCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getFileSizeFromCommitWorker(
    const Structs::GitRepoGetFileSizeFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerGetFileSizeFromCommitError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const qint64 fileSize =
      GitRepo::getFileSizeFromCommit(repo, command.filePath, command.commit);

  if (fileSize < 0) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetFileSizeFromCommitError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    GitRepoGetFileSizeFromCommitCommandResult commandResult;
    commandResult.fileSize = fileSize;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetFileSizeFromCommitResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commandResult)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getActiveBranchWorker(
    const Structs::GitRepoGetActiveBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetActiveBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QString activeBranch =
      GitRepo::getActiveBranch(repo, command.branchType, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetActiveBranchError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    GitRepoGetActiveBranchCommandResult result;
    result.branchName = activeBranch;
    result.branchType = command.branchType;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetActiveBranchResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getCommitBranchesWorker(
    const Structs::GitRepoGetCommitBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitBranchesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QStringList branches =
      GitRepo::getCommitBranches(repo, command.commitOid, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitBranchesError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (branches.isEmpty()) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetCommitBranchesError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitBranchesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(branches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getCommitHeadsWorker(
    const Structs::GitRepoGetCommitHeadsCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitHeadsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QStringList branches =
      GitRepo::getCommitHeads(repo, command.commitOid, *quit);
  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitHeadsError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (branches.isEmpty()) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetCommitHeadsError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitHeadsResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(branches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getCommitIsHeadWorker(
    const Structs::GitRepoGetCommitIsHeadCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitIsHeadError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const bool head = GitRepo::getCommitIsHead(repo, command.commitOid, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitIsHeadError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitIsHeadResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(head)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getCommitStatusWorker(
    const Structs::GitRepoGetCommitStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const auto commitStatus =
          GitRepo::getCommitStatus(repo, command.commitOid);
      commitStatus.commit.isEmpty()) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetCommitStatusError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitStatusResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commitStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getLfsStorageStatusWorker(
    const Structs::GitRepoGetLfsStorageStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetLfsStorageStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const auto status = GitRepo::getLfsStorageStatus(repo);
  QVariantMap statusMap;
  statusMap.insert(QStringLiteral("repositoryPath"), status.repositoryPath);
  statusMap.insert(QStringLiteral("objectCount"), status.objectCount);
  statusMap.insert(QStringLiteral("totalBytes"), status.totalBytes);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerGetLfsStorageStatusResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(statusMap)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getStashStatusWorker(
    const Structs::GitRepoGetStashStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetStashStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  if (const auto stashStatus = GitRepo::getStashStatus(repo, command.index);
      stashStatus.commit.isEmpty()) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetStashStatusError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetStashStatusResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(stashStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getIsMergeInProgressWorker(
    const Structs::GitRepoGetIsMergeInProgressCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerGetIsMergeInProgressError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const auto mergeInProgress = GitRepo::getIsMergeInProgress(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, mergeInProgress});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getIsRebaseInProgressWorker(
    const Structs::GitRepoGetIsRebaseInProgressCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerGetIsRebaseInProgressError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const auto rebaseInProgress = GitRepo::getIsRebaseInProgress(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerGetIsRebaseInProgressResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, rebaseInProgress});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getBranchHistoryWorker(
    const Structs::GitRepoGetBranchHistoryCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetBranchHistoryError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *pQuit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    pQuit = &ctx->quit;
  }

  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      repo, command.branchName, command.branchType, command.page, *pQuit);

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetBranchHistoryError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (!history.valid) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetBranchHistoryError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetBranchHistoryResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(history)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getSubmodulePathWorker(
    const Structs::GitRepoGetSubmodulePathCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetSubmodulePath,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const auto path = GitRepo::getSubmodulePath(repo, command.submoduleName);
  if (path.isEmpty()) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetSubmodulePathError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    GitRepoGetSubmodulePathCommandResult result;
    result.submodulePath = path;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetSubmodulePathResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getUpstreamWorker(
    const Structs::GitRepoGetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetUpstreamError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const auto &[success, remoteName, remoteBranchName] =
      GitRepo::getUpstream(repo, command.localBranchName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetUpstreamError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    GitRepoGetUpstreamCommandResult result;
    result.remoteName = remoteName;
    result.remoteBranchName = remoteBranchName;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetUpstreamResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::initRepoWorker(
    const Structs::GitRepoInitRepoCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerInitRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Git, "Internal error. Context not initialized.");
    return;
  }

  const bool success = GitRepo::initRepository(
      command.repoName, command.workingDir, command.defaultBranch,
      command.author, command.email);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerInitRepoError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    const QString repoPath = command.workingDir + "/" + command.repoName;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerInitRepoResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, repoPath});
    _eventManager.send(resultEvent);
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = nullptr;
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listConflictsWorker(
    const Structs::GitRepoListConflictsCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListConflictsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const QString fileContentWithMarkers =
      GitRepo::listConflictsForAFile(repo, command.filePath);

  GitRepoListConflictsCommandResult result;
  result.fileContentWithMarkers = fileContentWithMarkers;

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListConflictsResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listLocalBranchesWorker(
    const Structs::GitRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListLocalBranchesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QStringList localBranches = GitRepo::listLocalBranches(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListLocalBranchesError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListLocalBranchesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(localBranches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listRemoteBranchesWorker(
    const Structs::GitRepoListRemoteBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListRemoteBranchesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QStringList remoteBranches = GitRepo::listRemoteBranches(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListRemoteBranchesError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(remoteBranches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listRemoteBranchesWithStatusWorker(
    const Structs::GitRepoListRemoteBranchesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::
                    GitRepoManagerListRemoteBranchesWithStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QVector<GitRepo::BranchInfo> remoteBranchesWithStatus =
      GitRepo::listRemoteBranchesWithStatus(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::
            GitRepoManagerListRemoteBranchesWithStatusError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::
            GitRepoManagerListRemoteBranchesWithStatusResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type,
                      QVariant::fromValue(remoteBranchesWithStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listStashesWorker(
    const Structs::GitRepoListStashesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListStashesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  const QStringList stashes = GitRepo::listStashes(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListStashesError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListStashesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(stashes)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listSubmodulesWorker(
    const Structs::GitRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListSubmodulesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const QStringList submodules = GitRepo::listSubmodules(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(submodules)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listSubmodulesWithStatusWorker(
    const Structs::GitRepoListSubmodulesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerListSubmodulesWithStatusError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool quit = false;

  const QVector<GitRepo::SubmoduleInfo> submodulesWithStatus =
      GitRepo::listSubmodulesWithStatus(repo, quit);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatusResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(submodulesWithStatus)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListSubmodulesWithStatus(
    const Structs::GitRepoListSubmodulesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::GitRepoManagerListSubmodulesWithStatusError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(
      new GitRepoListSubmodulesWithStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listTagsWorker(
    const Structs::GitRepoListTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const QStringList tags = GitRepo::listTags(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListTagsResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(tags)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listWorktreesWorker(
    const Structs::GitRepoListWorktreesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListWorktreesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const QVector<GitRepo::WorktreeInfo> worktrees = GitRepo::listWorktrees(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListWorktreesResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(worktrees)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::addWorktreeWorker(
    const Structs::GitRepoAddWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAddWorktreeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::addWorktree(repo, command.name, command.path, command.branch,
                           command.createBranch, command.baseBranch);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAddWorktreeError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAddWorktreeResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::removeWorktreeWorker(
    const Structs::GitRepoRemoveWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerRemoveWorktreeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::removeWorktree(repo, command.name, command.force);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerRemoveWorktreeError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerRemoveWorktreeResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::lockWorktreeWorker(
    const Structs::GitRepoLockWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerLockWorktreeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::lockWorktree(repo, command.name, command.reason);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerLockWorktreeError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerLockWorktreeResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::unlockWorktreeWorker(
    const Structs::GitRepoUnlockWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerUnlockWorktreeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::unlockWorktree(repo, command.name);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerUnlockWorktreeError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerUnlockWorktreeResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::mergeWorker(
    const Structs::GitRepoMergeCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerMergeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool merged =
      GitRepo::merge(repo, command.sourceBranch, command.destinationBranch,
                     command.author, command.email);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerMergeResult, command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, merged});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForOpenRepo(
    const GitRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerOpenRepoError,
                tr("Git repository has already been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc = std::bind(
      &GitRepoManagerProcessingState::workersThreadFunc, this, command.uuid);

  auto newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->ownerSyncUuid = command.syncUuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new GitRepoOpenCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForPull(
    const Structs::GitRepoPullCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerPullError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoPullCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForPush(
    const Structs::GitRepoPushCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerPushError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoPushCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForPushTag(
    const Structs::GitRepoPushTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerPushTagError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoPushTagCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForPushAllTags(
    const Structs::GitRepoPushAllTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerPushAllTagsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoPushAllTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForRenameBranch(
    const Structs::GitRepoRenameBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerRenameBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRenameBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForRenameTag(
    const Structs::GitRepoRenameTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerRenameTagError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRenameTagCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForReset(
    const Structs::GitRepoResetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerResetError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoResetCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForRevert(
    const Structs::GitRepoRevertCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerRevertError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRevertCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForSetUpstream(
    const Structs::GitRepoSetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerSetUpstreamError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoSetUpstreamCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForStageFiles(
    const Structs::GitRepoStageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerStageFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoStageFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForStash(
    const Structs::GitRepoStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerStashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoStashCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForStatus(
    const Structs::GitRepoStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForSubmoduleUpdate(
    const Structs::GitRepoSubmoduleUpdateCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerSubmoduleUpdateError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoSubmoduleUpdateCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForUnstageFiles(
    const Structs::GitRepoUnstageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerUnstageFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoUnstageFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListRemotes(
    const Structs::GitRepoListRemotesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListRemotesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListRemotesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAddRemote(
    const Structs::GitRepoAddRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAddRemoteError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAddRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForRemoveRemote(
    const Structs::GitRepoRemoveRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerRemoveRemoteError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRemoveRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForUpdateRemote(
    const Structs::GitRepoUpdateRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerUpdateRemoteError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoUpdateRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForSetDefaultRemote(
    const Structs::GitRepoSetDefaultRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerSetDefaultRemoteError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoSetDefaultRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForWorkflow(
    const Structs::GitRepoWorkflowCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerWorkflowError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoWorkflowCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAddTarget(
    const Structs::GitRepoAddTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAddTargetError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAddTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForEditTarget(
    const Structs::GitRepoEditTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerEditTargetError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoEditTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteTarget(
    const Structs::GitRepoDeleteTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteTargetError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForSetDefaultTarget(
    const Structs::GitRepoSetDefaultTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerSetDefaultTargetError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoSetDefaultTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForQuit(
    const Structs::GitRepoQuitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerQuitError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoQuitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::openRepoWorker(
    const GitRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  git_repository *repo =
      GitRepo::openRepository(command.path, command.author, command.email);
  if (!repo) {
    const git_error *gitError = git_error_last();
    reportError(EventManager::EventType::GitRepoManagerOpenRepoError,
                gitError->message, command);
    LOG_PERF(0, "Exit");
    return;
  }

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    GitRepo::closeRepository(repo);
    reportError(EventManager::EventType::GitRepoManagerOpenRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = repo;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerOpenRepoError, command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerOpenRepoResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, QVariant()});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::pullWorker(
    const Structs::GitRepoPullCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerPullError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::pull(repo, command.activeBranchName, command.rebase);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerPullError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerPullResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::pushWorker(
    const Structs::GitRepoPushCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerPushError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::push(repo, command.activeBranchName, command.force);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerPushError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerPushResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::pushTagWorker(
    const Structs::GitRepoPushTagCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerPushTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::pushTag(repo, command.tagName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerPushTagError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerPushTagResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::pushAllTagsWorker(
    const Structs::GitRepoPushAllTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerPushAllTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::pushAllTags(repo);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerPushAllTagsError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerPushAllTagsResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::renameBranchWorker(
    const Structs::GitRepoRenameBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerRenameBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::renameBranch(repo, command.oldBranchName, command.newBranchName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerRenameBranchError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerRenameBranchResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::renameTagWorker(
    const Structs::GitRepoRenameTagCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerRenameTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::renameTag(repo, command.oldTagName, command.newTagName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerRenameTagError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerRenameTagResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::resetWorker(
    const Structs::GitRepoResetCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerResetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::resetToCommit(repo, command.commit, command.resetType);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerResetError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerResetResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::revertWorker(
    const Structs::GitRepoRevertCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerRevertError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::revertCommit(
      repo, command.commit, command.author, command.email, command.message);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerRevertError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerRevertResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::cherryPickWorker(
    const Structs::GitRepoCherryPickCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCherryPickError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::cherryPickCommit(repo, command.commit,
                                                 command.author, command.email);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerCherryPickError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCherryPickResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCherryPick(
    const GitRepoCherryPickCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCherryPickError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCherryPickCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::setUpstreamWorker(
    const Structs::GitRepoSetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerSetUpstreamError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::setUpstream(repo, command.remoteName, command.remoteBranchName,
                           command.localBranchName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerSetUpstreamError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerSetUpstreamResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::stageFilesWorker(
    const Structs::GitRepoStageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerStageFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::stageFiles(repo, command.files);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerStageFilesError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerStageFilesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::stashWorker(
    const Structs::GitRepoStashCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerStashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::stash(repo, command.note);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerStashError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerStashResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::statusWorker(
    const Structs::GitRepoStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  if (!repo || *quit) {
    reportError(EventManager::EventType::GitRepoManagerStatusError,
                tr("The repository was closed."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const GitRepo::GitStatus status = GitRepo::getStatus(repo, *quit);

  if (*quit) {
    reportError(EventManager::EventType::GitRepoManagerStatusError,
                tr("The repository was closed."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerStatusResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(status)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::submoduleUpdateWorker(
    const Structs::GitRepoSubmoduleUpdateCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerSubmoduleUpdateError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::updateSubmodule(repo, command.submoduleName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerSubmoduleUpdateError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerSubmoduleUpdateResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::unstageFilesWorker(
    const Structs::GitRepoUnstageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerUnstageFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::unstageFiles(repo, command.files);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerUnstageFilesError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerUnstageFilesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::listRemotesWorker(
    const Structs::GitRepoListRemotesCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListRemotesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const auto remotes = GitRepo::listRemotes(repo);
  {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListRemotesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(remotes)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::addRemoteWorker(
    const Structs::GitRepoAddRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerAddRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::addRemote(repo, command.remoteName,
                                          command.fetchUrl, command.pushUrl);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerAddRemoteError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerAddRemoteResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::removeRemoteWorker(
    const Structs::GitRepoRemoveRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerRemoveRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::removeRemote(repo, command.remoteName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerRemoveRemoteError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerRemoveRemoteResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::updateRemoteWorker(
    const Structs::GitRepoUpdateRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerUpdateRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success =
      GitRepo::updateRemote(repo, command.remoteName, command.newRemoteName,
                            command.fetchUrl, command.pushUrl);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerUpdateRemoteError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerUpdateRemoteResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::setDefaultRemoteWorker(
    const Structs::GitRepoSetDefaultRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerSetDefaultRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const bool success = GitRepo::setDefaultRemote(repo, command.remoteName);
  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerSetDefaultRemoteError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerSetDefaultRemoteResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::workflowWorker(
    const Structs::GitRepoWorkflowCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerWorkflowError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  GitRepo::WorkflowState state;
  state.type = command.workflow;
  bool success = true;
  if (command.write) {
    success = GitRepo::setWorkflowType(repo, command.workflow);
  } else {
    state.type = GitRepo::getWorkflowType(repo);
  }

  // Without a readable configuration the repository counts as not
  // initialized, which the menu offers to fix.
  // The git-flow-next configuration is read for every workflow, so it can be
  // saved as a custom workflow from any of them.
  if (success) {
    GitRepo::readGitFlowNextConfig(repo, &state.gitFlowNext);
    if (state.type == GitRepo::WorkflowType::GitFlowNext) {
      GitRepo::getGitFlowNextWorkflowReference(repo, &state.customWorkflowId,
                                               &state.customWorkflowRevision);
    }
  }

  if (!success) {
    const auto lastError = GitRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerWorkflowError, command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerWorkflowResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(state)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::reportError(
    const EventManager::EventType eventType, const QString &message,
    const Structs::GitRepoCommand &command) const {
  LOG_PERF(0, "Enter");

  GitRepoError error;
  error.error = -1;
  error.message = message;
  error.uuid = command.uuid;

  EventManager::Event errorEvent(eventType, command.syncUuid);
  errorEvent.addData(error);
  _eventManager.send(errorEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAbortMerge(
    const Structs::GitRepoAbortMergeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAbortMergeError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAbortMergeCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAbortRebase(
    const Structs::GitRepoAbortRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAbortRebaseError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAbortRebaseCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAddSubmodule(
    const Structs::GitRepoAddSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAddSubmoduleError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAddSubmoduleCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAmend(
    const Structs::GitRepoAmendCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAmendError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAmendCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForApplyPatch(
    const Structs::GitRepoApplyPatchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerApplyPatchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoApplyPatchCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForApplyStash(
    const Structs::GitRepoApplyStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerApplyStashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoApplyStashCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCheckoutBranch(
    const Structs::GitRepoCheckoutBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCheckoutBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCheckoutCommit(
    const Structs::GitRepoCheckoutCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutCommitError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCheckoutCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCloneRepo(
    const Structs::GitRepoCloneCommand &command) {
  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                tr("Git repository is already in use."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc = std::bind(
      &GitRepoManagerProcessingState::workersThreadFunc, this, command.uuid);

  const auto newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->ownerSyncUuid = command.syncUuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new GitRepoCloneCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCloseRepo(
    const GitRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCloseRepoError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->quit = true;
  (*it)->closeSyncUuid = command.syncUuid;
  (*it)->commandsQueue.push(new GitRepoCloseCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCommitStagedRepo(
    const Structs::GitRepoCommitStagedCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCommitStagedError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCommitStagedCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCommitRebaseRepo(
    const Structs::GitRepoCommitRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCommitRebaseError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCommitRebaseCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCreateBranchFromCommit(
    const Structs::GitRepoCreateBranchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::GitRepoManagerCreateBranchFromCommitError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCreateBranchFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCreatePatchFromCommit(
    const Structs::GitRepoCreatePatchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::GitRepoManagerCreatePatchFromCommitError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCreatePatchFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForCreateTagFromCommit(
    const Structs::GitRepoCreateTagFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCreateTagFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetActiveBranch(
    const Structs::GitRepoGetActiveBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetActiveBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetActiveBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetCommitBranches(
    const Structs::GitRepoGetCommitBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitBranchesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::getCommitTagsWorker(
    const Structs::GitRepoGetCommitTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = nullptr;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
    quit = &ctx->quit;
  }

  // A commit without tags has an empty list; only a failure of the lookup is
  // an error.
  const GitRepo::GitErrorContext errorBefore = GitRepo::getLastError();
  const QStringList tags =
      GitRepo::getCommitTags(repo, command.commitOid, *quit);
  const GitRepo::GitErrorContext errorAfter = GitRepo::getLastError();
  const bool failed =
      tags.isEmpty() &&
      errorAfter.error == GitRepo::GitError::GetCommitTagsFailedToListMatchingTags &&
      (errorBefore.error != errorAfter.error ||
       errorBefore.errorMessage != errorAfter.errorMessage);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitTagsError,
        command.syncUuid);
    resultEvent.addData(GitRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (failed) {
    EventManager::Event errorEvent(
        EventManager::EventType::GitRepoManagerGetCommitTagsError,
        command.syncUuid);
    errorEvent.addData(GitRepoError{command.uuid, errorAfter.errorMessage,
                                    errorAfter.errorCode, errorAfter.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetCommitTagsResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(tags)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetCommitTags(
    const Structs::GitRepoGetCommitTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitTagsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetCommitHeads(
    const Structs::GitRepoGetCommitHeadsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitHeadsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitHeadsCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetCommitIsHead(
    const Structs::GitRepoGetCommitIsHeadCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitIsHeadError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitIsHeadCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetCommitStatus(
    const Structs::GitRepoGetCommitStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetLfsStorageStatus(
    const Structs::GitRepoGetLfsStorageStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetLfsStorageStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetLfsStorageStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetStashStatus(
    const Structs::GitRepoGetStashStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetStashStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetStashStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetIsMergeInProgress(
    const Structs::GitRepoGetIsMergeInProgressCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::GitRepoManagerGetIsMergeInProgressError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetIsMergeInProgressCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetIsRebaseInProgress(
    const Structs::GitRepoGetIsRebaseInProgressCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::GitRepoManagerGetIsRebaseInProgressError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetIsRebaseInProgressCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetSubmodulePath(
    const Structs::GitRepoGetSubmodulePathCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetSubmodulePathError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetSubmodulePathCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetUpstream(
    const Structs::GitRepoGetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetUpstreamError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetUpstreamCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteAllStashes(
    const Structs::GitRepoDeleteAllStashesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteAllStashesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteAllStashesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteBranch(
    const Structs::GitRepoDeleteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteRemoteBranch(
    const Structs::GitRepoDeleteRemoteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteRemoteBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteRemoteBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteStash(
    const Structs::GitRepoDeleteStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteStashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteStashCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteSubmodule(
    const Structs::GitRepoDeleteSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteSubmoduleError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteSubmoduleCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteTag(
    const Structs::GitRepoDeleteTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteTagError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteTagCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDeleteRemoteTag(
    const Structs::GitRepoDeleteRemoteTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDeleteRemoteTagError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteRemoteTagCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForBlame(
    const Structs::GitRepoBlameCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerBlameError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoBlameCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDiffFiles(
    const Structs::GitRepoDiffFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDiffFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDiffFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDiffFilesFromCommit(
    const Structs::GitRepoDiffFilesFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDiffFilesFromCommitError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDiffFilesFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetFileSizeFromCommit(
    const Structs::GitRepoGetFileSizeFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::GitRepoManagerGetFileSizeFromCommitError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetFileSizeFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForDiscardFiles(
    const Structs::GitRepoDiscardFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerDiscardFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDiscardFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForGetBranchHistory(
    const Structs::GitRepoGetBranchHistoryCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerGetBranchHistoryError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetBranchHistoryCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForInitRepo(
    const Structs::GitRepoInitRepoCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerInitRepoError,
                tr("Git repository is already in use."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc = std::bind(
      &GitRepoManagerProcessingState::workersThreadFunc, this, command.uuid);

  auto *newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->ownerSyncUuid = command.syncUuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new GitRepoInitRepoCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListConflicts(
    const Structs::GitRepoListConflictsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListConflictsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListConflictsCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListLocalBranches(
    const Structs::GitRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListLocalBranchesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListLocalBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListRemoteBranches(
    const Structs::GitRepoListRemoteBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListRemoteBranchesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListRemoteBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::
    scheduleWorkerForListRemoteBranchesWithStatus(
        const Structs::GitRepoListRemoteBranchesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::
                    GitRepoManagerListRemoteBranchesWithStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(
      new GitRepoListRemoteBranchesWithStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListStashes(
    const Structs::GitRepoListStashesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListStashesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListStashesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListSubmodules(
    const Structs::GitRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListSubmodulesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListSubmodulesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListTags(
    const Structs::GitRepoListTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListTagsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForListWorktrees(
    const Structs::GitRepoListWorktreesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerListWorktreesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListWorktreesCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForAddWorktree(
    const Structs::GitRepoAddWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerAddWorktreeError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoAddWorktreeCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForRemoveWorktree(
    const Structs::GitRepoRemoveWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerRemoveWorktreeError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRemoveWorktreeCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForLockWorktree(
    const Structs::GitRepoLockWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerLockWorktreeError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoLockWorktreeCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForUnlockWorktree(
    const Structs::GitRepoUnlockWorktreeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerUnlockWorktreeError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoUnlockWorktreeCommand(command));

  LOG_PERF(0, "Exit");
}

void GitRepoManagerProcessingState::scheduleWorkerForMerge(
    const Structs::GitRepoMergeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::GitRepoManagerMergeError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoMergeCommand(command));

  LOG_PERF(0, "Exit");
}

// The error event of a command, for answering it without running it.
static EventManager::EventType errorEventFor(const GitRepoCommandType type) {
  switch (type) {
  case GitRepoCommandType::AbortMerge:
    return EventManager::EventType::GitRepoManagerAbortMergeError;
  case GitRepoCommandType::AbortRebase:
    return EventManager::EventType::GitRepoManagerAbortRebaseError;
  case GitRepoCommandType::AddRemote:
    return EventManager::EventType::GitRepoManagerAddRemoteError;
  case GitRepoCommandType::AddSubmodule:
    return EventManager::EventType::GitRepoManagerAddSubmoduleError;
  case GitRepoCommandType::AddTarget:
    return EventManager::EventType::GitRepoManagerAddTargetError;
  case GitRepoCommandType::AddWorktree:
    return EventManager::EventType::GitRepoManagerAddWorktreeError;
  case GitRepoCommandType::EditTarget:
    return EventManager::EventType::GitRepoManagerEditTargetError;
  case GitRepoCommandType::DeleteTarget:
    return EventManager::EventType::GitRepoManagerDeleteTargetError;
  case GitRepoCommandType::SetDefaultTarget:
    return EventManager::EventType::GitRepoManagerSetDefaultTargetError;
  case GitRepoCommandType::Amend:
    return EventManager::EventType::GitRepoManagerAmendError;
  case GitRepoCommandType::Blame:
    return EventManager::EventType::GitRepoManagerBlameError;
  case GitRepoCommandType::ApplyPatch:
    return EventManager::EventType::GitRepoManagerApplyPatchError;
  case GitRepoCommandType::ApplyStash:
    return EventManager::EventType::GitRepoManagerApplyStashError;
  case GitRepoCommandType::CheckoutBranch:
    return EventManager::EventType::GitRepoManagerCheckoutBranchError;
  case GitRepoCommandType::CheckoutCommit:
    return EventManager::EventType::GitRepoManagerCheckoutCommitError;
  case GitRepoCommandType::CherryPick:
    return EventManager::EventType::GitRepoManagerCherryPickError;
  case GitRepoCommandType::Clone:
    return EventManager::EventType::GitRepoManagerCloneRepoError;
  case GitRepoCommandType::Close:
    return EventManager::EventType::GitRepoManagerCloseRepoError;
  case GitRepoCommandType::CommitStaged:
    return EventManager::EventType::GitRepoManagerCommitStagedError;
  case GitRepoCommandType::CommitRebase:
    return EventManager::EventType::GitRepoManagerCommitRebaseError;
  case GitRepoCommandType::CreateBranchFromCommit:
    return EventManager::EventType::GitRepoManagerCreateBranchFromCommitError;
  case GitRepoCommandType::CreatePatchFromCommit:
    return EventManager::EventType::GitRepoManagerCreatePatchFromCommitError;
  case GitRepoCommandType::CreateTagFromCommit:
    return EventManager::EventType::GitRepoManagerCreateTagFromCommitError;
  case GitRepoCommandType::DeleteBranch:
    return EventManager::EventType::GitRepoManagerDeleteBranchError;
  case GitRepoCommandType::DeleteRemoteBranch:
    return EventManager::EventType::GitRepoManagerDeleteRemoteBranchError;
  case GitRepoCommandType::DeleteStash:
    return EventManager::EventType::GitRepoManagerDeleteStashError;
  case GitRepoCommandType::DeleteAllStashes:
    return EventManager::EventType::GitRepoManagerDeleteAllStashesError;
  case GitRepoCommandType::DeleteSubmodule:
    return EventManager::EventType::GitRepoManagerDeleteSubmoduleError;
  case GitRepoCommandType::DeleteTag:
    return EventManager::EventType::GitRepoManagerDeleteTagError;
  case GitRepoCommandType::DeleteRemoteTag:
    return EventManager::EventType::GitRepoManagerDeleteRemoteTagError;
  case GitRepoCommandType::DiffFiles:
    return EventManager::EventType::GitRepoManagerDiffFilesError;
  case GitRepoCommandType::DiffFilesFromCommit:
    return EventManager::EventType::GitRepoManagerDiffFilesFromCommitError;
  case GitRepoCommandType::DiscardFiles:
    return EventManager::EventType::GitRepoManagerDiscardFilesError;
  case GitRepoCommandType::FetchAll:
    return EventManager::EventType::GitRepoManagerFetchAllError;
  case GitRepoCommandType::FetchRemote:
    return EventManager::EventType::GitRepoManagerFetchRemoteError;
  case GitRepoCommandType::GetActiveBranch:
    return EventManager::EventType::GitRepoManagerGetActiveBranchError;
  case GitRepoCommandType::GetBranchHistory:
    return EventManager::EventType::GitRepoManagerGetBranchHistoryError;
  case GitRepoCommandType::GetCommitBranches:
    return EventManager::EventType::GitRepoManagerGetCommitBranchesError;
  case GitRepoCommandType::GetCommitTags:
    return EventManager::EventType::GitRepoManagerGetCommitTagsError;
  case GitRepoCommandType::GetCommitHeads:
    return EventManager::EventType::GitRepoManagerGetCommitHeadsError;
  case GitRepoCommandType::GetCommitIsHead:
    return EventManager::EventType::GitRepoManagerGetCommitIsHeadError;
  case GitRepoCommandType::GetIsMergeInProgress:
    return EventManager::EventType::GitRepoManagerGetIsMergeInProgressError;
  case GitRepoCommandType::GetIsRebaseInProgress:
    return EventManager::EventType::GitRepoManagerGetIsRebaseInProgressError;
  case GitRepoCommandType::GetCommitStatus:
    return EventManager::EventType::GitRepoManagerGetCommitStatusError;
  case GitRepoCommandType::GetFileSizeFromCommit:
    return EventManager::EventType::GitRepoManagerGetFileSizeFromCommitError;
  case GitRepoCommandType::GetStashStatus:
    return EventManager::EventType::GitRepoManagerGetStashStatusError;
  case GitRepoCommandType::GetSubmodulePath:
    return EventManager::EventType::GitRepoManagerGetSubmodulePathError;
  case GitRepoCommandType::GetUpstream:
    return EventManager::EventType::GitRepoManagerGetUpstreamError;
  case GitRepoCommandType::GitFlow:
    return EventManager::EventType::GitRepoManagerGitFlowError;
  case GitRepoCommandType::Init:
    return EventManager::EventType::GitRepoManagerInitRepoError;
  case GitRepoCommandType::ListConflicts:
    return EventManager::EventType::GitRepoManagerListConflictsError;
  case GitRepoCommandType::ListLocalBranches:
    return EventManager::EventType::GitRepoManagerListLocalBranchesError;
  case GitRepoCommandType::ListRemoteBranches:
    return EventManager::EventType::GitRepoManagerListRemoteBranchesError;
  case GitRepoCommandType::ListRemoteBranchesWithStatus:
    return EventManager::EventType::GitRepoManagerListRemoteBranchesWithStatusError;
  case GitRepoCommandType::ListRemotes:
    return EventManager::EventType::GitRepoManagerListRemotesError;
  case GitRepoCommandType::ListStashes:
    return EventManager::EventType::GitRepoManagerListStashesError;
  case GitRepoCommandType::ListSubmodules:
    return EventManager::EventType::GitRepoManagerListSubmodulesError;
  case GitRepoCommandType::ListSubmodulesWithStatus:
    return EventManager::EventType::GitRepoManagerListSubmodulesWithStatusError;
  case GitRepoCommandType::ListTags:
    return EventManager::EventType::GitRepoManagerListTagsError;
  case GitRepoCommandType::ListWorktrees:
    return EventManager::EventType::GitRepoManagerListWorktreesError;
  case GitRepoCommandType::LockWorktree:
    return EventManager::EventType::GitRepoManagerLockWorktreeError;
  case GitRepoCommandType::UnlockWorktree:
    return EventManager::EventType::GitRepoManagerUnlockWorktreeError;
  case GitRepoCommandType::Merge:
    return EventManager::EventType::GitRepoManagerMergeError;
  case GitRepoCommandType::Rebase:
    return EventManager::EventType::GitRepoManagerRebaseError;
  case GitRepoCommandType::Squash:
    return EventManager::EventType::GitRepoManagerSquashError;
  case GitRepoCommandType::Bisect:
    return EventManager::EventType::GitRepoManagerBisectError;
  case GitRepoCommandType::Open:
    return EventManager::EventType::GitRepoManagerOpenRepoError;
  case GitRepoCommandType::Pull:
    return EventManager::EventType::GitRepoManagerPullError;
  case GitRepoCommandType::Push:
    return EventManager::EventType::GitRepoManagerPushError;
  case GitRepoCommandType::PushTag:
    return EventManager::EventType::GitRepoManagerPushTagError;
  case GitRepoCommandType::PushAllTags:
    return EventManager::EventType::GitRepoManagerPushAllTagsError;
  case GitRepoCommandType::RemoveRemote:
    return EventManager::EventType::GitRepoManagerRemoveRemoteError;
  case GitRepoCommandType::RemoveWorktree:
    return EventManager::EventType::GitRepoManagerRemoveWorktreeError;
  case GitRepoCommandType::RenameBranch:
    return EventManager::EventType::GitRepoManagerRenameBranchError;
  case GitRepoCommandType::RenameTag:
    return EventManager::EventType::GitRepoManagerRenameTagError;
  case GitRepoCommandType::Reset:
    return EventManager::EventType::GitRepoManagerResetError;
  case GitRepoCommandType::Revert:
    return EventManager::EventType::GitRepoManagerRevertError;
  case GitRepoCommandType::SetDefaultRemote:
    return EventManager::EventType::GitRepoManagerSetDefaultRemoteError;
  case GitRepoCommandType::Workflow:
    return EventManager::EventType::GitRepoManagerWorkflowError;
  case GitRepoCommandType::SetUpstream:
    return EventManager::EventType::GitRepoManagerSetUpstreamError;
  case GitRepoCommandType::StageFiles:
    return EventManager::EventType::GitRepoManagerStageFilesError;
  case GitRepoCommandType::Stash:
    return EventManager::EventType::GitRepoManagerStashError;
  case GitRepoCommandType::Status:
    return EventManager::EventType::GitRepoManagerStatusError;
  case GitRepoCommandType::SubmoduleUpdate:
    return EventManager::EventType::GitRepoManagerSubmoduleUpdateError;
  case GitRepoCommandType::UnstageFiles:
    return EventManager::EventType::GitRepoManagerUnstageFilesError;
  case GitRepoCommandType::UpdateRemote:
    return EventManager::EventType::GitRepoManagerUpdateRemoteError;
  case GitRepoCommandType::GetLfsStorageStatus:
    return EventManager::EventType::GitRepoManagerGetLfsStorageStatusError;
  default:
    return EventManager::EventType::Null;
  }
}

void GitRepoManagerProcessingState::workersThreadFunc(const QString &uuid) {
  LOG_PERF(0, "Enter");

  GitRepo::initForCurrentThread();

  bool quit = false;

  while (!quit) {
    GitRepoContext *ctx = context(uuid);
    if (!ctx)
      break;

    GitRepoCommand *command = ctx->commandsQueue.pop();
    if (!command) {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Enqueued command is null.");
      continue;
    }

    // Commands queued behind an open that failed find no repository. They
    // are answered without running: GitRepo expects a repository.
    bool hasRepository = false;
    {
      LOCKGUARD(_contextsMutex);
      hasRepository = ctx->gitRepo != nullptr;
    }
    const bool needsRepository =
        command->type != GitRepoCommandType::Open &&
        command->type != GitRepoCommandType::Clone &&
        command->type != GitRepoCommandType::Init &&
        command->type != GitRepoCommandType::Close &&
        command->type != GitRepoCommandType::Quit;
    if (needsRepository && !hasRepository) {
      const EventManager::EventType errorEvent = errorEventFor(command->type);
      if (errorEvent != EventManager::EventType::Null) {
        reportError(errorEvent, tr("Git repository has not been opened."),
                    *command);
      }
      delete command;
      continue;
    }

    switch (command->type) {
    case GitRepoCommandType::AbortMerge: {
      const auto abortMergeCommand =
          dynamic_cast<const GitRepoAbortMergeCommand *>(command);
      if (abortMergeCommand) {
        abortMergeWorker(*abortMergeCommand);
      }
      break;
    }
    case GitRepoCommandType::AbortRebase: {
      const auto abortRebaseCommand =
          dynamic_cast<const GitRepoAbortRebaseCommand *>(command);
      if (abortRebaseCommand) {
        abortRebaseWorker(*abortRebaseCommand);
      }
      break;
    }
    case GitRepoCommandType::AddSubmodule: {
      const auto addSubmoduleCommand =
          dynamic_cast<const GitRepoAddSubmoduleCommand *>(command);
      if (addSubmoduleCommand) {
        addSubmoduleWorker(*addSubmoduleCommand);
      }
      break;
    }
    case GitRepoCommandType::AddRemote: {
      const auto addRemoteCommand =
          dynamic_cast<const GitRepoAddRemoteCommand *>(command);
      if (addRemoteCommand) {
        addRemoteWorker(*addRemoteCommand);
      }
      break;
    }
    case GitRepoCommandType::Amend: {
      const auto amendCommand =
          dynamic_cast<const GitRepoAmendCommand *>(command);
      if (amendCommand) {
        amendWorker(*amendCommand);
      }
      break;
    }
    case GitRepoCommandType::ApplyPatch: {
      const auto applyPatchCommand =
          dynamic_cast<const GitRepoApplyPatchCommand *>(command);
      if (applyPatchCommand) {
        applyPatchWorker(*applyPatchCommand);
      }
      break;
    }
    case GitRepoCommandType::Blame: {
      const auto blameCommand =
          dynamic_cast<const GitRepoBlameCommand *>(command);
      if (blameCommand) {
        blameWorker(*blameCommand);
      }
      break;
    }
    case GitRepoCommandType::ApplyStash: {
      const auto applyStashCommand =
          dynamic_cast<const GitRepoApplyStashCommand *>(command);
      if (applyStashCommand) {
        applyStashWorker(*applyStashCommand);
      }
      break;
    }
    case GitRepoCommandType::CheckoutBranch: {
      const auto checkoutCommand =
          dynamic_cast<const GitRepoCheckoutBranchCommand *>(command);
      if (checkoutCommand) {
        checkoutBranchWorker(*checkoutCommand);
      }
      break;
    }
    case GitRepoCommandType::CheckoutCommit: {
      const auto checkoutCommand =
          dynamic_cast<const GitRepoCheckoutCommitCommand *>(command);
      if (checkoutCommand) {
        checkoutCommitWorker(*checkoutCommand);
      }
      break;
    }
    case GitRepoCommandType::Clone: {
      const auto cloneCommand =
          dynamic_cast<const GitRepoCloneCommand *>(command);
      if (cloneCommand) {
        cloneRepoWorker(*cloneCommand);
      }
      break;
    }
    case GitRepoCommandType::Close: {
      const auto closeCommand =
          dynamic_cast<const GitRepoCloseCommand *>(command);
      if (closeCommand) {
        closeRepoWorker(*closeCommand);
      }
      break;
    }
    case GitRepoCommandType::CommitStaged: {
      const auto commitStagedCommand =
          dynamic_cast<const GitRepoCommitStagedCommand *>(command);
      if (commitStagedCommand) {
        commitStagedWorker(*commitStagedCommand);
      }
      break;
    }
    case GitRepoCommandType::CommitRebase: {
      const auto commitRebaseCommand =
          dynamic_cast<const GitRepoCommitRebaseCommand *>(command);
      if (commitRebaseCommand) {
        commitRebaseWorker(*commitRebaseCommand);
      }
      break;
    }
    case GitRepoCommandType::CreateBranchFromCommit: {
      const auto createBranchFromCommitCommand =
          dynamic_cast<const GitRepoCreateBranchFromCommitCommand *>(command);
      if (createBranchFromCommitCommand) {
        createBranchFromCommitWorker(*createBranchFromCommitCommand);
      }
      break;
    }
    case GitRepoCommandType::CreatePatchFromCommit: {
      const auto createPatchFromCommitCommand =
          dynamic_cast<const GitRepoCreatePatchFromCommitCommand *>(command);
      if (createPatchFromCommitCommand) {
        createPatchFromCommitWorker(*createPatchFromCommitCommand);
      }
      break;
    }
    case GitRepoCommandType::CreateTagFromCommit: {
      const auto createTagFromCommitCommand =
          dynamic_cast<const GitRepoCreateTagFromCommitCommand *>(command);
      if (createTagFromCommitCommand) {
        createTagFromCommitWorker(*createTagFromCommitCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteAllStashes: {
      const auto deleteAllStashesCommand =
          dynamic_cast<const GitRepoDeleteAllStashesCommand *>(command);
      if (deleteAllStashesCommand) {
        deleteAllStashesWorker(*deleteAllStashesCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteBranch: {
      const auto deleteBranchCommand =
          dynamic_cast<const GitRepoDeleteBranchCommand *>(command);
      if (deleteBranchCommand) {
        deleteBranchWorker(*deleteBranchCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteRemoteBranch: {
      const auto deleteRemoteBranchCommand =
          dynamic_cast<const GitRepoDeleteRemoteBranchCommand *>(command);
      if (deleteRemoteBranchCommand) {
        deleteRemoteBranchWorker(*deleteRemoteBranchCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteStash: {
      const auto deleteStashCommand =
          dynamic_cast<const GitRepoDeleteStashCommand *>(command);
      if (deleteStashCommand) {
        deleteStashWorker(*deleteStashCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteSubmodule: {
      const auto deleteSubmoduleCommand =
          dynamic_cast<const GitRepoDeleteSubmoduleCommand *>(command);
      if (deleteSubmoduleCommand) {
        deleteSubmoduleWorker(*deleteSubmoduleCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteTag: {
      const auto deleteTagCommand =
          dynamic_cast<const GitRepoDeleteTagCommand *>(command);
      if (deleteTagCommand) {
        deleteTagWorker(*deleteTagCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteRemoteTag: {
      const auto deleteRemoteTagCommand =
          dynamic_cast<const GitRepoDeleteRemoteTagCommand *>(command);
      if (deleteRemoteTagCommand) {
        deleteRemoteTagWorker(*deleteRemoteTagCommand);
      }
      break;
    }
    case GitRepoCommandType::DiffFiles: {
      const auto diffFilesCommand =
          dynamic_cast<const GitRepoDiffFilesCommand *>(command);
      if (diffFilesCommand) {
        diffFilesWorker(*diffFilesCommand);
      }
      break;
    }
    case GitRepoCommandType::DiffFilesFromCommit: {
      const auto diffFilesFromCommitCommand =
          dynamic_cast<const GitRepoDiffFilesFromCommitCommand *>(command);
      if (diffFilesFromCommitCommand) {
        diffFilesFromCommitWorker(*diffFilesFromCommitCommand);
      }
      break;
    }
    case GitRepoCommandType::DiscardFiles: {
      const auto discardFilesCommand =
          dynamic_cast<const GitRepoDiscardFilesCommand *>(command);
      if (discardFilesCommand) {
        discardFilesWorker(*discardFilesCommand);
      }
      break;
    }
    case GitRepoCommandType::GetFileSizeFromCommit: {
      const auto getFileSizeFromCommitCommand =
          dynamic_cast<const GitRepoGetFileSizeFromCommitCommand *>(command);
      if (getFileSizeFromCommitCommand) {
        getFileSizeFromCommitWorker(*getFileSizeFromCommitCommand);
      }
      break;
    }
    case GitRepoCommandType::GetActiveBranch: {
      const auto getActiveBranchCommand =
          dynamic_cast<const GitRepoGetActiveBranchCommand *>(command);
      if (getActiveBranchCommand) {
        getActiveBranchWorker(*getActiveBranchCommand);
      }
      break;
    }
    case GitRepoCommandType::GetBranchHistory: {
      const auto *branchHistoryCommand =
          dynamic_cast<const GitRepoGetBranchHistoryCommand *>(command);
      if (branchHistoryCommand) {
        getBranchHistoryWorker(*branchHistoryCommand);
      }
      break;
    }
    case GitRepoCommandType::GetCommitBranches: {
      const auto *getCommitBranchesCommand =
          dynamic_cast<const GitRepoGetCommitBranchesCommand *>(command);
      if (getCommitBranchesCommand) {
        getCommitBranchesWorker(*getCommitBranchesCommand);
      }
      break;
    }
    case GitRepoCommandType::GetCommitHeads: {
      const auto *getCommitHeadsCommand =
          dynamic_cast<const GitRepoGetCommitHeadsCommand *>(command);
      if (getCommitHeadsCommand) {
        getCommitHeadsWorker(*getCommitHeadsCommand);
      }
      break;
    }
    case GitRepoCommandType::GetCommitIsHead: {
      const auto *getCommitIsHeadCommand =
          dynamic_cast<const GitRepoGetCommitIsHeadCommand *>(command);
      if (getCommitIsHeadCommand) {
        getCommitIsHeadWorker(*getCommitIsHeadCommand);
      }
      break;
    }
    case GitRepoCommandType::GetCommitStatus: {
      const auto *getCommitStatusCommand =
          dynamic_cast<const GitRepoGetCommitStatusCommand *>(command);
      if (getCommitStatusCommand) {
        getCommitStatusWorker(*getCommitStatusCommand);
      }
      break;
    }
    case GitRepoCommandType::GetCommitTags: {
      const auto *getCommitTagsCommand =
          dynamic_cast<const GitRepoGetCommitTagsCommand *>(command);
      if (getCommitTagsCommand) {
        getCommitTagsWorker(*getCommitTagsCommand);
      }
      break;
    }
    case GitRepoCommandType::GetIsMergeInProgress: {
      const auto *getIsMergeInProgressCommand =
          dynamic_cast<const GitRepoGetIsMergeInProgressCommand *>(command);
      if (getIsMergeInProgressCommand) {
        getIsMergeInProgressWorker(*getIsMergeInProgressCommand);
      }
      break;
    }
    case GitRepoCommandType::GetIsRebaseInProgress: {
      const auto *getIsRebaseInProgressCommand =
          dynamic_cast<const GitRepoGetIsRebaseInProgressCommand *>(command);
      if (getIsRebaseInProgressCommand) {
        getIsRebaseInProgressWorker(*getIsRebaseInProgressCommand);
      }
      break;
    }
    case GitRepoCommandType::GetStashStatus: {
      const auto *getStashStatusCommand =
          dynamic_cast<const GitRepoGetStashStatusCommand *>(command);
      if (getStashStatusCommand) {
        getStashStatusWorker(*getStashStatusCommand);
      }
      break;
    }
    case GitRepoCommandType::GetSubmodulePath: {
      const auto *getSubmodulePathCommand =
          dynamic_cast<const GitRepoGetSubmodulePathCommand *>(command);
      if (getSubmodulePathCommand) {
        getSubmodulePathWorker(*getSubmodulePathCommand);
      }
      break;
    }
    case GitRepoCommandType::GetUpstream: {
      const auto *getUpstreamCommand =
          dynamic_cast<const GitRepoGetUpstreamCommand *>(command);
      if (getUpstreamCommand) {
        getUpstreamWorker(*getUpstreamCommand);
      }
      break;
    }
    case GitRepoCommandType::Init: {
      const auto *initRepoCommand =
          dynamic_cast<const GitRepoInitRepoCommand *>(command);
      if (initRepoCommand) {
        initRepoWorker(*initRepoCommand);
      }
      break;
    }
    case GitRepoCommandType::ListConflicts: {
      const auto *listConflictsCommand =
          dynamic_cast<const GitRepoListConflictsCommand *>(command);
      if (listConflictsCommand) {
        listConflictsWorker(*listConflictsCommand);
      }
      break;
    }
    case GitRepoCommandType::ListLocalBranches: {
      const auto *listLocalCommand =
          dynamic_cast<const GitRepoListLocalBranchesCommand *>(command);
      if (listLocalCommand) {
        listLocalBranchesWorker(*listLocalCommand);
      }
      break;
    }
    case GitRepoCommandType::ListRemoteBranches: {
      const auto *listRemoteCommand =
          dynamic_cast<const GitRepoListRemoteBranchesCommand *>(command);
      if (listRemoteCommand) {
        listRemoteBranchesWorker(*listRemoteCommand);
      }
      break;
    }
    case GitRepoCommandType::ListRemotes: {
      const auto *listRemotesCommand =
          dynamic_cast<const GitRepoListRemotesCommand *>(command);
      if (listRemotesCommand) {
        listRemotesWorker(*listRemotesCommand);
      }
      break;
    }
    case GitRepoCommandType::ListRemoteBranchesWithStatus: {
      const auto *listRemoteStatusCommand =
          dynamic_cast<const GitRepoListRemoteBranchesWithStatusCommand *>(
              command);
      if (listRemoteStatusCommand) {
        listRemoteBranchesWithStatusWorker(*listRemoteStatusCommand);
      }
      break;
    }
    case GitRepoCommandType::ListStashes: {
      const auto *listStashesCommand =
          dynamic_cast<const GitRepoListStashesCommand *>(command);
      if (listStashesCommand) {
        listStashesWorker(*listStashesCommand);
      }
      break;
    }
    case GitRepoCommandType::ListSubmodules: {
      const auto *listSubmodulesCommand =
          dynamic_cast<const GitRepoListSubmodulesCommand *>(command);
      if (listSubmodulesCommand) {
        listSubmodulesWorker(*listSubmodulesCommand);
      }
      break;
    }
    case GitRepoCommandType::ListSubmodulesWithStatus: {
      const auto *listSubmodulesWithStatusCommand =
          dynamic_cast<const GitRepoListSubmodulesWithStatusCommand *>(command);
      if (listSubmodulesWithStatusCommand) {
        listSubmodulesWithStatusWorker(*listSubmodulesWithStatusCommand);
      }
      break;
    }
    case GitRepoCommandType::ListTags: {
      const auto *listTagsCommand =
          dynamic_cast<const GitRepoListTagsCommand *>(command);
      if (listTagsCommand) {
        listTagsWorker(*listTagsCommand);
      }
      break;
    }
    case GitRepoCommandType::ListWorktrees: {
      const auto *listWorktreesCommand =
          dynamic_cast<const GitRepoListWorktreesCommand *>(command);
      if (listWorktreesCommand) {
        listWorktreesWorker(*listWorktreesCommand);
      }
      break;
    }
    case GitRepoCommandType::AddWorktree: {
      const auto *addWorktreeCommand =
          dynamic_cast<const GitRepoAddWorktreeCommand *>(command);
      if (addWorktreeCommand) {
        addWorktreeWorker(*addWorktreeCommand);
      }
      break;
    }
    case GitRepoCommandType::RemoveWorktree: {
      const auto *removeWorktreeCommand =
          dynamic_cast<const GitRepoRemoveWorktreeCommand *>(command);
      if (removeWorktreeCommand) {
        removeWorktreeWorker(*removeWorktreeCommand);
      }
      break;
    }
    case GitRepoCommandType::LockWorktree: {
      const auto *lockWorktreeCommand =
          dynamic_cast<const GitRepoLockWorktreeCommand *>(command);
      if (lockWorktreeCommand) {
        lockWorktreeWorker(*lockWorktreeCommand);
      }
      break;
    }
    case GitRepoCommandType::UnlockWorktree: {
      const auto *unlockWorktreeCommand =
          dynamic_cast<const GitRepoUnlockWorktreeCommand *>(command);
      if (unlockWorktreeCommand) {
        unlockWorktreeWorker(*unlockWorktreeCommand);
      }
      break;
    }
    case GitRepoCommandType::Merge: {
      const auto *mergeCommand =
          dynamic_cast<const GitRepoMergeCommand *>(command);
      if (mergeCommand) {
        mergeWorker(*mergeCommand);
      }
      break;
    }
    case GitRepoCommandType::Rebase: {
      const auto *rebaseCommand =
          dynamic_cast<const GitRepoRebaseCommand *>(command);
      if (rebaseCommand) {
        rebaseWorker(*rebaseCommand);
      }
      break;
    }
    case GitRepoCommandType::Bisect: {
      const auto *bisectCommand =
          dynamic_cast<const GitRepoBisectCommand *>(command);
      if (bisectCommand) {
        bisectWorker(*bisectCommand);
      }
      break;
    }
    case GitRepoCommandType::Squash: {
      const auto *squashCommand =
          dynamic_cast<const GitRepoSquashCommand *>(command);
      if (squashCommand) {
        squashWorker(*squashCommand);
      }
      break;
    }
    case GitRepoCommandType::GitFlow: {
      const auto *gitFlowCommand =
          dynamic_cast<const GitRepoGitFlowCommand *>(command);
      if (gitFlowCommand) {
        gitFlowWorker(*gitFlowCommand);
      }
      break;
    }
    case GitRepoCommandType::Open: {
      const auto *openCommand =
          dynamic_cast<const GitRepoOpenCommand *>(command);
      if (openCommand) {
        openRepoWorker(*openCommand);
      }
      break;
    }
    case GitRepoCommandType::Pull: {
      const auto *pullCommand =
          dynamic_cast<const GitRepoPullCommand *>(command);
      if (pullCommand) {
        pullWorker(*pullCommand);
      }
      break;
    }
    case GitRepoCommandType::Push: {
      const auto *pushCommand =
          dynamic_cast<const GitRepoPushCommand *>(command);
      if (pushCommand) {
        pushWorker(*pushCommand);
      }
      break;
    }
    case GitRepoCommandType::FetchAll: {
      const auto *fetchAllCommand =
          dynamic_cast<const GitRepoFetchAllCommand *>(command);
      if (fetchAllCommand) {
        fetchAllWorker(*fetchAllCommand);
      }
      break;
    }
    case GitRepoCommandType::FetchRemote: {
      const auto *fetchRemoteCommand =
          dynamic_cast<const GitRepoFetchRemoteCommand *>(command);
      if (fetchRemoteCommand) {
        fetchRemoteWorker(*fetchRemoteCommand);
      }
      break;
    }
    case GitRepoCommandType::PushTag: {
      const auto *pushTagCommand =
          dynamic_cast<const GitRepoPushTagCommand *>(command);
      if (pushTagCommand) {
        pushTagWorker(*pushTagCommand);
      }
      break;
    }
    case GitRepoCommandType::PushAllTags: {
      const auto *pushAllTagsCommand =
          dynamic_cast<const GitRepoPushAllTagsCommand *>(command);
      if (pushAllTagsCommand) {
        pushAllTagsWorker(*pushAllTagsCommand);
      }
      break;
    }
    case GitRepoCommandType::RemoveRemote: {
      const auto *removeRemoteCommand =
          dynamic_cast<const GitRepoRemoveRemoteCommand *>(command);
      if (removeRemoteCommand) {
        removeRemoteWorker(*removeRemoteCommand);
      }
      break;
    }
    case GitRepoCommandType::RenameBranch: {
      const auto *renameBranchCommand =
          dynamic_cast<const GitRepoRenameBranchCommand *>(command);
      if (renameBranchCommand) {
        renameBranchWorker(*renameBranchCommand);
      }
      break;
    }
    case GitRepoCommandType::RenameTag: {
      const auto *renameTagCommand =
          dynamic_cast<const GitRepoRenameTagCommand *>(command);
      if (renameTagCommand) {
        renameTagWorker(*renameTagCommand);
      }
      break;
    }
    case GitRepoCommandType::Reset: {
      const auto *resetCommand =
          dynamic_cast<const GitRepoResetCommand *>(command);
      if (resetCommand) {
        resetWorker(*resetCommand);
      }
      break;
    }
    case GitRepoCommandType::Revert: {
      const auto *revertCommand =
          dynamic_cast<const GitRepoRevertCommand *>(command);
      if (revertCommand) {
        revertWorker(*revertCommand);
      }
      break;
    }
    case GitRepoCommandType::CherryPick: {
      const auto *cherryPickCommand =
          dynamic_cast<const GitRepoCherryPickCommand *>(command);
      if (cherryPickCommand) {
        cherryPickWorker(*cherryPickCommand);
      }
      break;
    }
    case GitRepoCommandType::SetDefaultRemote: {
      const auto *setDefaultRemoteCommand =
          dynamic_cast<const GitRepoSetDefaultRemoteCommand *>(command);
      if (setDefaultRemoteCommand) {
        setDefaultRemoteWorker(*setDefaultRemoteCommand);
      }
      break;
    }
    case GitRepoCommandType::Workflow: {
      const auto *workflowCommand =
          dynamic_cast<const GitRepoWorkflowCommand *>(command);
      if (workflowCommand) {
        workflowWorker(*workflowCommand);
      }
      break;
    }
    case GitRepoCommandType::AddTarget: {
      const auto *addTargetCommand =
          dynamic_cast<const GitRepoAddTargetCommand *>(command);
      if (addTargetCommand) {
        addTargetWorker(*addTargetCommand);
      }
      break;
    }
    case GitRepoCommandType::EditTarget: {
      const auto *editTargetCommand =
          dynamic_cast<const GitRepoEditTargetCommand *>(command);
      if (editTargetCommand) {
        editTargetWorker(*editTargetCommand);
      }
      break;
    }
    case GitRepoCommandType::DeleteTarget: {
      const auto *deleteTargetCommand =
          dynamic_cast<const GitRepoDeleteTargetCommand *>(command);
      if (deleteTargetCommand) {
        deleteTargetWorker(*deleteTargetCommand);
      }
      break;
    }
    case GitRepoCommandType::SetDefaultTarget: {
      const auto *setDefaultTargetCommand =
          dynamic_cast<const GitRepoSetDefaultTargetCommand *>(command);
      if (setDefaultTargetCommand) {
        setDefaultTargetWorker(*setDefaultTargetCommand);
      }
      break;
    }
    case GitRepoCommandType::SetUpstream: {
      const auto *setUpstreamCommand =
          dynamic_cast<const GitRepoSetUpstreamCommand *>(command);
      if (setUpstreamCommand) {
        setUpstreamWorker(*setUpstreamCommand);
      }
      break;
    }
    case GitRepoCommandType::StageFiles: {
      const auto *stageFilesCommand =
          dynamic_cast<const GitRepoStageFilesCommand *>(command);
      if (stageFilesCommand) {
        stageFilesWorker(*stageFilesCommand);
      }
      break;
    }
    case GitRepoCommandType::Stash: {
      const auto *stashCommand =
          dynamic_cast<const GitRepoStashCommand *>(command);
      if (stashCommand) {
        stashWorker(*stashCommand);
      }
      break;
    }
    case GitRepoCommandType::Status: {
      const auto *statusCommand =
          dynamic_cast<const GitRepoStatusCommand *>(command);
      if (statusCommand) {
        statusWorker(*statusCommand);
      }
      break;
    }
    case GitRepoCommandType::SubmoduleUpdate: {
      const auto *submoduleUpdateCommand =
          dynamic_cast<const GitRepoSubmoduleUpdateCommand *>(command);
      if (submoduleUpdateCommand) {
        submoduleUpdateWorker(*submoduleUpdateCommand);
      }
      break;
    }
    case GitRepoCommandType::UnstageFiles: {
      const auto *unstageFilesCommand =
          dynamic_cast<const GitRepoUnstageFilesCommand *>(command);
      if (unstageFilesCommand) {
        unstageFilesWorker(*unstageFilesCommand);
      }
      break;
    }
    case GitRepoCommandType::UpdateRemote: {
      const auto *updateRemoteCommand =
          dynamic_cast<const GitRepoUpdateRemoteCommand *>(command);
      if (updateRemoteCommand) {
        updateRemoteWorker(*updateRemoteCommand);
      }
      break;
    }
    case GitRepoCommandType::GetLfsStorageStatus: {
      const auto *lfsCommand =
          dynamic_cast<const GitRepoGetLfsStorageStatusCommand *>(command);
      if (lfsCommand) {
        getLfsStorageStatusWorker(*lfsCommand);
      }
      break;
    }
    case GitRepoCommandType::Quit: {
      const auto *quitCommand =
          dynamic_cast<const GitRepoQuitCommand *>(command);
      if (quitCommand) {
        quitRepoWorker(*quitCommand);
      }
      quit = true;
      break;
    }
    default: {
      LOG_WARN("Unknown command type {}.", static_cast<int>(command->type));
      break;
    }
    }

    delete command;

    // Make sure no credentials overlay outlives the command that raised it.
    GitRepo::finishPendingCredentialPrompts();
  }

  GitRepo::shutdownForCurrentThread();

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
