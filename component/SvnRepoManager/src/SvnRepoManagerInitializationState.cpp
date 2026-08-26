#include "SvnRepoManagerInitializationState.h"
#include "SvnRepoManagerEvent.h"

#include <Log.h>

#define THREAD_ID "SvnRepoManagerInitializationState"

namespace SvnRepoManager {

SvnRepoManagerInitializationState::SvnRepoManagerInitializationState(
    SvnRepoManagerFsm &fsm)
    : SvnRepoManagerState{StateType::Initialization, fsm} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void SvnRepoManagerInitializationState::enter() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool SvnRepoManagerInitializationState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::Setup: {
    SvnRepoManagerEvent changeEvent(EventType::ChangeState);
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

void SvnRepoManagerInitializationState::exit() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace SvnRepoManager
