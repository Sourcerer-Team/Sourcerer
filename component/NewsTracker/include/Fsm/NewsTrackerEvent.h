#pragma once

#include <FsmEvent.h>

namespace NewsTracker {

namespace Fsm {

enum class EventType : int {
  Unknown = -2,

  Stop = -1,
  ChangeState = 0,

  Initialize,
  RetrieveNews,
  MarkNewsSeen,

  last
};

class NewsTrackerEvent final : public ::Fsm::FsmEvent {
public:
  explicit NewsTrackerEvent(EventType type);
};

} // namespace Fsm
} // namespace NewsTracker
