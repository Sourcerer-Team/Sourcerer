#pragma once

#include <thread>
#include <unordered_map>
#include <vector>

#include "Events.h"

#include <BlockingQueue.h>
#include <LockGuard.h>

namespace EventManager {

enum class EventRecordType { Quit = 0, Send };

typedef std::function<void(Event)> CallbackType;

struct EventRecord {
  EventRecord(std::function<void()> callback,
              EventRecordType type = EventRecordType::Send)
      : _callback(callback), _type(type) {}

  EventRecordType _type;
  std::function<void()> _callback;
};

class EventManager {
public:
  static EventManager &getInstance();

  void subscribe(EventType eventType, CallbackType callback);

  void send(Event event);

  void unsubscribe(EventType eventType, CallbackType callback);

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
  Util::Mutex _queueLocker;
};

} // namespace EventManager
