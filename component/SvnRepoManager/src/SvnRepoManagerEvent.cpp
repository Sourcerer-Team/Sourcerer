// MIT License
// Copyright (c) 2026 Sourcerer

#include "SvnRepoManagerEvent.h"

#include <Log.h>

#define THREAD_ID "SvnRepoManagerEvent"

namespace SvnRepoManager {

SvnRepoManagerEvent::SvnRepoManagerEvent(EventType type)
    : Fsm::FsmEvent{static_cast<int>(type)} {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Event type: {}", static_cast<int>(type));
  LOG_PERF(0, "Exit");
}

} // namespace SvnRepoManager
