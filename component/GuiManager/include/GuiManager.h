#pragma once

#include "InitResources.h"

namespace GuiManager {

class GuiManager {
public:
  GuiManager();
  virtual ~GuiManager();

  void start();
  void stop();

private:
  InitResources _initResources;
};

} // namespace GuiManager
