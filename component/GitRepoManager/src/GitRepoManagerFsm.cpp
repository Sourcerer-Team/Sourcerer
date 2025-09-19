
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
  case StateType::Initialization:
    return "Initialization";
  case StateType::Processing:
    return "Processing";
  default:
    return "Unknown";
  }
}

QString GitRepoManagerFsm::eventName(int event) const {
  switch (static_cast<EventType>(event)) {
  case EventType::ChangeState:
    return "ChangeState";
  case EventType::Stop:
    return "Stop";
  case EventType::CheckoutBranch:
    return "CheckoutBranch";
  case EventType::CloneRepo:
    return "CloneRepo";
  case EventType::CloseRepo:
    return "CloseRepo";
  case EventType::CommitStaged:
    return "CommitStaged";
  case EventType::DiffFiles:
    return "DiffFiles";
  case EventType::DiscardFiles:
    return "DiscardFiles";
  case EventType::GetActiveBranch:
    return "GetActiveBranch";
  case EventType::GetBranchHistory:
    return "GetBranchHistory";
  case EventType::GetCommitBranches:
    return "GetCommitBranches";
  case EventType::GetCommitHeads:
    return "GetCommitHeads";
  case EventType::GetCommitIsHead:
    return "GetCommitIsHead";
  case EventType::GetCommitStatus:
    return "GetCommitStatus";
  case EventType::Init:
    return "Init";
  case EventType::ListLocalBranches:
    return "ListLocalBranches";
  case EventType::ListRemoteBranches:
    return "ListRemoteBranches";
  case EventType::ListSubmodules:
    return "ListSubmodules";
  case EventType::ListTags:
    return "ListTags";
  case EventType::OpenRepo:
    return "OpenRepo";
  case EventType::Pull:
    return "Pull";
  case EventType::Setup:
    return "Setup";
  case EventType::StageFiles:
    return "StageFiles";
  case EventType::Status:
    return "Status";
  case EventType::UnstageFiles:
    return "UnstageFiles";
  default:
    return "Unknown";
  }
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
