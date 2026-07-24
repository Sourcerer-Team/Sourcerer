#pragma once

#include <Fsm/NewsTrackerState.h>
#include <NewsTrackerStructs.h>

#include <ConfigManager.h>
#include <EventManager.h>
#include <HttpClient.h>

#include <QJsonObject>
#include <QSet>

namespace NewsTracker {

namespace Fsm {

class TrackingState final : public NewsTrackerState {
  Q_OBJECT

public:
  explicit TrackingState(NewsTrackerFsm &fsm);
  ~TrackingState() override;

  void enter() override;
  bool process(::Fsm::FsmEvent event) override;
  void exit() override;

private:
  static bool parseNewsItem(const QJsonObject &object,
                            Structs::NewsItem &newsItem);
  static Structs::NewsType newsTypeFromString(const QString &type);
  static QDateTime parseDateTime(const QString &value);
  void pollOnce();
  void loadSeenNewsIds();
  void persistSeenNewsIds() const;
  void sendNews(const Structs::NewsItem &newsItem);

private:
  EventManager::EventManager &_eventManager;
  ConfigManager::ConfigManager &_configManager;
  Util::HttpClient _httpClient;
  QSet<QString> _seenNewsIds;
};

} // namespace Fsm
} // namespace NewsTracker
