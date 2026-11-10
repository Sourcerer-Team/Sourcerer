// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <FsmEvent.h>

namespace UpdateManager {

enum class EventType : int {
  Unknown = -2,

  Stop = -1,
  ChangeState = 0,

  ProcessUpdate,
  UpdateNewsReceived,

  last
};

class UpdateManagerEvent final : public ::Fsm::FsmEvent {
public:
  explicit UpdateManagerEvent(EventType type);
};

} // namespace UpdateManager
