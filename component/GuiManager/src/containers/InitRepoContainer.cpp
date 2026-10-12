#include "containers/InitRepoContainer.h"
#include "containers/ui_InitRepoContainer.h"
#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <SvnRepoManagerStructs.h>
#include <UserProfileManager.h>
#include <Uuid.h>

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>

#define THREAD_ID "InitRepoContainer"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;
using namespace SvnRepoManager::Structs;

namespace GuiManager {

namespace Containers {

InitRepoContainer::InitRepoContainer(QWidget *parent, InitType initType)
    : Widgets::ContainerWidget(Widgets::ContainerType::InitRepo, parent),
      ui(new Ui::InitRepoContainer),
      _toastHost(Widgets::ToastHostWidget::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _initType(initType),
      _uuid(Uuid::createUuid()) {

  LOG_PERF(0, "Enter");
  ui->setupUi(this);

  const bool gitInit = _initType == InitType::Git;
  setWindowTitle(gitInit ? tr("Init Git repository")
                         : tr("Init Subversion repository"));
  ui->labelWorkingDir->setText(gitInit ? tr("Working directory")
                                       : tr("Parent directory"));
  ui->labelDefaultBrnach->setVisible(gitInit);
  ui->lineEditDefaultBranch->setVisible(gitInit);

  QTimer::singleShot(0, ui->lineEditName,
                     [this] { ui->lineEditName->setFocus(); });

  connect(ui->pushButtonBrowse, &QPushButton::clicked, this,
          &InitRepoContainer::onBrowseClicked);
  connect(ui->pushButtonInit, &QPushButton::clicked, this,
          &InitRepoContainer::onInitClicked);

  const auto callbackFunc = std::bind(&InitRepoContainer::eventCallback,
                                      this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoError, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoResult, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepoError, callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepoResult, callbackFunc));

  LOG_PERF(0, "Exit");
}

InitRepoContainer::~InitRepoContainer() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc = std::bind(&InitRepoContainer::eventCallback,
                                      this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoError, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerInitRepoResult, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepoError, callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::SvnRepoManagerInitRepoResult, callbackFunc));

  delete ui;

  LOG_PERF(0, "Exit");
}

void InitRepoContainer::onBrowseClicked() {
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

void InitRepoContainer::onInitClicked() {
  LOG_PERF(0, "Enter");

  const Common::UserProfileManager &profileManager =
      Common::UserProfileManager::instance();
  if (!profileManager.hasCurrentProfile()) {
    showErrorToast(
        tr("You must create a Git profile in User Profiles and set it as the "
           "default profile before performing this operation."),
        tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  const Common::UserProfile &profile = profileManager.currentProfile();

  if (_initType == InitType::Subversion) {
    // The working copy is created in <parent directory>/<name>, the
    // repository next to it, see SvnRepo::initRepository.
    SvnRepoInitRepoCommand command;
    command.uuid = _uuid;
    command.repoName = ui->lineEditName->text();
    command.workingDir =
        QDir(ui->lineEditWorkingDir->text()).filePath(command.repoName);
    command.author = profile.author;
    command.email = profile.email;

    EventManager::Event initEvent(
        EventManager::EventType::SvnRepoManagerInitRepo);
    initEvent.addData(command);

    _eventManager.send(initEvent);

    LOG_PERF(0, "Exit");
    return;
  }

  GitRepoInitRepoCommand command;
  command.uuid = _uuid;
  command.repoName = ui->lineEditName->text();
  command.workingDir = ui->lineEditWorkingDir->text();
  command.defaultBranch = ui->lineEditDefaultBranch->text();
  command.author = profile.author;
  command.email = profile.email;

  EventManager::Event initEvent(
      EventManager::EventType::GitRepoManagerInitRepo);
  initEvent.addData(command);

  _eventManager.send(initEvent);

  LOG_PERF(0, "Exit");
}

void InitRepoContainer::eventCallback(EventManager::Event event) {
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
  case EventManager::EventType::SvnRepoManagerInitRepoError: {
    if (const SvnRepoError svnError = event.data<SvnRepoError>();
        svnError.uuid != _uuid)
      break;

    Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
      showErrorToast(tr("Unable to initialize Subversion repository"),
                     tr("Error"));
    });
    break;
  }
  case EventManager::EventType::SvnRepoManagerInitRepoResult: {
    if (const auto commandResult = event.data<SvnRepoResult>();
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

void InitRepoContainer::showToast(const QString &message,
                                     const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new InfoToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

void InitRepoContainer::showErrorToast(const QString &message,
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
