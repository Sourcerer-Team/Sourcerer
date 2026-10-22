#include "SvnRepoManagerProcessingState.h"
#include "SvnRepoManagerEvent.h"
#include "SvnRepoManagerStructs.h"

#include <Log.h>
#include <SvnRepo.h>

#include <functional>

#define THREAD_ID "SvnRepoManagerProcessingState"

using namespace SvnRepoManager::Structs;

namespace SvnRepoManager {

SvnRepoManagerProcessingState::SvnRepoManagerProcessingState(
    SvnRepoManagerFsm &fsm)
    : SvnRepoManagerState{StateType::Processing, fsm}, _quitAll{false},
      _eventManager{EventManager::EventManager::getInstance()} {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&SvnRepoManagerProcessingState::eventCallback, this,
                std::placeholders::_1);

  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerCloneRepoError,
                       eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerCloneRepoResult,
                       eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepoError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerInitRepoResult,
                       eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerCloseRepoResult,
                       eventCallbackFunc));

  LOG_PERF(0, "Exit");
}

SvnRepoManagerProcessingState::~SvnRepoManagerProcessingState() {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&SvnRepoManagerProcessingState::eventCallback, this,
                std::placeholders::_1);

  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerCloseRepoResult,
                       eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerInitRepoResult,
                       eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepoError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerCloneRepoError,
                       eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::SvnRepoManagerCloneRepoResult,
                       eventCallbackFunc));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::enter() {
  LOG_PERF(0, "Enter");

  _quitAll = false;

  LOG_PERF(0, "Exit");
}

