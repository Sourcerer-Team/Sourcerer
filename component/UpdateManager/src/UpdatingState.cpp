#include "UpdatingState.h"
#include "UpdateManagerEvent.h"
#include "UpdateManagerFsm.h"

#include <Log.h>
#include <NewsTrackerStructs.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QFileInfo>
#include <QNetworkReply>
#include <QProcess>
#include <QTemporaryFile>
#include <QTextStream>
#include <QUrl>
#include <QStringList>

#define THREAD_ID "UpdatingState"

namespace UpdateManager {

namespace {
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

#ifdef Q_OS_LINUX
QString resolveRunningAppImagePath() {
  const QString envAppImage =
      QString::fromLocal8Bit(qgetenv("APPIMAGE")).trimmed();

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

    const QStringList mountFields = parts.first().split(' ', Qt::SkipEmptyParts);
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

  LOG_WARN(
      "Could not resolve running AppImage file. Falling back to direct launch.");
  return QString();
}
#endif
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

    if (updated) {
      EventManager::Event updatedEvent(EventManager::EventType::ApplicationUpdated);
      updatedEvent.addData(news.version);
      _eventManager.send(updatedEvent);
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

  QString downloadedFilePath;
  if (!downloadUpdate(newsItem, downloadedFilePath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!launchInstaller(downloadedFilePath)) {
    LOG_WARN("Failed to launch downloaded update '{}'.",
             downloadedFilePath.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool UpdatingState::downloadUpdate(
    const NewsTracker::Structs::NewsItem &newsItem,
    QString &downloadedFilePath) {
  LOG_PERF(0, "Enter");

  const QString downloadUrl = normalizedDownloadUrl(newsItem.downloadUrl());
  const Util::HttpReply reply = _httpClient.sendRequest(
      downloadUrl, Util::HttpMethod::Get, {}, QByteArray());
  if (reply.error() != QNetworkReply::NoError) {
    LOG_WARN("Failed to download update '{}' from '{}'. Error: {}",
             newsItem.id.toStdString(), downloadUrl.toStdString(),
             static_cast<int>(reply.error()));
    LOG_PERF(0, "Exit");
    return false;
  }

  const QByteArray response = reply.response();
  if (response.isEmpty()) {
    LOG_WARN("Downloaded update '{}' is empty.", newsItem.id.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString downloadPath = buildDownloadFilePath(newsItem, downloadUrl);
  QTemporaryFile tempFile(downloadPath);
  tempFile.setAutoRemove(false);

  if (!tempFile.open()) {
    LOG_WARN("Failed to create temporary file for update '{}'.",
             newsItem.id.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  if (tempFile.write(response) != response.size()) {
    LOG_WARN("Failed to write downloaded update '{}'.",
             newsItem.id.toStdString());
    tempFile.close();
    QFile::remove(tempFile.fileName());
    LOG_PERF(0, "Exit");
    return false;
  }

  tempFile.flush();
  downloadedFilePath = tempFile.fileName();
  tempFile.close();

  if (!makeExecutableIfNeeded(downloadedFilePath)) {
    LOG_WARN("Failed to update permissions for downloaded update '{}'.",
             downloadedFilePath.toStdString());
  }

  LOG_INFO("Downloaded update '{}' to '{}'.", newsItem.id.toStdString(),
           downloadedFilePath.toStdString());

  LOG_PERF(0, "Exit");
  return true;
}

bool UpdatingState::launchInstaller(const QString &downloadedFilePath) const {
  LOG_PERF(0, "Enter");

#ifdef Q_OS_WIN
  const QFileInfo fileInfo(downloadedFilePath);
  if (fileInfo.suffix().compare(QStringLiteral("msi"), Qt::CaseInsensitive) ==
      0) {
    const QString program = QStringLiteral("msiexec");
    const QStringList arguments = {
                                   QStringLiteral("/i"),
        QDir::toNativeSeparators(downloadedFilePath),
        QStringLiteral("/quiet"),
        QStringLiteral("/norestart"),
        QStringLiteral("/log"),
        QString("%1/update_%2.log")
            .arg(ConfigManager::ConfigManager::getInstance().getLogsDir(),
                 QDateTime::currentDateTime()
                     .toString()
                     .replace(" ", "_")
                     .replace("/", "_")
                     .replace(":", "_"))};
    const bool started = QProcess::startDetached(program, arguments);
    if (!started) {
      LOG_WARN("Failed to start msiexec for '{}'.",
               downloadedFilePath.toStdString());
    }
    LOG_PERF(0, "Exit");
    return started;
  }
#endif

#ifdef Q_OS_LINUX
  const QString runningAppImagePath = resolveRunningAppImagePath();
  if (!runningAppImagePath.isEmpty()) {
    const QFileInfo runningAppImageInfo(runningAppImagePath);
    if (runningAppImageInfo.exists()) {
      if (!runningAppImageInfo.isFile()) {
        LOG_WARN("Resolved AppImage path '{}' is not a regular file.",
                 runningAppImagePath.toStdString());
        LOG_PERF(0, "Exit");
        return false;
      }

      if (QFile::exists(runningAppImagePath) &&
          !QFile::remove(runningAppImagePath)) {
        LOG_WARN("Failed to remove existing AppImage '{}'.",
                 runningAppImagePath.toStdString());
        LOG_PERF(0, "Exit");
        return false;
      }

      if (!QFile::copy(downloadedFilePath, runningAppImagePath)) {
        LOG_WARN(
            "Failed to copy downloaded update '{}' over running AppImage '{}'.",
            downloadedFilePath.toStdString(),
            runningAppImagePath.toStdString());
        LOG_PERF(0, "Exit");
        return false;
      }

      QFile copiedFile(runningAppImagePath);
      if (!copiedFile.setPermissions(
              QFileDevice::ReadOwner | QFileDevice::WriteOwner |
              QFileDevice::ExeOwner | QFileDevice::ReadUser |
              QFileDevice::WriteUser | QFileDevice::ExeUser |
              QFileDevice::ReadGroup | QFileDevice::ExeGroup |
              QFileDevice::ReadOther | QFileDevice::ExeOther)) {
        LOG_WARN("Failed to update permissions for copied AppImage '{}'.",
                 runningAppImagePath.toStdString());
      }

      const bool started = QProcess::startDetached(
          runningAppImagePath, QStringList{QStringLiteral("--integrate")});
      if (!started) {
        LOG_WARN("Failed to start AppImage '{}' with --integrate.",
                 runningAppImagePath.toStdString());
      }

      LOG_PERF(0, "Exit");
      return started;
    }
  }
#endif

  const bool started =
      QProcess::startDetached(downloadedFilePath, QStringList());
  if (!started) {
    LOG_WARN("Failed to start downloaded update '{}'.",
             downloadedFilePath.toStdString());
  }

  LOG_PERF(0, "Exit");
  return started;
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
