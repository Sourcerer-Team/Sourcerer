#include "containers/GitKeysContainer.h"
#include "ui_GitKeysContainer.h"

#include <Log.h>

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QHeaderView>
#include <QInputDialog>
#include <QMessageBox>
#include <QStandardItemModel>

#define THREAD_ID "GitKeysContainer"

namespace GuiManager {
namespace Containers {

GitKeysContainer::GitKeysContainer(QWidget *parent)
    : ContainerWidget(ContainerType::GitKeys, parent),
      ui(new Ui::GitKeysContainer),
      _keyStorage(GitRepo::GitKeyStorage::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  // Create model
  _model = new QStandardItemModel(0, 4, this);
  _model->setHorizontalHeaderLabels(
      {"Host", "Key name", "Private key path", "Created"});
  ui->tableView->setModel(_model);

  // Configure table view
  ui->tableView->setEditTriggers(QAbstractItemView::NoEditTriggers);
  ui->tableView->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);

  // Connect signals
  connect(ui->generateButton, &QPushButton::clicked, this,
          &GitKeysContainer::onGenerateClicked);
  connect(ui->addButton, &QPushButton::clicked, this,
          &GitKeysContainer::onAddClicked);
  connect(ui->editButton, &QPushButton::clicked, this,
          &GitKeysContainer::onEditClicked);
  connect(ui->deleteButton, &QPushButton::clicked, this,
          &GitKeysContainer::onDeleteClicked);
  connect(ui->refreshButton, &QPushButton::clicked, this,
          &GitKeysContainer::onRefreshClicked);
  connect(ui->tableView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &GitKeysContainer::onSelectionChanged);

  loadKeyPairs();

