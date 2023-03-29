
#include "GitRepoManagerEvent.h"
#include "GitRepoManagerState.h"
#include "GitRepoManagerFsm.h"
#include "InitializationState.h"
#include "ProcessingState.h"

namespace GitRepoManager {

GitRepoManager::GitRepoManagerFsm::GitRepoManagerFsm() : Fsm::FsmBase() {}

GitRepoManagerFsm::~GitRepoManagerFsm() {}

QString GitRepoManagerFsm::stateName(int state) const
{
  switch(static_cast<StateType>(state)) {
  case StateType::Initialization: return "Initialization";
  case StateType::Processing: return "Processing";
  default: return "Unknown";
  }
}

QString GitRepoManagerFsm::eventName(int event) const
{
  switch(static_cast<EventType>(event)) {
  case EventType::ChangeState: return "ChangeState";
  case EventType::Stop: return "Stop";
  default: return "Unknown";
  }
}

void GitRepoManagerFsm::initStateTable(QMap<int, Fsm::FsmState *> &table)
{
  table[(int)StateType::Initialization] = new InitializationState(*this);
  table[(int)StateType::Processing] = new ProcessingState(*this);
}

void GitRepoManagerFsm::initChangeTable(QMap<int, QMap<int, bool> > &table)
{
  table[(int)StateType::Initialization][(int)StateType::Processing] = true;
  table[(int)StateType::Processing][(int)StateType::Initialization] = true;
}

} // namespace GitRepoManager
