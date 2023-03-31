#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"

#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#define THREAD_ID "GitRepoContainer"

using namespace GitRepoManager::Structs;

namespace GuiManager {

namespace Containers {

GitRepoContainer::GitRepoContainer(const QString &path, QWidget *parent)
    : ContainerWidget(Widgets::ContainerType::Unknown, parent),
      ui(new Ui::GitRepoContainer), _uuid(Uuid::createUuid()), _path(path),
      _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  refreshViews();

  LOG_PERF(0, "Exit")
}

GitRepoContainer::~GitRepoContainer() {
  LOG_PERF(0, "Enter");

  closeRepo();

  delete ui;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::closeRepo() const
{
  LOG_PERF(0, "Enter");

  GitRepoCloseCommand closeCommand;
  closeCommand.uuid = _uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::GitRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::openRepo() const
{
  LOG_PERF(0, "Enter");

  GitRepoOpenCommand openCommand;
  openCommand.uuid = _uuid;
  openCommand.path = _path;

  EventManager::Event openRepoEvent(
      EventManager::EventType::GitRepoManagerOpenRepo);
  openRepoEvent.addData(openCommand);
  _eventManager.send(openRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::refreshViews() {
  LOG_PERF(0, "Enter");

  openRepo();

  LOG_PERF(0, "Exit")
}

} // namespace Containers
} // namespace GuiManager
