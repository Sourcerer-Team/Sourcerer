#pragma once

#include "UpdateManagerState.h"

#include <EventManager.h>
#include <NewsTrackerStructs.h>

#include <HttpClient.h>

namespace UpdateManager {

class UpdatingState final : public UpdateManagerState {
  Q_OBJECT

public:
  explicit UpdatingState(UpdateManagerFsm &fsm);
  ~UpdatingState() override;

  void enter() override;
  bool process(Fsm::FsmEvent event) override;
  void exit() override;

private:
  bool workerLoop(const NewsTracker::Structs::NewsItem &newsItem);
  bool downloadUpdate(const NewsTracker::Structs::NewsItem &newsItem,
                      QString &downloadedFilePath);
  static QString buildStagedUpdateFilePath(const QString &downloadUrl,
                                           const QString &installDir);
  static QString
  buildDownloadFilePath(const NewsTracker::Structs::NewsItem &newsItem,
                        const QString &downloadUrl);
  static QString sanitizeFileName(QString value);
  static bool makeExecutableIfNeeded(const QString &downloadedFilePath);

private:
  EventManager::EventManager &_eventManager;
  Util::HttpClient _httpClient;
};

} // namespace UpdateManager
