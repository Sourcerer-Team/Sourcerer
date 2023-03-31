#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"

#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <functional>

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

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc);
  _eventManager.subscribe(EventManager::EventType::GitRepoManagerOpenRepoError,
                          eventCallbackFunc);
  _eventManager.subscribe(EventManager::EventType::GitRepoManagerOpenRepoResult,
                          eventCallbackFunc);

  refreshViews();

  LOG_PERF(0, "Exit")
}

GitRepoContainer::~GitRepoContainer() {
  LOG_PERF(0, "Enter");

  closeRepo();

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerOpenRepoResult, eventCallbackFunc);

  delete ui;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::closeRepo() const {
  LOG_PERF(0, "Enter");

  GitRepoCloseCommand closeCommand;
  closeCommand.uuid = _uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::GitRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerListLocalBranchesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranchesResult: {
    const QStringList localBranches = event.data<QStringList>();
    updateLocalBranchesModel(localBranches);
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoResult: {
    getLocalBranches();
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getLocalBranches() const {
  LOG_PERF(0, "Enter");

  GitRepoListLocalBranchesCommand command;
  command.uuid = _uuid;

  EventManager::Event listLocalEvent(
      EventManager::EventType::GitRepoManagerListLocalBranches);
  listLocalEvent.addData(command);
  _eventManager.send(listLocalEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::openRepo() const {
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

void GitRepoContainer::updateLocalBranchesModel(
    const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  ui->localPage->updateModel(localBranches);

  LOG_PERF(0, "Exit")
}

} // namespace Containers
} // namespace GuiManager
