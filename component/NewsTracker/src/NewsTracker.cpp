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

  const auto callbackFunc =
      std::bind(&NewsTracker::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::ApplicationUpdated, callbackFunc));

  LOG_PERF(0, "Exit");
}

NewsTracker::~NewsTracker() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&NewsTracker::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::ApplicationUpdated, callbackFunc));

  LOG_PERF(0, "Exit");
}

void NewsTracker::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  Fsm::NewsTrackerEvent initializeEvent(Fsm::EventType::Initialize);
  _fsm.send(initializeEvent);

  const qlonglong pollInterval =
      _configManager.getLong(ConfigManager::ConfigKey::NewsTrackerPollInterval);

  _timer.setInterval(
      [&]() {
        Fsm::NewsTrackerEvent retrieveEvent(Fsm::EventType::RetrieveNews);
        _fsm.send(retrieveEvent);
      },
      pollInterval);

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

void NewsTracker::eventCallback(const EventManager::Event &event) {
  LOG_PERF(0, "Enter");

  switch (event.type()) {
  case EventManager::EventType::ApplicationUpdated: {
    // Update news is only marked as seen once the update has been staged, so a
    // failed download is retried on the next poll.
    const auto news = event.data<Structs::NewsItem>();

    Fsm::NewsTrackerEvent markSeenEvent(Fsm::EventType::MarkNewsSeen);
    markSeenEvent.addParam(news.id);
    _fsm.send(markSeenEvent);
    break;
  }
  default: {
    LOG_WARN("Unknown event type.")
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace NewsTracker
