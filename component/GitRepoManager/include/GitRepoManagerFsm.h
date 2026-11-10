// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <FsmBase.h>
#include <FsmState.h>

namespace GitRepoManager {

class GitRepoManagerFsm : public Fsm::FsmBase {
public:
  GitRepoManagerFsm();
  virtual ~GitRepoManagerFsm();

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

} // namespace GitRepoManager
