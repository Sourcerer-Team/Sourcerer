#pragma once

#include "Events.h"

#include <BlockingQueue.h>
#include <LockGuard.h>

#include <QObject>
#include <QThread>

#include <atomic>
#include <functional>
#include <limits>

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <map>
#include <set>
#include <unordered_map>

namespace EventManager {

#define CAPTURE_CALLBACK(type, callback) {type, this, callback}

const unsigned long DEFAULT_WAIT_TIMEOUT =
    (std::numeric_limits<unsigned long>::max)(); // indefinite wait time

enum class EventRecordType { Quit = 0, Send };

typedef std::function<void(Event)> CallbackType;

typedef std::function<void()> EventRecordCallbackType;

struct EventCallback {
  EventCallback() : _type(EventType::Null), _receiver(nullptr) {}
  EventCallback(EventType type, void *receiver, CallbackType callback)
      : _type(type), _receiver(receiver), _callback(callback) {}

  EventType _type;
  void *_receiver;
  CallbackType _callback;
};

struct EventRecord {
  EventRecord() : _type(EventRecordType::Quit), _eventType(EventType::Quit) {}

  EventRecord(EventCallback callbackRecord, EventRecordCallbackType callback,
              EventRecordType type, EventType eventType)
      : _callbackRecord(callbackRecord), _callback(callback), _type(type),
        _eventType(eventType) {}

  EventRecordType _type;
  EventCallback _callbackRecord;
  EventRecordCallbackType _callback;
  EventType _eventType;
};

struct WaitEventRecord {
  WaitEventRecord(EventRecordType type, const Event &event)
      : _type(type), _event(event) {}

  EventRecordType _type;
  Event _event;
};

class EventManager : public QObject {
  Q_OBJECT
public:
  static EventManager &getInstance();

  void subscribe(EventCallback callback);

  void send(Event event);

  void unsubscribe(EventCallback callback);

  int startWait();

  std::tuple<bool, Event>
  waitOneOf(int token, const std::set<EventType> &eventTypes,
            std::function<bool(Event)> predicate = nullptr,
            unsigned long timeout = DEFAULT_WAIT_TIMEOUT);

  std::tuple<bool, Event> waitForAnswer(int token, const QString &uuid,
                                        unsigned long timeout = DEFAULT_WAIT_TIMEOUT);

  std::tuple<bool, Event> waitForAnswer(int token, const QString &uuid,
                                        const std::set<EventType> &types,
                                        unsigned long timeout = DEFAULT_WAIT_TIMEOUT);

  QString eventName(EventType type) const;

private:
  EventManager();

  virtual ~EventManager();

  void threadLoop();

  void start();

  void stop();

private:
  QThread *_thread;
  std::unordered_map<EventType, std::vector<EventCallback>> _subscribers;
  BlockingQueue<EventRecord> _eventsQueue;
  std::map<int, BlockingQueue<WaitEventRecord>> _waitEventsQueue;
  Util::Mutex _queueLocker;

  static std::atomic<int> _waitId;
};

} // namespace EventManager
