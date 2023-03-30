#include "ProcessingState.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerStructs.h"

#include <GitRepo.h>
#include <Log.h>

#include <functional>

#define THREAD_ID "ProcessingState"

namespace GitRepoManager {

ProcessingState::ProcessingState(GitRepoManagerFsm &fsm)
    : GitRepoManagerState{StateType::Processing, fsm},
      _eventManager{EventManager::EventManager::getInstance()} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void ProcessingState::enter() {
  LOG_PERF(0, "Enter");
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
  case EventType::OpenRepo: {
    const Structs::GitRepoOpenCommand command =
        event.nextParam<Structs::GitRepoOpenCommand>();
    scheduleWorkerForOpenRepo(command);
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
  LOG_PERF(0, "Exit");
}

Structs::GitRepoContext *ProcessingState::context(const QString &uuid) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  LOG_PERF(0, "Exit");

  return _contexts[uuid];
}

void ProcessingState::scheduleWorkerForOpenRepo(
    const Structs::GitRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_contextsMutex);

  const auto it = _contexts.constFind(command.uuid);
  if (it != _contexts.constEnd()) {
    reportError(Structs::GitRepoErrorType::AlreadyExist,
                tr("Git repository has already been opened."), command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto workersThreadFunc =
      std::bind(&ProcessingState::workersThreadFunc, this, command.uuid);

  Structs::GitRepoContext *newContext = new Structs::GitRepoContext();
  _contexts[command.uuid] = newContext;

  newContext->uuid = command.uuid;
  newContext->workersThread = std::move(std::thread{workersThreadFunc});
  newContext->commandsQueue.push(new Structs::GitRepoOpenCommand(command));

  LOG_PERF(0, "Exit");
}

void ProcessingState::openRepoWorker(
    const Structs::GitRepoOpenCommand &command) {
  LOG_PERF(0, "Enter");

  git_repository *repo = GitRepo::openRepository(command.path);
  if (!repo) {
    const git_error *gitError = git_error_last();
    reportError(Structs::GitRepoErrorType::ErrorWhileOpening, gitError->message,
                command.uuid);
    LOG_PERF(0, "Exit");
    return;
  }

  Structs::GitRepoContext *ctx = context(command.uuid);
  if (!ctx) {
    GitRepo::closeRepository(repo);
    reportError(Structs::GitRepoErrorType::InternalError,
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

void ProcessingState::reportError(Structs::GitRepoErrorType errorType,
                                  const QString &message,
                                  const QString &uuid) const {
  LOG_PERF(0, "Enter");

  Structs::GitRepoError error;
  error.error = errorType;
  error.message = message;
  error.uuid = uuid;

  EventManager::Event errorEvent(
      EventManager::EventType::GitRepoManagerOpenRepoError);
  errorEvent.addData(error);
  _eventManager.send(errorEvent);

  LOG_PERF(0, "Exit");
}

void ProcessingState::workersThreadFunc(const QString &uuid) {
  LOG_PERF(0, "Enter");

  bool quit = false;

  while (!quit) {
    Structs::GitRepoContext *ctx = context(uuid);
    if(!ctx) {
        LOG_ERROR(Log::ErrorId::Git, "Internal error. Context is not initialized.");
        break;
    }

    Structs::GitRepoCommand *command = ctx->commandsQueue.pop();
    if (!command) {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Enqued command is null.");
      continue;
    }

    const Structs::GitRepoCommandType commandType = command->type;

    switch (commandType) {
    case Structs::GitRepoCommandType::Open: {
      const Structs::GitRepoOpenCommand *openCommand =
          static_cast<const Structs::GitRepoOpenCommand *>(command);
      openRepoWorker(*openCommand);
      break;
    }
    case Structs::GitRepoCommandType::Quit: {
      // Best case handling. Client can add new commands after we finish
      // flushing of the queue.
      while (!ctx->commandsQueue.empty()) {
          Structs::GitRepoCommand *commandToDelete = ctx->commandsQueue.pop();
        delete commandToDelete;
      }

      {
        LOCKGUARD(_contextsMutex);
        _contexts.remove(uuid);
      }

      quit = true;
      break;
    }
    default: {
      break;
    }
    }

    delete command;
  }

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
