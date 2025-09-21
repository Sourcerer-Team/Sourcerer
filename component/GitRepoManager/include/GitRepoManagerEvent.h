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
  DiffFiles,
  DiscardFiles,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  Init,
  ListLocalBranches,
  ListRemoteBranches,
  ListStashes,
  ListSubmodules,
  ListTags,
  OpenRepo,
  Pull,
  Push,
  Setup,
  StageFiles,
  Stash,
  Status,
  UnstageFiles,

  last
};

class GitRepoManagerEvent final : public Fsm::FsmEvent {
public:
  explicit GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
