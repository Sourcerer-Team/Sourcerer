#include "EventManager.h"

#include <General.h>
#include <Log.h>

#include <QDateTime>

#define THREAD_ID "EventManager"

namespace EventManager {

EventManager::EventManager() { start(); }

EventManager::~EventManager() { stop(); }

EventManager &EventManager::getInstance() {
  static EventManager instance;
  return instance;
}

void EventManager::threadLoop() {
  General::setThreadName(THREAD_ID);

  while (true) {
    const EventRecord record = _eventsQueue.pop();
    if (record._type == EventRecordType::Quit) {

      break;
    }

    const qint64 start = QDateTime::currentMSecsSinceEpoch();

    record._callback();

    const qint64 end = QDateTime::currentMSecsSinceEpoch();

    LOG_PERF(end - start, "Event execution duration");
  }
}

void EventManager::subscribe(EventType eventType, CallbackType callback) {
  LOCKGUARD(_queueLocker);

  _subscribers[eventType].push_back(callback);
}

void EventManager::unsubscribe(EventType eventType, CallbackType callback) {
  LOCKGUARD(_queueLocker);

  _subscribers[eventType].erase(
      std::remove_if(_subscribers[eventType].begin(),
                     _subscribers[eventType].end(),
                     [=](const CallbackType &cb) {
                       return (cb.target_type() == callback.target_type()) &&
                              cb.target<void (*)(Event)>() ==
                                  callback.target<void (*)(Event)>();
                     }),
      _subscribers[eventType].end());
}

void EventManager::send(Event event) {
  LOCKGUARD(_queueLocker);
  const std::vector<CallbackType> &callbacks = _subscribers[event._type];
  const size_t callbackSize = callbacks.size();

  if (callbackSize > 0) {
    for (size_t i = 0; i < callbackSize; i++) {
      const CallbackType &callback = callbacks[i];
      const auto EventDataFunction = [callback, event]() { callback(event); };

      _eventsQueue.push({EventDataFunction});

      LOG_DEBUG("Event has been queued: {}", static_cast<int>(event._type));
    }
  } else
    LOG_DEBUG("Event has NOT been queued: {}", static_cast<int>(event._type));
}

void EventManager::start() {
  if (_thread.joinable())
    return;

  _thread = std::thread(&EventManager::threadLoop, this);

  LOG_DEBUG("EventManager started.");
}

void EventManager::stop() {
  {
    LOCKGUARD(_queueLocker);

    _eventsQueue.push({nullptr, EventRecordType::Quit});
  }

  _thread.join();

  LOG_DEBUG("EventManager stopped.");
}

} // namespace EventManager
