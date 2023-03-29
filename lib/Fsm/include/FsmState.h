#pragma once

#include "FsmEvent.h"

namespace Fsm {

class FsmState {
public:
  FsmState(int type) : _type(type) {}

  virtual ~FsmState() {}

  virtual void enter() = 0;
  virtual bool process(const FsmEvent &event) = 0;
  virtual void exit() = 0;

  int type() { return _type; }

private:
  int _type;
};

} // namespace Fsm
