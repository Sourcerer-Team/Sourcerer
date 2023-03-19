#pragma once

#include <any>

namespace EventManager {

enum EventType {
  Null = 0,

  // ComponentMonitor
  ComponentStarted,

  last
};

struct Event {
  Event(EventType eventType) : _type(eventType) {}

  void addData(std::any data) { _anyData = data; }

  template <typename T> T getData() { return std::any_cast<T>(_anyData); }

  EventType _type;
  std::any _anyData;
};

} // namespace EventManager
