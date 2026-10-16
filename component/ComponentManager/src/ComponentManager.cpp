#include "ComponentManager.h"

#include <General.h>
#include <Log.h>

#include <chrono>
#include <functional>
#include <thread>

#define THREAD_ID "ComponentManager"

namespace ComponentManager {

using namespace std::chrono_literals;

ComponentManager::ComponentManager()
    : _started(false), _stopped(false),
      _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const auto waitFunc =
      std::bind(&ComponentManager::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::ComponentStarted, waitFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::ComponentStopped, waitFunc));

  LOG_PERF(0, "Exit");
}

ComponentManager::~ComponentManager() {
  LOG_PERF(0, "Enter");

  const auto waitFunc =
      std::bind(&ComponentManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::ComponentStarted, waitFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::ComponentStopped, waitFunc));

  LOG_PERF(0, "Exit");
}

void ComponentManager::stopAll() {
  LOG_PERF(0, "Enter");

  for (int i = _process.length() - 1; i >= 0; --i) {
    Component *component = _process[i];
    if (component) {
      _stopped = false;
      component->stop();
      waitForComponentToStop();
      delete component;
    }
  }

  _process.clear();

  for (int i = _threads.length() - 1; i >= 0; --i) {
    Component *component = _threads[i];
    if (component) {
      _stopped = false;
      component->stop();
      waitForComponentToStop();
      delete component;
    }
  }

  _threads.clear();

  LOG_PERF(0, "Exit");
}

void ComponentManager::waitForComponentToStart() {
  LOG_PERF(0, "Enter");

  using namespace std::chrono;
  const uint64_t start =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
  uint64_t end = start;

  while (!_started && (end - start < 30000)) {
    std::this_thread::sleep_for(1ms);
    end = duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count();
  }

  if (!_started) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Component didn't start in {} milliseconds", end - start);
  } else {
    LOG_DEBUG("Component started in {} milliseconds", end - start);
  }

  LOG_PERF(0, "Exit");
}

void ComponentManager::waitForComponentToStop() {
  LOG_PERF(0, "Enter");

  using namespace std::chrono;
  const uint64_t start =
      duration_cast<milliseconds>(system_clock::now().time_since_epoch())
          .count();
  uint64_t end = start;

  while (!_stopped && (end - start < 30000)) {
    std::this_thread::sleep_for(1ms);
    end = duration_cast<milliseconds>(system_clock::now().time_since_epoch())
              .count();
  }

  if (!_stopped) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Component didn't start in {} milliseconds", end - start);
  } else {
    LOG_DEBUG("Component started in {} milliseconds", end - start);
  }

  LOG_PERF(0, "Exit");
}

void ComponentManager::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Received event  {}.", _eventManager.eventName(eventType));

  switch (eventType) {
  case EventManager::EventType::ComponentStarted: {
    _started = true;
    break;
  }
  case EventManager::EventType::ComponentStopped: {
    _stopped = true;
    break;
  }
  default: {
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace ComponentManager
