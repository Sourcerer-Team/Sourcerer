#pragma once

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,
  Stop = -1,
  ChangeState = 0,
  CheckoutBranch,
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
  Status,
  last
};

class GitRepoManagerEvent final : public Fsm::FsmEvent {
public:
  explicit GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
