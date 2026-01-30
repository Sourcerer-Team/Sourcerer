#include "containers/GitCredentialsContainer.h"
#include "ui_GitCredentialsContainer.h"

#include <Log.h>

#include <QDateTime>
#include <QDir>
#include <QHeaderView>
#include <QMessageBox>
#include <QStandardItemModel>

#define THREAD_ID "GitCredentialsContainer"

namespace GuiManager {
namespace Containers {

GitCredentialsContainer::GitCredentialsContainer(QWidget *parent)
    : ContainerWidget(ContainerType::GitCredentials, parent),
      ui(new Ui::GitCredentialsContainer),
      _credentialStorage(GitRepo::GitCredentialStorage::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  // Create model
  _model = new QStandardItemModel(0, 3, this);
  _model->setHorizontalHeaderLabels({"Repository path", "Username", "Created"});
  ui->tableView->setModel(_model);

  // Configure table view
  ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui->tableView->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);

  // Connect signals
  connect(ui->refreshButton, &QPushButton::clicked, this,
          &GitCredentialsContainer::onRefreshClicked);
  connect(ui->deleteButton, &QPushButton::clicked, this,
          &GitCredentialsContainer::onDeleteClicked);
  connect(ui->tableView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &GitCredentialsContainer::onSelectionChanged);

  loadCredentials();

  LOG_PERF(0, "Exit");
}

GitCredentialsContainer::~GitCredentialsContainer() {
  LOG_PERF(0, "Enter");
  delete ui;
  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::loadCredentials() {
  LOG_PERF(0, "Enter");

  // Clear existing rows
  _model->removeRows(0, _model->rowCount());

  // Get all repository paths
  QList<QString> repoPaths = _credentialStorage.getAllRepositoryPaths();

  LOG_INFO("Loading {} stored credentials", repoPaths.size());

  // Add each credential to the table
  for (const QString &repoPath : repoPaths) {
    QString username;
    qint64 createdTime;

    if (_credentialStorage.getCredentialsInfo(repoPath, username,
                                              createdTime)) {
      QList<QStandardItem *> row;

      // Repository path - convert to native separators for display
      QString displayPath = QDir::toNativeSeparators(repoPath);
      QStandardItem *pathItem = new QStandardItem(displayPath);
      pathItem->setToolTip(displayPath);
      row.append(pathItem);

      // Username
      QStandardItem *userItem = new QStandardItem(username);
      row.append(userItem);

      // Created time
      QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(createdTime);
      QStandardItem *timeItem =
          new QStandardItem(dateTime.toString("yyyy-MM-dd hh:mm:ss"));
      row.append(timeItem);

      _model->appendRow(row);
    }
  }

  // Resize columns to content, except first column which stretches
  ui->tableView->resizeColumnsToContents();
  ui->tableView->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);

  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::onDeleteClicked() {
  LOG_PERF(0, "Enter");

  QModelIndexList selected = ui->tableView->selectionModel()->selectedRows();
  if (selected.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Get the repository path from the selected row
  int row = selected.first().row();
  QString repoPath = _model->item(row, 0)->text();

  // Confirm deletion
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, "Delete credentials",
      QString(
          "Are you sure you want to delete the stored credentials for:\n\n%1")
          .arg(repoPath),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    LOG_INFO("Deleting credentials for repository: {}", repoPath);
    _credentialStorage.removeCredentials(QDir::fromNativeSeparators(repoPath));

    // Reload the table
    loadCredentials();

    LOG_INFO("Credentials deleted successfully");
  }

  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::onRefreshClicked() {
  LOG_PERF(0, "Enter");
  loadCredentials();
  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::onSelectionChanged() {
  bool hasSelection =
      !ui->tableView->selectionModel()->selectedRows().isEmpty();
  ui->deleteButton->setEnabled(hasSelection);
}

void GitCredentialsContainer::selected() {
  LOG_PERF(0, "Enter");
  loadCredentials();
  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::deselected() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
