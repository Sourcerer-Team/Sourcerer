#pragma once

#include "NewsTrackerFsm.h"

#include <Component.h>

namespace NewsTracker {

class NewsTracker final : public ComponentManager::Component {
public:
  NewsTracker();
  ~NewsTracker() override;

  void start() override;
  void stop() override;

private:
  NewsTrackerFsm _fsm;
};

} // namespace NewsTracker
