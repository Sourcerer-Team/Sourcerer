#include "EventManager.h"

#include <General.h>
#include <Log.h>

#include <chrono>

#define THREAD_ID "EventManager"

namespace EventManager {

std::atomic<int> EventManager::_waitId = 0;

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
    if (record._type == EventRecordType::Quit)
      break;

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

int EventManager::startWait()
{
  LOG_PERF(0, "Enter");

  LOCKGUARD(_queueLocker);

  const int index = _waitId++;

  const auto &queue = _waitEventsQueue[index];

  LOG_PERF(0, "Exit");

  return index;
}

std::tuple<bool, Event>
EventManager::waitOneOf(int token, const std::set<EventType> &eventTypes,
                        std::function<bool(Event)> predicate, ulong timeout) {
  LOG_PERF(0, "Enter");

  const int index = token;

  _queueLocker.lock();

  auto &queue = _waitEventsQueue[index];

  _queueLocker.unlock();

  const WaitEventRecord defaultEventRecord(
      {EventRecordType::Quit, {EventType::Null}});

  Event resultEvent(EventType::Null);

  const std::chrono::high_resolution_clock::time_point start =
      std::chrono::high_resolution_clock::now();
  std::chrono::high_resolution_clock::time_point end = start;

  bool success = false;
  bool quit = false;

  do {
    const WaitEventRecord eventRecord = queue.pop(timeout, defaultEventRecord);

    switch (eventRecord._type) {
    case EventRecordType::Send: {
      resultEvent = eventRecord._event;
      const bool found = eventTypes.count(resultEvent.type()) > 0;
      if(predicate)
          success = found && predicate(resultEvent);
      else
          success = found;

      break;
    }
    case EventRecordType::Quit: {
      quit = true;
      break;
    }
    default: {
      LOG_WARN("Internal error. Unknown command type.");
      quit = true;
      break;
    }
    }

    end = std::chrono::high_resolution_clock::now();

  } while (!quit && !success && (end - start).count() < timeout);

  _queueLocker.lock();

  _waitEventsQueue.erase(index);

  _queueLocker.unlock();

  LOG_PERF(0, "Exit");

  return std::make_tuple(success, resultEvent);
}

void EventManager::send(Event event) {
  LOCKGUARD(_queueLocker);
  const EventType type = event.type();
  const std::vector<CallbackType> &callbacks = _subscribers[type];
  const size_t callbackSize = callbacks.size();

  if (callbackSize > 0) {
    for (size_t i = 0; i < callbackSize; i++) {
      const CallbackType &callback = callbacks[i];
      const auto EventDataFunction = [callback, event]() { callback(event); };

      _eventsQueue.push({EventDataFunction});

      for (auto &pair : _waitEventsQueue) {
        pair.second.push({EventRecordType::Send, event});
      }

      LOG_DEBUG("Event has been queued: {}", static_cast<int>(type));
    }
  } else
    LOG_DEBUG("Event has NOT been queued: {}", static_cast<int>(type));
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

    const Event event(EventType::Null);

    for (auto &pair : _waitEventsQueue) {
      pair.second.push({EventRecordType::Quit, event});
    }
  }

  _thread.join();

  LOG_DEBUG("EventManager stopped.");
}

} // namespace EventManager
