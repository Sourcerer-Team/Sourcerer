#include <ComponentManager.h>
#include <GitRepoManager.h>
#include <GuiManager.h>

int main(int argc, char *argv[]) {

  ComponentManager::ComponentManager componentManager;
  componentManager.addThread<GitRepoManager::GitRepoManager>();
  componentManager.addProcess<GuiManager::GuiManager>();
  componentManager.stopAll();
}
