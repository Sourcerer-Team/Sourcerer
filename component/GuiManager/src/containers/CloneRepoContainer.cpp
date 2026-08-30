#include "containers/CloneRepoContainer.h"
#include "ui_CloneRepoContainer.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <SvnRepoManagerStructs.h>
#include <Uuid.h>

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QUrl>

#define THREAD_ID "CloneRepoContainer"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;
using namespace SvnRepoManager::Structs;

namespace GuiManager {

namespace Containers {

CloneRepoContainer::CloneRepoContainer(QWidget *parent, CloneType cloneType)
    : ContainerWidget(ContainerType::Clone, parent),
      ui(new Ui::CloneRepoContainer),
      _eventManager(EventManager::EventManager::getInstance()),
      _cloneType(cloneType) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  const bool gitClone = _cloneType == CloneType::Git;
  setWindowTitle(gitClone ? tr("Clone Git repository")
                          : tr("Checkout Subversion repository"));
  ui->labelWhereToDir->setText(gitClone ? tr("Clone to") : tr("Checkout to"));
  ui->labelUrl->setText(gitClone ? tr("Git URL") : tr("Subversion URL"));
  ui->pushButtonClone->setText(gitClone ? tr("Clone") : tr("Checkout"));

  ui->labelFullDir->setVisible(false);
  ui->labelFullDirPrefix->setVisible(false);
  ui->lineEditFullDir->setVisible(false);

  connect(ui->lineEditWhereToDir, &QLineEdit::textChanged, this,
          &CloneRepoContainer::onTextChanged);
  connect(ui->lineEditUrl, &QLineEdit::textChanged, this,
          &CloneRepoContainer::onTextChanged);

  connect(ui->pushButtonBrowse, &QPushButton::clicked, this,
          &CloneRepoContainer::onBrowseClicked);
  connect(ui->pushButtonClone, &QPushButton::clicked, this,
          &CloneRepoContainer::onCloneClicked);

  LOG_PERF(0, "Exit");
}

CloneRepoContainer::~CloneRepoContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void CloneRepoContainer::onBrowseClicked() {
  LOG_PERF(0, "Enter");

  const QStringList documentsPaths =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
  if (documentsPaths.length() == 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString path = QFileDialog::getExistingDirectory(
      this,
      _cloneType == CloneType::Git ? tr("Clone Git repository to")
                                   : tr("Checkout Subversion repository to"),
      documentsPaths[0]);
  if (!path.isEmpty())
    ui->lineEditWhereToDir->setText(QDir::toNativeSeparators(path));

  LOG_PERF(0, "Exit");
}

void CloneRepoContainer::onCloneClicked() {
  LOG_PERF(0, "Enter");

  if (ui->lineEditWhereToDir->text().isEmpty() ||
      ui->lineEditUrl->text().isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  _uuid = Uuid::createUuid();

  const QString destinationDir =
      ui->labelFullDirPrefix->text() + ui->lineEditFullDir->text();

  if (_cloneType == CloneType::Git) {
    EventManager::Event cloneEvent(
        EventManager::EventType::GitRepoManagerCloneRepo);

    GitRepoCloneCommand cloneCommand;
    cloneCommand.uuid = _uuid;
    cloneCommand.destinationDir = destinationDir;
    cloneCommand.url = ui->lineEditUrl->text();

    cloneEvent.addData(cloneCommand);
    _eventManager.send(cloneEvent);
  } else {
    EventManager::Event cloneEvent(
        EventManager::EventType::SvnRepoManagerCloneRepo);

    SvnRepoCloneCommand cloneCommand;
    cloneCommand.uuid = _uuid;
    cloneCommand.destinationDir = destinationDir;
    cloneCommand.url = ui->lineEditUrl->text();

    cloneEvent.addData(cloneCommand);
    _eventManager.send(cloneEvent);
  }

  emit finished();

  LOG_PERF(0, "Exit");
}

void CloneRepoContainer::onTextChanged(const QString &text) const {
  LOG_PERF(0, "Enter");

  ui->labelFullDir->setVisible(true);
  ui->labelFullDirPrefix->setVisible(true);
  ui->lineEditFullDir->setVisible(true);

  ui->labelFullDirPrefix->setText(ui->lineEditWhereToDir->text() +
                                  QDir::separator());

  const QUrl url(ui->lineEditUrl->text());
  const QString fileName = url.fileName();
  const QFileInfo fi(fileName);

  ui->lineEditFullDir->setText(fi.baseName());

  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
