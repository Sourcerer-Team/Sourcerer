#pragma once

#include "widgets/ToastWidget.h"

#include <NewsTrackerStructs.h>

namespace GuiManager::Widgets {

class NewsToast final : public ToastWidget {
  Q_OBJECT
public:
  explicit NewsToast(const NewsTracker::Structs::NewsItem &news,
                     QWidget *parent = nullptr);
  ~NewsToast() override;

private:
  void applyNewsColors(const NewsTracker::Structs::NewsItem &news);
};

} // namespace GuiManager::Widgets
