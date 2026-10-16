#include "UpdatingState.h"
#include "UpdateManagerEvent.h"
#include "UpdateManagerFsm.h"

#include <Log.h>
#include <NewsTrackerStructs.h>
#include <UpdateStaging.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QNetworkReply>
#include <QStringList>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrl>

#include <chrono>
#include <thread>

#define THREAD_ID "UpdatingState"

namespace UpdateManager {

namespace {
constexpr int DOWNLOAD_RETRY_COUNT = 5;
constexpr int DOWNLOAD_RETRY_DELAY_MS = 3000;

QString normalizedDownloadUrl(const QString &downloadUrl) {
  return downloadUrl.trimmed();
}

QString fileSuffixFromUrl(const QString &downloadUrl) {
  const QFileInfo urlInfo(QUrl(downloadUrl).path());
  const QString suffix = urlInfo.suffix().trimmed();
  if (suffix.isEmpty()) {
    return QString();
  }

  return QStringLiteral(".") + suffix;
}

#ifdef Q_OS_WIN
QString buildWindowsInstallerLogPath(const QString &installerKind) {
  const QString logsDir =
      ConfigManager::ConfigManager::getInstance().getLogsDir();
  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
  return QStringLiteral("%1/update_%2_%3.log")
      .arg(logsDir, installerKind, timestamp);
}
#endif

#ifdef Q_OS_LINUX
QString resolveRunningAppImagePath() {
  const QString envAppImage =
      QString::fromLocal8Bit(qgetenv("ARGV0")).trimmed();

  const QFileInfo appFileInfo(QCoreApplication::applicationFilePath());
  const QString appDir = appFileInfo.absolutePath();

  QFile mountInfo(QStringLiteral("/proc/self/mountinfo"));
  if (!mountInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return envAppImage;
  }

  QTextStream stream(&mountInfo);
  while (!stream.atEnd()) {
    const QString line = stream.readLine();
    const QStringList parts = line.split(QStringLiteral(" - "));
    if (parts.size() != 2) {
      continue;
    }

    const QStringList mountFields =
        parts.first().split(' ', Qt::SkipEmptyParts);
    const QStringList fsFields = parts.last().split(' ', Qt::SkipEmptyParts);
    if (mountFields.size() < 5 || fsFields.size() < 2) {
      continue;
    }

    const QString mountPoint = mountFields.at(4);
    if (QDir::cleanPath(mountPoint) != QDir::cleanPath(appDir)) {
      continue;
    }

    const QString mountSource = fsFields.at(1);
    if (!mountSource.trimmed().isEmpty()) {
      return mountSource.trimmed();
    }
  }

  if (!envAppImage.isEmpty()) {
    const QFileInfo envInfo(envAppImage);
    if (envInfo.exists() && envInfo.isFile() &&
        envInfo.fileName().endsWith(QStringLiteral(".AppImage"),
                                    Qt::CaseInsensitive)) {
      return envAppImage;
    }
  }

  return QString();
}
#endif

QString installationDirectory() {
#ifdef Q_OS_MACOS
  // Never stage inside the signed .app bundle; see UpdateStaging.h.
  const QString stagingDir = Util::UpdateStaging::macosStagingDirectory();
  QDir().mkpath(stagingDir);
  return stagingDir;
#endif
#ifdef Q_OS_LINUX
  const QString runningAppImagePath = resolveRunningAppImagePath();
  if (!runningAppImagePath.isEmpty()) {
    return QFileInfo(runningAppImagePath).absolutePath();
  }
#endif
  return QCoreApplication::applicationDirPath();
}

bool stageDownloadedUpdate(const QString &downloadedFilePath,
                           const QString &stagedFilePath) {
  const QDir stagedDir = QFileInfo(stagedFilePath).absoluteDir();
  const QFileInfoList existingUpdates = stagedDir.entryInfoList(
      {QStringLiteral("Sourcerer-Update*")}, QDir::Files | QDir::Readable);
  for (const QFileInfo &existingUpdate : existingUpdates) {
    if (existingUpdate.absoluteFilePath() == stagedFilePath) {
      continue;
    }

    if (!QFile::remove(existingUpdate.absoluteFilePath())) {
      return false;
    }
  }

  if (QFile::exists(stagedFilePath) && !QFile::remove(stagedFilePath)) {
    return false;
  }

  if (!QFile::copy(downloadedFilePath, stagedFilePath)) {
    return false;
  }

#ifndef Q_OS_WIN
  QFile stagedFile(stagedFilePath);
  const QFile::Permissions permissions =
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
      QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser |
      QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther |
      QFileDevice::ExeOther;
  if (!stagedFile.setPermissions(permissions)) {
    return false;
  }
#endif

  return true;
}

} // namespace

UpdatingState::UpdatingState(UpdateManagerFsm &fsm)
    : UpdateManagerState{StateType::Updating, fsm},
      _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

UpdatingState::~UpdatingState() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void UpdatingState::enter() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool UpdatingState::process(Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::ProcessUpdate: {
    const auto news = event.nextParam<NewsTracker::Structs::NewsItem>();

    const bool updated = workerLoop(news);
    if (!updated) {
      LOG_WARN("Update staging failed for news '{}'.", news.id.toStdString());
    }

    UpdateManagerEvent changeEvent(EventType::ChangeState);
    changeEvent.addParam((int)StateType::Waiting);
    fsm().send(changeEvent);
    break;
  }
  default: {
    LOG_WARN("Unhandled event {}", (int)type);
    return false;
  }
  }

