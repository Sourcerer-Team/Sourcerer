#include "containers/BugReportContainer.h"
#include "ui_BugReportContainer.h"

#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"
#include "widgets/ToastHostWidget.h"

#include <CloudCredentialStorage.h>
#include <CloudRepo.h>
#include <JWTDecoder.h>
#include <Log.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QTimer>

#include <zip.h>

#define THREAD_ID "BugReportContainer"

namespace GuiManager::Containers {

BugReportContainer::BugReportContainer(QWidget *parent)
    : ContainerWidget(ContainerType::BugReport, parent),
      ui(new Ui::BugReportContainer),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _spinnerOverlay(new SpinnerOverlayWidget(this)) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  _spinnerOverlay->hide();

  // Disable submit button initially
  ui->pushButtonSubmit->setEnabled(false);

  connect(ui->pushButtonSubmit, &QPushButton::clicked, this,
          &BugReportContainer::onSubmitClicked);
  connect(ui->pushButtonCancel, &QPushButton::clicked, this,
          &BugReportContainer::onCancelClicked);

  // Connect input fields to validation
  connect(ui->lineEditTitle, &QLineEdit::textChanged, this,
          &BugReportContainer::onInputChanged);
  connect(ui->textEditDescription, &QTextEdit::textChanged, this,
          &BugReportContainer::onInputChanged);

  // Connect to CloudRepo signals for upload status
  connect(&Util::CloudRepo::getInstance(),
          &Util::CloudRepo::uploadBugReportFailed, this,
          &BugReportContainer::onUploadFailed);

  LOG_PERF(0, "Exit");
}

BugReportContainer::~BugReportContainer() {
  LOG_PERF(0, "Enter");

  showSpinnerOverlay(false);

  // Clean up temporary zip file
  if (!_tempZipFilePath.isEmpty() && QFile::exists(_tempZipFilePath)) {
    QFile::remove(_tempZipFilePath);
  }

  delete ui;

  LOG_PERF(0, "Exit");
}

void BugReportContainer::selected() {
  LOG_PERF(0, "Enter");

  // Set focus with a slight delay to ensure widget is fully shown
  QTimer::singleShot(0, ui->lineEditTitle,
                     [this]() { ui->lineEditTitle->setFocus(); });

  LOG_PERF(0, "Exit");
}

void BugReportContainer::deselected() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void BugReportContainer::onSubmitClicked() {
  LOG_PERF(0, "Enter");

  if (!validateInputs()) {
    ToastHostWidget::getInstance().addToast(
        new ErrorToast(tr("Please fill in all required fields"), tr("Error")));
    LOG_PERF(0, "Exit");
    return;
  }

  // Disable inputs while submitting
  ui->lineEditTitle->setEnabled(false);
  ui->textEditDescription->setEnabled(false);
  ui->pushButtonSubmit->setEnabled(false);
  ui->pushButtonCancel->setEnabled(false);

  submitBugReport();

  LOG_PERF(0, "Exit");
}

void BugReportContainer::onCancelClicked() {
  LOG_PERF(0, "Enter");

  // Hide spinner overlay if visible
  showSpinnerOverlay(false);

  // Clear inputs
  ui->lineEditTitle->clear();
  ui->textEditDescription->clear();

  // Close the container
  emit close();

  LOG_PERF(0, "Exit");
}

void BugReportContainer::onInputChanged() {
  // Enable submit button only if both fields have content
  bool hasTitle = !ui->lineEditTitle->text().trimmed().isEmpty();
  bool hasDescription =
      !ui->textEditDescription->toPlainText().trimmed().isEmpty();

  ui->pushButtonSubmit->setEnabled(hasTitle && hasDescription);
}

void BugReportContainer::onUploadFailed(const QString &error) {
  LOG_PERF(0, "Enter");

  QString errorMessage = tr("Failed to upload bug report: ") + error;

  LOG_ERROR(Log::ErrorId::Network, "Upload error: {}", errorMessage);

  showSpinnerOverlay(false);
  restoreSubmissionUi();

  ToastHostWidget::getInstance().addToast(
      new ErrorToast(errorMessage, tr("Upload Error")));

  // Remove temporary zip file
  if (!_tempZipFilePath.isEmpty() && QFile::exists(_tempZipFilePath)) {
    QFile::remove(_tempZipFilePath);
    _tempZipFilePath.clear();
  }

  LOG_PERF(0, "Exit");
}

