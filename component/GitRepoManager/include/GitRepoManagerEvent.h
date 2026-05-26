#pragma once

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,

  Stop = -1,
  ChangeState = 0,
  AbortMerge,
  AddSubmodule,
  Amend,
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
  DeleteAllStashes,
  DeleteBranch,
  DeleteStash,
  DeleteSubmodule,
  DeleteTag,
  DiffFiles,
  DiffFilesFromCommit,
  DiscardFiles,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  GetFileSizeFromCommit,
  GetIsMergeInProgress,
  GetSubmodulePath,
  GetUpstream,
  Init,
  ListConflicts,
  ListLocalBranches,
  ListRemoteBranches,
  ListStashes,
  ListSubmodules,
  ListTags,
  Merge,
  OpenRepo,
  Pull,
  Push,
  RenameBranch,
  RenameTag,
  Reset,
  Revert,
  Setup,
  SetUpstream,
  StageFiles,
  Stash,
  Status,
  SubmoduleUpdate,
  UnstageFiles,

  last
};

class GitRepoManagerEvent final : public Fsm::FsmEvent {
public:
  explicit GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
