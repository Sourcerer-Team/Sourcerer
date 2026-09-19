#include "containers/GitKeysContainer.h"
#include "ui_GitKeysContainer.h"

#include <Log.h>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QColor>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QIcon>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPixmap>
#include <QPushButton>
#include <QSize>
#include <QStandardItemModel>
#include <QStyle>
#include <QTextEdit>
#include <QVBoxLayout>

#include <StylesheetManager.h>

#define THREAD_ID "GitKeysContainer"

namespace GuiManager {
namespace Containers {

static QPixmap overlayButtonIcon(const QString &iconPath) {
  const QColor iconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();

  if (iconPath.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
    return StylesheetManager::StylesheetManager::renderSvgToPixmap(iconPath,
                                                                   iconColor);
  }

  return StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(iconPath), iconColor);
}

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
  QDialog passphraseDialog(this);
  passphraseDialog.setWindowTitle("Passphrase (optional)");
  passphraseDialog.setMinimumWidth(520);

  auto *passphraseLayout = new QVBoxLayout(&passphraseDialog);
  auto *passphraseLabel =
      new QLabel("Enter a passphrase to protect the private key\n(leave empty "
                 "for no passphrase):",
                 &passphraseDialog);
  passphraseLabel->setWordWrap(true);
  passphraseLayout->addWidget(passphraseLabel);

  auto *passphraseInputRow = new QHBoxLayout();
  auto *passphraseEdit = new QLineEdit(&passphraseDialog);
  passphraseEdit->setEchoMode(QLineEdit::Password);
  passphraseEdit->setText("");
  passphraseEdit->setPlaceholderText("Passphrase");

  const QColor eyeIconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();
  QAction *showPassphraseAction = passphraseEdit->addAction(
      QIcon(StylesheetManager::StylesheetManager::recolorPixmap(
          overlayButtonIcon(":/icons/eye.svg"), eyeIconColor)),
      QLineEdit::TrailingPosition);
  showPassphraseAction->setCheckable(true);
  showPassphraseAction->setToolTip("Show passphrase");
  connect(
      showPassphraseAction, &QAction::toggled, &passphraseDialog,
      [=](bool visible) {
        passphraseEdit->setEchoMode(visible ? QLineEdit::Normal
                                            : QLineEdit::Password);
        showPassphraseAction->setIcon(
            QIcon(StylesheetManager::StylesheetManager::recolorPixmap(
                overlayButtonIcon(visible ? ":/icons/eye-off.svg" : ":/icons/eye.svg"),
                eyeIconColor)));
        showPassphraseAction->setToolTip(visible ? "Hide passphrase"
                                                 : "Show passphrase");
      });

  passphraseInputRow->addWidget(passphraseEdit);
  passphraseLayout->addLayout(passphraseInputRow);

  auto *passphraseButtonRow = new QHBoxLayout();
  auto *passphraseCancelButton =
      new QPushButton(tr("Cancel"), &passphraseDialog);
  auto *passphraseOkButton = new QPushButton(tr("OK"), &passphraseDialog);
  passphraseOkButton->setDefault(true);
  passphraseButtonRow->addStretch();

#ifdef Q_OS_WIN32
  passphraseButtonRow->addWidget(passphraseOkButton);
  passphraseButtonRow->addWidget(passphraseCancelButton);
#else
  passphraseCancelButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogCancelButton));
  passphraseOkButton->setIcon(
      QApplication::style()->standardIcon(QStyle::SP_DialogOkButton));
  passphraseButtonRow->addWidget(passphraseCancelButton);
  passphraseButtonRow->addWidget(passphraseOkButton);
#endif

  passphraseLayout->addLayout(passphraseButtonRow);

  connect(passphraseOkButton, &QPushButton::clicked, &passphraseDialog,
          &QDialog::accept);
  connect(passphraseCancelButton, &QPushButton::clicked, &passphraseDialog,
          &QDialog::reject);

  if (passphraseDialog.exec() != QDialog::Accepted) {
    LOG_PERF(0, "Exit - Cancelled");
    return;
  }

  QString passphrase = passphraseEdit->text();

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

  if (host.isEmpty()) {
    QMessageBox::information(this, "Failure",
                             QString("SSH key pair will be removed because you "
                                     "didn't associate the host name."));

    QFile::remove(privateKeyPath);
    QFile::remove(publicKeyPath);

    LOG_PERF(0, "Exit");
    return;
  }

  // Read and display the public key content
  QFile publicKeyFile(publicKeyPath);
  QString publicKeyContent;
  if (publicKeyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    publicKeyContent = QString::fromUtf8(publicKeyFile.readAll());
    publicKeyFile.close();
  }

  // Create a dialog with public key content and copy button
  QDialog dialog(this);
  dialog.setWindowTitle("SSH Key Generated Successfully");
  dialog.setMinimumWidth(600);

  QVBoxLayout *dialogLayout = new QVBoxLayout(&dialog);

  QLabel *titleLabel = new QLabel(
      "Your SSH key pair has been generated and associated with the host.");
  QFont titleFont = titleLabel->font();
  titleFont.setPointSize(titleFont.pointSize() + 1);
  titleLabel->setFont(titleFont);
  dialogLayout->addWidget(titleLabel);

  QLabel *publicKeyLabel = new QLabel("Public Key:");
  publicKeyLabel->setStyleSheet("font-weight: bold; margin-top: 10px;");
  dialogLayout->addWidget(publicKeyLabel);

  QTextEdit *publicKeyDisplay = new QTextEdit();
  publicKeyDisplay->setPlainText(publicKeyContent);
  publicKeyDisplay->setReadOnly(true);
  publicKeyDisplay->setMaximumHeight(150);
  dialogLayout->addWidget(publicKeyDisplay);

  QHBoxLayout *buttonLayout = new QHBoxLayout();
  QPushButton *copyButton = new QPushButton("Copy to Clipboard");
  connect(copyButton, &QPushButton::clicked, [publicKeyContent]() {
    QApplication::clipboard()->setText(publicKeyContent);
  });
  buttonLayout->addWidget(copyButton);
  buttonLayout->addStretch();
  dialogLayout->addLayout(buttonLayout);

  QLabel *detailsLabel = new QLabel();
  detailsLabel->setText(QString("Private key: %1\nPublic key: %2")
                            .arg(privateKeyPath, publicKeyPath));
  detailsLabel->setStyleSheet("margin-top: 10px; color: gray;");
  detailsLabel->setWordWrap(true);
  dialogLayout->addWidget(detailsLabel);

  dialogLayout->addStretch();

  QHBoxLayout *closeLayout = new QHBoxLayout();
  QPushButton *okButton = new QPushButton("OK");
  connect(okButton, &QPushButton::clicked, &dialog, &QDialog::accept);
  closeLayout->addStretch();
  closeLayout->addWidget(okButton);
  dialogLayout->addLayout(closeLayout);

  dialog.exec();

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
