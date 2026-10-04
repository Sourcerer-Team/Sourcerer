#include "Fsm/TrackingState.h"
#include "Fsm/NewsTrackerEvent.h"

#include <Log.h>

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QVersionNumber>

#include <chrono>

#include <QStringList>
#include <algorithm>

#define THREAD_ID "TrackingState"

namespace NewsTracker {

namespace Fsm {

namespace {
constexpr const char *NEWS_ARRAY_KEY = "news";

static QDateTime parseDateTimeString(const QString &value) {
  if (value.trimmed().isEmpty()) {
    return QDateTime();
  }

  QDateTime dateTime = QDateTime::fromString(value, Qt::ISODateWithMs);
  if (!dateTime.isValid()) {
    dateTime = QDateTime::fromString(value, Qt::ISODate);
  }

  return dateTime;
}
} // namespace

TrackingState::TrackingState(NewsTrackerFsm &fsm)
    : NewsTrackerState{StateType::Tracking, fsm},
      _eventManager{EventManager::EventManager::getInstance()},
      _configManager{ConfigManager::ConfigManager::getInstance()} {
  LOG_PERF(0, "Enter");

  loadSeenNewsIds();

  LOG_PERF(0, "Exit");
}

TrackingState::~TrackingState() {
  LOG_PERF(0, "Enter");

  exit();

  LOG_PERF(0, "Exit");
}

void TrackingState::enter() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

bool TrackingState::process(::Fsm::FsmEvent event) {
  LOG_PERF(0, "Enter");

  const EventType type = static_cast<EventType>(event.type());

  switch (type) {
  case EventType::RetrieveNews: {
    pollOnce();
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

void TrackingState::exit() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

Structs::NewsType TrackingState::newsTypeFromString(const QString &type) {
  if (type.compare(QStringLiteral("update"), Qt::CaseInsensitive) == 0) {
    return Structs::NewsType::Update;
  }

  if (type.compare(QStringLiteral("general"), Qt::CaseInsensitive) == 0) {
    return Structs::NewsType::General;
  }

  return Structs::NewsType::Unknown;
}

QDateTime TrackingState::parseDateTime(const QString &value) {
  return parseDateTimeString(value);
}

bool TrackingState::isNewerVersion(const QString &candidate,
                                   const QString &reference) {
  const QString trimmedCandidate = candidate.trimmed();
  const QString trimmedReference = reference.trimmed();

  if (trimmedCandidate.isEmpty()) {
    return false;
  }

  qsizetype candidateSuffixIndex = 0;
  const QVersionNumber candidateVersion =
      QVersionNumber::fromString(trimmedCandidate, &candidateSuffixIndex);
  qsizetype referenceSuffixIndex = 0;
  const QVersionNumber referenceVersion =
      QVersionNumber::fromString(trimmedReference, &referenceSuffixIndex);

  if (candidateVersion.isNull() || referenceVersion.isNull()) {
    return trimmedCandidate.compare(trimmedReference, Qt::CaseInsensitive) > 0;
  }

  return QVersionNumber::compare(candidateVersion, referenceVersion) > 0;
}

bool TrackingState::parseNewsItem(const QJsonObject &object,
                                  Structs::NewsItem &newsItem) {
  const QString id = object.value(QStringLiteral("id")).toString().trimmed();
  if (id.isEmpty()) {
    return false;
  }

  newsItem.id = id;
  newsItem.type =
      newsTypeFromString(object.value(QStringLiteral("type")).toString());
  newsItem.publishedAt =
      parseDateTime(object.value(QStringLiteral("published_at")).toString());
  newsItem.text = object.value(QStringLiteral("text")).toString().trimmed();
  newsItem.createdAt =
      parseDateTime(object.value(QStringLiteral("created_at")).toString());
  newsItem.updatedAt =
      parseDateTime(object.value(QStringLiteral("updated_at")).toString());
  newsItem.version =
      object.value(QStringLiteral("version")).toString().trimmed();
  newsItem.downloadUrlLinux =
      object.value(QStringLiteral("download_url_linux")).toString().trimmed();
  newsItem.downloadUrlWindows =
      object.value(QStringLiteral("download_url_windows")).toString().trimmed();
  newsItem.downloadUrlMacos =
      object.value(QStringLiteral("download_url_macos")).toString().trimmed();

  return newsItem.type != Structs::NewsType::Unknown;
}

void TrackingState::pollOnce() {
  LOG_PERF(0, "Enter");

  const QString newsUrl =
      _configManager.getString(ConfigManager::ConfigKey::NewsTrackerUrl);
  if (newsUrl.trimmed().isEmpty()) {
    LOG_WARN("News tracker URL is empty.");
    LOG_PERF(0, "Exit");
    return;
  }

  const Util::HttpReply reply =
      _httpClient.sendRequest(newsUrl, Util::HttpMethod::Get, {}, QByteArray());
  if (reply.error() != QNetworkReply::NoError) {
    LOG_WARN("Failed to poll news feed: {}", (int)reply.error());
    LOG_PERF(0, "Exit");
    return;
  }

  const QJsonDocument document = QJsonDocument::fromJson(reply.response());
  if (!document.isObject()) {
    LOG_WARN("News feed response is not a JSON object.");
    LOG_PERF(0, "Exit");
    return;
  }

  const QJsonObject rootObject = document.object();
  const QJsonValue newsValue =
      rootObject.value(QString::fromLatin1(NEWS_ARRAY_KEY));
  if (!newsValue.isArray()) {
    LOG_WARN("News feed response does not contain a 'news' array.");
    LOG_PERF(0, "Exit");
    return;
  }

  const QJsonArray newsArray = newsValue.toArray();
  Structs::NewsItem newestUpdateNewsItem;
  bool hasNewestUpdateNewsItem = false;
  QStringList newlySeenIds;

  for (const QJsonValue &value : newsArray) {
    if (!value.isObject()) {
      continue;
    }

    Structs::NewsItem newsItem;
    if (!parseNewsItem(value.toObject(), newsItem)) {
      continue;
    }

    if (_seenNewsIds.contains(newsItem.id)) {
      continue;
    }

    newlySeenIds.append(newsItem.id);

    if (!newsItem.isUpdate()) {
      sendNews(newsItem);
      continue;
    }

    if (!hasNewestUpdateNewsItem ||
        isNewerVersion(newsItem.version, newestUpdateNewsItem.version)) {
      newestUpdateNewsItem = newsItem;
      hasNewestUpdateNewsItem = true;
    }
  }

  for (const QString &id : newlySeenIds) {
    _seenNewsIds.insert(id);
  }

  if (!newlySeenIds.isEmpty()) {
    persistSeenNewsIds();
  }

  if (hasNewestUpdateNewsItem) {
    sendNews(newestUpdateNewsItem);
  }

  LOG_PERF(0, "Exit");
}

void TrackingState::loadSeenNewsIds() {
  LOG_PERF(0, "Enter");

  _seenNewsIds.clear();

  const QString storedIds = _configManager.getString(
      ConfigManager::ConfigKey::NewsTrackerSeenNewsIds);
  for (const QString &id : storedIds.split(',', Qt::SkipEmptyParts)) {
    const QString trimmedId = id.trimmed();
    if (!trimmedId.isEmpty()) {
      _seenNewsIds.insert(trimmedId);
    }
  }

  LOG_PERF(0, "Exit");
}

void TrackingState::persistSeenNewsIds() const {
  LOG_PERF(0, "Enter");

  QStringList storedIds;
  for (const QString &id : _seenNewsIds) {
    storedIds.append(id);
  }
  storedIds.sort();

  _configManager.setString(ConfigManager::ConfigKey::NewsTrackerSeenNewsIds,
                           storedIds.join(','));
  _configManager.save();

  LOG_PERF(0, "Exit");
}

void TrackingState::sendNews(const Structs::NewsItem &newsItem) {
  LOG_PERF(0, "Enter");

  EventManager::Event event(EventManager::EventType::NewsTrackerNews);
  event.addData(newsItem);
  _eventManager.send(event);

  LOG_PERF(0, "Exit");
}

} // namespace Fsm
} // namespace NewsTracker
