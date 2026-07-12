#include "containers/GitSigningContainer.h"

#include <Log.h>

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QSignalBlocker>
#include <QVBoxLayout>

#define THREAD_ID "GitSigningContainer"

namespace GuiManager::Containers {

GitSigningContainer::GitSigningContainer(QWidget *parent)
    : ContainerWidget(::GuiManager::Widgets::ContainerType::GitSigning, parent),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _enabledCheckBox(nullptr), _formatComboBox(nullptr),
      _signingKeyEdit(nullptr), _signingKeyBrowseButton(nullptr),
      _programEdit(nullptr), _programBrowseButton(nullptr), _saveButton(nullptr),
      _resetButton(nullptr), _titleLabel(nullptr), _descriptionLabel(nullptr),
      _formatLabel(nullptr), _keyLabel(nullptr), _programLabel(nullptr),
      _savedEnabled(false), _savedFormat(QStringLiteral("gpg")),
      _savedSigningKey(), _savedProgram(QStringLiteral("gpg")) {
  LOG_PERF(0, "Enter");

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(20, 50, 20, 20);
  mainLayout->setSpacing(20);

  _titleLabel = new QLabel(tr("Commit signing"), this);
  QFont titleFont = _titleLabel->font();
  titleFont.setPointSize(16);
  titleFont.setBold(true);
  _titleLabel->setFont(titleFont);
  mainLayout->addWidget(_titleLabel);

  _descriptionLabel = new QLabel(
      tr("Enable cryptographic signing for Git commits. When enabled, the "
         "Sign commit checkbox appears in the commit panel."),
      this);
  _descriptionLabel->setWordWrap(true);
  mainLayout->addWidget(_descriptionLabel);

  _enabledCheckBox = new QCheckBox(tr("Enable commit signing"), this);
  mainLayout->addWidget(_enabledCheckBox);

  auto *formatRow = new QHBoxLayout();
  formatRow->setSpacing(10);
  _formatLabel = new QLabel(tr("Signing format"), this);
  _formatComboBox = new QComboBox(this);
  _formatComboBox->addItem(tr("GPG / OpenPGP"), QStringLiteral("gpg"));
  _formatComboBox->addItem(tr("SSH"), QStringLiteral("ssh"));
  formatRow->addWidget(_formatLabel);
  formatRow->addWidget(_formatComboBox, 1);
  mainLayout->addLayout(formatRow);

  auto *keyRow = new QHBoxLayout();
  keyRow->setSpacing(10);
  _keyLabel = new QLabel(tr("Signing key"), this);
  _signingKeyEdit = new QLineEdit(this);
  _signingKeyEdit->setPlaceholderText(tr("Key ID, fingerprint, or key path"));
  _signingKeyBrowseButton = new QPushButton(tr("Browse"), this);
  _signingKeyBrowseButton->setObjectName("signingKeyBrowseButton");
  keyRow->addWidget(_keyLabel);
  keyRow->addWidget(_signingKeyEdit, 1);
  keyRow->addWidget(_signingKeyBrowseButton);
  mainLayout->addLayout(keyRow);

  auto *programRow = new QHBoxLayout();
  programRow->setSpacing(10);
  _programLabel = new QLabel(tr("Signing program"), this);
  _programEdit = new QLineEdit(this);
  _programEdit->setPlaceholderText(tr("Optional path to gpg executable"));
  _programBrowseButton = new QPushButton(tr("Browse"), this);
  _programBrowseButton->setObjectName("signingProgramBrowseButton");
  programRow->addWidget(_programLabel);
  programRow->addWidget(_programEdit, 1);
  programRow->addWidget(_programBrowseButton);
  mainLayout->addLayout(programRow);

  mainLayout->addStretch();

  auto *buttonRow = new QHBoxLayout();
  buttonRow->setSpacing(10);
  buttonRow->addStretch();
  _resetButton = new QPushButton(tr("Reset"), this);
  _resetButton->setObjectName("signingResetButton");
  _resetButton->setMinimumSize(QSize(120, 0));
  _resetButton->setMaximumSize(QSize(120, 16777215));
  _saveButton = new QPushButton(tr("Save"), this);
  _saveButton->setObjectName("signingSaveButton");
  _saveButton->setMinimumSize(QSize(120, 0));
  _saveButton->setMaximumSize(QSize(120, 16777215));
  buttonRow->addWidget(_resetButton);
  buttonRow->addWidget(_saveButton);
  mainLayout->addLayout(buttonRow);

  connect(_enabledCheckBox, &QCheckBox::toggled, this,
          &GitSigningContainer::onEnabledToggled);
  connect(_formatComboBox,
          static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
          this, &GitSigningContainer::onFormatChanged);
  connect(_signingKeyEdit, &QLineEdit::textChanged, this, [this](const QString &) {
    updateSaveButtonState();
  });
  connect(_programEdit, &QLineEdit::textChanged, this, [this](const QString &) {
    updateSaveButtonState();
  });
  connect(_signingKeyBrowseButton, &QPushButton::clicked, this,
          &GitSigningContainer::onBrowseKeyClicked);
  connect(_programBrowseButton, &QPushButton::clicked, this,
          &GitSigningContainer::onBrowseProgramClicked);
  connect(_saveButton, &QPushButton::clicked, this,
          &GitSigningContainer::onSaveClicked);
  connect(_resetButton, &QPushButton::clicked, this,
          &GitSigningContainer::onResetClicked);

  _saveButton->setEnabled(false);

  loadSettings();

  LOG_PERF(0, "Exit");
}

GitSigningContainer::~GitSigningContainer() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::selected() {
  LOG_PERF(0, "Enter");
  loadSettings();
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::deselected() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::onEnabledToggled(bool enabled) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(enabled);
  updateFormatUi();
  updateSaveButtonState();
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::onFormatChanged(int index) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(index);
  updateFormatUi();
  updateSaveButtonState();
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::onBrowseKeyClicked() {
  LOG_PERF(0, "Enter");

  const QString currentPath = _signingKeyEdit->text().trimmed();
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Select signing key"), currentPath.isEmpty() ? QString()
                                                             : currentPath,
      tr("All files (*)"));

  if (!path.isEmpty()) {
    _signingKeyEdit->setText(path);
  }

  LOG_PERF(0, "Exit");
}

void GitSigningContainer::onBrowseProgramClicked() {
  LOG_PERF(0, "Enter");

  const QString currentPath = _programEdit->text().trimmed();
  const QString path = QFileDialog::getOpenFileName(
      this, tr("Select signing program"), currentPath.isEmpty() ? QString()
                                                                 : currentPath,
      tr("Executable files (*)"));

  if (!path.isEmpty()) {
    _programEdit->setText(path);
  }

  LOG_PERF(0, "Exit");
}

void GitSigningContainer::onSaveClicked() {
  LOG_PERF(0, "Enter");
  saveSettings();
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::onResetClicked() {
  LOG_PERF(0, "Enter");
  loadSettings();
  LOG_PERF(0, "Exit");
}

void GitSigningContainer::loadSettings() {
  LOG_PERF(0, "Enter");

  QSignalBlocker enabledBlocker(_enabledCheckBox);
  QSignalBlocker formatBlocker(_formatComboBox);
  QSignalBlocker keyBlocker(_signingKeyEdit);
  QSignalBlocker programBlocker(_programEdit);

  _enabledCheckBox->setChecked(
      _configManager.getBoolean(ConfigManager::ConfigKey::GitRepoSigningEnabled));

  const QString format =
      _configManager.getString(ConfigManager::ConfigKey::GitRepoSigningFormat)
          .trimmed();
  const int formatIndex = _formatComboBox->findData(
      format.compare(QStringLiteral("ssh"), Qt::CaseInsensitive) == 0
          ? QStringLiteral("ssh")
          : QStringLiteral("gpg"));
  _formatComboBox->setCurrentIndex(formatIndex >= 0 ? formatIndex : 0);

  _signingKeyEdit->setText(
      _configManager.getString(ConfigManager::ConfigKey::GitRepoSigningKey)
          .trimmed());
  _programEdit->setText(
      _configManager.getString(ConfigManager::ConfigKey::GitRepoSigningProgram)
          .trimmed());

  updateFormatUi();
  updateDefaultProgram();

  _savedEnabled = _enabledCheckBox->isChecked();
  _savedFormat = currentFormat();
  _savedSigningKey = _signingKeyEdit->text().trimmed();
  _savedProgram = _programEdit->text().trimmed();
  updateSaveButtonState();

  LOG_PERF(0, "Exit");
}

void GitSigningContainer::saveSettings() {
  LOG_PERF(0, "Enter");

  const bool enabled = _enabledCheckBox->isChecked();
  const QString format = currentFormat();
  const QString signingKey = _signingKeyEdit->text().trimmed();
  const QString program = _programEdit->text().trimmed();

  if (enabled && format == QStringLiteral("ssh") && signingKey.isEmpty()) {
    QMessageBox::warning(this, tr("Signing key missing"),
                         tr("SSH signing requires a signing key path."));
    LOG_PERF(0, "Exit");
    return;
  }

  _configManager.setBoolean(ConfigManager::ConfigKey::GitRepoSigningEnabled,
                            enabled);
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSigningFormat,
                           format);
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSigningKey,
                           signingKey);
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSigningProgram,
                           program);
  _configManager.save();

  _savedEnabled = enabled;
  _savedFormat = format;
  _savedSigningKey = signingKey;
  _savedProgram = program;
  updateSaveButtonState();

  LOG_DEBUG("Commit signing settings saved: enabled={}, format='{}'",
            enabled, format.toStdString());

  LOG_PERF(0, "Exit");
}

