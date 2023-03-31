#pragma once

#include "InitResources.h"

#include <Component.h>

namespace GuiManager {

class GuiManager : public ComponentManager::Component{
public:
  GuiManager();
  virtual ~GuiManager();

  void start() override;
  void stop() override;

private:
  InitResources _initResources;
};

} // namespace GuiManager
