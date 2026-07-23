#pragma once

#include "NewsTrackerState.h"

#include <FsmBase.h>
#include <FsmState.h>

namespace NewsTracker {

class NewsTrackerFsm : public Fsm::FsmBase {
public:
  NewsTrackerFsm();
  virtual ~NewsTrackerFsm();

  void changeState(StateType state);

protected:
  QString stateName(int state) const override;
  QString eventName(int event) const override;
  void initStateTable(QMap<int, Fsm::FsmState *> &table) override;
  void initChangeTable(QMap<int, QMap<int, bool>> &table) override;
  Fsm::FsmState *
  firstState(const QMap<int, Fsm::FsmState *> &table) const override;
};

} // namespace NewsTracker
