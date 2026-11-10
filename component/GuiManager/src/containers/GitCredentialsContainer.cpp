// MIT License
// Copyright (c) 2026 Sourcerer

#include "containers/GitCredentialsContainer.h"
#include "ui_GitCredentialsContainer.h"
#include "widgets/PinnedCertificatesWidget.h"

#include <GitRepo.h>
#include <Log.h>

#include <QAction>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QStandardItemModel>
#include <QVBoxLayout>

#include <StylesheetManager.h>

#define THREAD_ID "GitCredentialsContainer"

namespace GuiManager {
namespace Containers {

static QPixmap overlayButtonIcon(const QString &iconPath) {
  const QColor iconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();

  if (iconPath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
    return StylesheetManager::StylesheetManager::renderSvgToPixmap(iconPath,
                                                                   iconColor);
  }

  return StylesheetManager::StylesheetManager::recolorPixmap(QPixmap(iconPath),
                                                             iconColor);
}

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
  connect(ui->editButton, &QPushButton::clicked, this,
          &GitCredentialsContainer::onEditClicked);
  connect(ui->deleteButton, &QPushButton::clicked, this,
          &GitCredentialsContainer::onDeleteClicked);
  connect(ui->tableView, &QTableView::doubleClicked, this,
          [this](const QModelIndex &) { onEditClicked(); });
  connect(ui->tableView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &GitCredentialsContainer::onSelectionChanged);

  loadCredentials();

  // Self-signed HTTPS certificates trusted on first use.
  _pinnedCertificates = new PinnedCertificatesWidget(
      GitRepo::PinnedCertificateType::Tls, this);
  ui->mainLayout->addWidget(_pinnedCertificates);

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
    QString normalizedRepoPath = QDir::fromNativeSeparators(repoPath);
    _credentialStorage.removeCredentials(normalizedRepoPath);

    // Clear cached credentials to ensure next operation prompts for new
    // credentials
    GitRepo::clearCachedCredentials(normalizedRepoPath);

    // Reload the table
    loadCredentials();

    LOG_INFO("Credentials deleted successfully");
  }

  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::onEditClicked() {
  LOG_PERF(0, "Enter");

  QModelIndexList selected = ui->tableView->selectionModel()->selectedRows();
  if (selected.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Get the repository path from the selected row
  int row = selected.first().row();
  QString displayPath = _model->item(row, 0)->text();
  QString repoPath = QDir::fromNativeSeparators(displayPath);

  // Retrieve current credentials
  QString currentUsername;
  QString currentPassword;

  if (!_credentialStorage.getCredentials(repoPath, currentUsername,
                                         currentPassword)) {
    LOG_WARN("Failed to retrieve credentials for repository: {}", repoPath);
    QMessageBox::warning(this, "Edit credentials",
                         "Failed to retrieve credentials for editing.");
    LOG_PERF(0, "Exit");
    return;
  }

  // Create edit dialog
  QDialog editDialog(this);
  editDialog.setWindowTitle(
      QString("Edit Git credentials - %1").arg(displayPath));
  editDialog.setMinimumWidth(400);

  QVBoxLayout *mainLayout = new QVBoxLayout(&editDialog);

  // Repository path display
  QLabel *repoLabel = new QLabel("Repository:", &editDialog);
  QLineEdit *repoEdit = new QLineEdit(&editDialog);
  repoEdit->setText(displayPath);
  repoEdit->setReadOnly(true);

  QHBoxLayout *repoLayout = new QHBoxLayout();
  repoLayout->addWidget(repoLabel);
  repoLayout->addWidget(repoEdit);
  mainLayout->addLayout(repoLayout);

  // Username
  QLabel *usernameLabel = new QLabel("Username:", &editDialog);
  QLineEdit *usernameEdit = new QLineEdit(&editDialog);
  usernameEdit->setText(currentUsername);

  QHBoxLayout *usernameLayout = new QHBoxLayout();
  usernameLayout->addWidget(usernameLabel);
  usernameLayout->addWidget(usernameEdit);
  mainLayout->addLayout(usernameLayout);

  // Password
  QLabel *passwordLabel = new QLabel("Password:", &editDialog);
  QLineEdit *passwordEdit = new QLineEdit(&editDialog);
  passwordEdit->setEchoMode(QLineEdit::Password);
  passwordEdit->setText(currentPassword);

  const QColor eyeIconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();
  QAction *showPasswordAction = passwordEdit->addAction(
      QIcon(StylesheetManager::StylesheetManager::recolorPixmap(
          overlayButtonIcon(":/icons/eye.svg"), eyeIconColor)),
      QLineEdit::TrailingPosition);
  showPasswordAction->setCheckable(true);
  showPasswordAction->setToolTip("Show password");
  connect(showPasswordAction, &QAction::toggled, &editDialog,
          [=](bool visible) {
            passwordEdit->setEchoMode(visible ? QLineEdit::Normal
                                              : QLineEdit::Password);
            showPasswordAction->setIcon(
                QIcon(StylesheetManager::StylesheetManager::recolorPixmap(
                    overlayButtonIcon(visible ? ":/icons/eye-off.svg"
                                              : ":/icons/eye.svg"),
                    eyeIconColor)));
            showPasswordAction->setToolTip(visible ? "Hide password"
                                                   : "Show password");
          });

  QHBoxLayout *passwordLayout = new QHBoxLayout();
  passwordLayout->addWidget(passwordLabel);
  passwordLayout->addWidget(passwordEdit);
  mainLayout->addLayout(passwordLayout);

  // Buttons
  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *okButton = new QPushButton("Save", &editDialog);
  QPushButton *cancelButton = new QPushButton("Cancel", &editDialog);

  buttonLayout->addStretch();
  buttonLayout->addWidget(okButton);
  buttonLayout->addWidget(cancelButton);
  mainLayout->addLayout(buttonLayout);

  // Connect button signals
  connect(okButton, &QPushButton::clicked, &editDialog, &QDialog::accept);
  connect(cancelButton, &QPushButton::clicked, &editDialog, &QDialog::reject);

  // Show dialog
  if (editDialog.exec() == QDialog::Accepted) {
    QString newUsername = usernameEdit->text();
    QString newPassword = passwordEdit->text();

    if (newUsername.isEmpty() || newPassword.isEmpty()) {
      LOG_WARN("Username or password is empty");
      QMessageBox::warning(this, "Edit credentials",
                           "Username and password cannot be empty.");
    } else {
      LOG_INFO("Updating credentials for repository: {}", repoPath);
      _credentialStorage.storeCredentials(repoPath, newUsername, newPassword,
                                          true);

      // Clear cached credentials to ensure fresh ones are used
      GitRepo::clearCachedCredentials(repoPath);

      // Reload the table
      loadCredentials();

      LOG_INFO("Credentials updated successfully");
      QMessageBox::information(this, "Edit credentials",
                               "Credentials updated successfully.");
    }
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
  ui->editButton->setEnabled(hasSelection);
  ui->deleteButton->setEnabled(hasSelection);
}

void GitCredentialsContainer::selected() {
  LOG_PERF(0, "Enter");
  loadCredentials();
  _pinnedCertificates->reload();
  LOG_PERF(0, "Exit");
}

void GitCredentialsContainer::deselected() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
