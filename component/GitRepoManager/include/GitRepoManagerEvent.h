#pragma once

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,
  Stop = -1,
  ChangeState = 0,
  CloneRepo,
  CloseRepo,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  Init,
  ListLocalBranches,
  ListRemoteBranches,
  ListSubmodules,
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
