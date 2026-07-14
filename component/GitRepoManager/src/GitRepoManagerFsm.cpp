
#include "GitRepoManagerFsm.h"
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerState.h"
#include "InitializationState.h"
#include "ProcessingState.h"

#include <Log.h>

#define THREAD_ID "GitRepoManagerFsm"

namespace GitRepoManager {

GitRepoManager::GitRepoManagerFsm::GitRepoManagerFsm() : Fsm::FsmBase() {}

GitRepoManagerFsm::~GitRepoManagerFsm() {}

void GitRepoManagerFsm::start() {
  LOG_PERF(0, "Enter");

  Fsm::FsmBase::start();

  GitRepoManagerEvent setupEvent(EventType::Setup);
  send(setupEvent);

  LOG_PERF(0, "Exit");
}

QString GitRepoManagerFsm::stateName(int state) const {
  switch (static_cast<StateType>(state)) {
  case StateType::Unknown:
    return "Unknown";
  case StateType::Initialization:
    return "Initialization";
  case StateType::Processing:
    return "Processing";
  case StateType::last:
    return "last";
  }

  return "Unknown";
}

QString GitRepoManagerFsm::eventName(int event) const {
  switch (static_cast<EventType>(event)) {
  case EventType::Unknown:
    return "Unknown";
  case EventType::Stop:
    return "Stop";
  case EventType::ChangeState:
    return "ChangeState";
  case EventType::AbortMerge:
    return "AbortMerge";
  case EventType::AddRemote:
    return "AddRemote";
  case EventType::AddSubmodule:
    return "AddSubmodule";
  case EventType::AddTarget:
    return "AddTarget";
  case EventType::Amend:
    return "Amend";
  case EventType::ApplyPatch:
    return "ApplyPatch";
  case EventType::ApplyStash:
    return "ApplyStash";
  case EventType::CheckoutBranch:
    return "CheckoutBranch";
  case EventType::CheckoutCommit:
    return "CheckoutCommit";
  case EventType::CloneRepo:
    return "CloneRepo";
  case EventType::CloseRepo:
    return "CloseRepo";
  case EventType::CommitStaged:
    return "CommitStaged";
  case EventType::CreateBranchFromCommit:
    return "CreateBranchFromCommit";
  case EventType::CreatePatchFromCommit:
    return "CreatePatchFromCommit";
  case EventType::CreateTagFromCommit:
    return "CreateTagFromCommit";
  case EventType::DeleteAllStashes:
    return "DeleteAllStashes";
  case EventType::DeleteBranch:
    return "DeleteBranch";
  case EventType::DeleteRemoteBranch:
    return "DeleteRemoteBranch";
  case EventType::DeleteStash:
    return "DeleteStash";
  case EventType::DeleteSubmodule:
    return "DeleteSubmodule";
  case EventType::DeleteTag:
    return "DeleteTag";
  case EventType::DeleteTarget:
    return "DeleteTarget";
  case EventType::DeleteRemoteTag:
    return "DeleteRemoteTag";
  case EventType::DiffFiles:
    return "DiffFiles";
  case EventType::DiffFilesFromCommit:
    return "DiffFilesFromCommit";
  case EventType::DiscardFiles:
    return "DiscardFiles";
  case EventType::EditTarget:
    return "EditTarget";
  case EventType::FetchAll:
    return "FetchAll";
  case EventType::FetchRemote:
    return "FetchRemote";
  case EventType::GetActiveBranch:
    return "GetActiveBranch";
  case EventType::GetBranchHistory:
    return "GetBranchHistory";
  case EventType::GetCommitBranches:
    return "GetCommitBranches";
  case EventType::GetCommitTags:
    return "GetCommitTags";
  case EventType::GetCommitHeads:
    return "GetCommitHeads";
  case EventType::GetCommitIsHead:
    return "GetCommitIsHead";
  case EventType::GetCommitStatus:
    return "GetCommitStatus";
  case EventType::GetFileSizeFromCommit:
    return "GetFileSizeFromCommit";
  case EventType::GetIsMergeInProgress:
    return "GetIsMergeInProgress";
  case EventType::GetStashStatus:
    return "GetStashStatus";
  case EventType::GetSubmodulePath:
    return "GetSubmodulePath";
  case EventType::GetUpstream:
    return "GetUpstream";
  case EventType::Init:
    return "Init";
  case EventType::ListConflicts:
    return "ListConflicts";
  case EventType::ListLocalBranches:
    return "ListLocalBranches";
  case EventType::ListRemoteBranches:
    return "ListRemoteBranches";
  case EventType::ListRemoteBranchesWithStatus:
    return "ListRemoteBranchesWithStatus";
  case EventType::ListRemotes:
    return "ListRemotes";
  case EventType::ListStashes:
    return "ListStashes";
  case EventType::ListSubmodules:
    return "ListSubmodules";
  case EventType::ListSubmodulesWithStatus:
    return "ListSubmodulesWithStatus";
  case EventType::ListTags:
    return "ListTags";
  case EventType::Merge:
    return "Merge";
  case EventType::Rebase:
    return "Rebase";
  case EventType::OpenRepo:
    return "OpenRepo";
  case EventType::Pull:
    return "Pull";
  case EventType::Push:
    return "Push";
  case EventType::PushTag:
    return "PushTag";
  case EventType::PushAllTags:
    return "PushAllTags";
  case EventType::RemoveRemote:
    return "RemoveRemote";
  case EventType::RenameBranch:
    return "RenameBranch";
  case EventType::RenameTag:
    return "RenameTag";
  case EventType::Reset:
    return "Reset";
  case EventType::Revert:
    return "Revert";
  case EventType::CherryPick:
    return "CherryPick";
  case EventType::SetDefaultRemote:
    return "SetDefaultRemote";
  case EventType::SetDefaultTarget:
    return "SetDefaultTarget";
  case EventType::Setup:
    return "Setup";
  case EventType::SetUpstream:
    return "SetUpstream";
  case EventType::StageFiles:
    return "StageFiles";
  case EventType::Stash:
    return "Stash";
  case EventType::Status:
    return "Status";
  case EventType::SubmoduleUpdate:
    return "SubmoduleUpdate";
  case EventType::UnstageFiles:
    return "UnstageFiles";
  case EventType::UpdateRemote:
    return "UpdateRemote";
  }

  return "Unknown";
}

void GitRepoManagerFsm::initStateTable(QMap<int, Fsm::FsmState *> &table) {
  LOG_PERF(0, "Enter");

  table[(int)StateType::Initialization] = new InitializationState(*this);
  table[(int)StateType::Processing] = new ProcessingState(*this);

  LOG_PERF(0, "Exit");
}

void GitRepoManagerFsm::initChangeTable(QMap<int, QMap<int, bool>> &table) {
  LOG_PERF(0, "Enter");

  table[(int)StateType::Initialization][(int)StateType::Processing] = true;
  table[(int)StateType::Processing][(int)StateType::Initialization] = true;

  LOG_PERF(0, "Exit");
}

Fsm::FsmState *
GitRepoManagerFsm::firstState(const QMap<int, Fsm::FsmState *> &table) const {
  LOG_PERF(0, "Enter");

  const auto it = table.constFind((int)StateType::Initialization);
  if (it == table.constEnd()) {
    LOG_PERF(0, "Exit");
    return nullptr;
  } else {
    LOG_PERF(0, "Exit");
    return *it;
  }
}

} // namespace GitRepoManager
