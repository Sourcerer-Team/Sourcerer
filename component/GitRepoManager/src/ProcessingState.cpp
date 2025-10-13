#include "ProcessingState.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerStructs.h"

#include <GitRepo.h>
#include <Log.h>

#include <git2/errors.h>

#include <functional>

#define THREAD_ID "ProcessingState"

using namespace GitRepoManager::Structs;

namespace GitRepoManager {

ProcessingState::ProcessingState(GitRepoManagerFsm &fsm)
  : GitRepoManagerState{StateType::Processing, fsm}, _quitAll{false},
    _eventManager{EventManager::EventManager::getInstance()} {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&ProcessingState::eventCallback, this, std::placeholders::_1);

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

  LOG_PERF(0, "Exit");
}

ProcessingState::~ProcessingState() {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&ProcessingState::eventCallback, this, std::placeholders::_1);

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

  LOG_PERF(0, "Exit");
}

void ProcessingState::enter() {
  LOG_PERF(0, "Enter");

  _quitAll = false;

  LOG_PERF(0, "Exit");
}

bool ProcessingState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  switch (const auto type = static_cast<EventType>(event.type())) {
  case EventType::Setup: {
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
  case EventType::DeleteBranch: {
    const auto deleteBranchCommand =
        event.nextParam<GitRepoDeleteBranchCommand>();
    scheduleWorkerForDeleteBranch(deleteBranchCommand);
    break;
  }
  case EventType::DeleteStash: {
    const auto deleteStashCommand =
        event.nextParam<GitRepoDeleteStashCommand>();
    scheduleWorkerForDeleteStash(deleteStashCommand);
    break;
  }
  case EventType::DiffFiles: {
    const auto diffFilesCommand = event.nextParam<GitRepoDiffFilesCommand>();
    scheduleWorkerForDiffFiles(diffFilesCommand);
    break;
  }
  case EventType::DiscardFiles: {
    const auto discardFilesCommand =
        event.nextParam<GitRepoDiscardFilesCommand>();
    scheduleWorkerForDiscardFiles(discardFilesCommand);
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
  case EventType::ListTags: {
    const auto listTagsCommand = event.nextParam<GitRepoListTagsCommand>();
    scheduleWorkerForListTags(listTagsCommand);
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
  case EventType::RenameBranch: {
    const auto renameBranchCommand = event.nextParam<
      GitRepoRenameBranchCommand>();
    scheduleWorkerForRenameBranch(renameBranchCommand);
    break;
  }
  case EventType::RenameTag: {
    const auto renameTagCommand = event.nextParam<
      GitRepoRenameTagCommand>();
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
  case EventType::UnstageFiles: {
    const auto unstageFilesCommand =
        event.nextParam<GitRepoUnstageFilesCommand>();
    scheduleWorkerForUnstageFiles(unstageFilesCommand);
    break;
  }
  default: {
    LOG_WARN("Unhandled event {}", static_cast<int>(type));
    return false;
  }
  }

  return true;
}

void ProcessingState::exit() {
  LOG_PERF(0, "Enter");

  destroyContextsMap();

  LOG_PERF(0, "Exit");
}

void ProcessingState::applyPatchWorker(
    const Structs::GitRepoApplyPatchCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const bool success = GitRepo::applyPatch(repo, command.patchContent);
  if (!success) {
    reportError(EventManager::EventType::GitRepoManagerApplyPatchError,
                tr("Can't apply stash."), command);
    LOG_ERROR(Log::ErrorId::Git, "Failed to apply patch.");
    return;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerApplyPatchResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::applyStashWorker(
    const Structs::GitRepoApplyStashCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const bool success = GitRepo::applyStash(repo, command.index);
  if (!success) {
    reportError(EventManager::EventType::GitRepoManagerApplyStashError,
                tr("Can't apply stash."), command);
    LOG_ERROR(Log::ErrorId::Git, "Failed to apply stash \"{}\".",
              command.index);
    return;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerApplyStashError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerApplyStashResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(success)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::checkoutBranchWorker(
    const Structs::GitRepoCheckoutBranchCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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
    reportError(EventManager::EventType::GitRepoManagerCheckoutBranchError,
                tr("Can't checkout branch."), command);
    LOG_ERROR(Log::ErrorId::Git, "Failed to checkout branch \"{}\".",
              command.branchName);
    return;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCheckoutBranchError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    GitRepoCheckoutBranchCommandResult result;
    result.branchName = command.branchName;
    result.branchType = command.branchType;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCheckoutBranchResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::checkoutCommitWorker(
    const Structs::GitRepoCheckoutCommitCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const bool success = GitRepo::checkoutCommit(repo, command.commit);
  if (!success) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutCommitError,
                tr("Can't checkout commit."), command);
    LOG_ERROR(Log::ErrorId::Git, "Failed to checkout commit \"{}\".",
              command.commit);
    return;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCheckoutCommitError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCheckoutCommitResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, success});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::cloneRepoWorker(const GitRepoCloneCommand &command) {
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
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                tr("Can't clone repository."), command);
    LOG_ERROR(Log::ErrorId::Git, "Can't clone repository.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = nullptr;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCloneRepoError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCloneRepoResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, command.destinationDir});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::closeRepoWorker(
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

  GitRepo::closeRepository(repo);

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = nullptr;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCloseRepoError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerCloseRepoResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, QVariant()});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::commitStagedWorker(
    const Structs::GitRepoCommitStagedCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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
      repo, command.summary, command.message, command.author, command.email);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCommitStagedResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, success});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::createBranchFromCommitWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, success});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::createPatchFromCommitWorker(
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

  GitRepoCreatePatchFromCommitCommandResult result;
  result.patchContent = patchContent;

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(result)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::createTagFromCommitWorker(
    const Structs::GitRepoCreateTagFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  const GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(
        EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
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
      GitRepo::createTagFromCommit(repo, command.commit, command.tagName,
                                   command.author, command.email,
                                   command.message);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCreateTagFromCommitResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, success});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::quitRepoWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerQuitResult, command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, QVariant()});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

