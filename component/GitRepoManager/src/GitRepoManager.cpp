#include "GitRepoManager.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerStructs.h"

#include <Log.h>

#include <functional>

#define THREAD_ID "GitRepoManager"

namespace GitRepoManager {

GitRepoManager::GitRepoManager()
    : _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitRepoManager::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(EventManager::EventType::GitRepoManagerOpenRepo,
                          callbackFunc);

  LOG_PERF(0, "Exit");
}

GitRepoManager::~GitRepoManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void GitRepoManager::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManager::stop() {
  LOG_PERF(0, "Enter");

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  _eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

void GitRepoManager::eventCallback(const EventManager::Event &event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Received event of type {}", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerOpenRepo: {
    const Structs::GitRepoOpenCommand command =
        event.data<Structs::GitRepoOpenCommand>();
    GitRepoManagerEvent openRepoEvent(EventType::OpenRepo);
    openRepoEvent.addParam(QVariant::fromValue(command));
    _fsm.send(openRepoEvent);
    break;
  }
  default: {
    LOG_WARN("Unknown event type.")
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
