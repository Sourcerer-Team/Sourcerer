// MIT License
// Copyright (c) 2026 Sourcerer

#include "Fsm/NewsTrackerEvent.h"

#include <Log.h>

#define THREAD_ID "NewsTrackerEvent"

namespace NewsTracker {

namespace Fsm {

NewsTrackerEvent::NewsTrackerEvent(EventType type)
    : ::Fsm::FsmEvent{static_cast<int>(type)} {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Event type: {}", static_cast<int>(type));
  LOG_PERF(0, "Exit");
}

} // namespace Fsm
} // namespace NewsTracker