#pragma once

#include <FsmBase.h>
#include <FsmState.h>

namespace HgRepoManager {

class HgRepoManagerFsm : public Fsm::FsmBase {
public:
  HgRepoManagerFsm();
  virtual ~HgRepoManagerFsm();

  void start() override;

  // Fsm interface
protected:
  QString stateName(int state) const override;
  QString eventName(int event) const override;
  void initStateTable(QMap<int, Fsm::FsmState *> &table) override;
  void initChangeTable(QMap<int, QMap<int, bool>> &table) override;
  Fsm::FsmState *
  firstState(const QMap<int, Fsm::FsmState *> &table) const override;
};

} // namespace HgRepoManager
