#include "GitRepoManagerEvent.h"

#include <Log.h>

#define THREAD_ID "GitRepoManagerEvent"

namespace GitRepoManager {

GitRepoManagerEvent::GitRepoManagerEvent(EventType type)
    : Fsm::FsmEvent{static_cast<int>(type)} {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Event type: {}", static_cast<int>(type));
  LOG_PERF(0, "Exit");
}

} // namespace GitRepoManager
