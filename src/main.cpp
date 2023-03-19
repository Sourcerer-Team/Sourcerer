#include <ComponentManager.h>
#include <GuiManager.h>

int main(int argc, char *argv[]) {

  ComponentManager::ComponentManager componentManager;
  componentManager.addProcess<GuiManager::GuiManager>();
  componentManager.stopAll();
}
