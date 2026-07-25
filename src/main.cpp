#include <ComponentManager.h>
#include <GitRepoManager.h>
#include <GuiManager.h>
#include <NewsTracker.h>
#include <UpdateManager.h>

int main(int argc, char *argv[]) {

  ComponentManager::ComponentManager componentManager;
  componentManager.addThread<GitRepoManager::GitRepoManager>();
  componentManager.addThread<NewsTracker::NewsTracker>();
  componentManager.addThread<UpdateManager::UpdateManager>();
  componentManager.startProcess<GuiManager::GuiManager>();
  componentManager.stopAll();

  return 0;
}
