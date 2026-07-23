#include "NewsTracker.h"

#include <EventManager.h>
#include <Log.h>

#define THREAD_ID "NewsTracker"

namespace NewsTracker {

NewsTracker::NewsTracker() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

NewsTracker::~NewsTracker() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void NewsTracker::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  LOG_PERF(0, "Exit");
}

void NewsTracker::stop() {
  LOG_PERF(0, "Enter");

  _fsm.stop();

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  EventManager::EventManager::getInstance().send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

} // namespace NewsTracker
