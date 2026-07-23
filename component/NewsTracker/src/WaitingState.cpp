#include "WaitingState.h"
#include "NewsTrackerFsm.h"

#include <Log.h>

#include <functional>

#define THREAD_ID "WaitingState"

namespace NewsTracker {

WaitingState::WaitingState(NewsTrackerFsm &fsm)
    : NewsTrackerState{StateType::Waiting, fsm},
      _eventManager{EventManager::EventManager::getInstance()} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

WaitingState::~WaitingState() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&WaitingState::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::ComponentStarted, callbackFunc));

  LOG_PERF(0, "Exit");
}

void WaitingState::enter() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&WaitingState::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::ComponentStarted, callbackFunc));

  EventManager::Event startedEvent(EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

bool WaitingState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(event);
  LOG_PERF(0, "Exit");
  return false;
}

void WaitingState::exit() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&WaitingState::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::ComponentStarted, callbackFunc));

  LOG_PERF(0, "Exit");
}

void WaitingState::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  if (event.type() == EventManager::EventType::ComponentStarted) {
    fsm().changeState(StateType::Tracking);
  }

  LOG_PERF(0, "Exit");
}

} // namespace NewsTracker
