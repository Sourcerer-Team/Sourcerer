// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "HgRepoManagerFsm.h"

#include <Component.h>
#include <EventManager.h>

#include <memory>

namespace HgRepoManager {

namespace Workers {
class AskPassServer;
}

class HgRepoManager : public ComponentManager::Component {
public:
  HgRepoManager();
  virtual ~HgRepoManager();

  void start() override;
  void stop() override;

private:
  void eventCallback(const EventManager::Event &event);

private:
  EventManager::EventManager &_eventManager;
  HgRepoManagerFsm _fsm;
  // Asks for SSH key passphrases, see HgRepoManagerAskPass.h.
  std::unique_ptr<Workers::AskPassServer> _askPass;
};

} // namespace HgRepoManager
