#include "containers/GitCredentialsContainer.h"

#include <Log.h>

#include <QDateTime>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>
#include <QTableView>
#include <QVBoxLayout>

#define THREAD_ID "GitCredentialsContainer"

namespace GuiManager {
namespace Containers {

GitCredentialsContainer::GitCredentialsContainer(QWidget *parent)
    : ContainerWidget(ContainerType::GitCredentials, parent),
      _credentialStorage(GitRepo::GitCredentialStorage::getInstance()) {
  LOG_PERF(0, "Enter");

  setupUi();
  loadCredentials();

  LOG_PERF(0, "Exit");
}

GitCredentialsContainer::~GitCredentialsContainer() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::setupUi() {
  LOG_PERF(0, "Enter");

  // Main layout
  QVBoxLayout *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 20, 20, 20);
  mainLayout->setSpacing(20);

  // Title
  QLabel *titleLabel = new QLabel("Stored Git credentials", this);
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(16);
  titleFont.setBold(true);
  titleLabel->setFont(titleFont);
  mainLayout->addWidget(titleLabel);

  // Description
  QLabel *descLabel =
      new QLabel("Manage stored credentials for Git repositories.", this);
  descLabel->setWordWrap(true);
  mainLayout->addWidget(descLabel);

  // Table view
  _tableView = new QTableView(this);
  _tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
  _tableView->setSelectionMode(QAbstractItemView::SingleSelection);
  _tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  _tableView->setAlternatingRowColors(true);
  _tableView->horizontalHeader()->setStretchLastSection(false);
  _tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
  _tableView->verticalHeader()->setVisible(false);

  // Model
  _model = new QStandardItemModel(0, 3, this);
  _model->setHorizontalHeaderLabels({"Repository path", "Username", "Created"});
  _tableView->setModel(_model);

  mainLayout->addWidget(_tableView);

  // Button layout
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  buttonLayout->setSpacing(10);

  _refreshButton = new QPushButton("Refresh", this);
  _refreshButton->setMinimumWidth(100);
  buttonLayout->addWidget(_refreshButton);

  _deleteButton = new QPushButton("Delete selected", this);
  _deleteButton->setMinimumWidth(120);
  _deleteButton->setEnabled(false);
  buttonLayout->addWidget(_deleteButton);

  buttonLayout->addStretch();

  mainLayout->addLayout(buttonLayout);

  // Connect signals
  connect(_refreshButton, &QPushButton::clicked, this,
          &GitCredentialsContainer::onRefreshClicked);
  connect(_deleteButton, &QPushButton::clicked, this,
          &GitCredentialsContainer::onDeleteClicked);
  connect(_tableView->selectionModel(), &QItemSelectionModel::selectionChanged,
          this, &GitCredentialsContainer::onSelectionChanged);

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

      // Repository path
      QStandardItem *pathItem = new QStandardItem(repoPath);
      pathItem->setToolTip(repoPath);
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
  _tableView->resizeColumnsToContents();
  _tableView->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::onDeleteClicked() {
  LOG_PERF(0, "Enter");

  QModelIndexList selected = _tableView->selectionModel()->selectedRows();
  if (selected.isEmpty()) {
    LOG_PERF(0, "Exit - No selection");
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
    _credentialStorage.removeCredentials(repoPath);

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
  bool hasSelection = !_tableView->selectionModel()->selectedRows().isEmpty();
  _deleteButton->setEnabled(hasSelection);
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
