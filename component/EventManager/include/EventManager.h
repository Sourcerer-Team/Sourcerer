#pragma once

#include <atomic>
#include <map>
#include <set>
#include <thread>
#include <unordered_map>

#include "Events.h"

#include <BlockingQueue.h>
#include <LockGuard.h>

namespace EventManager {

const ulong DEFAULT_WAIT_TIMEOUT = 2 * 60 * 1000; // 2 minutes

enum class EventRecordType { Quit = 0, Send };

typedef std::function<void(Event)> CallbackType;

struct EventRecord {
  EventRecord(std::function<void()> callback,
              EventRecordType type = EventRecordType::Send)
      : _callback(callback), _type(type) {}

  EventRecordType _type;
  std::function<void()> _callback;
};

struct WaitEventRecord {
  WaitEventRecord(EventRecordType type, const Event &event)
      : _type(type), _event(event) {}

  EventRecordType _type;
  Event _event;
};

class EventManager {
public:
  static EventManager &getInstance();

  void subscribe(EventType eventType, CallbackType callback);

  void send(Event event);

  void unsubscribe(EventType eventType, CallbackType callback);

  int startWait();

  std::tuple<bool, Event>
  waitOneOf(int token, const std::set<EventType> &eventTypes,
            std::function<bool(Event)> predicate = nullptr,
            ulong timeout = DEFAULT_WAIT_TIMEOUT);

private:
  EventManager();

  virtual ~EventManager();

  void threadLoop();

  void start();

  void stop();

private:
  std::thread _thread;
  std::unordered_map<EventType, std::vector<CallbackType>> _subscribers;
  BlockingQueue<EventRecord> _eventsQueue;
  std::map<int, BlockingQueue<WaitEventRecord>> _waitEventsQueue;
  Util::Mutex _queueLocker;

  static std::atomic<int> _waitId;
};

} // namespace EventManager
