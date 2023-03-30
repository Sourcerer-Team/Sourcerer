#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"

#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#define THREAD_ID "GitRepoContainer"

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

  delete ui;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::refreshViews() {
  LOG_PERF(0, "Enter");

  GitRepoManager::Structs::GitRepoOpenCommand command;
  command.uuid = _uuid;
  command.path = _path;

  EventManager::Event openRepoEvent(
      EventManager::EventType::GitRepoManagerOpenRepo);
  openRepoEvent.addData(command);
  _eventManager.send(openRepoEvent);

  LOG_PERF(0, "Exit")
}

} // namespace Containers
} // namespace GuiManager
