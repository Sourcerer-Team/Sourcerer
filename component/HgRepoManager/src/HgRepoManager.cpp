#include "HgRepoManager.h"
#include "HgRepoManagerAskPass.h"
#include "HgRepoManagerEvent.h"
#include "HgRepoManagerStructs.h"

#include <Log.h>

#include <functional>

#define THREAD_ID "HgRepoManager"

using namespace HgRepoManager::Structs;

namespace HgRepoManager {

namespace {

// Request events of every command, see HG_REPO_MANAGER_COMMAND_LIST.
const QVector<EventManager::EventType> &requestEvents() {
  static const QVector<EventManager::EventType> events = {
#define HG_REPO_REQUEST_EVENT(X, name)                                         \
  EventManager::EventType::HgRepoManager##name,
      HG_REPO_MANAGER_COMMAND_LIST(HG_REPO_REQUEST_EVENT, unused)
#undef HG_REPO_REQUEST_EVENT
  };
  return events;
}

} // namespace

HgRepoManager::HgRepoManager()
    : _eventManager(EventManager::EventManager::getInstance()),
      _askPass(std::make_unique<Workers::AskPassServer>()) {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&HgRepoManager::eventCallback, this, std::placeholders::_1);
  for (const EventManager::EventType type : requestEvents()) {
    _eventManager.subscribe(CAPTURE_CALLBACK(type, callbackFunc));
  }

  LOG_PERF(0, "Exit");
}

HgRepoManager::~HgRepoManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&HgRepoManager::eventCallback, this, std::placeholders::_1);
  for (const EventManager::EventType type : requestEvents()) {
    _eventManager.unsubscribe(CAPTURE_CALLBACK(type, callbackFunc));
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManager::start() {
  LOG_PERF(0, "Enter");

  // Before any command server starts, they inherit its environment.
  _askPass->start(Workers::AskPassServer::defaultProgram());
  _fsm.start();

  const EventManager::Event startedEvent(
      EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void HgRepoManager::stop() {
  LOG_PERF(0, "Enter");

  _fsm.stop();
  _askPass->stop();

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  _eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

void HgRepoManager::eventCallback(const EventManager::Event &event) {
  LOG_PERF(0, "Enter");

  LOG_DEBUG("Received event {}", _eventManager.eventName(event.type()));

  // The command is handed to the FSM thread, which queues it for the
  // repository's worker thread.
  HgRepoCommandPtr command;
  switch (event.type()) {
#define HG_REPO_COMMAND_CASE(X, name)                                          \
  case EventManager::EventType::HgRepoManager##name:                           \
    command = std::make_shared<HgRepo##name##Command>(                         \
        event.data<HgRepo##name##Command>());                                  \
    break;
    HG_REPO_MANAGER_COMMAND_LIST(HG_REPO_COMMAND_CASE, unused)
#undef HG_REPO_COMMAND_CASE
  default:
    LOG_WARN("Unhandled event {}", _eventManager.eventName(event.type()));
    LOG_PERF(0, "Exit");
    return;
  }

  HgRepoManagerEvent commandEvent(EventType::Command);
  commandEvent.addParam(QVariant::fromValue(command));
  _fsm.send(commandEvent);

  LOG_PERF(0, "Exit");
}

} // namespace HgRepoManager