bool BugReportContainer::validateInputs() const {
  LOG_PERF(0, "Enter");

  if (ui->lineEditTitle->text().trimmed().isEmpty()) {
    LOG_DEBUG("Bug title is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (ui->textEditDescription->toPlainText().trimmed().isEmpty()) {
    LOG_DEBUG("Bug description is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

QString BugReportContainer::createLogsZipFile(const QString &title,
                                              const QString &description) {
  LOG_PERF(0, "Enter");

  const QString logsDir = _configManager.getLogsDir();
  LOG_DEBUG("Logs directory: {}", logsDir);

  QDir dir(logsDir);
  if (!dir.exists()) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Logs directory does not exist: {}",
              logsDir);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get all .log files
  QStringList logFiles = dir.entryList(QStringList() << "*.log", QDir::Files);
  if (logFiles.isEmpty()) {
    LOG_WARN("No log files found in {}", logsDir);
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_DEBUG("Found {} log files", logFiles.size());

  // Get email, first_name, and last_name from JWT
  QString email = "unknown";
  QString firstName = "unknown";
  QString lastName = "unknown";

  QString jwt;
  if (Util::CloudCredentialtorage::getInstance().getJWT(jwt)) {
    const auto claims = Util::JWTDecoder::decodeClaims(jwt);
    if (!claims.email.isEmpty()) {
      email = claims.email;
    }
    if (!claims.givenName.isEmpty()) {
      firstName = claims.givenName;
    }
    if (!claims.familyName.isEmpty()) {
      lastName = claims.familyName;
    }
    LOG_DEBUG("Retrieved JWT claims - Email: {}, First Name: {}, Last Name: {}",
              email, firstName, lastName);
  } else {
    LOG_WARN("No JWT token found, using default values for filename");
  }

  // Get current time in UTC epoch (seconds since Jan 1, 1970)
  const auto now = QDateTime::currentDateTime().toUTC().toMSecsSinceEpoch();

  // Generate filename: bug_report_<email>_<first_name>_<last_name>_<epoch>.zip
  QString filename = QString("bug-report-%1-%2-%3-%4.zip")
                         .arg(email, firstName, lastName, QString::number(now));

  QString zipPath;

  {
    // Create temporary zip file with the new filename format
    QTemporaryFile tempFile(QDir::tempPath() + "/" + filename);
    if (!tempFile.open()) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Failed to create temporary zip file");
      LOG_PERF(0, "Exit");
      return {};
    }

    zipPath = tempFile.fileName();
    tempFile.close();
    LOG_DEBUG("Created temporary file: {}", zipPath);
  }

  LOG_DEBUG("Creating zip file: {}", zipPath);

  // Create zip archive using libzip
  int error = 0;
  zip_t *archive =
      zip_open(zipPath.toUtf8().constData(), ZIP_CREATE | ZIP_TRUNCATE, &error);

  if (!archive) {
    zip_error_t zipError;
    zip_error_init_with_code(&zipError, error);
    LOG_ERROR(Log::ErrorId::FileSystem, "Failed to create zip archive: {}",
              zip_error_strerror(&zipError));
    zip_error_fini(&zipError);
    QFile::remove(zipPath);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Create BugDescription.txt content
  QString descriptionContent = QString("Bug Report\n"
                                       "============\n\n"
                                       "Title: %1\n\n"
                                       "Description:\n"
                                       "%2")
                                   .arg(title, description);

  QByteArray descriptionBytes = descriptionContent.toUtf8();

  // Add BugDescription.txt to archive
  zip_source_t *descSource = zip_source_buffer(
      archive, descriptionBytes.constData(), descriptionBytes.size(), 0);
  if (!descSource) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to create zip source for BugDescription.txt: {}",
              zip_strerror(archive));
  } else if (zip_file_add(archive, "BugDescription.txt", descSource,
                          ZIP_FL_ENC_UTF_8) < 0) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to add BugDescription.txt to archive: {}",
              zip_strerror(archive));
    zip_source_free(descSource);
  }

  // Add each log file to the archive
  int filesAdded = 0;
  for (const QString &logFile : logFiles) {
    QString fullPath = dir.absoluteFilePath(logFile);

    // Open the file to read
    zip_source_t *source =
        zip_source_file(archive, fullPath.toUtf8().constData(), 0, -1);
    if (!source) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Failed to create zip source for {}: {}", logFile,
                zip_strerror(archive));
      continue;
    }

    // Add file to archive with just the filename (not full path)
    if (zip_file_add(archive, logFile.toUtf8().constData(), source,
                     ZIP_FL_ENC_UTF_8) < 0) {
      LOG_ERROR(Log::ErrorId::FileSystem, "Failed to add {} to archive: {}",
                logFile, zip_strerror(archive));
      zip_source_free(source);
      continue;
    }

    filesAdded++;
  }

  // Close the archive
  if (zip_close(archive) < 0) {
    LOG_ERROR(Log::ErrorId::FileSystem, "Failed to close zip archive: {}",
              zip_strerror(archive));
    zip_discard(archive);
    QFile::remove(zipPath);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (filesAdded == 0) {
    LOG_WARN("No files were added to the zip archive");
    QFile::remove(zipPath);
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_DEBUG("Successfully created zip file with {} files", filesAdded);
  LOG_PERF(0, "Exit");
  return zipPath;
}

void BugReportContainer::submitBugReport() {
  LOG_PERF(0, "Enter");

  showSpinnerOverlay(true);
  _spinnerOverlay->setText(tr("Creating log archive..."));

  // Capture title and description on main thread
  QString title = ui->lineEditTitle->text();
  QString description = ui->textEditDescription->toPlainText();

  // Create weak reference for safe access from background thread
  auto guard = getSelf().toWeakRef();

  // Create zip file with logs in background thread
  _dispatcher.dispatch([=]() {
    LOG_PERF(0, "Enter zip creation thread");

    // Create the zip file on background thread
    const QString zipPath = createLogsZipFile(title, description);

    ::Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { onZipCreationFinished(guard, zipPath); });

    LOG_PERF(0, "Exit zip creation thread");
  });

  LOG_PERF(0, "Exit");
}

