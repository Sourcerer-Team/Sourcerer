#include "ProcessingState.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerStructs.h"

#include <GitRepo.h>
#include <Log.h>

#include <functional>

#define THREAD_ID "ProcessingState"

using namespace GitRepoManager::Structs;

namespace GitRepoManager {

ProcessingState::ProcessingState(GitRepoManagerFsm &fsm)
    : GitRepoManagerState{StateType::Processing, fsm},
      _eventManager{EventManager::EventManager::getInstance()},
      _quitAll{false} {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&ProcessingState::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(EventManager::EventType::GitRepoManagerInitRepoError,
                          eventCallbackFunc);
  _eventManager.subscribe(EventManager::EventType::GitRepoManagerOpenRepoError,
                          eventCallbackFunc);

  LOG_PERF(0, "Exit");
}

ProcessingState::~ProcessingState() {
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&ProcessingState::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerInitRepoError, eventCallbackFunc);

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

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::Setup: {
    break;
  }
  case EventType::CloneRepo: {
    const GitRepoCloneCommand cloneCommand =
        event.nextParam<GitRepoCloneCommand>();
    scheduleWorkerForCloneRepo(cloneCommand);
    break;
  }
  case EventType::CloseRepo: {
    const GitRepoCloseCommand closeCommand =
        event.nextParam<GitRepoCloseCommand>();
    scheduleWorkerForCloseRepo(closeCommand);
    break;
  }
  case EventType::GetActiveBranch: {
    const GitRepoGetActiveBranchCommand getActiveBranchCommand =
        event.nextParam<GitRepoGetActiveBranchCommand>();
    scheduleWorkerForGetActiveBranch(getActiveBranchCommand);
    break;
  }
  case EventType::Init: {
    const GitRepoInitRepoCommand initCommand =
        event.nextParam<GitRepoInitRepoCommand>();
    scheduleWorkerForInitRepo(initCommand);
    break;
  }
  case EventType::ListLocalBranches: {
    const GitRepoListLocalBranchesCommand listLocalCommand =
        event.nextParam<GitRepoListLocalBranchesCommand>();
    scheduleWorkerForListLocalBranches(listLocalCommand);
    break;
  }
  case EventType::ListRemoteBranches: {
    const GitRepoListRemoteBranchesCommand listRemoteCommand =
        event.nextParam<GitRepoListRemoteBranchesCommand>();
    scheduleWorkerForListRemoteBranches(listRemoteCommand);
    break;
  }
  case EventType::ListSubmodules: {
    const GitRepoListSubmodulesCommand listSubmodulesCommand =
        event.nextParam<GitRepoListSubmodulesCommand>();
    scheduleWorkerForListSubmodules(listSubmodulesCommand);
    break;
  }
  case EventType::ListTags: {
    const GitRepoListTagsCommand listTagsCommand =
        event.nextParam<GitRepoListTagsCommand>();
    scheduleWorkerForListTags(listTagsCommand);
    break;
  }
  case EventType::OpenRepo: {
    const GitRepoOpenCommand openCommand =
        event.nextParam<GitRepoOpenCommand>();
    scheduleWorkerForOpenRepo(openCommand);
    break;
  }
  default: {
    LOG_WARN("Unhandled event {}", (int)type);
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

void ProcessingState::cloneRepoWorker(const GitRepoCloneCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  git_repository* repo = GitRepo::cloneRepository(command.url, command.destinationDir);
  if (!repo) {
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                GitRepoErrorType::NotCloned, tr("Can't clone repository."),
                command.uuid);
    LOG_ERROR(Log::ErrorId::Git, "Can't clone repository.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = repo;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCloneRepoResult);
  resultEvent.addData(command.destinationDir);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");

}

void ProcessingState::closeRepoWorker(
    const Structs::GitRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerCloseRepoError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    GitRepo::closeRepository(ctx->gitRepo);
    ctx->gitRepo = nullptr;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerCloseRepoResult);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::quitRepoWorker(
    const Structs::GitRepoQuitCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerQuitError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerOpenRepoResult);
  _eventManager.send(resultEvent);

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
  if (ctx->workersThread.joinable())
    ctx->workersThread.join();

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

void ProcessingState::eventCallback(const EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Processing event {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerInitRepoError: {
    const GitRepoError error = event.data<GitRepoError>();
    destroyContext(error.uuid);
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

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerGetActiveBranchError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QString activeBranch;

  {
    LOCKGUARD(_contextsMutex);
    activeBranch = GitRepo::getActiveBranch(ctx->gitRepo);
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult);
  resultEvent.addData(activeBranch);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::initRepoWorker(
    const Structs::GitRepoInitRepoCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerInitRepoError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Git, "Internal error. Context not initialized.");
    return;
  }

  git_repository *repo = GitRepo::initRepository(
      command.repoName, command.workingDir, command.defaultBranch);
  if (!repo) {
    reportError(EventManager::EventType::GitRepoManagerInitRepoError,
                GitRepoErrorType::NotInitialized, tr("Can't init repository."),
                command.uuid);
    LOG_ERROR(Log::ErrorId::Git, "Can't init repository.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = repo;
  }

  const QString repoPath = command.workingDir + "/" + command.repoName;

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerInitRepoResult);
  resultEvent.addData(repoPath);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::listLocalBranchesWorker(
    const Structs::GitRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListLocalBranchesError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QStringList localBranches;

  {
    LOCKGUARD(_contextsMutex);
    localBranches = GitRepo::listLocalBranches(ctx->gitRepo);
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult);
  resultEvent.addData(localBranches);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::listRemoteBranchesWorker(
    const Structs::GitRepoListRemoteBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListRemoteBranchesError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QStringList remoteBranches;

  {
    LOCKGUARD(_contextsMutex);
    remoteBranches = GitRepo::listRemoteBranches(ctx->gitRepo);
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult);
  resultEvent.addData(remoteBranches);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::listSubmodulesWorker(
    const Structs::GitRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListSubmodulesError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QStringList submodules;

  {
    LOCKGUARD(_contextsMutex);
    submodules = GitRepo::listSubmodules(ctx->gitRepo);
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListSubmodulesResult);
  resultEvent.addData(submodules);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::listTagsWorker(
    const Structs::GitRepoListTagsCommand &command) {
  LOG_PERF(0, "Enter");

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    reportError(EventManager::EventType::GitRepoManagerListTagsError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  QStringList tags;

  {
    LOCKGUARD(_contextsMutex);
    tags = GitRepo::listTags(ctx->gitRepo);
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerListTagsResult);
  resultEvent.addData(tags);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForOpenRepo(
    const GitRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerOpenRepoError,
                GitRepoErrorType::AlreadyExist,
                tr("Git repository has already been opened."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  GitRepoContext *newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = std::move(std::thread{workersThreadFunc});
  newContext->commandsQueue.push(new GitRepoOpenCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForQuit(
    const Structs::GitRepoQuitCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerQuitError,
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
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
                GitRepoErrorType::ErrorWhileOpening, gitError->message,
                command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    GitRepo::closeRepository(repo);
    reportError(EventManager::EventType::GitRepoManagerOpenRepoError,
                GitRepoErrorType::InternalError,
                tr("Internal error. Context not initialized."), command.uuid);
    LOG_ERROR(Log::ErrorId::Gui, "Internal error. Context not initialized.");
    return;
  }

  {
    LOCKGUARD(_contextsMutex);
    ctx->gitRepo = repo;
  }

  EventManager::Event resultEvent(
      EventManager::EventType::GitRepoManagerOpenRepoResult);
  _eventManager.send(resultEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::reportError(EventManager::EventType eventType,
                                  GitRepoErrorType errorType,
                                  const QString &message,
                                  const QString &uuid) const {
  LOG_PERF(0, "Enter");

  GitRepoError error;
  error.error = errorType;
  error.message = message;
  error.uuid = uuid;

  EventManager::Event errorEvent(eventType);
  errorEvent.addData(error);
  _eventManager.send(errorEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForCloneRepo(
    const Structs::GitRepoCloneCommand &command) {
  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCloneRepoError,
                GitRepoErrorType::AlreadyExist,
                tr("Git repository is already in use."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  GitRepoContext *newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = std::move(std::thread{workersThreadFunc});
  newContext->commandsQueue.push(new GitRepoCloneCommand(command));

  LOG_PERF(0, "Exit");

}

void ProcessingState::scheduleWorkerForCloseRepo(
    const GitRepoCloseCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerCloseRepoError,
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoCloseCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForGetActiveBranch(
    const Structs::GitRepoGetActiveBranchCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerGetActiveBranchError,
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoGetActiveBranchCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForInitRepo(
    const Structs::GitRepoInitRepoCommand &command) {
  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerInitRepoError,
                GitRepoErrorType::AlreadyExist,
                tr("Git repository is already in use."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  GitRepoContext *newContext = new GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = std::move(std::thread{workersThreadFunc});
  newContext->commandsQueue.push(new GitRepoInitRepoCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListLocalBranches(
    const Structs::GitRepoListLocalBranchesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListLocalBranchesError,
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
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
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  (*it)->commandsQueue.push(new GitRepoListRemoteBranchesCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::scheduleWorkerForListSubmodules(
    const Structs::GitRepoListSubmodulesCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it == _contexts.constEnd()) {
    reportError(EventManager::EventType::GitRepoManagerListSubmodulesError,
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
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
                GitRepoErrorType::NotOpened,
                tr("Git repository has not been opened."), command.uuid);
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
    case GitRepoCommandType::Clone: {
      const GitRepoCloneCommand *cloneCommand =
          static_cast<const GitRepoCloneCommand *>(command);
      cloneRepoWorker(*cloneCommand);
      break;
    }
    case GitRepoCommandType::Close: {
      const GitRepoCloseCommand *closeCommand =
          static_cast<const GitRepoCloseCommand *>(command);
      closeRepoWorker(*closeCommand);
      break;
    }
    case GitRepoCommandType::GetActiveBranch: {
      const GitRepoGetActiveBranchCommand *getActiveBranchCommand =
          static_cast<const GitRepoGetActiveBranchCommand *>(command);
      getActiveBranchWorker(*getActiveBranchCommand);
      break;
    }
    case GitRepoCommandType::Init: {
      const GitRepoInitRepoCommand *initRepoCommand =
          static_cast<const GitRepoInitRepoCommand *>(command);
      initRepoWorker(*initRepoCommand);
      break;
    }
    case GitRepoCommandType::ListLocalBranches: {
      const GitRepoListLocalBranchesCommand *listLocalCommand =
          static_cast<const GitRepoListLocalBranchesCommand *>(command);
      listLocalBranchesWorker(*listLocalCommand);
      break;
    }
    case GitRepoCommandType::ListRemoteBranches: {
      const GitRepoListRemoteBranchesCommand *listRemoteCommand =
          static_cast<const GitRepoListRemoteBranchesCommand *>(command);
      listRemoteBranchesWorker(*listRemoteCommand);
      break;
    }
    case GitRepoCommandType::ListSubmodules: {
      const GitRepoListSubmodulesCommand *listSubmodulesCommand =
          static_cast<const GitRepoListSubmodulesCommand *>(command);
      listSubmodulesWorker(*listSubmodulesCommand);
      break;
    }
    case GitRepoCommandType::ListTags: {
      const GitRepoListTagsCommand *listTagsCommand =
          static_cast<const GitRepoListTagsCommand *>(command);
      listTagsWorker(*listTagsCommand);
      break;
    }
    case GitRepoCommandType::Open: {
      const GitRepoOpenCommand *openCommand =
          static_cast<const GitRepoOpenCommand *>(command);
      openRepoWorker(*openCommand);
      break;
    }
    case GitRepoCommandType::Quit: {
      const GitRepoQuitCommand *quitCommand =
          static_cast<const GitRepoQuitCommand *>(command);
      quitRepoWorker(*quitCommand);
      quit = true;
      break;
    }
    default: {
      LOG_WARN("Unknown command type {}.", (int)command->type);
      break;
    }
    }

    delete command;
  }

  GitRepo::shutdownForCurrentThread();

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
