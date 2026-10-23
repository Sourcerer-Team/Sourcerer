#include "HgRepoManagerInitializationState.h"
#include "HgRepoManagerEvent.h"

#include <Log.h>

#define THREAD_ID "HgRepoManagerInitializationState"

namespace HgRepoManager {

HgRepoManagerInitializationState::HgRepoManagerInitializationState(
    HgRepoManagerFsm &fsm)
    : HgRepoManagerState{StateType::Initialization, fsm} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void HgRepoManagerInitializationState::enter() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool HgRepoManagerInitializationState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::Setup: {
    HgRepoManagerEvent changeEvent(EventType::ChangeState);
    changeEvent.addParam((int)StateType::Processing);
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

void HgRepoManagerInitializationState::exit() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace HgRepoManager
