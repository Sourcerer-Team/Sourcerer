#include "UpdateManager.h"
#include "UpdateManagerEvent.h"

#include <Log.h>
#include <NewsTrackerStructs.h>
#include <UpdateStaging.h>

#include <QVersionNumber>

#define THREAD_ID "UpdateManager"

namespace UpdateManager {

namespace {
bool isNewerVersion(const QString &newsVersion, const QString &currentVersion) {
  const QString trimmedNewsVersion = newsVersion.trimmed();
  if (trimmedNewsVersion.isEmpty()) {
    return true;
  }

  qsizetype newsSuffixIndex = 0;
  const QVersionNumber newsNumber =
      QVersionNumber::fromString(trimmedNewsVersion, &newsSuffixIndex);
  qsizetype currentSuffixIndex = 0;
  const QVersionNumber currentNumber =
      QVersionNumber::fromString(currentVersion.trimmed(), &currentSuffixIndex);

  if (newsNumber.isNull() || currentNumber.isNull()) {
    return true;
  }

  return QVersionNumber::compare(newsNumber, currentNumber) > 0;
}
} // namespace

UpdateManager::UpdateManager()
    : _eventManager(EventManager::EventManager::getInstance()) {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&UpdateManager::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::NewsTrackerNews, callbackFunc));

  LOG_PERF(0, "Exit");
}

UpdateManager::~UpdateManager() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&UpdateManager::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::NewsTrackerNews, callbackFunc));

  LOG_PERF(0, "Exit");
}

void UpdateManager::start() {
  LOG_PERF(0, "Enter");

  _fsm.start();

  const EventManager::Event startedEvent(
      EventManager::EventType::ComponentStarted);
  _eventManager.send(startedEvent);

  LOG_PERF(0, "Exit");
}

void UpdateManager::stop() {
  LOG_PERF(0, "Enter");

  _fsm.stop();

  EventManager::Event stoppedEvent(EventManager::EventType::ComponentStopped);
  _eventManager.send(stoppedEvent);

  LOG_PERF(0, "Exit");
}

void UpdateManager::eventCallback(const EventManager::Event &event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();

  LOG_DEBUG("Received event {}", _eventManager.eventName(eventType));

  switch (eventType) {
  case EventManager::EventType::NewsTrackerNews: {
    const auto news = event.data<NewsTracker::Structs::NewsItem>();
    if (news.isUpdate() && !news.downloadUrl().trimmed().isEmpty()) {
#ifdef Q_OS_MACOS
      const QString blockedReason =
          Util::UpdateStaging::macosSelfUpdateBlockedReason(
              Util::UpdateStaging::macosBundlePath());
      if (!blockedReason.isEmpty()) {
        // The news toast still offers the download link for a manual update.
        LOG_INFO("Not staging update '{}' automatically: {}.",
                 news.id.toStdString(), blockedReason.toStdString());
        break;
      }
#endif
      const QString currentVersion =
          ConfigManager::ConfigManager::getInstance().getAppVersion();

      if (isNewerVersion(news.version, currentVersion)) {
        UpdateManagerEvent receivedEvent(EventType::UpdateNewsReceived);
        receivedEvent.addParam(QVariant::fromValue(news));
        _fsm.send(receivedEvent);
      } else {
        LOG_INFO("Ignoring update news '{}' because version '{}' is not newer.",
                 news.id.toStdString(), news.version.toStdString());
      }
    }
    break;
  }
  default: {
    LOG_WARN("Unknown event type.")
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace UpdateManager