Structs::GitRepoContext *ProcessingState::context(const QString &uuid) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  LOG_PERF(0, "Exit");

  return _contexts[uuid];
}

void ProcessingState::destroyContext(const QString &uuid) {
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

void ProcessingState::destroyContextsMap() {
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

void ProcessingState::deleteBranchWorker(
    const Structs::GitRepoDeleteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const bool success = GitRepo::deleteBranch(repo, command.branchName);
  if (!success) {
    reportError(EventManager::EventType::GitRepoManagerDeleteBranchError,
                tr("Can't delete branch."), command);
    LOG_ERROR(Log::ErrorId::Git, "Failed to delete branch \"{}\".",
              command.branchName);
    return;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerDeleteBranchResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::deleteStashWorker(
    const Structs::GitRepoDeleteStashCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const bool success = GitRepo::deleteStash(repo, command.index);
  if (!success) {
    reportError(EventManager::EventType::GitRepoManagerDeleteStashError,
                tr("Can't delete stash."), command);
    LOG_ERROR(Log::ErrorId::Git, "Failed to delete stash \"{}\".",
              command.index);
    return;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerDeleteStashError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
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

void ProcessingState::diffFilesWorker(
    const Structs::GitRepoDiffFilesCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const QString diff = GitRepo::diffFiles(repo, command.files);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerDiffFilesResult, command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, diff});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::discardFilesWorker(
    const Structs::GitRepoDiscardFilesCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
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

  const bool success = GitRepo::discardFiles(repo, command.files);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerDiscardFilesResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type, success});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::eventCallback(const EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Processing event {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerCloneRepoError: {
    const GitRepoError error = event.data<GitRepoError>();
    destroyContext(error.uuid);
    break;
  }
  case EventManager::EventType::GitRepoManagerCloneRepoResult: {
    const auto commandResult = event.data<GitRepoResult>();
    destroyContext(commandResult.uuid);
    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepoError: {
    const GitRepoError error = event.data<GitRepoError>();
    destroyContext(error.uuid);
    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepoResult: {
    const auto commandResult = event.data<GitRepoResult>();
    destroyContext(commandResult.uuid);
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoError: {
    const GitRepoError error = event.data<GitRepoError>();
    destroyContext(error.uuid);
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::getActiveBranchWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
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

void ProcessingState::getCommitBranchesWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
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

void ProcessingState::getCommitHeadsWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
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

void ProcessingState::getCommitIsHeadWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
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

void ProcessingState::getCommitStatusWorker(
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

  const auto commitStatus = GitRepo::getCommitStatus(repo, command.commitOid);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerGetCommitStatusResult,
      command.syncUuid);
  resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                    QVariant::fromValue(commitStatus)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::getBranchHistoryWorker(
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

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const GitRepo::GitHistory history = GitRepo::getBranchHistory(
      repo, command.branchName, command.branchType, ctx->quit);

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerGetBranchHistoryError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
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

void ProcessingState::getUpstreamWorker(
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

  const auto &[remoteName, remoteBranchName] =
      GitRepo::getUpstream(repo, command.localBranchName);

  GitRepoGetUpstreamCommandResult result;
  result.remoteName = remoteName;
  result.remoteBranchName = remoteBranchName;

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerGetUpstreamResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(result)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::initRepoWorker(
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
      command.repoName, command.workingDir, command.defaultBranch);
  if (!success) {
    reportError(EventManager::EventType::GitRepoManagerInitRepoError,
                tr("Can't init repository."), command);
    LOG_ERROR(Log::ErrorId::Git, "Can't init repository.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = nullptr;
  }

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerInitRepoError, command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    const QString repoPath = command.workingDir + "/" + command.repoName;

    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerInitRepoResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type, repoPath});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::listLocalBranchesWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
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

void ProcessingState::listRemoteBranchesWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
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

void ProcessingState::listStashesWorker(
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
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

void ProcessingState::listSubmodulesWorker(
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

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListSubmodulesError,
        command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListSubmodulesResult,
        command.syncUuid);
    resultEvent.addData(GitRepoResult{command.uuid, command.type,
                                      QVariant::fromValue(submodules)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::listTagsWorker(
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

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListTagsError, command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerListTagsResult,
        command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(tags)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForOpenRepo(
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

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  auto newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new GitRepoOpenCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForPull(
    const Structs::GitRepoPullCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerPullError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoPullCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForPush(
    const Structs::GitRepoPushCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerPushError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoPushCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForRenameBranch(
    const Structs::GitRepoRenameBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerRenameBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRenameBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForRenameTag(
    const Structs::GitRepoRenameTagCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerRenameTagError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRenameTagCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForReset(
    const Structs::GitRepoResetCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerResetError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoResetCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForRevert(
    const Structs::GitRepoRevertCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerRevertError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoRevertCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForSetUpstream(
    const Structs::GitRepoSetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerSetUpstreamError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoSetUpstreamCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForStageFiles(
    const Structs::GitRepoStageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerStageFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoStageFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForStash(
    const Structs::GitRepoStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerStashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoStashCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForStatus(
    const Structs::GitRepoStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForUnstageFiles(
    const Structs::GitRepoUnstageFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerUnstageFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoUnstageFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForQuit(
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

void ProcessingState::openRepoWorker(const GitRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  git_repository *repo = GitRepo::openRepository(command.path);
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
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
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

void ProcessingState::pullWorker(const Structs::GitRepoPullCommand &command) {
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

  const bool success = GitRepo::pull(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerPullResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::pushWorker(const Structs::GitRepoPushCommand &command) {
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

  const bool success = GitRepo::push(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerPushResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::renameBranchWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerRenameBranchResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::renameTagWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerRenameTagResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::resetWorker(const Structs::GitRepoResetCommand &command) {
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerResetResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::revertWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerRevertResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::setUpstreamWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerSetUpstreamResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::stageFilesWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerStageFilesResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::stashWorker(const Structs::GitRepoStashCommand &command) {
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

  const bool stashed = GitRepo::stash(repo);

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerStashResult, command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(stashed)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::statusWorker(
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

  {
    LOCKGUARD(_contextsMutex);
    repo = ctx->gitRepo;
  }

  const GitRepo::GitStatus status = GitRepo::getStatus(repo);

  bool quit = false;

  {
    LOCKGUARD(_contextsMutex);
    quit = ctx->quit;
  }

  if (quit) {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerStatusError, command.syncUuid);
    resultEvent.addData(
        GitRepoError{command.uuid, QString(), GitRepo::getLastError()});
    _eventManager.send(resultEvent);
  } else {
    EventManager::Event resultEvent(
        EventManager::EventType::GitRepoManagerStatusResult, command.syncUuid);
    resultEvent.addData(
        GitRepoResult{command.uuid, command.type, QVariant::fromValue(status)});
    _eventManager.send(resultEvent);
  }

  LOG_PERF(0, "Exit");
}

void ProcessingState::unstageFilesWorker(
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

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerUnstageFilesResult,
      command.syncUuid);
  resultEvent.addData(
      GitRepoResult{command.uuid, command.type, QVariant::fromValue(success)});
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::reportError(
    const EventManager::EventType eventType, const QString &message,
    const Structs::GitRepoCommand &command) const {
  LOG_PERF(0, "Enter");

  GitRepoError error;
  error.error = GitRepo::getLastError();
  error.message = message;
  error.uuid = command.uuid;

  EventManager::Event errorEvent(eventType, command.syncUuid);
  errorEvent.addData(error);
  _eventManager.send(errorEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForApplyPatch(
    const Structs::GitRepoApplyPatchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerApplyPatchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoApplyPatchCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForApplyStash(
    const Structs::GitRepoApplyStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerApplyStashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoApplyStashCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCheckoutBranch(
    const Structs::GitRepoCheckoutBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCheckoutBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCheckoutCommit(
    const Structs::GitRepoCheckoutCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCheckoutCommitError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCheckoutCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCloneRepo(
    const Structs::GitRepoCloneCommand &command) {
  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                tr("Git repository is already in use."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  const auto newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new GitRepoCloneCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCloseRepo(
    const GitRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCloseRepoError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->quit = true;
  (*it)->commandsQueue.push(new GitRepoCloseCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCommitStagedRepo(
    const Structs::GitRepoCommitStagedCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCommitStagedError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCommitStagedCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCreateBranchFromCommit(
    const Structs::GitRepoCreateBranchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(
        EventManager::EventType::GitRepoManagerCreateBranchFromCommitError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCreateBranchFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCreatePatchFromCommit(
    const Structs::GitRepoCreatePatchFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(
        EventManager::EventType::GitRepoManagerCreatePatchFromCommitError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCreatePatchFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCreateTagFromCommit(
    const Structs::GitRepoCreateTagFromCommitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(
        EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
        tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCreateTagFromCommitCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetActiveBranch(
    const Structs::GitRepoGetActiveBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetActiveBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetActiveBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetCommitBranches(
    const Structs::GitRepoGetCommitBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitBranchesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetCommitHeads(
    const Structs::GitRepoGetCommitHeadsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitHeadsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitHeadsCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetCommitIsHead(
    const Structs::GitRepoGetCommitIsHeadCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitIsHeadError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitIsHeadCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetCommitStatus(
    const Structs::GitRepoGetCommitStatusCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetCommitStatusError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetCommitStatusCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetUpstream(
    const Structs::GitRepoGetUpstreamCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetUpstreamError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetUpstreamCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForDeleteBranch(
    const Structs::GitRepoDeleteBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerDeleteBranchError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForDeleteStash(
    const Structs::GitRepoDeleteStashCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerDeleteStashError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDeleteStashCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForDiffFiles(
    const Structs::GitRepoDiffFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerDiffFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDiffFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForDiscardFiles(
    const Structs::GitRepoDiscardFilesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerDiscardFilesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoDiscardFilesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetBranchHistory(
    const Structs::GitRepoGetBranchHistoryCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetBranchHistoryError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetBranchHistoryCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForInitRepo(
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

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  GitRepoContext *newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = QThread::create(workersThreadFunc);
  newContext->commandsQueue.push(new GitRepoInitRepoCommand(command));

  connect(newContext->workersThread, &QThread::finished,
          newContext->workersThread, &QObject::deleteLater);

  newContext->workersThread->start();

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListLocalBranches(
    const Structs::GitRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListLocalBranchesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListLocalBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListRemoteBranches(
    const Structs::GitRepoListRemoteBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListRemoteBranchesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListRemoteBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListStashes(
    const Structs::GitRepoListStashesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListStashesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListStashesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListSubmodules(
    const Structs::GitRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListSubmodulesError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListSubmodulesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListTags(
    const Structs::GitRepoListTagsCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListTagsError,
                tr("Git repository has not been opened."), command);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListTagsCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::workersThreadFunc(const QString &uuid) {
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

    switch (command->type) {
    case GitRepoCommandType::ApplyPatch: {
      const auto applyPatchCommand =
          dynamic_cast<const GitRepoApplyPatchCommand *>(command);
      if (applyPatchCommand) {
        applyPatchWorker(*applyPatchCommand);
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
    case GitRepoCommandType::DeleteBranch: {
      const auto deleteBranchCommand =
          dynamic_cast<const GitRepoDeleteBranchCommand *>(command);
      if (deleteBranchCommand) {
        deleteBranchWorker(*deleteBranchCommand);
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
    case GitRepoCommandType::DiffFiles: {
      const auto diffFilesCommand =
          dynamic_cast<const GitRepoDiffFilesCommand *>(command);
      if (diffFilesCommand) {
        diffFilesWorker(*diffFilesCommand);
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
    case GitRepoCommandType::GetActiveBranch: {
      const auto getActiveBranchCommand =
          dynamic_cast<const GitRepoGetActiveBranchCommand *>(command);
      if (getActiveBranchCommand) {
        getActiveBranchWorker(*getActiveBranchCommand);
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
    case GitRepoCommandType::GetBranchHistory: {
      const auto *branchHistoryCommand =
          dynamic_cast<const GitRepoGetBranchHistoryCommand *>(command);
      if (branchHistoryCommand) {
        getBranchHistoryWorker(*branchHistoryCommand);
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
    case GitRepoCommandType::ListTags: {
      const auto *listTagsCommand =
          dynamic_cast<const GitRepoListTagsCommand *>(command);
      if (listTagsCommand) {
        listTagsWorker(*listTagsCommand);
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
    case GitRepoCommandType::UnstageFiles: {
      const auto *unstageFilesCommand =
          dynamic_cast<const GitRepoUnstageFilesCommand *>(command);
      if (unstageFilesCommand) {
        unstageFilesWorker(*unstageFilesCommand);
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
  }

  GitRepo::shutdownForCurrentThread();

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager