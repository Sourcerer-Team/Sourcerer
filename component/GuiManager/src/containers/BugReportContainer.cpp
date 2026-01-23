#include "containers/BugReportContainer.h"
#include "ui_BugReportContainer.h"

#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"
#include "widgets/ToastHostWidget.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHttpPart>
#include <QMessageBox>
#include <QNetworkRequest>
#include <QTemporaryFile>
#include <QTimer>
#include <QUrl>

#include <zip.h>

#define THREAD_ID "BugReportContainer"

// Configuration: Set your bug report endpoint here
#define BUG_REPORT_ENDPOINT "https://api.yourcompany.com/bug-reports"

namespace GuiManager::Containers {

BugReportContainer::BugReportContainer(QWidget *parent)
    : ContainerWidget(ContainerType::BugReport, parent),
      ui(new Ui::BugReportContainer),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _networkManager(new QNetworkAccessManager(this)), _currentReply(nullptr),
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

  LOG_PERF(0, "Exit");
}

BugReportContainer::~BugReportContainer() {
  LOG_PERF(0, "Enter");

  // Clean up spinner overlay
  if (_spinnerOverlay) {
    _spinnerOverlay->hide();
    _spinnerOverlay->stopSpinning();
    _spinnerOverlay->deleteLater();
    _spinnerOverlay = nullptr;
  }

  if (_currentReply) {
    _currentReply->abort();
    _currentReply->deleteLater();
  }

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

void BugReportContainer::onUploadProgress(qint64 bytesSent, qint64 bytesTotal) {
  if (bytesTotal > 0) {
    int percentage = static_cast<int>((bytesSent * 100) / bytesTotal);
    LOG_DEBUG("Upload progress: {}%", percentage);
    updateProgressLabel(percentage);
  }
}

void BugReportContainer::onUploadFinished() {
  LOG_PERF(0, "Enter");

  if (!_currentReply) {
    LOG_PERF(0, "Exit");
    return;
  }

  QNetworkReply::NetworkError error = _currentReply->error();

  if (error == QNetworkReply::NoError) {
    LOG_DEBUG("Bug report uploaded successfully");

    // Read response
    QByteArray response = _currentReply->readAll();
    LOG_DEBUG("Server response: {}", QString::fromUtf8(response));

    showSpinnerOverlay(false);

    // Re-enable inputs
    ui->lineEditTitle->setEnabled(true);
    ui->textEditDescription->setEnabled(true);
    ui->pushButtonSubmit->setEnabled(true);
    ui->pushButtonCancel->setEnabled(true);

    // Clear form
    ui->lineEditTitle->clear();
    ui->textEditDescription->clear();

    ToastHostWidget::getInstance().addToast(new InfoToast(
        tr("Bug report submitted successfully. Thank you!"), tr("Success")));
  } else {
    onUploadError(error);
  }

  // Clean up
  _currentReply->deleteLater();
  _currentReply = nullptr;

  // Remove temporary zip file
  if (!_tempZipFilePath.isEmpty() && QFile::exists(_tempZipFilePath)) {
    QFile::remove(_tempZipFilePath);
    _tempZipFilePath.clear();
  }

  LOG_PERF(0, "Exit");
}

void BugReportContainer::onUploadError(QNetworkReply::NetworkError code) {
  LOG_PERF(0, "Enter");

  QString errorMessage = tr("Failed to upload bug report: ");

  if (_currentReply) {
    errorMessage += _currentReply->errorString();
  } else {
    errorMessage += tr("Unknown error");
  }

  LOG_ERROR(Log::ErrorId::Network, "Upload error: {} (code: {})", errorMessage,
            static_cast<int>(code));

  showSpinnerOverlay(false);

  // Re-enable inputs
  ui->lineEditTitle->setEnabled(true);
  ui->textEditDescription->setEnabled(true);
  ui->pushButtonSubmit->setEnabled(true);
  ui->pushButtonCancel->setEnabled(true);

  ToastHostWidget::getInstance().addToast(
      new ErrorToast(errorMessage, tr("Upload Error")));

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

QString BugReportContainer::createLogsZipFile() {
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

  QString zipPath;

  {
    // Create temporary zip file
    QTemporaryFile tempFile(QDir::tempPath() + "/bug_report_XXXXXX.zip");
    if (!tempFile.open()) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Failed to create temporary zip file");
      LOG_PERF(0, "Exit");
      return {};
    }

    zipPath = tempFile.fileName();
    tempFile.close();
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

  // Create weak reference for safe access from background thread
  auto guard = getSelf().toWeakRef();

  // Create zip file with logs in background thread
  _dispatcher.dispatch([=]() {
    LOG_PERF(0, "Enter zip creation thread");

    // Create the zip file on background thread
    QString zipPath = createLogsZipFile();

    LOG_PERF(0, "Exit zip creation thread");

    // Notify main thread when done
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      if (auto self = guard.lock()) {
        self->onZipCreationFinished(zipPath);
      }
    });
  });

  LOG_PERF(0, "Exit");
}

void BugReportContainer::onZipCreationFinished(const QString &zipPath) {
  LOG_PERF(0, "Enter");

  _tempZipFilePath = zipPath;
  if (_tempZipFilePath.isEmpty()) {
    LOG_WARN("No logs to attach, submitting without log files");
  }

  // Update progress message
  updateProgressLabel(0);

  // Create multipart form data
  QHttpMultiPart *multiPart = new QHttpMultiPart(QHttpMultiPart::FormDataType);

  // Add title field
  QHttpPart titlePart;
  titlePart.setHeader(QNetworkRequest::ContentDispositionHeader,
                      QVariant("form-data; name=\"title\""));
  titlePart.setBody(ui->lineEditTitle->text().toUtf8());
  multiPart->append(titlePart);

  // Add description field
  QHttpPart descriptionPart;
  descriptionPart.setHeader(QNetworkRequest::ContentDispositionHeader,
                            QVariant("form-data; name=\"description\""));
  descriptionPart.setBody(ui->textEditDescription->toPlainText().toUtf8());
  multiPart->append(descriptionPart);

  // Add zip file if available
  if (!_tempZipFilePath.isEmpty()) {
    QFile *file = new QFile(_tempZipFilePath);
    if (file->open(QIODevice::ReadOnly)) {
      QHttpPart filePart;
      filePart.setHeader(QNetworkRequest::ContentTypeHeader,
                         QVariant("application/zip"));
      filePart.setHeader(
          QNetworkRequest::ContentDispositionHeader,
          QVariant(QString("form-data; name=\"logs\"; filename=\"%1\"")
                       .arg(QFileInfo(_tempZipFilePath).fileName())));
      filePart.setBodyDevice(file);
      file->setParent(multiPart); // File will be deleted with multiPart
      multiPart->append(filePart);

      LOG_DEBUG("Attached log file: {}", _tempZipFilePath);
    } else {
      LOG_ERROR(Log::ErrorId::FileSystem, "Failed to open zip file for upload");
      delete file;
    }
  }

  // Create request
  QNetworkRequest request(QUrl(BUG_REPORT_ENDPOINT));
  request.setHeader(QNetworkRequest::UserAgentHeader, "Sourcerer/1.0");

  // Send request
  _currentReply = _networkManager->post(request, multiPart);
  multiPart->setParent(_currentReply); // Delete multiPart with reply

  // Connect signals
  connect(_currentReply, &QNetworkReply::uploadProgress, this,
          &BugReportContainer::onUploadProgress);
  connect(_currentReply, &QNetworkReply::finished, this,
          &BugReportContainer::onUploadFinished);

  LOG_DEBUG("Sending bug report to {}", BUG_REPORT_ENDPOINT);

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
