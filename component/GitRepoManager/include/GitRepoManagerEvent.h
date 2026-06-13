#pragma once

#include <FsmEvent.h>

namespace GitRepoManager {

enum class EventType : int {
  Unknown = -2,

  Stop = -1,
  ChangeState = 0,
  AbortMerge,
  AddRemote,
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
  DeleteRemoteBranch,
  DeleteStash,
  DeleteSubmodule,
  DeleteTag,
  DeleteRemoteTag,
  DiffFiles,
  DiffFilesFromCommit,
  DiscardFiles,
  FetchAll,
  GetActiveBranch,
  GetBranchHistory,
  GetCommitBranches,
  GetCommitHeads,
  GetCommitIsHead,
  GetCommitStatus,
  GetFileSizeFromCommit,
  GetIsMergeInProgress,
  GetStashStatus,
  GetSubmodulePath,
  GetUpstream,
  Init,
  ListConflicts,
  ListLocalBranches,
  ListRemoteBranches,
  ListRemoteBranchesWithStatus,
  ListRemotes,
  ListStashes,
  ListSubmodules,
  ListSubmodulesWithStatus,
  ListTags,
  Merge,
  OpenRepo,
  Pull,
  Push,
  PushTag,
  PushAllTags,
  RemoveRemote,
  RenameBranch,
  RenameTag,
  Reset,
  Revert,
  CherryPick,
  SetDefaultRemote,
  Setup,
  SetUpstream,
  StageFiles,
  Stash,
  Status,
  SubmoduleUpdate,
  UnstageFiles,
  UpdateRemote,

  last
};

class GitRepoManagerEvent final : public Fsm::FsmEvent {
public:
  explicit GitRepoManagerEvent(EventType type);
};

} // namespace GitRepoManager
