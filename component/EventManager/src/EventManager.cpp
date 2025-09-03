#include "EventManager.h"

#include <General.h>
#include <Log.h>

#include <QDateTime>

#include <functional>

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

void EventManager::subscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  _subscribers[callback._type].push_back(callback);
}

void EventManager::unsubscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  _subscribers[callback._type].erase(
    std::remove_if(_subscribers[callback._type].begin(),
                   _subscribers[callback._type].end(),
                   [=](const EventCallback &cb) {
                     return (cb._callback.target_type() ==
                             callback._callback.target_type()) &&
                            (cb._callback.target<void (*)(Event)>() ==
                             callback._callback.target<void (*)(Event)>()) &&
                            (cb._receiver == callback._receiver);
                   }),
    _subscribers[callback._type].end());
}

int EventManager::startWait() {
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

  const qint64 start = QDateTime::currentMSecsSinceEpoch();
  qint64 end = start;

  bool success = false;
  bool quit = false;

  do {
    const WaitEventRecord eventRecord = queue.pop(timeout, defaultEventRecord);

    switch (eventRecord._type) {
    case EventRecordType::Send: {
      resultEvent = eventRecord._event;
      const bool found = eventTypes.count(resultEvent.type()) > 0;
      if (predicate)
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

    end = QDateTime::currentMSecsSinceEpoch();

  } while (!quit && !success && (end - start) < timeout);

  _queueLocker.lock();

  _waitEventsQueue.erase(index);

  _queueLocker.unlock();

  LOG_PERF(0, "Exit");

  return std::make_tuple(success, resultEvent);
}

std::tuple<bool, Event>
EventManager::waitForAnswer(int token, const QString &uuid, ulong timeout) {
  LOG_PERF(0, "Enter");

  static const std::set<EventType> types{EventType::Error, EventType::Success};

  LOG_PERF(0, "Exit");

  return waitForAnswer(token, uuid, types, timeout);
}

std::tuple<bool, Event>
EventManager::waitForAnswer(int token, const QString &uuid,
                            const std::set<EventType> &types, ulong timeout) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return waitOneOf(
    token, types, [&](Event event) -> bool { return event.uuid() == uuid; },
    timeout);
}

void EventManager::send(Event event) {
  LOCKGUARD(_queueLocker);
  const EventType type = event.type();
  const std::vector<EventCallback> &callbacks = _subscribers[type];
  const size_t callbackSize = callbacks.size();

  if (callbackSize > 0) {
    for (size_t i = 0; i < callbackSize; i++) {
      const EventCallback &callback = callbacks[i];
      const CallbackType &callbackFunc = callback._callback;
      const auto EventDataFunction = std::bind(callbackFunc, event);

      _eventsQueue.push({EventDataFunction});

      LOG_DEBUG("Event has been queued: {}", static_cast<int>(type));
    }
  } else
    LOG_DEBUG("Event has NOT been queued: {}", static_cast<int>(type));

  for (auto &pair : _waitEventsQueue) {
    pair.second.push({EventRecordType::Send, event});
  }
}

void EventManager::start() {
  if (_thread.joinable())
    return;

  _thread = std::thread(&EventManager::threadLoop, this);

  LOG_DEBUG("EventManager started.");
}

void EventManager::stop() {
  // Some strange bug is causing the following code to crahs on exit.
  /*
  {
    LOCKGUARD(_queueLocker);

    _eventsQueue.push({nullptr, EventRecordType::Quit});
  }

  _thread.join();
  */

  LOG_DEBUG("EventManager stopped.");
}

} // namespace EventManager
