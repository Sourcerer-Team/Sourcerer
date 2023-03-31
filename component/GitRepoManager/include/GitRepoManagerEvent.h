#pragma once

#include "GitRepoManagerFsm.h"

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,
  Stop = -1,
  ChangeState = 0,
  Setup,
  CloseRepo,
  OpenRepo,
  last
};

class GitRepoManagerEvent : public Fsm::FsmEvent {
public:
  GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