void GitSigningContainer::updateFormatUi() {
  LOG_PERF(0, "Enter");

  const bool enabled = _enabledCheckBox->isChecked();
  const QString format = currentFormat();
  const bool sshFormat = format == QStringLiteral("ssh");

  _formatLabel->setEnabled(enabled);
  _formatComboBox->setEnabled(enabled);
  _keyLabel->setEnabled(enabled);
  _signingKeyEdit->setEnabled(enabled);
  _signingKeyBrowseButton->setEnabled(enabled && sshFormat);
  _programLabel->setVisible(enabled);
  _programEdit->setVisible(enabled);
  _programBrowseButton->setVisible(enabled);

  if (sshFormat) {
    _keyLabel->setText(tr("SSH private key path"));
    _signingKeyEdit->setPlaceholderText(tr("Path to the private SSH key"));
    _programEdit->setPlaceholderText(tr("Optional path to ssh-keygen executable"));
  } else {
    _keyLabel->setText(tr("Signing key"));
    _signingKeyEdit->setPlaceholderText(tr("Key ID or fingerprint"));
    _programEdit->setPlaceholderText(tr("Optional path to gpg executable"));
  }

  _programLabel->setText(tr("Signing program"));
  updateDefaultProgram();

  LOG_PERF(0, "Exit");
}

