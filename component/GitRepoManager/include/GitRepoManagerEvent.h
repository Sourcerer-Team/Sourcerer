#pragma once

#include "GitRepoManagerFsm.h"

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,
  Stop = -1,
  ChangeState = 0,
  CloseRepo,
  ListLocalBranches,
  ListRemoteBranches,
  ListTags,
  OpenRepo,
  Setup,
  last
};

class GitRepoManagerEvent : public Fsm::FsmEvent {
public:
  GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