  LOG_PERF(0, "Exit");

  return true;
}

void UpdatingState::exit() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

bool UpdatingState::workerLoop(const NewsTracker::Structs::NewsItem &newsItem) {
  LOG_PERF(0, "Enter");

  if (!newsItem.isValid() || !newsItem.isUpdate()) {
    LOG_WARN("Update worker started without a valid update item.");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString downloadUrl = normalizedDownloadUrl(newsItem.downloadUrl());
  if (downloadUrl.isEmpty()) {
    LOG_WARN("Update news '{}' does not contain a download URL.",
             newsItem.id.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

#ifdef Q_OS_MACOS
  {
    const QUrl url(downloadUrl);
    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) !=
            0 ||
        !url.path().endsWith(QStringLiteral(".dmg"), Qt::CaseInsensitive)) {
      LOG_WARN("Refusing macOS update '{}': '{}' is not an https URL of a "
               ".dmg file.",
               newsItem.id.toStdString(), downloadUrl.toStdString());
      LOG_PERF(0, "Exit");
      return false;
    }
  }
#endif

  QString downloadedFilePath;
  if (!downloadUpdate(newsItem, downloadedFilePath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString installDir = installationDirectory();
  const QString stagedFilePath =
      buildStagedUpdateFilePath(downloadUrl, installDir);

  if (!stageDownloadedUpdate(downloadedFilePath, stagedFilePath)) {
    LOG_WARN("Failed to stage downloaded update '{}' to '{}'.",
             downloadedFilePath.toStdString(), stagedFilePath.toStdString());
    QFile::remove(downloadedFilePath);
    LOG_PERF(0, "Exit");
    return false;
  }

  QFile::remove(downloadedFilePath);

  EventManager::Event updatedEvent(EventManager::EventType::ApplicationUpdated);
  updatedEvent.addData(newsItem);

  _eventManager.send(updatedEvent);

  LOG_INFO("Staged update '{}' at '{}'.", newsItem.id.toStdString(),
           stagedFilePath.toStdString());

  LOG_PERF(0, "Exit");

  return true;
}

bool UpdatingState::downloadUpdate(
    const NewsTracker::Structs::NewsItem &newsItem,
    QString &downloadedFilePath) {
  LOG_PERF(0, "Enter");

  const QString downloadUrl = normalizedDownloadUrl(newsItem.downloadUrl());
  const QString downloadPath = buildDownloadFilePath(newsItem, downloadUrl);

  for (int attempt = 1; attempt <= DOWNLOAD_RETRY_COUNT; ++attempt) {
    QTemporaryFile tempFile(downloadPath);
    tempFile.setAutoRemove(false);

    if (!tempFile.open()) {
      LOG_WARN("Failed to create temporary file for update '{}' on attempt "
               "{}/{}.",
               newsItem.id.toStdString(), attempt, DOWNLOAD_RETRY_COUNT);
    } else {
      const QString tempFilePath = tempFile.fileName();
      tempFile.close();

      // Installers are large, so stream them to disk instead of using
      // sendRequest(), whose timeout caps the duration of the whole request.
      const Util::HttpReply reply =
          _httpClient.downloadFile(downloadUrl, tempFilePath);
      if (reply.error() != QNetworkReply::NoError) {
        LOG_WARN("Failed to download update '{}' from '{}' on attempt {}/{}. "
                 "Error: {}, '{}'",
                 newsItem.id.toStdString(), downloadUrl.toStdString(), attempt,
                 DOWNLOAD_RETRY_COUNT, static_cast<int>(reply.error()),
                 reply.errorString().toStdString());
        QFile::remove(tempFilePath);
      } else if (QFileInfo(tempFilePath).size() == 0) {
        LOG_WARN("Downloaded update '{}' is empty on attempt {}/{}.",
                 newsItem.id.toStdString(), attempt, DOWNLOAD_RETRY_COUNT);
        QFile::remove(tempFilePath);
      } else {
        downloadedFilePath = tempFilePath;

        if (!makeExecutableIfNeeded(downloadedFilePath)) {
          LOG_WARN("Failed to update permissions for downloaded update '{}'.",
                   downloadedFilePath.toStdString());
        }

        LOG_INFO("Downloaded update '{}' to '{}' on attempt {}/{}.",
                 newsItem.id.toStdString(), downloadedFilePath.toStdString(),
                 attempt, DOWNLOAD_RETRY_COUNT);

        LOG_PERF(0, "Exit");
        return true;
      }
    }

    if (attempt < DOWNLOAD_RETRY_COUNT) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DOWNLOAD_RETRY_DELAY_MS));
    }
  }

  LOG_PERF(0, "Exit");
  return false;
}