  LOG_PERF(0, "Exit");
}

GitKeysContainer::~GitKeysContainer() {
  LOG_PERF(0, "Enter");
  delete ui;
  LOG_PERF(0, "Exit");
}

void GitKeysContainer::loadKeyPairs() {
  LOG_PERF(0, "Enter");

  // Clear existing rows
  _model->removeRows(0, _model->rowCount());

  // Get all key pairs
  QVector<GitRepo::GitKeyPair> keyPairs = _keyStorage.getAllKeyPairs();

  LOG_INFO("Loading {} SSH key pairs", keyPairs.size());

  // Add each key pair to the table
  for (const GitRepo::GitKeyPair &keyPair : keyPairs) {
    QList<QStandardItem *> row;

    // Host
    QStandardItem *hostItem = new QStandardItem(keyPair.host);
    hostItem->setToolTip(keyPair.host);
    row.append(hostItem);

    // Key Name
    QStandardItem *keyNameItem = new QStandardItem(keyPair.keyName);
    row.append(keyNameItem);

    // Private Key Path - convert to native separators for display
    QString displayPath = QDir::toNativeSeparators(keyPair.privateKeyPath);
    QStandardItem *pathItem = new QStandardItem(displayPath);
    pathItem->setToolTip(displayPath);
    row.append(pathItem);

    // Created time
    QDateTime dateTime = QDateTime::fromMSecsSinceEpoch(keyPair.createdTime);
    QStandardItem *timeItem =
        new QStandardItem(dateTime.toString("yyyy-MM-dd hh:mm:ss"));
    row.append(timeItem);

    _model->appendRow(row);
  }

  // Resize columns to content
  ui->tableView->resizeColumnsToContents();
  ui->tableView->horizontalHeader()->setSectionResizeMode(0,
                                                          QHeaderView::Stretch);

  LOG_PERF(0, "Exit");
}

void GitKeysContainer::onGenerateClicked() {
  LOG_PERF(0, "Enter");

  // Get key name from user
  bool ok;
  QString keyName = QInputDialog::getText(
      this, "Generate SSH key pair",
      "Enter a name for the new key (e.g., "
      "id_rsa_github, id_rsa_gitlab):",
      QLineEdit::Normal,
      "id_rsa_" + QString::number(QDateTime::currentSecsSinceEpoch()), &ok);

  if (!ok || keyName.isEmpty()) {
    LOG_PERF(0, "Exit - Cancelled");
    return;
  }

  // Check if key already exists
  if (_keyStorage.keyPairExists(keyName)) {
    QMessageBox::warning(
        this, "Key exists",
        QString("A key pair with the name '%1' already exists.\n\n"
                "Please choose a different name or delete the existing key "
                "first.")
            .arg(keyName));
    LOG_PERF(0, "Exit - Key exists");
    return;
  }

  // Ask for passphrase (optional)
  QString passphrase = QInputDialog::getText(
      this, "Passphrase (optional)",
      "Enter a passphrase to protect the private key\n(leave empty for no "
      "passphrase):",
      QLineEdit::Password, "", &ok);

  if (!ok) {
    LOG_PERF(0, "Exit - Cancelled");
    return;
  }

  // Generate the key pair
  QString privateKeyPath, publicKeyPath, errorMessage;
  if (!_keyStorage.generateKeyPair(keyName, passphrase, privateKeyPath,
                                   publicKeyPath, errorMessage)) {
    QMessageBox::critical(
        this, "Key generation failed",
        QString("Failed to generate SSH key pair:\n\n%1").arg(errorMessage));
    LOG_PERF(0, "Exit - Generation failed");
    return;
  }

  // Ask for host to associate with
  QString host = QInputDialog::getText(
      this, "Associate with host",
      QString(
          "Key pair generated successfully:\n%1\n%2\n\nEnter a host to "
          "associate with this key\n(e.g., github.com, gitlab.com, or leave "
          "empty to skip):")
          .arg(privateKeyPath, publicKeyPath),
      QLineEdit::Normal, "", &ok);

  if (ok && !host.isEmpty()) {
    _keyStorage.setKeyPair(host, keyName, privateKeyPath, publicKeyPath);
  }

  QMessageBox::information(
      this, "Success",
      QString("SSH key pair generated successfully!\n\nPrivate key: "
              "%1\nPublic key: %2\n\nThe public key has been saved and can be "
              "added to your Git hosting service.")
          .arg(privateKeyPath, publicKeyPath));

  // Reload the table
  loadKeyPairs();

  LOG_INFO("SSH key pair generated: {}", keyName);
  LOG_PERF(0, "Exit");
}

void GitKeysContainer::onAddClicked() {
  LOG_PERF(0, "Enter");

  // Ask for host
  bool ok;
  QString host = QInputDialog::getText(
      this, "Add SSH key mapping",
      "Enter the Git host (e.g., github.com, gitlab.com):", QLineEdit::Normal,
      "", &ok);

  if (!ok || host.isEmpty()) {
    LOG_PERF(0, "Exit - Cancelled");
    return;
  }

  // Check if host already has a key
  GitRepo::GitKeyPair existingKeyPair;
  if (_keyStorage.getKeyPairForHost(host, existingKeyPair)) {
    QMessageBox::StandardButton reply = QMessageBox::question(
        this, "Host already mapped",
        QString("Host '%1' is already mapped to key '%2'.\n\n"
                "Do you want to change the key mapping?")
            .arg(host, existingKeyPair.keyName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::No) {
      LOG_PERF(0, "Exit - User cancelled replacement");
      return;
    }
  }

  // Browse for private key file
  QString sshDir = GitRepo::GitKeyStorage::getSshDirectoryPath();
  QString privateKeyPath = QFileDialog::getOpenFileName(
      this, "Select private key file", sshDir, "Private Keys (id_* *)");

  if (privateKeyPath.isEmpty()) {
    LOG_PERF(0, "Exit - No file selected");
    return;
  }

  // Assume public key is private key + .pub
  QString publicKeyPath = privateKeyPath + ".pub";
  if (!QFile::exists(publicKeyPath)) {
    QMessageBox::warning(
        this, "Public key not found",
        QString("Could not find the public key file:\n%1\n\n"
                "Please ensure the public key exists with the .pub extension.")
            .arg(publicKeyPath));
    LOG_PERF(0, "Exit - Public key not found");
    return;
  }

  // Extract key name from path
  QString keyName = QFileInfo(privateKeyPath).fileName();

  // Add the key pair
  if (_keyStorage.setKeyPair(host, keyName, privateKeyPath, publicKeyPath)) {
    QMessageBox::information(
        this, "Success",
        QString("SSH key mapping added successfully!\n\nHost: %1\nKey: %2")
            .arg(host, keyName));

    // Reload the table
    loadKeyPairs();

    LOG_INFO("SSH key mapping added for host '{}'", host);
  } else {
    QMessageBox::critical(this, "Error",
                          "Failed to add SSH key mapping.\n\n"
                          "Please check the log for details.");
  }

  LOG_PERF(0, "Exit");
}

void GitKeysContainer::onEditClicked() {
  LOG_PERF(0, "Enter");

  QModelIndexList selected = ui->tableView->selectionModel()->selectedRows();
  if (selected.isEmpty()) {
    LOG_PERF(0, "Exit - No selection");
    return;
  }

  // Get the current host
  int row = selected.first().row();
  QString oldHost = _model->item(row, 0)->text();
  QString keyName = _model->item(row, 1)->text();
  QString privateKeyPath = _model->item(row, 2)->text();

  // Ask for new host
  bool ok;
  QString newHost = QInputDialog::getText(
      this, "Edit host mapping",
      QString("Current host: %1\n\nEnter new host:").arg(oldHost),
      QLineEdit::Normal, oldHost, &ok);

  if (!ok || newHost.isEmpty()) {
    LOG_PERF(0, "Exit - Cancelled");
    return;
  }

  if (newHost == oldHost) {
    LOG_PERF(0, "Exit - No change");
    return;
  }

  // Check if new host already exists
  GitRepo::GitKeyPair existingKeyPair;
  if (_keyStorage.getKeyPairForHost(newHost, existingKeyPair)) {
    QMessageBox::warning(
        this, "Host already mapped",
        QString("Host '%1' is already mapped to key '%2'.\n\n"
                "Please choose a different host or delete the existing mapping "
                "first.")
            .arg(newHost, existingKeyPair.keyName));
    LOG_PERF(0, "Exit - Host exists");
    return;
  }

  // Remove old mapping and add new one
  _keyStorage.removeKeyPair(oldHost);
  QString publicKeyPath = privateKeyPath + ".pub";
  _keyStorage.setKeyPair(newHost, keyName, privateKeyPath, publicKeyPath);

  // Reload the table
  loadKeyPairs();

  LOG_INFO("SSH key mapping updated: '{}' -> '{}'", oldHost, newHost);
  LOG_PERF(0, "Exit");
}

void GitKeysContainer::onDeleteClicked() {
  LOG_PERF(0, "Enter");

  QModelIndexList selected = ui->tableView->selectionModel()->selectedRows();
  if (selected.isEmpty()) {
    LOG_PERF(0, "Exit - No selection");
    return;
  }

  // Get the host from the selected row
  int row = selected.first().row();
  QString host = _model->item(row, 0)->text();
  QString keyName = _model->item(row, 1)->text();

  // Confirm deletion
  QMessageBox::StandardButton reply = QMessageBox::question(
      this, "Delete key mapping",
      QString("Are you sure you want to delete the key mapping for:\n\n"
              "Host: %1\nKey: %2\n\n"
              "Note: This will only remove the mapping. The actual key files "
              "will not be deleted.")
          .arg(host, keyName),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply == QMessageBox::Yes) {
    LOG_INFO("Deleting key mapping for host: {}", host);
    _keyStorage.removeKeyPair(host);

    // Reload the table
    loadKeyPairs();

    LOG_INFO("Key mapping deleted successfully");
  }

  LOG_PERF(0, "Exit");
}

void GitKeysContainer::onRefreshClicked() {
  LOG_PERF(0, "Enter");
  loadKeyPairs();
  LOG_PERF(0, "Exit");
}

void GitKeysContainer::onSelectionChanged() {
  bool hasSelection =
      !ui->tableView->selectionModel()->selectedRows().isEmpty();
  ui->editButton->setEnabled(hasSelection);
  ui->deleteButton->setEnabled(hasSelection);
}

void GitKeysContainer::selected() {
  LOG_PERF(0, "Enter");
  loadKeyPairs();
  LOG_PERF(0, "Exit");
}

void GitKeysContainer::deselected() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
