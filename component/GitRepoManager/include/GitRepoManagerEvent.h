#pragma once

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,

  Stop = -1,
  ChangeState = 0,
  ApplyPatch,
  ApplyStash,
  CheckoutBranch,
  CheckoutCommit,
  CloneRepo,
  CloseRepo,
  CommitStaged,
  CreateBranchFromCommit,
  CreatePatchFromCommit,
  CreateTagFromCommit,
  DeleteBranch,
  DeleteStash,
  DiffFiles,
  DiscardFiles,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  GetUpstream,
  Init,
  ListLocalBranches,
  ListRemoteBranches,
  ListStashes,
  ListSubmodules,
  ListTags,
  OpenRepo,
  Pull,
  Push,
  RenameBranch,
  Reset,
  Revert,
  Setup,
  SetUpstream,
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
