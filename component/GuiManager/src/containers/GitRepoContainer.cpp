#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"
#include "widgets/InfoToast.h"

#include <Dispatch.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <QMovie>

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
      _toastHost(ToastHostWidget::getInstance()),
      _historyModel(new Models::GitHistoryModel(this)),
      _spinnerMovie(new QMovie(":/animations/transparent_spinner.gif", QByteArray(), this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->localPage->setContentsMargins(0, 0, 0, 0);
  ui->remotePage->setContentsMargins(0, 0, 0, 0);
  ui->tagsPage->setContentsMargins(0, 0, 0, 0);
  ui->toolBox->setItemText(0, tr("Local Branches"));
  ui->toolBox->setItemText(1, tr("Remote Branches"));
  ui->toolBox->setItemText(2, tr("Tags"));
  ui->toolBox->setItemText(3, tr("Submodules"));
  ui->splitter->setStretchFactor(0, 1);
  ui->splitter->setStretchFactor(1, 8);
  ui->splitter->setStretchFactor(2, 1);
  ui->submodulesPage->setContentsMargins(0, 0, 0, 0);
  ui->historyView->setModel(_historyModel);
  ui->labelSpinner->setMovie(_spinnerMovie);

  showSpinner();

  const QByteArray splitterState = QByteArray::fromHex(
      _configManager.getString(ConfigManager::ConfigKey::GitRepoSplitterPos)
          .toUtf8());
  ui->splitter->restoreState(splitterState);

  connect(ui->splitter, &QSplitter::splitterMoved, this,
          &GitRepoContainer::onSplitterMoved);

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTagsError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerListTagsResult,
                       eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerOpenRepoResult,
                       eventCallbackFunc));

  refreshViews();

  LOG_PERF(0, "Exit")
}

GitRepoContainer::~GitRepoContainer() {
  LOG_PERF(0, "Enter");

  closeRepo();

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTagsError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerListTagsResult,
                       eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerOpenRepoResult,
                       eventCallbackFunc));

  delete _spinnerMovie;
  delete ui;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resizeEvent(QResizeEvent *event) {
  ContainerWidget::resizeEvent(event);
  stretchBranchHistoryTableColumns();
}

void GitRepoContainer::onSplitterMoved(const int pos, const int index) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("pos = {}, index = {}", pos, index);

  const QString splitterState = ui->splitter->saveState().toHex();
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSplitterPos,
                           splitterState);
  _configManager.save();

  stretchBranchHistoryTableColumns();

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
  case EventManager::EventType::GitRepoManagerGetBranchHistoryError: {
    showSpinner(false);

    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerGetBranchHistoryResult: {
    const GitRepo::GitHistory history = event.data<GitRepo::GitHistory>();
    Dispatch::dispatchToMainThread([=]() { updateBranchHistory(history); });
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
    if (gitError.uuid != _uuid)
      break;

    Dispatch::dispatchToMainThread(
        [=]() { showToast(tr("Can't open repository")); });
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

void GitRepoContainer::getBranchHistory() const {
  LOG_PERF(0, "Enter");

  GitRepoGetBranchHistoryCommand command;
  command.uuid = _uuid;
  command.branchName = _activeBranch;

  EventManager::Event historyEvent(
      EventManager::EventType::GitRepoManagerGetBranchHistory);
  historyEvent.addData(command);
  _eventManager.send(historyEvent);

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

void GitRepoContainer::refreshViews() const {
  LOG_PERF(0, "Enter");

  openRepo();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::stretchBranchHistoryTableColumns() const {
  LOG_PERF(0, "Enter");

  const int totalWidth = ui->historyView->viewport()->width();
  int colSummary = totalWidth / 2;
  if (constexpr int maxSummaryWidth = 600; colSummary > maxSummaryWidth)
    colSummary = maxSummaryWidth;

  const int remaining = totalWidth - colSummary;
  const int colOther = remaining / 5;

  ui->historyView->setColumnWidth(0, colOther);
  ui->historyView->setColumnWidth(1, colSummary);
  ui->historyView->setColumnWidth(2, colOther);
  ui->historyView->setColumnWidth(3, colOther);
  ui->historyView->setColumnWidth(4, colOther);
  ui->historyView->setColumnWidth(5, colOther);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showSpinner(const bool spinnerEnabled) const {
  LOG_PERF(0, "Enter");

  ui->stackedWidget->setCurrentIndex(spinnerEnabled ? 0 : 1);

  if (spinnerEnabled) {
    _spinnerMovie->start();
  } else {
    _spinnerMovie->stop();
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showToast(const QString &message) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  _toastHost.addToast(new InfoToast(message));

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateBranchHistory(
    const GitRepo::GitHistory &history) const {
  LOG_PERF(0, "Enter");

  _historyModel->updateHistory(history);

  showSpinner(false);

  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModel(
    const QStringList &localBranches) const {
  LOG_PERF(0, "Enter");

  ui->localPage->updateModel(localBranches);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModelActiveBranch(
    const QString &activeBranch) {
  LOG_PERF(0, "Enter");

  _activeBranch = activeBranch;

  ui->localPage->updateActiveBranch(activeBranch);

  getBranchHistory();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModel(
    const QStringList &remoteBranches) const {
  LOG_PERF(0, "Enter");

  ui->remotePage->updateModel(remoteBranches);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateSubmodulesModel(const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  ui->submodulesPage->updateModel(tags);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateTagsModel(const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  ui->tagsPage->updateModel(tags);

  LOG_PERF(0, "Exit")
}

} // namespace Containers
} // namespace GuiManager
