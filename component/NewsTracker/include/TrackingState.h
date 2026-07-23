#pragma once

#include <NewsTrackerState.h>
#include <NewsTrackerStructs.h>

#include <ConfigManager.h>
#include <EventManager.h>
#include <HttpClient.h>

#include <QJsonObject>
#include <QSet>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

namespace NewsTracker {

class TrackingState final : public NewsTrackerState {
  Q_OBJECT

public:
  explicit TrackingState(NewsTrackerFsm &fsm);
  ~TrackingState() override;

  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  static bool parseNewsItem(const QJsonObject &object,
                            Structs::NewsItem &newsItem);
  static Structs::NewsType newsTypeFromString(const QString &type);
  static QDateTime parseDateTime(const QString &value);
  void pollingLoop();
  void pollOnce();
  void loadSeenNewsIds();
  void persistSeenNewsIds() const;
  void sendNews(const Structs::NewsItem &newsItem);

private:
  EventManager::EventManager &_eventManager;
  ConfigManager::ConfigManager &_configManager;
  Util::HttpClient _httpClient;
  std::thread _worker;
  std::atomic_bool _stopRequested;
  std::condition_variable _wakeCondition;
  std::mutex _wakeMutex;
  QSet<QString> _seenNewsIds;
};

} // namespace NewsTracker