bool SvnRepoManagerProcessingState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  switch (const auto type = static_cast<EventType>(event.type())) {
  case EventType::Setup: {
    break;
  }
  case EventType::AbortMerge: {
    const auto abortMergeCommand = event.nextParam<SvnRepoAbortMergeCommand>();
    scheduleWorkerForAbortMerge(abortMergeCommand);
    break;
  }
  case EventType::ResolveTreeConflict: {
    const auto resolveCommand =
        event.nextParam<SvnRepoResolveTreeConflictCommand>();
    scheduleWorkerForResolveTreeConflict(resolveCommand);
    break;
  }
  case EventType::AddSubmodule: {
    const auto addSubmoduleCommand =
        event.nextParam<SvnRepoAddSubmoduleCommand>();
    scheduleWorkerForAddSubmodule(addSubmoduleCommand);
    break;
  }
  case EventType::AddRemote: {
    const auto addRemoteCommand = event.nextParam<SvnRepoAddRemoteCommand>();
    scheduleWorkerForAddRemote(addRemoteCommand);
    break;
  }
  case EventType::Blame: {
    const auto blameCommand = event.nextParam<SvnRepoBlameCommand>();
    scheduleWorkerForBlame(blameCommand);
    break;
  }
  case EventType::ApplyPatch: {
    const auto applyPatchCommand = event.nextParam<SvnRepoApplyPatchCommand>();
    scheduleWorkerForApplyPatch(applyPatchCommand);
    break;
  }
  case EventType::ApplyStash: {
    const auto applyStashCommand = event.nextParam<SvnRepoApplyStashCommand>();
    scheduleWorkerForApplyStash(applyStashCommand);
    break;
  }
  case EventType::CheckoutBranch: {
    const auto checkoutCommand =
        event.nextParam<SvnRepoCheckoutBranchCommand>();
    scheduleWorkerForCheckoutBranch(checkoutCommand);
    break;
  }
  case EventType::CheckoutCommit: {
    const auto checkoutCommand =
        event.nextParam<SvnRepoCheckoutCommitCommand>();
    scheduleWorkerForCheckoutCommit(checkoutCommand);
    break;
  }
  case EventType::CloneRepo: {
    const auto cloneCommand = event.nextParam<SvnRepoCloneCommand>();
    scheduleWorkerForCloneRepo(cloneCommand);
    break;
  }
  case EventType::CloseRepo: {
    const auto closeCommand = event.nextParam<SvnRepoCloseCommand>();
    scheduleWorkerForCloseRepo(closeCommand);
    break;
  }
  case EventType::CommitStaged: {
    const auto commitStagedCommand =
        event.nextParam<SvnRepoCommitStagedCommand>();
    scheduleWorkerForCommitStagedRepo(commitStagedCommand);
    break;
  }
  case EventType::CreateBranchFromCommit: {
    const auto createBranchFromCommitCommand =
        event.nextParam<SvnRepoCreateBranchFromCommitCommand>();
    scheduleWorkerForCreateBranchFromCommit(createBranchFromCommitCommand);
    break;
  }
  case EventType::CreatePatchFromCommit: {
    const auto createPatchFromCommitCommand =
        event.nextParam<SvnRepoCreatePatchFromCommitCommand>();
    scheduleWorkerForCreatePatchFromCommit(createPatchFromCommitCommand);
    break;
  }
  case EventType::CreateTagFromCommit: {
    const auto createTagFromCommitCommand =
        event.nextParam<SvnRepoCreateTagFromCommitCommand>();
    scheduleWorkerForCreateTagFromCommit(createTagFromCommitCommand);
    break;
  }
  case EventType::DeleteAllStashes: {
    const auto deleteAllStashesCommand =
        event.nextParam<SvnRepoDeleteAllStashesCommand>();
    scheduleWorkerForDeleteAllStashes(deleteAllStashesCommand);
    break;
  }
  case EventType::DeleteBranch: {
    const auto deleteBranchCommand =
        event.nextParam<SvnRepoDeleteBranchCommand>();
    scheduleWorkerForDeleteBranch(deleteBranchCommand);
    break;
  }
  case EventType::DeleteRemoteBranch: {
    const auto deleteRemoteBranchCommand =
        event.nextParam<SvnRepoDeleteRemoteBranchCommand>();
    scheduleWorkerForDeleteRemoteBranch(deleteRemoteBranchCommand);
    break;
  }
  case EventType::DeleteStash: {
    const auto deleteStashCommand =
        event.nextParam<SvnRepoDeleteStashCommand>();
    scheduleWorkerForDeleteStash(deleteStashCommand);
    break;
  }
  case EventType::DeleteSubmodule: {
    const auto deleteSubmoduleCommand =
        event.nextParam<SvnRepoDeleteSubmoduleCommand>();
    scheduleWorkerForDeleteSubmodule(deleteSubmoduleCommand);
    break;
  }
  case EventType::DeleteTag: {
    const auto deleteTagCommand = event.nextParam<SvnRepoDeleteTagCommand>();
    scheduleWorkerForDeleteTag(deleteTagCommand);
    break;
  }
  case EventType::DeleteRemoteTag: {
    const auto deleteRemoteTagCommand =
        event.nextParam<SvnRepoDeleteRemoteTagCommand>();
    scheduleWorkerForDeleteRemoteTag(deleteRemoteTagCommand);
    break;
  }
  case EventType::DiffFiles: {
    const auto diffFilesCommand = event.nextParam<SvnRepoDiffFilesCommand>();
    scheduleWorkerForDiffFiles(diffFilesCommand);
    break;
  }
  case EventType::DiffFilesFromCommit: {
    const auto diffFilesFromCommitCommand =
        event.nextParam<SvnRepoDiffFilesFromCommitCommand>();
    scheduleWorkerForDiffFilesFromCommit(diffFilesFromCommitCommand);
    break;
  }
  case EventType::DiscardFiles: {
    const auto discardFilesCommand =
        event.nextParam<SvnRepoDiscardFilesCommand>();
    scheduleWorkerForDiscardFiles(discardFilesCommand);
    break;
  }
  case EventType::FetchAll: {
    const auto fetchAllCommand = event.nextParam<SvnRepoFetchAllCommand>();
    scheduleWorkerForFetchAll(fetchAllCommand);
    break;
  }
  case EventType::FetchRemote: {
    const auto fetchRemoteCommand =
        event.nextParam<SvnRepoFetchRemoteCommand>();
    scheduleWorkerForFetchRemote(fetchRemoteCommand);
    break;
  }
  case EventType::GetFileSizeFromCommit: {
    const auto getFileSizeFromCommitCommand =
        event.nextParam<SvnRepoGetFileSizeFromCommitCommand>();
    scheduleWorkerForGetFileSizeFromCommit(getFileSizeFromCommitCommand);
    break;
  }
  case EventType::GetActiveBranch: {
    const auto getActiveBranchCommand =
        event.nextParam<SvnRepoGetActiveBranchCommand>();
    scheduleWorkerForGetActiveBranch(getActiveBranchCommand);
    break;
  }
  case EventType::GetBranchHistory: {
    const auto branchHistoryCommand =
        event.nextParam<SvnRepoGetBranchHistoryCommand>();
    scheduleWorkerForGetBranchHistory(branchHistoryCommand);
    break;
  }
  case EventType::GetCommitBranches: {
    const auto getCommitBranchesCommand =
        event.nextParam<SvnRepoGetCommitBranchesCommand>();
    scheduleWorkerForGetCommitBranches(getCommitBranchesCommand);
    break;
  }
  case EventType::GetCommitTags: {
    const auto getCommitTagsCommand =
        event.nextParam<SvnRepoGetCommitTagsCommand>();
    scheduleWorkerForGetCommitTags(getCommitTagsCommand);
    break;
  }
  case EventType::GetCommitHeads: {
    const auto getCommitHeadsCommand =
        event.nextParam<SvnRepoGetCommitHeadsCommand>();
    scheduleWorkerForGetCommitHeads(getCommitHeadsCommand);
    break;
  }
  case EventType::GetCommitIsHead: {
    const auto getCommitIsHeadCommand =
        event.nextParam<SvnRepoGetCommitIsHeadCommand>();
    scheduleWorkerForGetCommitIsHead(getCommitIsHeadCommand);
    break;
  }
  case EventType::GetCommitStatus: {
    const auto commitStatusCommand =
        event.nextParam<SvnRepoGetCommitStatusCommand>();
    scheduleWorkerForGetCommitStatus(commitStatusCommand);
    break;
  }
  case EventType::GetStashStatus: {
    const auto stashStatusCommand =
        event.nextParam<SvnRepoGetStashStatusCommand>();
    scheduleWorkerForGetStashStatus(stashStatusCommand);
    break;
  }
  case EventType::GetIsMergeInProgress: {
    const auto getIsMergeInProgressCommand =
        event.nextParam<SvnRepoGetIsMergeInProgressCommand>();
    scheduleWorkerForGetIsMergeInProgress(getIsMergeInProgressCommand);
    break;
  }
  case EventType::GetSubmodulePath: {
    const auto getSubmodulePathCommand =
        event.nextParam<SvnRepoGetSubmodulePathCommand>();
    scheduleWorkerForGetSubmodulePath(getSubmodulePathCommand);
    break;
  }
  case EventType::GetUpstream: {
    const auto getUpstreamCommand =
        event.nextParam<SvnRepoGetUpstreamCommand>();
    scheduleWorkerForGetUpstream(getUpstreamCommand);
    break;
  }
  case EventType::Init: {
    const auto initCommand = event.nextParam<SvnRepoInitRepoCommand>();
    scheduleWorkerForInitRepo(initCommand);
    break;
  }
  case EventType::ListConflicts: {
    const auto listConflictsCommand =
        event.nextParam<SvnRepoListConflictsCommand>();
    scheduleWorkerForListConflicts(listConflictsCommand);
    break;
  }
  case EventType::ListLocalBranches: {
    const auto listLocalCommand =
        event.nextParam<SvnRepoListLocalBranchesCommand>();
    scheduleWorkerForListLocalBranches(listLocalCommand);
    break;
  }
  case EventType::ListRemoteBranches: {
    const auto listRemoteCommand =
        event.nextParam<SvnRepoListRemoteBranchesCommand>();
    scheduleWorkerForListRemoteBranches(listRemoteCommand);
    break;
  }
  case EventType::ListRemoteBranchesWithStatus: {
    const auto listRemoteStatusCommand =
        event.nextParam<SvnRepoListRemoteBranchesWithStatusCommand>();
    scheduleWorkerForListRemoteBranchesWithStatus(listRemoteStatusCommand);
    break;
  }
  case EventType::ListRemotes: {
    const auto listRemotesCommand =
        event.nextParam<SvnRepoListRemotesCommand>();
    scheduleWorkerForListRemotes(listRemotesCommand);
    break;
  }
  case EventType::ListStashes: {
    const auto listStashesCommand =
        event.nextParam<SvnRepoListStashesCommand>();
    scheduleWorkerForListStashes(listStashesCommand);
    break;
  }
  case EventType::ListSubmodules: {
    const auto listSubmodulesCommand =
        event.nextParam<SvnRepoListSubmodulesCommand>();
    scheduleWorkerForListSubmodules(listSubmodulesCommand);
    break;
  }
  case EventType::ListSubmodulesWithStatus: {
    const auto listSubmodulesWithStatusCommand =
        event.nextParam<SvnRepoListSubmodulesWithStatusCommand>();
    scheduleWorkerForListSubmodulesWithStatus(listSubmodulesWithStatusCommand);
    break;
  }
  case EventType::ListTags: {
    const auto listTagsCommand = event.nextParam<SvnRepoListTagsCommand>();
    scheduleWorkerForListTags(listTagsCommand);
    break;
  }
  case EventType::ListRemoteTags: {
    const auto listRemoteTagsCommand =
        event.nextParam<SvnRepoListRemoteTagsCommand>();
    scheduleWorkerForListRemoteTags(listRemoteTagsCommand);
    break;
  }
  case EventType::Merge: {
    const auto mergeCommand = event.nextParam<SvnRepoMergeCommand>();
    scheduleWorkerForMerge(mergeCommand);
    break;
  }
  case EventType::Rebase: {
    const auto rebaseCommand = event.nextParam<SvnRepoRebaseCommand>();
    scheduleWorkerForRebase(rebaseCommand);
    break;
  }
  case EventType::OpenRepo: {
    const auto openCommand = event.nextParam<SvnRepoOpenCommand>();
    scheduleWorkerForOpenRepo(openCommand);
    break;
  }
  case EventType::Pull: {
    const auto pullCommand = event.nextParam<SvnRepoPullCommand>();
    scheduleWorkerForPull(pullCommand);
    break;
  }
  case EventType::Push: {
    const auto pushCommand = event.nextParam<SvnRepoPushCommand>();
    scheduleWorkerForPush(pushCommand);
    break;
  }
  case EventType::PushTag: {
    const auto pushTagCommand = event.nextParam<SvnRepoPushTagCommand>();
    scheduleWorkerForPushTag(pushTagCommand);
    break;
  }
  case EventType::PushAllTags: {
    const auto pushAllTagsCommand =
        event.nextParam<SvnRepoPushAllTagsCommand>();
    scheduleWorkerForPushAllTags(pushAllTagsCommand);
    break;
  }
  case EventType::RemoveRemote: {
    const auto removeRemoteCommand =
        event.nextParam<SvnRepoRemoveRemoteCommand>();
    scheduleWorkerForRemoveRemote(removeRemoteCommand);
    break;
  }
  case EventType::RenameBranch: {
    const auto renameBranchCommand =
        event.nextParam<SvnRepoRenameBranchCommand>();
    scheduleWorkerForRenameBranch(renameBranchCommand);
    break;
  }
  case EventType::RenameTag: {
    const auto renameTagCommand = event.nextParam<SvnRepoRenameTagCommand>();
    scheduleWorkerForRenameTag(renameTagCommand);
    break;
  }
  case EventType::Reset: {
    const auto resetCommand = event.nextParam<SvnRepoResetCommand>();
    scheduleWorkerForReset(resetCommand);
    break;
  }
  case EventType::Revert: {
    const auto revertCommand = event.nextParam<SvnRepoRevertCommand>();
    scheduleWorkerForRevert(revertCommand);
    break;
  }
  case EventType::CherryPick: {
    const auto cherryPickCommand = event.nextParam<SvnRepoCherryPickCommand>();
    scheduleWorkerForCherryPick(cherryPickCommand);
    break;
  }
  case EventType::SetDefaultRemote: {
    const auto setDefaultRemoteCommand =
        event.nextParam<SvnRepoSetDefaultRemoteCommand>();
    scheduleWorkerForSetDefaultRemote(setDefaultRemoteCommand);
    break;
  }
  case EventType::AddTarget: {
    const auto addTargetCommand = event.nextParam<SvnRepoAddTargetCommand>();
    scheduleWorkerForAddTarget(addTargetCommand);
    break;
  }
  case EventType::EditTarget: {
    const auto editTargetCommand = event.nextParam<SvnRepoEditTargetCommand>();
    scheduleWorkerForEditTarget(editTargetCommand);
    break;
  }
  case EventType::DeleteTarget: {
    const auto deleteTargetCommand =
        event.nextParam<SvnRepoDeleteTargetCommand>();
    scheduleWorkerForDeleteTarget(deleteTargetCommand);
    break;
  }
  case EventType::SetDefaultTarget: {
    const auto setDefaultTargetCommand =
        event.nextParam<SvnRepoSetDefaultTargetCommand>();
    scheduleWorkerForSetDefaultTarget(setDefaultTargetCommand);
    break;
  }
  case EventType::SetUpstream: {
    const auto setUpstreamCommand =
        event.nextParam<SvnRepoSetUpstreamCommand>();
    scheduleWorkerForSetUpstream(setUpstreamCommand);
    break;
  }
  case EventType::IgnoreFiles: {
    const auto ignoreFilesCommand = event.nextParam<SvnRepoIgnoreFilesCommand>();
    scheduleWorkerForIgnoreFiles(ignoreFilesCommand);
    break;
  }
  case EventType::StageFiles: {
    const auto stageFilesCommand = event.nextParam<SvnRepoStageFilesCommand>();
    scheduleWorkerForStageFiles(stageFilesCommand);
    break;
  }
  case EventType::Stash: {
    const auto stashCommand = event.nextParam<SvnRepoStashCommand>();
    scheduleWorkerForStash(stashCommand);
    break;
  }
  case EventType::Status: {
    const auto statusCommand = event.nextParam<SvnRepoStatusCommand>();
    scheduleWorkerForStatus(statusCommand);
    break;
  }
  case EventType::SubmoduleUpdate: {
    const auto submoduleUpdateCommand =
        event.nextParam<SvnRepoSubmoduleUpdateCommand>();
    scheduleWorkerForSubmoduleUpdate(submoduleUpdateCommand);
    break;
  }
  case EventType::UnstageFiles: {
    const auto unstageFilesCommand =
        event.nextParam<SvnRepoUnstageFilesCommand>();
    scheduleWorkerForUnstageFiles(unstageFilesCommand);
    break;
  }
  case EventType::UpdateRemote: {
    const auto updateRemoteCommand =
        event.nextParam<SvnRepoUpdateRemoteCommand>();
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

void SvnRepoManagerProcessingState::exit() {
  LOG_PERF(0, "Enter");

  destroyContextsMap();

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::resolveTreeConflictWorker(
    const Structs::SvnRepoResolveTreeConflictCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerResolveTreeConflictError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::resolveTreeConflict(repo, command.filePath, command.keepLocal);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerResolveTreeConflictError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerResolveTreeConflictResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::abortMergeWorker(
    const Structs::SvnRepoAbortMergeCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerAbortMergeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::abortMerge(repo);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerAbortMergeError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerAbortMergeResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::addSubmoduleWorker(
    const Structs::SvnRepoAddSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerAddSubmoduleError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::addSubmodule(repo, command.url, command.name,
                                             command.branch, command.uuid);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerAddSubmoduleError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerAddSubmoduleResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::blameWorker(
    const Structs::SvnRepoBlameCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerBlameError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const SvnRepo::BlameReport blameReport = SvnRepo::blame(repo, command.files, command.commit);
  if (!blameReport.valid) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerBlameError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
    LOG_PERF(0, "Exit");
    return;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerBlameResult, command.syncUuid);
  resultEvent.addData(SvnRepoResult{
      command.uuid, command.type,
      QVariant::fromValue(SvnRepoBlameCommandResult{blameReport})});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::applyPatchWorker(
    const Structs::SvnRepoApplyPatchCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerApplyPatchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::applyPatch(repo, command.patchContent);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerApplyPatchError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerApplyPatchResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::applyStashWorker(
    const Structs::SvnRepoApplyStashCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerApplyStashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::applyStash(repo, command.index); !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerApplyStashError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    // Check if there are conflicts after stash apply
    const auto status = SvnRepo::getStatus(repo, false);
    const bool hasConflicts = !status.conflictedFiles.isEmpty();
    QVariantMap conflictedFiles;

    for (const auto &filePath : status.conflictedFiles) {
      const QString fileContentWithMarkers =
          SvnRepo::listConflictsForStashFile(repo, command.index, filePath);
      if (!fileContentWithMarkers.isEmpty()) {
        conflictedFiles.insert(filePath, fileContentWithMarkers);
      }
    }

    QMap<QString, QVariant> resultData;
    resultData["success"] = true;
    resultData["hasConflicts"] = hasConflicts;
    resultData["conflictedFiles"] = conflictedFiles;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerApplyStashResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(resultData)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::checkoutBranchWorker(
    const Structs::SvnRepoCheckoutBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCheckoutBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::checkoutBranch(repo, command.branchName, command.branchType);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCheckoutBranchError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    LOG_DEBUG("Sending checkout error event for branch: '{}'",
              command.branchName.toStdString());
    _eventManager.send(errorEvent);
  } else {
    SvnRepoCheckoutBranchCommandResult result;
    result.branchName = command.branchName;
    result.branchType = command.branchType;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCheckoutBranchResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    LOG_DEBUG("Sending checkout success event for branch: '{}'",
              command.branchName.toStdString());
    _eventManager.send(resultEvent);
    LOG_DEBUG("Checkout success event sent for branch: '{}'",
              command.branchName.toStdString());
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::checkoutCommitWorker(
    const Structs::SvnRepoCheckoutCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCheckoutCommitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::checkoutCommit(repo, command.commit);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCheckoutCommitError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCheckoutCommitResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::cloneRepoWorker(
    const SvnRepoCloneCommand &command) {
  LOG_PERF(0, "Enter");

  SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCloneRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  const bool success = SvnRepo::cloneRepository(
      command.url, command.destinationDir, command.uuid);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCloneRepoError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCloneRepoResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, command.destinationDir});
    _eventManager.send(resultEvent);
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->SvnRepo.clear();
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::closeRepoWorker(
    const Structs::SvnRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCloseRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (!repo.isEmpty()) {
    SvnRepo::closeRepository(repo);

    LOCKGUARD(_contextsMutex);
    ctx->SvnRepo.clear();
  }

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerCloseRepoResult, command.syncUuid);
  resultEvent.addData(SvnRepoResult{command.uuid, command.type, QVariant()});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::commitStagedWorker(
    const Structs::SvnRepoCommitStagedCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCommitStagedError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::commitStaged(
      repo, command.summary, command.message, command.author, command.email,
      command.cleanup, command.signoff, command.signCommit);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCommitStagedError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCommitStagedResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::createBranchFromCommitWorker(
    const Structs::SvnRepoCreateBranchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::SvnRepoManagerCreateBranchFromCommitError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::createBranchFromCommit(repo, command.commit, command.branchName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCreateBranchFromCommitError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCreateBranchFromCommitResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::createPatchFromCommitWorker(
    const Structs::SvnRepoCreatePatchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::SvnRepoManagerCreatePatchFromCommitError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const QString patchContent =
      SvnRepo::createPatchFromCommit(repo, command.commit);
  if (patchContent.isEmpty()) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCreatePatchFromCommitError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    SvnRepoCreatePatchFromCommitCommandResult result;
    result.patchContent = patchContent;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCreatePatchFromCommitResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::createTagFromCommitWorker(
    const Structs::SvnRepoCreateTagFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCreateTagFromCommitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::createTagFromCommit(
      repo, command.commit, command.tagName, command.author, command.email,
      command.message);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCreateTagFromCommitError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCreateTagFromCommitResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::addTargetWorker(
    const Structs::SvnRepoAddTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerAddTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::addTarget(repo, command.remoteName, command.targetName,
                         command.fetchUrl, command.pushUrl);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerAddTargetError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerAddTargetResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::editTargetWorker(
    const Structs::SvnRepoEditTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerEditTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::editTarget(repo, command.remoteName, command.targetName,
                          command.newFetchUrl, command.newPushUrl);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerEditTargetError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerEditTargetResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteTargetWorker(
    const Structs::SvnRepoDeleteTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::deleteTarget(repo, command.remoteName, command.targetName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteTargetError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteTargetResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::setDefaultTargetWorker(
    const Structs::SvnRepoSetDefaultTargetCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerSetDefaultTargetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::setDefaultTarget(repo, command.remoteName, command.targetName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerSetDefaultTargetError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerSetDefaultTargetResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::quitRepoWorker(
    const Structs::SvnRepoQuitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerQuitError,
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
        EventManager::EventType::SvnRepoManagerQuitError, command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerQuitResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type, QVariant()});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

Structs::SvnRepoContext *
SvnRepoManagerProcessingState::context(const QString &uuid) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  LOG_PERF(0, "Exit");

  return _contexts.value(uuid, nullptr);
}

void SvnRepoManagerProcessingState::destroyContext(const QString &uuid) {
  LOG_PERF(0, "Enter");

  SvnRepoContext *ctx = context(uuid);
  if (!ctx) {
    LOG_ERROR(Log::ErrorId::Git, "Internal error. Context is not initialized.");
    LOG_PERF(0, "Exit");
    return;
  }

  SvnRepoQuitCommand quitCommand;
  quitCommand.uuid = uuid;
  scheduleWorkerForQuit(quitCommand);
  ctx->workersThread->wait();

  LOCKGUARD(_contextsMutex);

  while (!ctx->commandsQueue.empty()) {
    const SvnRepoCommand *commandToDelete = ctx->commandsQueue.pop();
    delete commandToDelete;
  }

  delete ctx;
  _contexts.remove(uuid);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::destroyContextsMap() {
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

void SvnRepoManagerProcessingState::deleteBranchWorker(
    const Structs::SvnRepoDeleteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::deleteBranch(repo, command.branchName);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteBranchError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteBranchResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteRemoteBranchWorker(
    const Structs::SvnRepoDeleteRemoteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteRemoteBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success =
          SvnRepo::deleteRemoteBranch(repo, command.branchName);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteRemoteBranchError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteRemoteBranchResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteStashWorker(
    const Structs::SvnRepoDeleteStashCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteStashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::deleteStash(repo, command.index);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteStashError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteStashResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteAllStashesWorker(
    const Structs::SvnRepoDeleteAllStashesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteAllStashesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::deleteAllStashes(repo); !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteAllStashesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteAllStashesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteSubmoduleWorker(
    const Structs::SvnRepoDeleteSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteSubmoduleError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::deleteSubmodule(repo, command.submoduleName,
                                                command.submodulePath);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteSubmoduleError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteSubmoduleResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteTagWorker(
    const Structs::SvnRepoDeleteTagCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::deleteTag(repo, command.tagName);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteTagError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteTagResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::deleteRemoteTagWorker(
    const Structs::SvnRepoDeleteRemoteTagCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteRemoteTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::deleteRemoteTag(repo, command.tagName);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDeleteRemoteTagError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDeleteRemoteTagResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::diffFilesWorker(
    const Structs::SvnRepoDiffFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDiffFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const SvnRepo::FilesDiff filesDiff = SvnRepo::diffFiles(repo, command.files);

  if (!filesDiff.valid) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDiffFilesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    SvnRepoDiffFilesCommandResult commandResult;
    commandResult.diff = filesDiff.diff;
    commandResult.filesContent = filesDiff.filesContent;
    commandResult.repoContent = filesDiff.repoContent;
    commandResult.deltas = filesDiff.deltas;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDiffFilesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commandResult)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::diffFilesFromCommitWorker(
    const Structs::SvnRepoDiffFilesFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDiffFilesFromCommitError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const SvnRepo::FilesDiff filesDiff =
      SvnRepo::diffFilesFromCommit(repo, command.files, command.commit);

  if (!filesDiff.valid) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDiffFilesFromCommitError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    SvnRepoDiffFilesFromCommitCommandResult commandResult;
    commandResult.diff = filesDiff.diff;
    commandResult.filesContent = filesDiff.filesContent;
    commandResult.repoContent = filesDiff.repoContent;
    commandResult.deltas = filesDiff.deltas;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDiffFilesFromCommitResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commandResult)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::discardFilesWorker(
    const Structs::SvnRepoDiscardFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerDiscardFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const bool success = SvnRepo::discardFiles(repo, command.files);
      !success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerDiscardFilesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerDiscardFilesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::eventCallback(
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
      const SvnRepoContext *ctx = _contexts.value(uuid, nullptr);
      matches = ctx && !event.uuid().isEmpty() &&
                event.uuid() ==
                    (close ? ctx->closeSyncUuid : ctx->ownerSyncUuid);
    }
    if (matches) {
      destroyContext(uuid);
    }
  };

  switch (eventType) {
  case EventManager::EventType::SvnRepoManagerCloneRepoError:
  case EventManager::EventType::SvnRepoManagerInitRepoError:
  case EventManager::EventType::SvnRepoManagerOpenRepoError: {
    destroyIfAnswerOf(event.data<SvnRepoError>().uuid, false);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCloneRepoResult:
  case EventManager::EventType::SvnRepoManagerInitRepoResult: {
    destroyIfAnswerOf(event.data<SvnRepoResult>().uuid, false);
    break;
  }
  case EventManager::EventType::SvnRepoManagerCloseRepoResult: {
    destroyIfAnswerOf(event.data<SvnRepoResult>().uuid, true);
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::fetchAllWorker(
    const Structs::SvnRepoFetchAllCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerFetchAllError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::fetchAll(repo);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerFetchAllError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerFetchAllResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::fetchRemoteWorker(
    const Structs::SvnRepoFetchRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerFetchRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::fetchRemote(repo, command.remoteName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerFetchRemoteError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerFetchRemoteResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::rebaseWorker(
    const Structs::SvnRepoRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerRebaseError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::rebaseBranches(
      repo, command.sourceBranch, command.destinationBranch, command.author,
      command.email, command.signCommit);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerRebaseError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerRebaseResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForFetchAll(
    const Structs::SvnRepoFetchAllCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerFetchAllError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoFetchAllCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForFetchRemote(
    const Structs::SvnRepoFetchRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerFetchRemoteError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoFetchRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForRebase(
    const Structs::SvnRepoRebaseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerRebaseError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoRebaseCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getFileSizeFromCommitWorker(
    const Structs::SvnRepoGetFileSizeFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const qint64 fileSize =
      SvnRepo::getFileSizeFromCommit(repo, command.filePath, command.commit);

  if (fileSize < 0) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    SvnRepoGetFileSizeFromCommitCommandResult commandResult;
    commandResult.fileSize = fileSize;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commandResult)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getActiveBranchWorker(
    const Structs::SvnRepoGetActiveBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetActiveBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const QString activeBranch =
      SvnRepo::getActiveBranch(repo, command.branchType, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetActiveBranchError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    SvnRepoGetActiveBranchCommandResult result;
    result.branchName = activeBranch;
    result.branchType = command.branchType;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetActiveBranchResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getCommitBranchesWorker(
    const Structs::SvnRepoGetCommitBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitBranchesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const quint64 errorsBefore = SvnRepo::getLastError().serial;
  const QStringList branches =
      SvnRepo::getCommitBranches(repo, command.commitOid, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitBranchesError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (branches.isEmpty() &&
             SvnRepo::getLastError().serial != errorsBefore) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetCommitBranchesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitBranchesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(branches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getCommitHeadsWorker(
    const Structs::SvnRepoGetCommitHeadsCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitHeadsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const quint64 errorsBefore = SvnRepo::getLastError().serial;
  const QStringList branches =
      SvnRepo::getCommitHeads(repo, command.commitOid, *quit);
  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitHeadsError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (branches.isEmpty() &&
             SvnRepo::getLastError().serial != errorsBefore) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetCommitHeadsError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitHeadsResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(branches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getCommitIsHeadWorker(
    const Structs::SvnRepoGetCommitIsHeadCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitIsHeadError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const bool head = SvnRepo::getCommitIsHead(repo, command.commitOid, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitIsHeadError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitIsHeadResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(head)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getCommitStatusWorker(
    const Structs::SvnRepoGetCommitStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const auto commitStatus =
          SvnRepo::getCommitStatus(repo, command.commitOid);
      commitStatus.commit.isEmpty()) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetCommitStatusError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitStatusResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(commitStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getStashStatusWorker(
    const Structs::SvnRepoGetStashStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetStashStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  if (const auto stashStatus = SvnRepo::getStashStatus(repo, command.index);
      stashStatus.commit.isEmpty()) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetStashStatusError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetStashStatusResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(stashStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getIsMergeInProgressWorker(
    const Structs::SvnRepoGetIsMergeInProgressCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::SvnRepoManagerGetIsMergeInProgressError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const auto mergeInProgress = SvnRepo::getIsMergeInProgress(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgressResult,
      command.syncUuid);
  resultEvent.addData(
      SvnRepoResult{command.uuid, command.type, mergeInProgress});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getBranchHistoryWorker(
    const Structs::SvnRepoGetBranchHistoryCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetBranchHistoryError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *pQuit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    pQuit = &ctx->quit;
  }

  const SvnRepo::SvnHistory history = SvnRepo::getBranchHistory(
      repo, command.branchName, command.branchType, command.page, *pQuit);

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetBranchHistoryError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (!history.valid) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetBranchHistoryError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetBranchHistoryResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(history)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getSubmodulePathWorker(
    const Structs::SvnRepoGetSubmodulePathCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetSubmodulePathError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const auto path = SvnRepo::getSubmodulePath(repo, command.submoduleName);
  if (path.isEmpty()) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetSubmodulePathError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    SvnRepoGetSubmodulePathCommandResult result;
    result.submodulePath = path;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetSubmodulePathResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getUpstreamWorker(
    const Structs::SvnRepoGetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetUpstreamError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const auto &[success, remoteName, remoteBranchName] =
      SvnRepo::getUpstream(repo, command.localBranchName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetUpstreamError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    SvnRepoGetUpstreamCommandResult result;
    result.remoteName = remoteName;
    result.remoteBranchName = remoteBranchName;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetUpstreamResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::initRepoWorker(
    const Structs::SvnRepoInitRepoCommand &command) {
  LOG_PERF(0, "Enter");

  SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerInitRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Git, "Internal error. Context not initialized.");
    return;
  }

  const bool success = SvnRepo::initRepository(
      command.repoName, command.workingDir, command.defaultBranch,
      command.author, command.email);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerInitRepoError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    // The working copy is checked out into workingDir itself, see
    // SvnRepo::initRepository.
    const QString repoPath = command.workingDir;

    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerInitRepoResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type, repoPath});
    _eventManager.send(resultEvent);
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->SvnRepo.clear();
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listConflictsWorker(
    const Structs::SvnRepoListConflictsCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListConflictsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const QString fileContentWithMarkers =
      SvnRepo::listConflictsForAFile(repo, command.filePath);

  SvnRepoListConflictsCommandResult result;
  result.fileContentWithMarkers = fileContentWithMarkers;
  result.treeConflict = SvnRepo::isTreeConflict(repo, command.filePath);

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerListConflictsResult,
      command.syncUuid);
  resultEvent.addData(
      SvnRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listLocalBranchesWorker(
    const Structs::SvnRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListLocalBranchesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const QStringList localBranches = SvnRepo::listLocalBranches(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListLocalBranchesError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListLocalBranchesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(localBranches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listRemoteBranchesWorker(
    const Structs::SvnRepoListRemoteBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListRemoteBranchesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const QStringList remoteBranches = SvnRepo::listRemoteBranches(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListRemoteBranchesError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListRemoteBranchesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(remoteBranches)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listRemoteBranchesWithStatusWorker(
    const Structs::SvnRepoListRemoteBranchesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::
                    SvnRepoManagerListRemoteBranchesWithStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const QVector<SvnRepo::BranchInfo> remoteBranchesWithStatus =
      SvnRepo::listRemoteBranchesWithStatus(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::
            SvnRepoManagerListRemoteBranchesWithStatusError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::
            SvnRepoManagerListRemoteBranchesWithStatusResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type,
                      QVariant::fromValue(remoteBranchesWithStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listStashesWorker(
    const Structs::SvnRepoListStashesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListStashesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const QStringList stashes = SvnRepo::listStashes(repo, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListStashesError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListStashesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(stashes)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listSubmodulesWorker(
    const Structs::SvnRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListSubmodulesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const QStringList submodules = SvnRepo::listSubmodules(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerListSubmodulesResult,
      command.syncUuid);
  resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(submodules)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listSubmodulesWithStatusWorker(
    const Structs::SvnRepoListSubmodulesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusError,
        tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const QVector<SvnRepo::SubmoduleInfo> submodulesWithStatus =
      SvnRepo::listSubmodulesWithStatus(repo, *quit);

  if (*quit) {
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusError,
        command.syncUuid);
    errorEvent.addData(
        SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{
        command.uuid, command.type, QVariant::fromValue(submodulesWithStatus)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListSubmodulesWithStatus(
    const Structs::SvnRepoListSubmodulesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusError,
        tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(
      new SvnRepoListSubmodulesWithStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listTagsWorker(
    const Structs::SvnRepoListTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const QStringList tags = SvnRepo::listTags(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerListTagsResult, command.syncUuid);
  resultEvent.addData(
      SvnRepoResult{command.uuid, command.type, QVariant::fromValue(tags)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listRemoteTagsWorker(
    const Structs::SvnRepoListRemoteTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListRemoteTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const QStringList tags = SvnRepo::listRemoteTags(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerListRemoteTagsResult,
      command.syncUuid);
  resultEvent.addData(
      SvnRepoResult{command.uuid, command.type, QVariant::fromValue(tags)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::mergeWorker(
    const Structs::SvnRepoMergeCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerMergeError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool merged =
      SvnRepo::merge(repo, command.sourceBranch, command.destinationBranch,
                     command.author, command.email);

  if (merged) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerMergeResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type, merged});
    _eventManager.send(resultEvent);
  } else {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerMergeError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForOpenRepo(
    const SvnRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::SvnRepoManagerOpenRepoError,
                tr("The working copy has already been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc = std::bind(
      &SvnRepoManagerProcessingState::workersThreadFunc, this, command.uuid);

  auto newContext = new SvnRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->ownerSyncUuid = command.syncUuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new SvnRepoOpenCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForPull(
    const Structs::SvnRepoPullCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerPullError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoPullCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForPush(
    const Structs::SvnRepoPushCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerPushError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoPushCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForPushTag(
    const Structs::SvnRepoPushTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerPushTagError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoPushTagCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForPushAllTags(
    const Structs::SvnRepoPushAllTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerPushAllTagsError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoPushAllTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForRenameBranch(
    const Structs::SvnRepoRenameBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerRenameBranchError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoRenameBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForRenameTag(
    const Structs::SvnRepoRenameTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerRenameTagError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoRenameTagCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForReset(
    const Structs::SvnRepoResetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerResetError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoResetCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForRevert(
    const Structs::SvnRepoRevertCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerRevertError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoRevertCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForSetUpstream(
    const Structs::SvnRepoSetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerSetUpstreamError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoSetUpstreamCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForIgnoreFiles(
    const Structs::SvnRepoIgnoreFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerIgnoreFilesError,
                tr("Subversion repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoIgnoreFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForStageFiles(
    const Structs::SvnRepoStageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerStageFilesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoStageFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForStash(
    const Structs::SvnRepoStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerStashError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoStashCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForStatus(
    const Structs::SvnRepoStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerStatusError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForSubmoduleUpdate(
    const Structs::SvnRepoSubmoduleUpdateCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerSubmoduleUpdateError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoSubmoduleUpdateCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForUnstageFiles(
    const Structs::SvnRepoUnstageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerUnstageFilesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoUnstageFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListRemotes(
    const Structs::SvnRepoListRemotesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListRemotesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListRemotesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForAddRemote(
    const Structs::SvnRepoAddRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerAddRemoteError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoAddRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForRemoveRemote(
    const Structs::SvnRepoRemoveRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerRemoveRemoteError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoRemoveRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForUpdateRemote(
    const Structs::SvnRepoUpdateRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerUpdateRemoteError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoUpdateRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForSetDefaultRemote(
    const Structs::SvnRepoSetDefaultRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerSetDefaultRemoteError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoSetDefaultRemoteCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForAddTarget(
    const Structs::SvnRepoAddTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerAddTargetError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoAddTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForEditTarget(
    const Structs::SvnRepoEditTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerEditTargetError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoEditTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteTarget(
    const Structs::SvnRepoDeleteTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteTargetError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForSetDefaultTarget(
    const Structs::SvnRepoSetDefaultTargetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerSetDefaultTargetError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoSetDefaultTargetCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForQuit(
    const Structs::SvnRepoQuitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::SvnRepoManagerQuitError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoQuitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::openRepoWorker(
    const SvnRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  const QString repo = SvnRepo::openRepository(command.path);
  if (repo.isEmpty()) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerOpenRepoError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid,
                                    lastError.errorMessage.isEmpty()
                                        ? tr("Unable to open the working copy.")
                                        : lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
    LOG_PERF(0, "Exit");
    return;
  }

  SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    SvnRepo::closeRepository(repo);
    reportError(EventManager::EventType::SvnRepoManagerOpenRepoError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->SvnRepo = repo;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerOpenRepoError, command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerOpenRepoResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type, QVariant()});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::pullWorker(
    const Structs::SvnRepoPullCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerPullError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::pull(repo, command.activeBranchName, command.rebase);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerPullError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerPullResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::pushWorker(
    const Structs::SvnRepoPushCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerPushError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::push(repo, command.activeBranchName, command.force);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerPushError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerPushResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::pushTagWorker(
    const Structs::SvnRepoPushTagCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerPushTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::pushTag(repo, command.tagName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerPushTagError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerPushTagResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::pushAllTagsWorker(
    const Structs::SvnRepoPushAllTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerPushAllTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::pushAllTags(repo);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerPushAllTagsError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerPushAllTagsResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::renameBranchWorker(
    const Structs::SvnRepoRenameBranchCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerRenameBranchError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::renameBranch(repo, command.oldBranchName, command.newBranchName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerRenameBranchError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerRenameBranchResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::renameTagWorker(
    const Structs::SvnRepoRenameTagCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerRenameTagError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::renameTag(repo, command.oldTagName, command.newTagName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerRenameTagError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerRenameTagResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::resetWorker(
    const Structs::SvnRepoResetCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerResetError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::resetToCommit(repo, command.commit, command.resetType);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerResetError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerResetResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::revertWorker(
    const Structs::SvnRepoRevertCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerRevertError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::revertCommit(
      repo, command.commit, command.author, command.email, command.message);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerRevertError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerRevertResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::cherryPickWorker(
    const Structs::SvnRepoCherryPickCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerCherryPickError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::cherryPickCommit(repo, command.commit,
                                                 command.author, command.email);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerCherryPickError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerCherryPickResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCherryPick(
    const SvnRepoCherryPickCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerCherryPickError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCherryPickCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::setUpstreamWorker(
    const Structs::SvnRepoSetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerSetUpstreamError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::setUpstream(repo, command.remoteName, command.remoteBranchName,
                           command.localBranchName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerSetUpstreamError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerSetUpstreamResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::ignoreFilesWorker(
    const Structs::SvnRepoIgnoreFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerIgnoreFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::ignoreFiles(repo, command.files, command.recursive);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerIgnoreFilesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerIgnoreFilesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::stageFilesWorker(
    const Structs::SvnRepoStageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerStageFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::stageFiles(repo, command.files);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerStageFilesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerStageFilesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::stashWorker(
    const Structs::SvnRepoStashCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerStashError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::stash(repo, command.note);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerStashError, command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerStashResult, command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::statusWorker(
    const Structs::SvnRepoStatusCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerStatusError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  if (repo.isEmpty() || *quit) {
    reportError(EventManager::EventType::SvnRepoManagerStatusError,
                tr("The repository was closed."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const SvnRepo::SvnStatus status = SvnRepo::getStatus(repo, *quit);

  if (*quit) {
    reportError(EventManager::EventType::SvnRepoManagerStatusError,
                tr("The repository was closed."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::SvnRepoManagerStatusResult, command.syncUuid);
  resultEvent.addData(
      SvnRepoResult{command.uuid, command.type, QVariant::fromValue(status)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::submoduleUpdateWorker(
    const Structs::SvnRepoSubmoduleUpdateCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerSubmoduleUpdateError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::updateSubmodule(repo, command.submoduleName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerSubmoduleUpdateError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerSubmoduleUpdateResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::unstageFilesWorker(
    const Structs::SvnRepoUnstageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerUnstageFilesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::unstageFiles(repo, command.files);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerUnstageFilesError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerUnstageFilesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::listRemotesWorker(
    const Structs::SvnRepoListRemotesCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerListRemotesError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const auto remotes = SvnRepo::listRemotes(repo);
  {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerListRemotesResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(remotes)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::addRemoteWorker(
    const Structs::SvnRepoAddRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerAddRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::addRemote(repo, command.remoteName,
                                          command.fetchUrl, command.pushUrl);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerAddRemoteError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerAddRemoteResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::removeRemoteWorker(
    const Structs::SvnRepoRemoveRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerRemoveRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::removeRemote(repo, command.remoteName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerRemoveRemoteError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerRemoveRemoteResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::updateRemoteWorker(
    const Structs::SvnRepoUpdateRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerUpdateRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success =
      SvnRepo::updateRemote(repo, command.remoteName, command.newRemoteName,
                            command.fetchUrl, command.pushUrl);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerUpdateRemoteError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerUpdateRemoteResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::setDefaultRemoteWorker(
    const Structs::SvnRepoSetDefaultRemoteCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerSetDefaultRemoteError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
  }

  const bool success = SvnRepo::setDefaultRemote(repo, command.remoteName);
  if (!success) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerSetDefaultRemoteError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerSetDefaultRemoteResult,
        command.syncUuid);
    resultEvent.addData(SvnRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::reportError(
    const EventManager::EventType eventType, const QString &message,
    const Structs::SvnRepoCommand &command) const {
  LOG_PERF(0, "Enter");

  SvnRepoError error;
  error.error = -1;
  error.message = message;
  error.uuid = command.uuid;

  EventManager::Event errorEvent(eventType, command.syncUuid);
  errorEvent.addData(error);
  _eventManager.send(errorEvent);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForResolveTreeConflict(
    const Structs::SvnRepoResolveTreeConflictCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::SvnRepoManagerResolveTreeConflictError,
        tr("Subversion repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoResolveTreeConflictCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForAbortMerge(
    const Structs::SvnRepoAbortMergeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerAbortMergeError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoAbortMergeCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForAddSubmodule(
    const Structs::SvnRepoAddSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerAddSubmoduleError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoAddSubmoduleCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForBlame(
    const Structs::SvnRepoBlameCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerBlameError,
                tr("Subversion repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoBlameCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForApplyPatch(
    const Structs::SvnRepoApplyPatchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerApplyPatchError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoApplyPatchCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForApplyStash(
    const Structs::SvnRepoApplyStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerApplyStashError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoApplyStashCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCheckoutBranch(
    const Structs::SvnRepoCheckoutBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerCheckoutBranchError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCheckoutBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCheckoutCommit(
    const Structs::SvnRepoCheckoutCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerCheckoutCommitError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCheckoutCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCloneRepo(
    const Structs::SvnRepoCloneCommand &command) {
  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::SvnRepoManagerCloneRepoError,
                tr("The working copy is already in use."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc = std::bind(
      &SvnRepoManagerProcessingState::workersThreadFunc, this, command.uuid);

  const auto newContext = new SvnRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->ownerSyncUuid = command.syncUuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new SvnRepoCloneCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCloseRepo(
    const SvnRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerCloseRepoError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->quit = true;
  (*it)->closeSyncUuid = command.syncUuid;
  (*it)->commandsQueue.push(new SvnRepoCloseCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCommitStagedRepo(
    const Structs::SvnRepoCommitStagedCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerCommitStagedError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCommitStagedCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCreateBranchFromCommit(
    const Structs::SvnRepoCreateBranchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::SvnRepoManagerCreateBranchFromCommitError,
        tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCreateBranchFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCreatePatchFromCommit(
    const Structs::SvnRepoCreatePatchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::SvnRepoManagerCreatePatchFromCommitError,
        tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCreatePatchFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForCreateTagFromCommit(
    const Structs::SvnRepoCreateTagFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerCreateTagFromCommitError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoCreateTagFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetActiveBranch(
    const Structs::SvnRepoGetActiveBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetActiveBranchError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetActiveBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetCommitBranches(
    const Structs::SvnRepoGetCommitBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitBranchesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetCommitBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::getCommitTagsWorker(
    const Structs::SvnRepoGetCommitTagsCommand &command) {
  LOG_PERF(0, "Enter");

  const SvnRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitTagsError,
                tr("Internal error. Context not initialized."), command);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString repo;
  const bool *quit = nullptr;

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->SvnRepo;
    quit = &ctx->quit;
  }

  const quint64 errorsBefore = SvnRepo::getLastError().serial;
  const QStringList tags =
      SvnRepo::getCommitTags(repo, command.commitOid, *quit);

  if (*quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitTagsError,
        command.syncUuid);
    resultEvent.addData(SvnRepoError{command.uuid, tr("The repository was closed."), -1});
    _eventManager.send(resultEvent);
  } else if (tags.isEmpty() &&
             SvnRepo::getLastError().serial != errorsBefore) {
    const auto lastError = SvnRepo::getLastError();
    EventManager::Event errorEvent(
        EventManager::EventType::SvnRepoManagerGetCommitTagsError,
        command.syncUuid);
    errorEvent.addData(SvnRepoError{command.uuid, lastError.errorMessage,
                                    lastError.errorCode, lastError.error});
    _eventManager.send(errorEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::SvnRepoManagerGetCommitTagsResult,
        command.syncUuid);
    resultEvent.addData(
        SvnRepoResult{command.uuid, command.type, QVariant::fromValue(tags)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetCommitTags(
    const Structs::SvnRepoGetCommitTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitTagsError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetCommitTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetCommitHeads(
    const Structs::SvnRepoGetCommitHeadsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitHeadsError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetCommitHeadsCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetCommitIsHead(
    const Structs::SvnRepoGetCommitIsHeadCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitIsHeadError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetCommitIsHeadCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetCommitStatus(
    const Structs::SvnRepoGetCommitStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetCommitStatusError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetCommitStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetStashStatus(
    const Structs::SvnRepoGetStashStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetStashStatusError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetStashStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetIsMergeInProgress(
    const Structs::SvnRepoGetIsMergeInProgressCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::SvnRepoManagerGetIsMergeInProgressError,
        tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetIsMergeInProgressCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetSubmodulePath(
    const Structs::SvnRepoGetSubmodulePathCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetSubmodulePathError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetSubmodulePathCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetUpstream(
    const Structs::SvnRepoGetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetUpstreamError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetUpstreamCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteAllStashes(
    const Structs::SvnRepoDeleteAllStashesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteAllStashesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteAllStashesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteBranch(
    const Structs::SvnRepoDeleteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteBranchError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteRemoteBranch(
    const Structs::SvnRepoDeleteRemoteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteRemoteBranchError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteRemoteBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteStash(
    const Structs::SvnRepoDeleteStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteStashError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteStashCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteSubmodule(
    const Structs::SvnRepoDeleteSubmoduleCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteSubmoduleError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteSubmoduleCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteTag(
    const Structs::SvnRepoDeleteTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteTagError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteTagCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDeleteRemoteTag(
    const Structs::SvnRepoDeleteRemoteTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDeleteRemoteTagError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDeleteRemoteTagCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDiffFiles(
    const Structs::SvnRepoDiffFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDiffFilesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDiffFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDiffFilesFromCommit(
    const Structs::SvnRepoDiffFilesFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDiffFilesFromCommitError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDiffFilesFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetFileSizeFromCommit(
    const Structs::SvnRepoGetFileSizeFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(
        EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitError,
        tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetFileSizeFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForDiscardFiles(
    const Structs::SvnRepoDiscardFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerDiscardFilesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoDiscardFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForGetBranchHistory(
    const Structs::SvnRepoGetBranchHistoryCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerGetBranchHistoryError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoGetBranchHistoryCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForInitRepo(
    const Structs::SvnRepoInitRepoCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::SvnRepoManagerInitRepoError,
                tr("The working copy is already in use."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc = std::bind(
      &SvnRepoManagerProcessingState::workersThreadFunc, this, command.uuid);

  auto *newContext = new SvnRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->ownerSyncUuid = command.syncUuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new SvnRepoInitRepoCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListConflicts(
    const Structs::SvnRepoListConflictsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListConflictsError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListConflictsCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListLocalBranches(
    const Structs::SvnRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListLocalBranchesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListLocalBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListRemoteBranches(
    const Structs::SvnRepoListRemoteBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListRemoteBranchesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListRemoteBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::
    scheduleWorkerForListRemoteBranchesWithStatus(
        const Structs::SvnRepoListRemoteBranchesWithStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::
                    SvnRepoManagerListRemoteBranchesWithStatusError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(
      new SvnRepoListRemoteBranchesWithStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListStashes(
    const Structs::SvnRepoListStashesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListStashesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListStashesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListSubmodules(
    const Structs::SvnRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListSubmodulesError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListSubmodulesCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListTags(
    const Structs::SvnRepoListTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListTagsError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForListRemoteTags(
    const Structs::SvnRepoListRemoteTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerListRemoteTagsError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoListRemoteTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerProcessingState::scheduleWorkerForMerge(
    const Structs::SvnRepoMergeCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd() || (*it)->quit) {
    reportError(EventManager::EventType::SvnRepoManagerMergeError,
                tr("The working copy has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new SvnRepoMergeCommand(command));

  LOG_PERF(0, "Exit");
}

// The error event of a command, for answering it without running it.
static EventManager::EventType errorEventFor(const SvnRepoCommandType type) {
  switch (type) {
  case SvnRepoCommandType::AbortMerge:
    return EventManager::EventType::SvnRepoManagerAbortMergeError;
  case SvnRepoCommandType::ResolveTreeConflict:
    return EventManager::EventType::SvnRepoManagerResolveTreeConflictError;
  case SvnRepoCommandType::AddSubmodule:
    return EventManager::EventType::SvnRepoManagerAddSubmoduleError;
  case SvnRepoCommandType::AddRemote:
    return EventManager::EventType::SvnRepoManagerAddRemoteError;
  case SvnRepoCommandType::Blame:
    return EventManager::EventType::SvnRepoManagerBlameError;
  case SvnRepoCommandType::ApplyPatch:
    return EventManager::EventType::SvnRepoManagerApplyPatchError;
  case SvnRepoCommandType::ApplyStash:
    return EventManager::EventType::SvnRepoManagerApplyStashError;
  case SvnRepoCommandType::CheckoutBranch:
    return EventManager::EventType::SvnRepoManagerCheckoutBranchError;
  case SvnRepoCommandType::CheckoutCommit:
    return EventManager::EventType::SvnRepoManagerCheckoutCommitError;
  case SvnRepoCommandType::Clone:
    return EventManager::EventType::SvnRepoManagerCloneRepoError;
  case SvnRepoCommandType::Close:
    return EventManager::EventType::SvnRepoManagerCloseRepoError;
  case SvnRepoCommandType::CommitStaged:
    return EventManager::EventType::SvnRepoManagerCommitStagedError;
  case SvnRepoCommandType::CreateBranchFromCommit:
    return EventManager::EventType::SvnRepoManagerCreateBranchFromCommitError;
  case SvnRepoCommandType::CreatePatchFromCommit:
    return EventManager::EventType::SvnRepoManagerCreatePatchFromCommitError;
  case SvnRepoCommandType::CreateTagFromCommit:
    return EventManager::EventType::SvnRepoManagerCreateTagFromCommitError;
  case SvnRepoCommandType::DeleteAllStashes:
    return EventManager::EventType::SvnRepoManagerDeleteAllStashesError;
  case SvnRepoCommandType::DeleteBranch:
    return EventManager::EventType::SvnRepoManagerDeleteBranchError;
  case SvnRepoCommandType::DeleteRemoteBranch:
    return EventManager::EventType::SvnRepoManagerDeleteRemoteBranchError;
  case SvnRepoCommandType::DeleteStash:
    return EventManager::EventType::SvnRepoManagerDeleteStashError;
  case SvnRepoCommandType::DeleteSubmodule:
    return EventManager::EventType::SvnRepoManagerDeleteSubmoduleError;
  case SvnRepoCommandType::DeleteTag:
    return EventManager::EventType::SvnRepoManagerDeleteTagError;
  case SvnRepoCommandType::DeleteRemoteTag:
    return EventManager::EventType::SvnRepoManagerDeleteRemoteTagError;
  case SvnRepoCommandType::DiffFiles:
    return EventManager::EventType::SvnRepoManagerDiffFilesError;
  case SvnRepoCommandType::DiffFilesFromCommit:
    return EventManager::EventType::SvnRepoManagerDiffFilesFromCommitError;
  case SvnRepoCommandType::DiscardFiles:
    return EventManager::EventType::SvnRepoManagerDiscardFilesError;
  case SvnRepoCommandType::GetFileSizeFromCommit:
    return EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitError;
  case SvnRepoCommandType::GetActiveBranch:
    return EventManager::EventType::SvnRepoManagerGetActiveBranchError;
  case SvnRepoCommandType::GetBranchHistory:
    return EventManager::EventType::SvnRepoManagerGetBranchHistoryError;
  case SvnRepoCommandType::GetCommitBranches:
    return EventManager::EventType::SvnRepoManagerGetCommitBranchesError;
  case SvnRepoCommandType::GetCommitHeads:
    return EventManager::EventType::SvnRepoManagerGetCommitHeadsError;
  case SvnRepoCommandType::GetCommitIsHead:
    return EventManager::EventType::SvnRepoManagerGetCommitIsHeadError;
  case SvnRepoCommandType::GetCommitStatus:
    return EventManager::EventType::SvnRepoManagerGetCommitStatusError;
  case SvnRepoCommandType::GetCommitTags:
    return EventManager::EventType::SvnRepoManagerGetCommitTagsError;
  case SvnRepoCommandType::GetIsMergeInProgress:
    return EventManager::EventType::SvnRepoManagerGetIsMergeInProgressError;
  case SvnRepoCommandType::GetStashStatus:
    return EventManager::EventType::SvnRepoManagerGetStashStatusError;
  case SvnRepoCommandType::GetSubmodulePath:
    return EventManager::EventType::SvnRepoManagerGetSubmodulePathError;
  case SvnRepoCommandType::GetUpstream:
    return EventManager::EventType::SvnRepoManagerGetUpstreamError;
  case SvnRepoCommandType::Init:
    return EventManager::EventType::SvnRepoManagerInitRepoError;
  case SvnRepoCommandType::ListConflicts:
    return EventManager::EventType::SvnRepoManagerListConflictsError;
  case SvnRepoCommandType::ListLocalBranches:
    return EventManager::EventType::SvnRepoManagerListLocalBranchesError;
  case SvnRepoCommandType::ListRemoteBranches:
    return EventManager::EventType::SvnRepoManagerListRemoteBranchesError;
  case SvnRepoCommandType::ListRemotes:
    return EventManager::EventType::SvnRepoManagerListRemotesError;
  case SvnRepoCommandType::ListStashes:
    return EventManager::EventType::SvnRepoManagerListStashesError;
  case SvnRepoCommandType::ListSubmodules:
    return EventManager::EventType::SvnRepoManagerListSubmodulesError;
  case SvnRepoCommandType::ListSubmodulesWithStatus:
    return EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusError;
  case SvnRepoCommandType::ListTags:
    return EventManager::EventType::SvnRepoManagerListTagsError;
  case SvnRepoCommandType::ListRemoteTags:
    return EventManager::EventType::SvnRepoManagerListRemoteTagsError;
  case SvnRepoCommandType::Merge:
    return EventManager::EventType::SvnRepoManagerMergeError;
  case SvnRepoCommandType::Rebase:
    return EventManager::EventType::SvnRepoManagerRebaseError;
  case SvnRepoCommandType::Open:
    return EventManager::EventType::SvnRepoManagerOpenRepoError;
  case SvnRepoCommandType::Pull:
    return EventManager::EventType::SvnRepoManagerPullError;
  case SvnRepoCommandType::Push:
    return EventManager::EventType::SvnRepoManagerPushError;
  case SvnRepoCommandType::FetchAll:
    return EventManager::EventType::SvnRepoManagerFetchAllError;
  case SvnRepoCommandType::FetchRemote:
    return EventManager::EventType::SvnRepoManagerFetchRemoteError;
  case SvnRepoCommandType::PushTag:
    return EventManager::EventType::SvnRepoManagerPushTagError;
  case SvnRepoCommandType::PushAllTags:
    return EventManager::EventType::SvnRepoManagerPushAllTagsError;
  case SvnRepoCommandType::RemoveRemote:
    return EventManager::EventType::SvnRepoManagerRemoveRemoteError;
  case SvnRepoCommandType::RenameBranch:
    return EventManager::EventType::SvnRepoManagerRenameBranchError;
  case SvnRepoCommandType::RenameTag:
    return EventManager::EventType::SvnRepoManagerRenameTagError;
  case SvnRepoCommandType::Reset:
    return EventManager::EventType::SvnRepoManagerResetError;
  case SvnRepoCommandType::Revert:
    return EventManager::EventType::SvnRepoManagerRevertError;
  case SvnRepoCommandType::CherryPick:
    return EventManager::EventType::SvnRepoManagerCherryPickError;
  case SvnRepoCommandType::SetDefaultRemote:
    return EventManager::EventType::SvnRepoManagerSetDefaultRemoteError;
  case SvnRepoCommandType::AddTarget:
    return EventManager::EventType::SvnRepoManagerAddTargetError;
  case SvnRepoCommandType::EditTarget:
    return EventManager::EventType::SvnRepoManagerEditTargetError;
  case SvnRepoCommandType::DeleteTarget:
    return EventManager::EventType::SvnRepoManagerDeleteTargetError;
  case SvnRepoCommandType::SetDefaultTarget:
    return EventManager::EventType::SvnRepoManagerSetDefaultTargetError;
  case SvnRepoCommandType::SetUpstream:
    return EventManager::EventType::SvnRepoManagerSetUpstreamError;
  case SvnRepoCommandType::IgnoreFiles:
    return EventManager::EventType::SvnRepoManagerIgnoreFilesError;
  case SvnRepoCommandType::StageFiles:
    return EventManager::EventType::SvnRepoManagerStageFilesError;
  case SvnRepoCommandType::Stash:
    return EventManager::EventType::SvnRepoManagerStashError;
  case SvnRepoCommandType::Status:
    return EventManager::EventType::SvnRepoManagerStatusError;
  case SvnRepoCommandType::SubmoduleUpdate:
    return EventManager::EventType::SvnRepoManagerSubmoduleUpdateError;
  case SvnRepoCommandType::UnstageFiles:
    return EventManager::EventType::SvnRepoManagerUnstageFilesError;
  case SvnRepoCommandType::UpdateRemote:
    return EventManager::EventType::SvnRepoManagerUpdateRemoteError;
  case SvnRepoCommandType::Quit:
    return EventManager::EventType::SvnRepoManagerQuitError;
  default:
    return EventManager::EventType::Null;
  }
}

void SvnRepoManagerProcessingState::workersThreadFunc(const QString &uuid) {
  LOG_PERF(0, "Enter");

  SvnRepo::initForCurrentThread();

  bool quit = false;

  while (!quit) {
    SvnRepoContext *ctx = context(uuid);
    if (!ctx)
      break;

    SvnRepoCommand *command = ctx->commandsQueue.pop();
    if (!command) {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Enqueued command is null.");
      continue;
    }

    // Commands queued behind an open that failed or behind the close find
    // no working copy. They are answered without running: an empty path
    // would be the process's current directory.
    bool hasRepository = false;
    {
      LOCKGUARD(_contextsMutex);
      hasRepository = !ctx->SvnRepo.isEmpty();
    }
    const bool needsRepository =
        command->type != SvnRepoCommandType::Open &&
        command->type != SvnRepoCommandType::Clone &&
        command->type != SvnRepoCommandType::Init &&
        command->type != SvnRepoCommandType::Close &&
        command->type != SvnRepoCommandType::Quit;
    if (needsRepository && !hasRepository) {
      const EventManager::EventType errorEvent = errorEventFor(command->type);
      if (errorEvent != EventManager::EventType::Null) {
        reportError(errorEvent, tr("The working copy has not been opened."),
                    *command);
      }
      delete command;
      continue;
    }

    switch (command->type) {
    case SvnRepoCommandType::AbortMerge: {
      const auto abortMergeCommand =
          dynamic_cast<const SvnRepoAbortMergeCommand *>(command);
      if (abortMergeCommand) {
        abortMergeWorker(*abortMergeCommand);
      }
      break;
    }
    case SvnRepoCommandType::ResolveTreeConflict: {
      const auto resolveCommand =
          dynamic_cast<const SvnRepoResolveTreeConflictCommand *>(command);
      if (resolveCommand) {
        resolveTreeConflictWorker(*resolveCommand);
      }
      break;
    }
    case SvnRepoCommandType::AddSubmodule: {
      const auto addSubmoduleCommand =
          dynamic_cast<const SvnRepoAddSubmoduleCommand *>(command);
      if (addSubmoduleCommand) {
        addSubmoduleWorker(*addSubmoduleCommand);
      }
      break;
    }
    case SvnRepoCommandType::AddRemote: {
      const auto addRemoteCommand =
          dynamic_cast<const SvnRepoAddRemoteCommand *>(command);
      if (addRemoteCommand) {
        addRemoteWorker(*addRemoteCommand);
      }
      break;
    }
    case SvnRepoCommandType::Blame: {
      const auto blameCommand =
          dynamic_cast<const SvnRepoBlameCommand *>(command);
      if (blameCommand) {
        blameWorker(*blameCommand);
      }
      break;
    }
    case SvnRepoCommandType::ApplyPatch: {
      const auto applyPatchCommand =
          dynamic_cast<const SvnRepoApplyPatchCommand *>(command);
      if (applyPatchCommand) {
        applyPatchWorker(*applyPatchCommand);
      }
      break;
    }
    case SvnRepoCommandType::ApplyStash: {
      const auto applyStashCommand =
          dynamic_cast<const SvnRepoApplyStashCommand *>(command);
      if (applyStashCommand) {
        applyStashWorker(*applyStashCommand);
      }
      break;
    }
    case SvnRepoCommandType::CheckoutBranch: {
      const auto checkoutCommand =
          dynamic_cast<const SvnRepoCheckoutBranchCommand *>(command);
      if (checkoutCommand) {
        checkoutBranchWorker(*checkoutCommand);
      }
      break;
    }
    case SvnRepoCommandType::CheckoutCommit: {
      const auto checkoutCommand =
          dynamic_cast<const SvnRepoCheckoutCommitCommand *>(command);
      if (checkoutCommand) {
        checkoutCommitWorker(*checkoutCommand);
      }
      break;
    }
    case SvnRepoCommandType::Clone: {
      const auto cloneCommand =
          dynamic_cast<const SvnRepoCloneCommand *>(command);
      if (cloneCommand) {
        cloneRepoWorker(*cloneCommand);
      }
      break;
    }
    case SvnRepoCommandType::Close: {
      const auto closeCommand =
          dynamic_cast<const SvnRepoCloseCommand *>(command);
      if (closeCommand) {
        closeRepoWorker(*closeCommand);
      }
      break;
    }
    case SvnRepoCommandType::CommitStaged: {
      const auto commitStagedCommand =
          dynamic_cast<const SvnRepoCommitStagedCommand *>(command);
      if (commitStagedCommand) {
        commitStagedWorker(*commitStagedCommand);
      }
      break;
    }
    case SvnRepoCommandType::CreateBranchFromCommit: {
      const auto createBranchFromCommitCommand =
          dynamic_cast<const SvnRepoCreateBranchFromCommitCommand *>(command);
      if (createBranchFromCommitCommand) {
        createBranchFromCommitWorker(*createBranchFromCommitCommand);
      }
      break;
    }
    case SvnRepoCommandType::CreatePatchFromCommit: {
      const auto createPatchFromCommitCommand =
          dynamic_cast<const SvnRepoCreatePatchFromCommitCommand *>(command);
      if (createPatchFromCommitCommand) {
        createPatchFromCommitWorker(*createPatchFromCommitCommand);
      }
      break;
    }
    case SvnRepoCommandType::CreateTagFromCommit: {
      const auto createTagFromCommitCommand =
          dynamic_cast<const SvnRepoCreateTagFromCommitCommand *>(command);
      if (createTagFromCommitCommand) {
        createTagFromCommitWorker(*createTagFromCommitCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteAllStashes: {
      const auto deleteAllStashesCommand =
          dynamic_cast<const SvnRepoDeleteAllStashesCommand *>(command);
      if (deleteAllStashesCommand) {
        deleteAllStashesWorker(*deleteAllStashesCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteBranch: {
      const auto deleteBranchCommand =
          dynamic_cast<const SvnRepoDeleteBranchCommand *>(command);
      if (deleteBranchCommand) {
        deleteBranchWorker(*deleteBranchCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteRemoteBranch: {
      const auto deleteRemoteBranchCommand =
          dynamic_cast<const SvnRepoDeleteRemoteBranchCommand *>(command);
      if (deleteRemoteBranchCommand) {
        deleteRemoteBranchWorker(*deleteRemoteBranchCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteStash: {
      const auto deleteStashCommand =
          dynamic_cast<const SvnRepoDeleteStashCommand *>(command);
      if (deleteStashCommand) {
        deleteStashWorker(*deleteStashCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteSubmodule: {
      const auto deleteSubmoduleCommand =
          dynamic_cast<const SvnRepoDeleteSubmoduleCommand *>(command);
      if (deleteSubmoduleCommand) {
        deleteSubmoduleWorker(*deleteSubmoduleCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteTag: {
      const auto deleteTagCommand =
          dynamic_cast<const SvnRepoDeleteTagCommand *>(command);
      if (deleteTagCommand) {
        deleteTagWorker(*deleteTagCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteRemoteTag: {
      const auto deleteRemoteTagCommand =
          dynamic_cast<const SvnRepoDeleteRemoteTagCommand *>(command);
      if (deleteRemoteTagCommand) {
        deleteRemoteTagWorker(*deleteRemoteTagCommand);
      }
      break;
    }
    case SvnRepoCommandType::DiffFiles: {
      const auto diffFilesCommand =
          dynamic_cast<const SvnRepoDiffFilesCommand *>(command);
      if (diffFilesCommand) {
        diffFilesWorker(*diffFilesCommand);
      }
      break;
    }
    case SvnRepoCommandType::DiffFilesFromCommit: {
      const auto diffFilesFromCommitCommand =
          dynamic_cast<const SvnRepoDiffFilesFromCommitCommand *>(command);
      if (diffFilesFromCommitCommand) {
        diffFilesFromCommitWorker(*diffFilesFromCommitCommand);
      }
      break;
    }
    case SvnRepoCommandType::DiscardFiles: {
      const auto discardFilesCommand =
          dynamic_cast<const SvnRepoDiscardFilesCommand *>(command);
      if (discardFilesCommand) {
        discardFilesWorker(*discardFilesCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetFileSizeFromCommit: {
      const auto getFileSizeFromCommitCommand =
          dynamic_cast<const SvnRepoGetFileSizeFromCommitCommand *>(command);
      if (getFileSizeFromCommitCommand) {
        getFileSizeFromCommitWorker(*getFileSizeFromCommitCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetActiveBranch: {
      const auto getActiveBranchCommand =
          dynamic_cast<const SvnRepoGetActiveBranchCommand *>(command);
      if (getActiveBranchCommand) {
        getActiveBranchWorker(*getActiveBranchCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetBranchHistory: {
      const auto *branchHistoryCommand =
          dynamic_cast<const SvnRepoGetBranchHistoryCommand *>(command);
      if (branchHistoryCommand) {
        getBranchHistoryWorker(*branchHistoryCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetCommitBranches: {
      const auto *getCommitBranchesCommand =
          dynamic_cast<const SvnRepoGetCommitBranchesCommand *>(command);
      if (getCommitBranchesCommand) {
        getCommitBranchesWorker(*getCommitBranchesCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetCommitHeads: {
      const auto *getCommitHeadsCommand =
          dynamic_cast<const SvnRepoGetCommitHeadsCommand *>(command);
      if (getCommitHeadsCommand) {
        getCommitHeadsWorker(*getCommitHeadsCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetCommitIsHead: {
      const auto *getCommitIsHeadCommand =
          dynamic_cast<const SvnRepoGetCommitIsHeadCommand *>(command);
      if (getCommitIsHeadCommand) {
        getCommitIsHeadWorker(*getCommitIsHeadCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetCommitStatus: {
      const auto *getCommitStatusCommand =
          dynamic_cast<const SvnRepoGetCommitStatusCommand *>(command);
      if (getCommitStatusCommand) {
        getCommitStatusWorker(*getCommitStatusCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetCommitTags: {
      const auto *getCommitTagsCommand =
          dynamic_cast<const SvnRepoGetCommitTagsCommand *>(command);
      if (getCommitTagsCommand) {
        getCommitTagsWorker(*getCommitTagsCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetIsMergeInProgress: {
      const auto *getIsMergeInProgressCommand =
          dynamic_cast<const SvnRepoGetIsMergeInProgressCommand *>(command);
      if (getIsMergeInProgressCommand) {
        getIsMergeInProgressWorker(*getIsMergeInProgressCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetStashStatus: {
      const auto *getStashStatusCommand =
          dynamic_cast<const SvnRepoGetStashStatusCommand *>(command);
      if (getStashStatusCommand) {
        getStashStatusWorker(*getStashStatusCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetSubmodulePath: {
      const auto *getSubmodulePathCommand =
          dynamic_cast<const SvnRepoGetSubmodulePathCommand *>(command);
      if (getSubmodulePathCommand) {
        getSubmodulePathWorker(*getSubmodulePathCommand);
      }
      break;
    }
    case SvnRepoCommandType::GetUpstream: {
      const auto *getUpstreamCommand =
          dynamic_cast<const SvnRepoGetUpstreamCommand *>(command);
      if (getUpstreamCommand) {
        getUpstreamWorker(*getUpstreamCommand);
      }
      break;
    }
    case SvnRepoCommandType::Init: {
      const auto *initRepoCommand =
          dynamic_cast<const SvnRepoInitRepoCommand *>(command);
      if (initRepoCommand) {
        initRepoWorker(*initRepoCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListConflicts: {
      const auto *listConflictsCommand =
          dynamic_cast<const SvnRepoListConflictsCommand *>(command);
      if (listConflictsCommand) {
        listConflictsWorker(*listConflictsCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListLocalBranches: {
      const auto *listLocalCommand =
          dynamic_cast<const SvnRepoListLocalBranchesCommand *>(command);
      if (listLocalCommand) {
        listLocalBranchesWorker(*listLocalCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListRemoteBranches: {
      const auto *listRemoteCommand =
          dynamic_cast<const SvnRepoListRemoteBranchesCommand *>(command);
      if (listRemoteCommand) {
        listRemoteBranchesWorker(*listRemoteCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListRemotes: {
      const auto *listRemotesCommand =
          dynamic_cast<const SvnRepoListRemotesCommand *>(command);
      if (listRemotesCommand) {
        listRemotesWorker(*listRemotesCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListRemoteBranchesWithStatus: {
      const auto *listRemoteStatusCommand =
          dynamic_cast<const SvnRepoListRemoteBranchesWithStatusCommand *>(
              command);
      if (listRemoteStatusCommand) {
        listRemoteBranchesWithStatusWorker(*listRemoteStatusCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListStashes: {
      const auto *listStashesCommand =
          dynamic_cast<const SvnRepoListStashesCommand *>(command);
      if (listStashesCommand) {
        listStashesWorker(*listStashesCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListSubmodules: {
      const auto *listSubmodulesCommand =
          dynamic_cast<const SvnRepoListSubmodulesCommand *>(command);
      if (listSubmodulesCommand) {
        listSubmodulesWorker(*listSubmodulesCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListSubmodulesWithStatus: {
      const auto *listSubmodulesWithStatusCommand =
          dynamic_cast<const SvnRepoListSubmodulesWithStatusCommand *>(command);
      if (listSubmodulesWithStatusCommand) {
        listSubmodulesWithStatusWorker(*listSubmodulesWithStatusCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListTags: {
      const auto *listTagsCommand =
          dynamic_cast<const SvnRepoListTagsCommand *>(command);
      if (listTagsCommand) {
        listTagsWorker(*listTagsCommand);
      }
      break;
    }
    case SvnRepoCommandType::ListRemoteTags: {
      const auto *listRemoteTagsCommand =
          dynamic_cast<const SvnRepoListRemoteTagsCommand *>(command);
      if (listRemoteTagsCommand) {
        listRemoteTagsWorker(*listRemoteTagsCommand);
      }
      break;
    }
    case SvnRepoCommandType::Merge: {
      const auto *mergeCommand =
          dynamic_cast<const SvnRepoMergeCommand *>(command);
      if (mergeCommand) {
        mergeWorker(*mergeCommand);
      }
      break;
    }
    case SvnRepoCommandType::Rebase: {
      const auto *rebaseCommand =
          dynamic_cast<const SvnRepoRebaseCommand *>(command);
      if (rebaseCommand) {
        rebaseWorker(*rebaseCommand);
      }
      break;
    }
    case SvnRepoCommandType::Open: {
      const auto *openCommand =
          dynamic_cast<const SvnRepoOpenCommand *>(command);
      if (openCommand) {
        openRepoWorker(*openCommand);
      }
      break;
    }
    case SvnRepoCommandType::Pull: {
      const auto *pullCommand =
          dynamic_cast<const SvnRepoPullCommand *>(command);
      if (pullCommand) {
        pullWorker(*pullCommand);
      }
      break;
    }
    case SvnRepoCommandType::Push: {
      const auto *pushCommand =
          dynamic_cast<const SvnRepoPushCommand *>(command);
      if (pushCommand) {
        pushWorker(*pushCommand);
      }
      break;
    }
    case SvnRepoCommandType::FetchAll: {
      const auto *fetchAllCommand =
          dynamic_cast<const SvnRepoFetchAllCommand *>(command);
      if (fetchAllCommand) {
        fetchAllWorker(*fetchAllCommand);
      }
      break;
    }
    case SvnRepoCommandType::FetchRemote: {
      const auto *fetchRemoteCommand =
          dynamic_cast<const SvnRepoFetchRemoteCommand *>(command);
      if (fetchRemoteCommand) {
        fetchRemoteWorker(*fetchRemoteCommand);
      }
      break;
    }
    case SvnRepoCommandType::PushTag: {
      const auto *pushTagCommand =
          dynamic_cast<const SvnRepoPushTagCommand *>(command);
      if (pushTagCommand) {
        pushTagWorker(*pushTagCommand);
      }
      break;
    }
    case SvnRepoCommandType::PushAllTags: {
      const auto *pushAllTagsCommand =
          dynamic_cast<const SvnRepoPushAllTagsCommand *>(command);
      if (pushAllTagsCommand) {
        pushAllTagsWorker(*pushAllTagsCommand);
      }
      break;
    }
    case SvnRepoCommandType::RemoveRemote: {
      const auto *removeRemoteCommand =
          dynamic_cast<const SvnRepoRemoveRemoteCommand *>(command);
      if (removeRemoteCommand) {
        removeRemoteWorker(*removeRemoteCommand);
      }
      break;
    }
    case SvnRepoCommandType::RenameBranch: {
      const auto *renameBranchCommand =
          dynamic_cast<const SvnRepoRenameBranchCommand *>(command);
      if (renameBranchCommand) {
        renameBranchWorker(*renameBranchCommand);
      }
      break;
    }
    case SvnRepoCommandType::RenameTag: {
      const auto *renameTagCommand =
          dynamic_cast<const SvnRepoRenameTagCommand *>(command);
      if (renameTagCommand) {
        renameTagWorker(*renameTagCommand);
      }
      break;
    }
    case SvnRepoCommandType::Reset: {
      const auto *resetCommand =
          dynamic_cast<const SvnRepoResetCommand *>(command);
      if (resetCommand) {
        resetWorker(*resetCommand);
      }
      break;
    }
    case SvnRepoCommandType::Revert: {
      const auto *revertCommand =
          dynamic_cast<const SvnRepoRevertCommand *>(command);
      if (revertCommand) {
        revertWorker(*revertCommand);
      }
      break;
    }
    case SvnRepoCommandType::CherryPick: {
      const auto *cherryPickCommand =
          dynamic_cast<const SvnRepoCherryPickCommand *>(command);
      if (cherryPickCommand) {
        cherryPickWorker(*cherryPickCommand);
      }
      break;
    }
    case SvnRepoCommandType::SetDefaultRemote: {
      const auto *setDefaultRemoteCommand =
          dynamic_cast<const SvnRepoSetDefaultRemoteCommand *>(command);
      if (setDefaultRemoteCommand) {
        setDefaultRemoteWorker(*setDefaultRemoteCommand);
      }
      break;
    }
    case SvnRepoCommandType::AddTarget: {
      const auto *addTargetCommand =
          dynamic_cast<const SvnRepoAddTargetCommand *>(command);
      if (addTargetCommand) {
        addTargetWorker(*addTargetCommand);
      }
      break;
    }
    case SvnRepoCommandType::EditTarget: {
      const auto *editTargetCommand =
          dynamic_cast<const SvnRepoEditTargetCommand *>(command);
      if (editTargetCommand) {
        editTargetWorker(*editTargetCommand);
      }
      break;
    }
    case SvnRepoCommandType::DeleteTarget: {
      const auto *deleteTargetCommand =
          dynamic_cast<const SvnRepoDeleteTargetCommand *>(command);
      if (deleteTargetCommand) {
        deleteTargetWorker(*deleteTargetCommand);
      }
      break;
    }
    case SvnRepoCommandType::SetDefaultTarget: {
      const auto *setDefaultTargetCommand =
          dynamic_cast<const SvnRepoSetDefaultTargetCommand *>(command);
      if (setDefaultTargetCommand) {
        setDefaultTargetWorker(*setDefaultTargetCommand);
      }
      break;
    }
    case SvnRepoCommandType::SetUpstream: {
      const auto *setUpstreamCommand =
          dynamic_cast<const SvnRepoSetUpstreamCommand *>(command);
      if (setUpstreamCommand) {
        setUpstreamWorker(*setUpstreamCommand);
      }
      break;
    }
    case SvnRepoCommandType::IgnoreFiles: {
      const auto *ignoreFilesCommand =
          dynamic_cast<const SvnRepoIgnoreFilesCommand *>(command);
      if (ignoreFilesCommand) {
        ignoreFilesWorker(*ignoreFilesCommand);
      }
      break;
    }
    case SvnRepoCommandType::StageFiles: {
      const auto *stageFilesCommand =
          dynamic_cast<const SvnRepoStageFilesCommand *>(command);
      if (stageFilesCommand) {
        stageFilesWorker(*stageFilesCommand);
      }
      break;
    }
    case SvnRepoCommandType::Stash: {
      const auto *stashCommand =
          dynamic_cast<const SvnRepoStashCommand *>(command);
      if (stashCommand) {
        stashWorker(*stashCommand);
      }
      break;
    }
    case SvnRepoCommandType::Status: {
      const auto *statusCommand =
          dynamic_cast<const SvnRepoStatusCommand *>(command);
      if (statusCommand) {
        statusWorker(*statusCommand);
      }
      break;
    }
    case SvnRepoCommandType::SubmoduleUpdate: {
      const auto *submoduleUpdateCommand =
          dynamic_cast<const SvnRepoSubmoduleUpdateCommand *>(command);
      if (submoduleUpdateCommand) {
        submoduleUpdateWorker(*submoduleUpdateCommand);
      }
      break;
    }
    case SvnRepoCommandType::UnstageFiles: {
      const auto *unstageFilesCommand =
          dynamic_cast<const SvnRepoUnstageFilesCommand *>(command);
      if (unstageFilesCommand) {
        unstageFilesWorker(*unstageFilesCommand);
      }
      break;
    }
    case SvnRepoCommandType::UpdateRemote: {
      const auto *updateRemoteCommand =
          dynamic_cast<const SvnRepoUpdateRemoteCommand *>(command);
      if (updateRemoteCommand) {
        updateRemoteWorker(*updateRemoteCommand);
      }
      break;
    }
    case SvnRepoCommandType::Quit: {
      const auto *quitCommand =
          dynamic_cast<const SvnRepoQuitCommand *>(command);
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
  }

  SvnRepo::shutdownForCurrentThread();

  LOG_PERF(0, "Exit");
}

} // namespace SvnRepoManager
