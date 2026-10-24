#pragma once

#include <FsmEvent.h>

namespace HgRepoManager {

enum class EventType : int {
  Unknown = -2,

  Stop = -1,
  ChangeState = 0,
  Setup,
  // A request for a repository: a Structs::HgRepoCommandPtr parameter.
  Command,
  // A repository's worker thread ended: its uuid.
  RetireContext,
  // A clone or init thread ended: its task id.
  RetireTask,

  last
};

class HgRepoManagerEvent final : public Fsm::FsmEvent {
public:
  explicit HgRepoManagerEvent(EventType type);
};

} // namespace HgRepoManager
