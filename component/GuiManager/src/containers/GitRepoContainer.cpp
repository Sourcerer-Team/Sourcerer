#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"
#include "widgets/InfoToast.h"

#include <Dispatch.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <functional>

#define THREAD_ID "GitRepoContainer"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

namespace GuiManager {

namespace Containers {

GitRepoContainer::GitRepoContainer(const QString &path, QWidget *parent)
    : ContainerWidget(Widgets::ContainerType::Unknown, parent),
      ui(new Ui::GitRepoContainer), _uuid(Uuid::createUuid()), _path(path),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _toastHost(ToastHostWidget::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->localPage->setContentsMargins(0, 0, 0, 0);
  ui->remotePage->setContentsMargins(0, 0, 0, 0);
  ui->tagsPage->setContentsMargins(0, 0, 0, 0);
  ui->splitter->setStretchFactor(0, 1);
  ui->splitter->setStretchFactor(1, 8);
  ui->splitter->setStretchFactor(2, 1);
  ui->submodulesPage->setContentsMargins(0, 0, 0, 0);

  const QByteArray splitterState = QByteArray::fromHex(
      _configManager.getString(ConfigManager::ConfigKey::GitRepoSplitterPos)
          .toUtf8());
  ui->splitter->restoreState(splitterState);

  connect(ui->splitter, &QSplitter::splitterMoved, this,
          &GitRepoContainer::onSplitterMoved);

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      eventCallbackFunc);
  _eventManager.subscribe(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      eventCallbackFunc);
  _eventManager.subscribe(EventManager::EventType::GitRepoManagerListTagsError,
                          eventCallbackFunc);
  _eventManager.subscribe(EventManager::EventType::GitRepoManagerListTagsResult,
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
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListTagsError, eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerListTagsResult, eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc);
  _eventManager.unsubscribe(
      EventManager::EventType::GitRepoManagerOpenRepoResult, eventCallbackFunc);

  delete ui;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onSplitterMoved(int pos, int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("pos = {}, index = {}", pos, index);

  const QString splitterState = ui->splitter->saveState().toHex();
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSplitterPos,
                           splitterState);
  _configManager.save();

  LOG_PERF(0, "Exit");
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
  case EventManager::EventType::GitRepoManagerGetActiveBranchError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerGetActiveBranchResult: {
    const QString activeBranch = event.data<QString>();
    Dispatch::dispatchToMainThread(
        [=]() { updateLocalBranchesModelActiveBranch(activeBranch); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranchesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranchesResult: {
    const QStringList localBranches = event.data<QStringList>();
    Dispatch::dispatchToMainThread(
        [=]() { updateLocalBranchesModel(localBranches); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemoteBranchesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemoteBranchesResult: {
    const QStringList remoteBranches = event.data<QStringList>();
    Dispatch::dispatchToMainThread(
        [=]() { updateRemoteBranchesModel(remoteBranches); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListSubmodulesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListSubmodulesResult: {
    const QStringList submodules = event.data<QStringList>();
    Dispatch::dispatchToMainThread(
        [=]() { updateSubmodulesModel(submodules); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListTagsError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListTagsResult: {
    const QStringList tags = event.data<QStringList>();
    Dispatch::dispatchToMainThread([=]() { updateTagsModel(tags); });
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoError: {
    // Show a toast with error
    const GitRepoError gitError = event.data<GitRepoError>();
    if(gitError.uuid != _uuid)
        break;

    Dispatch::dispatchToMainThread([=]() {
        showToast(tr("Can't open repository\n"));
    });
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoResult: {
    getLocalBranches();
    getActiveBranch();
    getRemoteBranches();
    getTags();
    getSubmodules();
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getActiveBranch() const {
  LOG_PERF(0, "Enter");

  GitRepoGetActiveBranchCommand command;
  command.uuid = _uuid;

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::GitRepoManagerGetActiveBranch);
  getActiveBranchEvent.addData(command);
  _eventManager.send(getActiveBranchEvent);

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

void GitRepoContainer::getRemoteBranches() const {
  LOG_PERF(0, "Enter");

  GitRepoListRemoteBranchesCommand command;
  command.uuid = _uuid;

  EventManager::Event listRemoteEvent(
      EventManager::EventType::GitRepoManagerListRemoteBranches);
  listRemoteEvent.addData(command);
  _eventManager.send(listRemoteEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getSubmodules() const {
  LOG_PERF(0, "Enter");

  GitRepoListSubmodulesCommand command;
  command.uuid = _uuid;

  EventManager::Event listSubmodulesEvent(
      EventManager::EventType::GitRepoManagerListSubmodules);
  listSubmodulesEvent.addData(command);
  _eventManager.send(listSubmodulesEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getTags() const {
  LOG_PERF(0, "Enter");

  GitRepoListTagsCommand command;
  command.uuid = _uuid;

  EventManager::Event listTagsEvent(
      EventManager::EventType::GitRepoManagerListTags);
  listTagsEvent.addData(command);
  _eventManager.send(listTagsEvent);

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

void GitRepoContainer::showToast(const QString &message)
{
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  _toastHost.addToast(new InfoToast(message));

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModel(
    const QStringList &localBranches) {
  LOG_PERF(0, "Enter");

  ui->localPage->updateModel(localBranches);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModelActiveBranch(
    const QString &activeBranch) {
  LOG_PERF(0, "Enter");

  ui->localPage->updateActiveBranch(activeBranch);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModel(
    const QStringList &remoteBranches) {
  LOG_PERF(0, "Enter");

  ui->remotePage->updateModel(remoteBranches);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateSubmodulesModel(const QStringList &tags) {
  LOG_PERF(0, "Enter");

  ui->submodulesPage->updateModel(tags);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateTagsModel(const QStringList &tags) {
  LOG_PERF(0, "Enter");

  ui->tagsPage->updateModel(tags);

  LOG_PERF(0, "Exit")
}

} // namespace Containers
} // namespace GuiManager