void GitSigningContainer::updateDefaultProgram() {
  LOG_PERF(0, "Enter");

  const QString format = currentFormat();
  const bool sshFormat = format == QStringLiteral("ssh");
  const QString desiredProgram =
      sshFormat ? QStringLiteral("ssh-keygen") : QStringLiteral("gpg");
  const QString currentProgram = _programEdit->text().trimmed();

  if (currentProgram.isEmpty() ||
      (sshFormat && currentProgram == QStringLiteral("gpg")) ||
      (!sshFormat && currentProgram == QStringLiteral("ssh-keygen"))) {
    QSignalBlocker programBlocker(_programEdit);
    _programEdit->setText(desiredProgram);
  }

  LOG_PERF(0, "Exit");
}

void GitSigningContainer::updateSaveButtonState() {
  LOG_PERF(0, "Enter");

  const bool currentEnabled = _enabledCheckBox->isChecked();
  const QString currentFormatValue = currentFormat();
  const QString currentSigningKey = _signingKeyEdit->text().trimmed();
  const QString currentProgram = _programEdit->text().trimmed();
  const bool dirty = currentEnabled != _savedEnabled ||
                     currentFormatValue != _savedFormat ||
                     currentSigningKey != _savedSigningKey ||
                     currentProgram != _savedProgram;
  _saveButton->setEnabled(dirty);

  LOG_PERF(0, "Exit");
}

QString GitSigningContainer::currentFormat() const {
  const QString format = _formatComboBox->currentData().toString().trimmed();
  if (format == QStringLiteral("ssh")) {
    return format;
  }

  return QStringLiteral("gpg");
}

} // namespace GuiManager::Containers
