#include "containers/InitGitRepoContainer.h"
#include "containers/ui_InitGitRepoContainer.h"
#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <UserProfileManager.h>
#include <Uuid.h>

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

#define THREAD_ID "InitGitRepoContainer"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

namespace GuiManager {

namespace Containers {

InitGitRepoContainer::InitGitRepoContainer(QWidget *parent)
    : Widgets::ContainerWidget(Widgets::ContainerType::InitGitRepo, parent),
      ui(new Ui::InitGitRepoContainer),
      _toastHost(Widgets::ToastHostWidget::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _uuid(Uuid::createUuid()) {

  LOG_PERF(0, "Enter");
  ui->setupUi(this);

  QTimer::singleShot(0, ui->lineEditName,
                     [this] { ui->lineEditName->setFocus(); });

  connect(ui->pushButtonBrowse, &QPushButton::clicked, this,
          &InitGitRepoContainer::onBrowseClicked);
  connect(ui->pushButtonInit, &QPushButton::clicked, this,
          &InitGitRepoContainer::onInitClicked);

  const auto callbackFunc = std::bind(&InitGitRepoContainer::eventCallback,
                                      this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoError, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoResult, callbackFunc));

  LOG_PERF(0, "Exit");
}

InitGitRepoContainer::~InitGitRepoContainer() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc = std::bind(&InitGitRepoContainer::eventCallback,
                                      this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoError, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoResult, callbackFunc));

  delete ui;

  LOG_PERF(0, "Exit");
}

void InitGitRepoContainer::onBrowseClicked() {
  LOG_PERF(0, "Enter");

  const QStringList documentsPaths =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
  if (documentsPaths.length() == 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString path = QFileDialog::getExistingDirectory(
      this, tr("Open Git repository"), documentsPaths[0]);
  if (!path.isEmpty())
    ui->lineEditWorkingDir->setText(QDir::toNativeSeparators(path));

  LOG_PERF(0, "Exit");
}

void InitGitRepoContainer::onInitClicked() const {
  LOG_PERF(0, "Enter");

  GitRepoInitRepoCommand command;
  command.uuid = _uuid;
  command.repoName = ui->lineEditName->text();
  command.workingDir = ui->lineEditWorkingDir->text();
  command.defaultBranch = ui->lineEditDefaultBranch->text();

  const Common::UserProfile &profile =
      Common::UserProfileManager::instance().currentProfile();
  command.author = profile.author;
  command.email = profile.email;

  EventManager::Event initEvent(
      EventManager::EventType::GitRepoManagerInitRepo);
  initEvent.addData(command);

  _eventManager.send(initEvent);

  LOG_PERF(0, "Exit");
}

void InitGitRepoContainer::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerInitRepoError: {
    if (const GitRepoError gitError = event.data<GitRepoError>();
        gitError.uuid != _uuid)
      break;

    Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
      showErrorToast(tr("Unable to initialize Git repository"), tr("Error"));
    });
    break;
  }
  case EventManager::EventType::GitRepoManagerInitRepoResult: {
    if (const auto commandResult = event.data<GitRepoResult>();
        commandResult.uuid != _uuid) {
      break;
    }

    emit initialized();
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void InitGitRepoContainer::showToast(const QString &message,
                                     const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new InfoToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

void InitGitRepoContainer::showErrorToast(const QString &message,
                                          const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new ErrorToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

} // namespace Containers
} // namespace GuiManager
