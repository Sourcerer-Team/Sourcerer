#include "GitRepoManagerEvent.h"
#include "ProcessingState.h"

#include <GitRepo.h>
#include <Log.h>

#define THREAD_ID "ProcessingState"

namespace GitRepoManager {

ProcessingState::ProcessingState(GitRepoManagerFsm &fsm)
    : GitRepoManagerState{StateType::Processing, fsm} {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void ProcessingState::enter() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool ProcessingState::process(const Fsm::FsmEvent &event) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::Setup: {
    break;
  }
  default: {
    LOG_WARN("Unhandled event {}", (int)type);
    return false;
  }
  }

  return true;
}

void ProcessingState::exit() {
  LOG_PERF(0, "Enter");

  GitRepo::shutdownForCurrentThread();

  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