void BugReportContainer::onZipCreationFinished(const WeakPtr &guard,
                                               const QString &zipPath) {
  LOG_PERF(0, "Enter");

  _tempZipFilePath = zipPath;
  if (_tempZipFilePath.isEmpty()) {
    LOG_WARN("No logs to attach, submitting without log files");

    showSpinnerOverlay(false);
    ToastHostWidget::getInstance().addToast(
        new ErrorToast(tr("Failed to create log archive"), tr("Error")));

    LOG_PERF(0, "Exit");
    return;
  }

  // Update progress message
  updateProgressLabel(0);
  _spinnerOverlay->setText(tr("Uploading bug report..."));

  LOG_DEBUG("Uploading bug report zip: {}", _tempZipFilePath);

  _dispatcher.dispatch([=]() {
    // Use CloudRepo to upload the bug report
    const auto result =
        Util::CloudRepo::getInstance().uploadBugReport(_tempZipFilePath);

    ::Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);

      if (result.first != QNetworkReply::NetworkError::NoError) {
        LOG_ERROR(Log::ErrorId::Network, "Failed to upload bug report: {}",
                  result.second);
        restoreSubmissionUi();
        ToastHostWidget::getInstance().addToast(
            new ErrorToast(result.second.isEmpty()
                               ? tr("Failed to upload bug report")
                               : QString(tr("Failed to upload bug report: %1"))
                                     .arg(result.second),
                           tr("Error")));
        return;
      }

      LOG_DEBUG("Bug report uploaded successfully");
      restoreSubmissionUi();
      ui->lineEditTitle->clear();
      ui->textEditDescription->clear();
      if (!_tempZipFilePath.isEmpty() && QFile::exists(_tempZipFilePath)) {
        QFile::remove(_tempZipFilePath);
        _tempZipFilePath.clear();
      }
      ToastHostWidget::getInstance().addToast(new InfoToast(
          tr("Bug report submitted successfully. Thank you!"), tr("Success")));
    });
  });

  LOG_PERF(0, "Exit");
}

void BugReportContainer::restoreSubmissionUi() {
  LOG_PERF(0, "Enter");

  ui->lineEditTitle->setEnabled(true);
  ui->textEditDescription->setEnabled(true);
  ui->pushButtonSubmit->setEnabled(true);
  ui->pushButtonCancel->setEnabled(true);

  LOG_PERF(0, "Exit");
}

void BugReportContainer::showSpinnerOverlay(bool show) {
  LOG_PERF(0, "Enter");

  if (show) {
    _spinnerOverlay->startSpinning();
    _spinnerOverlay->raise();
    _spinnerOverlay->show();
  } else {
    _spinnerOverlay->hide();
    _spinnerOverlay->stopSpinning();
  }

  LOG_PERF(0, "Exit");
}

void BugReportContainer::updateProgressLabel(int percentage) {
  QString text = tr("Uploading... %1%").arg(percentage);
  _spinnerOverlay->setText(text);
  LOG_DEBUG("Progress: {}%", percentage);
}

} // namespace GuiManager::Containers
