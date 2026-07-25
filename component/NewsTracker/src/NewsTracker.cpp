#include "NewsTracker.h"
#include "Fsm/NewsTrackerEvent.h"
#include "NewsTrackerStructs.h"

#include <EventManager.h>
#include <Log.h>

#define THREAD_ID "NewsTracker"

namespace NewsTracker {

NewsTracker::NewsTracker()
    : _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  qRegisterMetaType<Structs::NewsItem>("NewsItem");

  LOG_PERF(0, "Exit");
}

NewsTracker::~NewsTracker() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void NewsTracker::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  Fsm::NewsTrackerEvent initializeEvent(Fsm::EventType::Initialize);
  _fsm.send(initializeEvent);

  const qlonglong pollInterval =
      _configManager.getLong(ConfigManager::ConfigKey::NewsTrackerPollInterval);

  _timer.setInterval([&]() {
    Fsm::NewsTrackerEvent retrieveEvent(Fsm::EventType::RetrieveNews);
    _fsm.send(retrieveEvent);
  }, pollInterval);

  const EventManager::Event startedEvent(
      EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void NewsTracker::stop() {
  LOG_PERF(0, "Enter");

  _timer.stop();

  _fsm.stop();

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  _eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

} // namespace NewsTracker
