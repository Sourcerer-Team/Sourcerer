// MIT License
// Copyright (c) 2026 Sourcerer

#include "EventManager.h"

#include <General.h>
#include <Log.h>

#include <QDateTime>

#include <functional>

#define THREAD_ID "EventManager"

namespace EventManager {

std::atomic<int> EventManager::_waitId = 0;

static bool callbackComparator(EventCallback &callback0,
                               const EventCallback &callback1) {
  return (callback1._callback.target_type() ==
          callback0._callback.target_type()) &&
         (callback1._callback.target<void (*)(Event)>() ==
          callback0._callback.target<void (*)(Event)>()) &&
         (callback1._receiver == callback0._receiver);
}

EventManager::EventManager() : _thread(nullptr) {
  start();
}

EventManager::~EventManager() {
  stop();
}

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

    const auto predicate = std::bind(
        &callbackComparator, record._callbackRecord, std::placeholders::_1);

    _queueLocker.lock();

    const auto itSubscribersBegin = _subscribers[record._eventType].begin();
    const auto itSubscribersEnd = _subscribers[record._eventType].end();

    const auto it =
        std::find_if(itSubscribersBegin, itSubscribersEnd, predicate);

    if (it == itSubscribersEnd) {
      _queueLocker.unlock();
      LOG_DEBUG("Skipping event {}", eventName(record._eventType));
      continue;
    }

    _queueLocker.unlock();

    const qint64 start = QDateTime::currentMSecsSinceEpoch();

    LOG_DEBUG("Dispatching event {}", eventName(record._eventType));

    record._callback();

    const qint64 end = QDateTime::currentMSecsSinceEpoch();

    LOG_PERF(end - start, "Event execution duration");
  }
}

void EventManager::subscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  LOG_DEBUG("Subscribing to event {}", eventName(callback._type));

  _subscribers[callback._type].push_back(callback);
}

void EventManager::unsubscribe(EventCallback callback) {
  LOCKGUARD(_queueLocker);

  LOG_DEBUG("Unsubscribing from event {}", eventName(callback._type));

  const auto predicate =
      std::bind(&callbackComparator, callback, std::placeholders::_1);

  const auto itNewEnd =
      std::remove_if(_subscribers[callback._type].begin(),
                     _subscribers[callback._type].end(), predicate);

  LOG_DEBUG("Subscription removed: {}",
            itNewEnd != _subscribers[callback._type].end());

  _subscribers[callback._type].erase(itNewEnd,
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

  LOG_DEBUG("waitOneOf: token={}, timeout={}, eventTypes count={}", token,
            timeout, eventTypes.size());

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

  LOG_DEBUG("waitOneOf: token={}, success={}, elapsed={}ms", token, success,
            (end - start));

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

QString EventManager::eventName(const EventType type) const {
  switch (type) {
#define EVENT_TYPE_NAME(name)                                                  \
  case EventType::name:                                                        \
    return #name;
    EVENT_TYPE_LIST(EVENT_TYPE_NAME)
#undef EVENT_TYPE_NAME
  }

  return QString("Unknown(%1)").arg(static_cast<int>(type));
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

      _eventsQueue.push(
          {callback, EventDataFunction, EventRecordType::Send, event.type()});

      LOG_DEBUG("Event has been queued: {}", eventName(type));
    }
  } else
    LOG_DEBUG("Event has NOT been queued: {}", eventName(type));

  size_t queueCount = 0;
  for (auto &pair : _waitEventsQueue) {
    pair.second.push({EventRecordType::Send, event});
    queueCount++;
  }

  if (queueCount == 0) {
    LOG_WARN("Event '{}' has no wait queues to deliver to (callbacks: {})",
             eventName(type), callbackSize);
  } else {
    LOG_DEBUG("Event '{}' added to {} wait queue(s)", eventName(type),
              queueCount);
  }
}

void EventManager::start() {
  if (_thread)
    return;

  _thread = QThread::create([&]() { threadLoop(); });

  connect(_thread, &QThread::finished, _thread, &QObject::deleteLater);

  _thread->start();

  LOG_DEBUG("EventManager started.");
}

void EventManager::stop() {
  {
    LOCKGUARD(_queueLocker);

    _eventsQueue.push({{}, nullptr, EventRecordType::Quit, EventType::Quit});
  }

  _thread->wait();

  _thread = nullptr;

  LOG_DEBUG("EventManager stopped.");
}

} // namespace EventManager
