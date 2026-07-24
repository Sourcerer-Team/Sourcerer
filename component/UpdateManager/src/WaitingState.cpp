#include "WaitingState.h"
#include "UpdateManagerEvent.h"
#include "UpdateManagerFsm.h"

#include <NewsTrackerStructs.h>

#include <Log.h>

#define THREAD_ID "WaitingState"

namespace UpdateManager {


WaitingState::WaitingState(UpdateManagerFsm &fsm)
    : UpdateManagerState{StateType::Waiting, fsm} {
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

bool WaitingState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::UpdateNewsReceived: {
    UpdateManagerEvent changeEvent(EventType::ChangeState);
    changeEvent.addParam((int)StateType::Updating);
    fsm().send(changeEvent);

    const auto news = event.nextParam<NewsTracker::Structs::NewsItem>();

    UpdateManagerEvent processEvent(EventType::ProcessUpdate);
    processEvent.addParam(QVariant::fromValue(news));
    fsm().send(processEvent);
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

} // namespace UpdateManager
