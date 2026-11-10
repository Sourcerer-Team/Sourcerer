// MIT License
// Copyright (c) 2026 Sourcerer

#include "Fsm/WaitingState.h"
#include "Fsm/NewsTrackerEvent.h"
#include "Fsm/NewsTrackerFsm.h"

#include <Log.h>

#include <functional>

#define THREAD_ID "WaitingState"

namespace NewsTracker {

namespace Fsm {

WaitingState::WaitingState(NewsTrackerFsm &fsm)
    : NewsTrackerState{StateType::Waiting, fsm} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

WaitingState::~WaitingState() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void WaitingState::enter() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool WaitingState::process(::Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::Initialize: {
    NewsTrackerEvent changeEvent(EventType::ChangeState);
    changeEvent.addParam((int)StateType::Tracking);
    fsm().send(changeEvent);
    break;
  }
  default: {
    LOG_WARN("Unhandled event {}", (int)type);
    return false;
  }
  }

  LOG_PERF(0, "Exit");

  return true;
}

void WaitingState::exit() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace Fsm
} // namespace NewsTracker
