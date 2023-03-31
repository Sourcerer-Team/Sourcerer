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

  _eventManager.subscribe(EventManager::EventType::GitRepoManagerOpenRepoError,
                          eventCallbackFunc);

  LOG_PERF(0, "Exit");
}

ProcessingState::~ProcessingState()
{
  LOG_PERF(0, "Enter");

  const auto eventCallbackFunc =
      std::bind(&ProcessingState::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(EventManager::EventType::GitRepoManagerOpenRepoError,
                          eventCallbackFunc);

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
  case EventType::CloseRepo: {
    const GitRepoCloseCommand closeCommand = event.nextParam<GitRepoCloseCommand>();
    scheduleWorkerForCloseRepo(closeCommand);
    break;
  }
  case EventType::OpenRepo: {
    const GitRepoOpenCommand openCommand = event.nextParam<GitRepoOpenCommand>();
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

void ProcessingState::eventCallback(const EventManager::Event event)
{
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Processing event {}.", (int)eventType);

  switch(eventType) {
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

void ProcessingState::scheduleWorkerForCloseRepo(const GitRepoCloseCommand &command)
{
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

  LOG_PERF(0, "Exit");
}

void ProcessingState::workersThreadFunc(const QString &uuid) {
  LOG_PERF(0, "Enter");

  GitRepo::initForCurrentThread();

  bool quit = false;

  while (!quit) {
    GitRepoContext *ctx = context(uuid);
    if (!ctx) {
      LOG_ERROR(Log::ErrorId::Git,
                "Internal error. Context is not initialized.");
      break;
    }

    GitRepoCommand *command = ctx->commandsQueue.pop();
    if (!command) {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Enqueued command is null.");
      continue;
    }

    switch (command->type) {
    case GitRepoCommandType::Open: {
      const GitRepoOpenCommand *openCommand =
          static_cast<const GitRepoOpenCommand *>(command);
      openRepoWorker(*openCommand);
      break;
    }
    case GitRepoCommandType::Close: {
      const GitRepoCloseCommand *closeCommand =
          static_cast<const GitRepoCloseCommand *>(command);
      closeRepoWorker(*closeCommand);
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