QString UpdatingState::buildStagedUpdateFilePath(const QString &downloadUrl,
                                                 const QString &installDir) {
  LOG_PERF(0, "Enter");

  QString baseName = QStringLiteral("Sourcerer-Update");
  const QString suffix = fileSuffixFromUrl(downloadUrl);
  if (suffix.isEmpty()) {
    const QFileInfo urlInfo(QUrl(downloadUrl).path());
    const QString fallbackSuffix = urlInfo.suffix().trimmed();
    if (!fallbackSuffix.isEmpty()) {
      baseName += QStringLiteral(".") + fallbackSuffix;
    }
  } else {
    baseName += suffix;
  }

  const QString stagedPath = QDir(installDir).absoluteFilePath(baseName);

  LOG_PERF(0, "Exit");
  return stagedPath;
}

QString UpdatingState::buildDownloadFilePath(
    const NewsTracker::Structs::NewsItem &newsItem,
    const QString &downloadUrl) {
  LOG_PERF(0, "Enter");

  const QFileInfo urlInfo(QUrl(downloadUrl).path());
  QString baseName = sanitizeFileName(urlInfo.completeBaseName());
  if (baseName.isEmpty()) {
    baseName = sanitizeFileName(newsItem.version);
  }
  if (baseName.isEmpty()) {
    baseName = QStringLiteral("Sourcerer-Update");
  }

  const QString suffix = fileSuffixFromUrl(downloadUrl);
  const QString templatePath = QDir::temp().absoluteFilePath(
      baseName + QStringLiteral("-XXXXXX") + suffix);

  LOG_PERF(0, "Exit");
  return templatePath;
}

QString UpdatingState::sanitizeFileName(QString value) {
  LOG_PERF(0, "Enter");

  value = value.trimmed();
  for (QChar &ch : value) {
    if (ch.isLetterOrNumber() || ch == QLatin1Char('-') ||
        ch == QLatin1Char('_') || ch == QLatin1Char('.')) {
      continue;
    }
    ch = QLatin1Char('_');
  }

  while (value.startsWith(QLatin1Char('.'))) {
    value.remove(0, 1);
  }

  LOG_PERF(0, "Exit");
  return value;
}

bool UpdatingState::makeExecutableIfNeeded(const QString &downloadedFilePath) {
  LOG_PERF(0, "Enter");

#ifndef Q_OS_WIN
  QFile file(downloadedFilePath);
  const QFile::Permissions permissions =
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
      QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser |
      QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther |
      QFileDevice::ExeOther;
  if (!file.setPermissions(permissions)) {
    LOG_WARN("Failed to set executable permissions for '{}'.",
             downloadedFilePath.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }
#endif

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace UpdateManager
