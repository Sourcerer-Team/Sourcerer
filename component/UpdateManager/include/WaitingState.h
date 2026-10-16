#pragma once

#include <UpdateManagerState.h>

namespace UpdateManager {

class WaitingState final : public UpdateManagerState {
  Q_OBJECT

public:
  explicit WaitingState(UpdateManagerFsm &fsm);
  ~WaitingState() override;

  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;
};

} // namespace UpdateManager
