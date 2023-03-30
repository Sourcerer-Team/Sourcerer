#include "InitializationState.h"
#include "GitRepoManagerEvent.h"

#include <Log.h>

#define THREAD_ID "InitializationState"

namespace GitRepoManager {

InitializationState::InitializationState(GitRepoManagerFsm &fsm)
    : GitRepoManagerState{StateType::Initialization, fsm} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void InitializationState::enter() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool InitializationState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::Setup: {
    GitRepoManagerEvent changeEvent(EventType::ChangeState);
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

void InitializationState::exit() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
