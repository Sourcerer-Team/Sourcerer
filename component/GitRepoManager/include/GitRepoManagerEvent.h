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
  CommitStaged,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  Init,
  ListLocalBranches,
  ListRemoteBranches,
  ListSubmodules,
  ListTags,
  OpenRepo,
  Setup,
  StageFiles,
  Status,
  UnstageFiles,

  last
};

class GitRepoManagerEvent final : public Fsm::FsmEvent {
public:
  explicit GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
