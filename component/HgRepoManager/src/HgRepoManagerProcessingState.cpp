// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRepoManagerProcessingState.h"
#include "HgRepoManagerEvent.h"
#include "HgRepoManagerWorkers.h"

#include <Log.h>

#include <QUuid>

#define THREAD_ID "HgRepoManagerProcessingState"

using namespace HgRepoManager::Structs;

namespace HgRepoManager {

HgRepoManagerProcessingState::HgRepoManagerProcessingState(
    HgRepoManagerFsm &fsm)
    : HgRepoManagerState{StateType::Processing, fsm},
      _eventManager{EventManager::EventManager::getInstance()} {}

HgRepoManagerProcessingState::~HgRepoManagerProcessingState() {
  retireAll();
}

void HgRepoManagerProcessingState::enter() {
  LOG_PERF(0, "Enter");

  _quitTasks = false;

  LOG_PERF(0, "Exit");
}

bool HgRepoManagerProcessingState::process(Fsm::FsmEvent event) {
  switch (static_cast<EventType>(event.type())) {
  case EventType::Command:
    schedule(event.nextParam<HgRepoCommandPtr>());
    return true;
  case EventType::RetireContext:
    retireContext(event.nextParam<QString>());
    return true;
  case EventType::RetireTask:
    retireTask(event.nextParam<QString>());
    return true;
  default:
    return false;
  }
}

void HgRepoManagerProcessingState::exit() {
  LOG_PERF(0, "Enter");

  retireAll();

  LOG_PERF(0, "Exit");
}

void HgRepoManagerProcessingState::schedule(const HgRepoCommandPtr &command) {
  switch (command->type) {
  case HgRepoCommandType::CloneRepo:
  case HgRepoCommandType::InitRepo:
    startTask(command);
    return;
  case HgRepoCommandType::OpenRepo:
    openContext(command);
    return;
  default:
    break;
  }

  Context *context = _contexts.value(command->uuid);
  if (!context || context->closing) {
    reportError(*command, tr("The Mercurial repository is not open."));
    return;
  }

  if (command->type == HgRepoCommandType::CloseRepo) {
    closeContext(context, command);
    return;
  }

  context->commandsQueue.push(command);
}

void HgRepoManagerProcessingState::openContext(
    const HgRepoCommandPtr &command) {
  LOG_PERF(0, "Enter");

  if (_contexts.contains(command->uuid)) {
    reportError(*command, tr("The Mercurial repository is already open."));
    LOG_PERF(0, "Exit");
    return;
  }

  auto *context = new Context();
  context->uuid = command->uuid;
  context->commandsQueue.push(command);
  context->workersThread =
      QThread::create([this, context]() { workersThreadFunc(context); });
  _contexts.insert(command->uuid, context);

  context->workersThread->start();

  LOG_PERF(0, "Exit");
}

void HgRepoManagerProcessingState::closeContext(
    Context *context, const HgRepoCommandPtr &command) {
  LOG_PERF(0, "Enter");

  // The running command is cancelled and the queued ones are refused, so the
  // repository closes right away.
  context->closing = true;
  context->quit = true;
  refuseQueued(context, tr("The Mercurial repository was closed."));
  context->commandsQueue.push(command);

  // The uuid can be opened again while the worker finishes.
  _contexts.remove(context->uuid);
  _contexts.insert(QStringLiteral("closing:%1").arg(quintptr(context)),
                   context);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerProcessingState::startTask(const HgRepoCommandPtr &command) {
  const QString taskId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  Task task;
  task.thread = QThread::create(
      [this, command, taskId]() { taskThreadFunc(command, taskId); });
  _tasks.insert(taskId, task);
  task.thread->start();
}

void HgRepoManagerProcessingState::retireContext(const QString &key) {
  LOG_PERF(0, "Enter");

  Context *context = _contexts.take(key);
  if (!context) {
    LOG_PERF(0, "Exit");
    return;
  }

  context->workersThread->wait();
  delete context->workersThread;
  refuseQueued(context, tr("The Mercurial repository was closed."));
  delete context;

  LOG_PERF(0, "Exit");
}

void HgRepoManagerProcessingState::retireTask(const QString &taskId) {
  const Task task = _tasks.take(taskId);
  if (task.thread) {
    task.thread->wait();
    delete task.thread;
  }
}

void HgRepoManagerProcessingState::retireAll() {
  LOG_PERF(0, "Enter");

  _quitTasks = true;

  for (Context *context : std::as_const(_contexts)) {
    context->closing = true;
    context->quit = true;
    refuseQueued(context, tr("The Mercurial repository was closed."));
    context->commandsQueue.push(std::make_shared<HgRepoQuitCommand>());
  }

  for (const QString &key : _contexts.keys()) {
    retireContext(key);
  }

  for (const QString &taskId : _tasks.keys()) {
    retireTask(taskId);
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerProcessingState::refuseQueued(Context *context,
                                                const QString &message) const {
  while (!context->commandsQueue.empty()) {
    const HgRepoCommandPtr command =
        context->commandsQueue.pop(0, HgRepoCommandPtr());
    if (command && command->type != HgRepoCommandType::Quit) {
      reportError(*command, message);
    }
  }
}

void HgRepoManagerProcessingState::reportError(const HgRepoCommand &command,
                                               const QString &message) const {
  Workers::Outcome outcome;
  outcome.error.uuid = command.uuid;
  outcome.error.message = message;
  Workers::reply(command, outcome);
}

void HgRepoManagerProcessingState::workersThreadFunc(Context *context) {
  LOG_PERF(0, "Enter");

  Workers::initThread(&context->quit);

  Workers::Session session;
  session.uuid = context->uuid;
  session.quit = &context->quit;

  while (true) {
    const HgRepoCommandPtr command = context->commandsQueue.pop();
    if (!command || command->type == HgRepoCommandType::Quit) {
      break;
    }

    const Workers::Outcome outcome = Workers::execute(session, *command);
    Workers::reply(*command, outcome);

    if (command->type == HgRepoCommandType::CloseRepo ||
        (command->type == HgRepoCommandType::OpenRepo && !outcome.success)) {
      break;
    }
  }

  if (session.repo) {
    HgRepo::closeRepository(session.repo);
    session.repo = nullptr;
  }

  Workers::shutdownThread();

  // The key the context has now: its uuid, or the closing key.
  const QString key = context->closing
                          ? QStringLiteral("closing:%1").arg(quintptr(context))
                          : context->uuid;
  HgRepoManagerEvent retireEvent(EventType::RetireContext);
  retireEvent.addParam(key);
  fsm().send(retireEvent);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerProcessingState::taskThreadFunc(
    const HgRepoCommandPtr &command, const QString &taskId) {
  Workers::initThread(&_quitTasks);

  Workers::Session session;
  session.uuid = command->uuid;
  session.quit = &_quitTasks;
  Workers::reply(*command, Workers::execute(session, *command));

  Workers::shutdownThread();

  HgRepoManagerEvent retireEvent(EventType::RetireTask);
  retireEvent.addParam(taskId);
  fsm().send(retireEvent);
}

} // namespace HgRepoManager
