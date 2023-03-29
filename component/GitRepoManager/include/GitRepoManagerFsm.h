#pragma once

#include <FsmBase.h>
#include <FsmState.h>

namespace GitRepoManager {

class GitRepoManagerFsm : public Fsm::FsmBase {
public:
  GitRepoManagerFsm();
  virtual ~GitRepoManagerFsm();

  // Fsm interface
protected:
  QString stateName(int state) const override;
  QString eventName(int event) const override;
  void initStateTable(QMap<int, Fsm::FsmState *> &table) override;
  void initChangeTable(QMap<int, QMap<int, bool> > &table) override;
};

} // namespace GitRepoManager
