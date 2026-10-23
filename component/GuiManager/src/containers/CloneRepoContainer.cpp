#include "containers/CloneRepoContainer.h"
#include "ui_CloneRepoContainer.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <HgRepoManagerStructs.h>
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
using namespace HgRepoManager::Structs;
using namespace SvnRepoManager::Structs;

namespace GuiManager {

namespace Containers {

static QString canonicalCloneUrl(const QString &urlText) {
  const QString trimmed = urlText.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  const QUrl url = QUrl::fromUserInput(trimmed);
  if (!url.isValid() || url.scheme().isEmpty()) {
    return trimmed;
  }

  if (url.isLocalFile()) {
    return QDir::fromNativeSeparators(url.toLocalFile());
  }

  return url.adjusted(QUrl::NormalizePathSegments | QUrl::StripTrailingSlash)
      .toString(QUrl::FullyEncoded);
}

CloneRepoContainer::CloneRepoContainer(QWidget *parent, CloneType cloneType)
    : ContainerWidget(ContainerType::Clone, parent),
      ui(new Ui::CloneRepoContainer),
      _eventManager(EventManager::EventManager::getInstance()),
      _cloneType(cloneType) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  switch (_cloneType) {
  case CloneType::Git:
    setWindowTitle(tr("Clone Git repository"));
    ui->labelWhereToDir->setText(tr("Clone to"));
    ui->labelUrl->setText(tr("Git URL"));
    ui->pushButtonClone->setText(tr("Clone"));
    break;
  case CloneType::Subversion:
    setWindowTitle(tr("Checkout Subversion repository"));
    ui->labelWhereToDir->setText(tr("Checkout to"));
    ui->labelUrl->setText(tr("Subversion URL"));
    ui->pushButtonClone->setText(tr("Checkout"));
    break;
  case CloneType::Mercurial:
    setWindowTitle(tr("Clone Mercurial repository"));
    ui->labelWhereToDir->setText(tr("Clone to"));
    ui->labelUrl->setText(tr("Mercurial URL"));
    ui->pushButtonClone->setText(tr("Clone"));
    break;
  }

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

  QString title;
  switch (_cloneType) {
  case CloneType::Git:
    title = tr("Clone Git repository to");
    break;
  case CloneType::Subversion:
    title = tr("Checkout Subversion repository to");
    break;
  case CloneType::Mercurial:
    title = tr("Clone Mercurial repository to");
    break;
  }

  const QString path =
      QFileDialog::getExistingDirectory(this, title, documentsPaths[0]);
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
  const QString url = canonicalCloneUrl(ui->lineEditUrl->text());
  if (url.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (_cloneType == CloneType::Git) {
    EventManager::Event cloneEvent(
        EventManager::EventType::GitRepoManagerCloneRepo);

    GitRepoCloneCommand cloneCommand;
    cloneCommand.uuid = _uuid;
    cloneCommand.destinationDir = destinationDir;
    cloneCommand.url = url;

    cloneEvent.addData(cloneCommand);
    _eventManager.send(cloneEvent);
  } else if (_cloneType == CloneType::Mercurial) {
    EventManager::Event cloneEvent(
        EventManager::EventType::HgRepoManagerCloneRepo);

    HgRepoCloneRepoCommand cloneCommand;
    cloneCommand.uuid = _uuid;
    cloneCommand.destinationDir = destinationDir;
    cloneCommand.url = url;

    cloneEvent.addData(cloneCommand);
    _eventManager.send(cloneEvent);
  } else {
    EventManager::Event cloneEvent(
        EventManager::EventType::SvnRepoManagerCloneRepo);

    SvnRepoCloneCommand cloneCommand;
    cloneCommand.uuid = _uuid;
    cloneCommand.destinationDir = destinationDir;
    cloneCommand.url = url;

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
