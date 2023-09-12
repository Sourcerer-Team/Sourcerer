#include "containers/CloneGitRepoContainer.h"
#include "ui_CloneGitRepoContainer.h"

#include <Dispatch.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <QDir>
#include <QFileDialog>
#include <QStandardPaths>
#include <QUrl>

#define THREAD_ID "CloneGitRepoContainer"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

namespace GuiManager {

namespace Containers {

CloneGitRepoContainer::CloneGitRepoContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Clone, parent),
      ui(new Ui::CloneGitRepoContainer),
      _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->labelFullDir->setVisible(false);
  ui->labelFullDirPrefix->setVisible(false);
  ui->lineEditFullDir->setVisible(false);

  connect(ui->lineEditWhereToDir, &QLineEdit::textChanged, this,
          &CloneGitRepoContainer::onTextChanged);
  connect(ui->lineEditUrl, &QLineEdit::textChanged, this,
          &CloneGitRepoContainer::onTextChanged);

  connect(ui->pushButtonBrowse, &QPushButton::clicked, this,
          &CloneGitRepoContainer::onBrowseClicked);
  connect(ui->pushButtonClone, &QPushButton::clicked, this,
          &CloneGitRepoContainer::onCloneClicked);

  LOG_PERF(0, "Exit");
}

CloneGitRepoContainer::~CloneGitRepoContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void CloneGitRepoContainer::onBrowseClicked() {
  LOG_PERF(0, "Enter");

  const QStringList documentsPaths =
      QStandardPaths::standardLocations(QStandardPaths::DocumentsLocation);
  if (documentsPaths.length() == 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString path = QFileDialog::getExistingDirectory(
      this, tr("Clone Git repository to"), documentsPaths[0]);
  if (!path.isEmpty())
    ui->lineEditWhereToDir->setText(QDir::toNativeSeparators(path));

  LOG_PERF(0, "Exit");
}

void CloneGitRepoContainer::onCloneClicked() {
  LOG_PERF(0, "Enter");

  if (ui->lineEditWhereToDir->text().isEmpty() ||
      ui->lineEditUrl->text().isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  _uuid = Uuid::createUuid();

  EventManager::Event cloneEvent(
      EventManager::EventType::GitRepoManagerCloneRepo);

  GitRepoCloneCommand cloneCommand;
  cloneCommand.uuid = _uuid;
  cloneCommand.destinationDir =
      ui->labelFullDirPrefix->text() + ui->lineEditFullDir->text();
  cloneCommand.url = ui->lineEditUrl->text();

  cloneEvent.addData(cloneCommand);
  _eventManager.send(cloneEvent);

  emit finished();

  LOG_PERF(0, "Exit");
}

void CloneGitRepoContainer::onTextChanged(const QString &text) {
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
