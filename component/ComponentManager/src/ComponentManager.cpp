#include "ComponentManager.h"

#include <General.h>
#include <Log.h>

#include <chrono>
#include <functional>
#include <iostream>
#include <list>

#define THREAD_ID "ComponentManager"

namespace ComponentManager {

using namespace std::chrono_literals;

ComponentManager::ComponentManager() : _done(false) {
  const auto waitFunc =
      std::bind(&ComponentManager::eventCallback, this, std::placeholders::_1);
  EventManager::EventManager &controller =
      EventManager::EventManager::getInstance();
  controller.subscribe(EventManager::EventType::ComponentStarted, waitFunc);
}

ComponentManager::~ComponentManager() {}

void ComponentManager::stopAll() {
  _done = true;

  exit(0);
}

void ComponentManager::waitForComponent() {
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
}

void ComponentManager::eventCallback(EventManager::Event event) {
  switch (event.type()) {
  case EventManager::EventType::ComponentStarted: {
    _started = true;
    break;
  }
  default: {
    break;
  }
  }
}

} // namespace ComponentManager
