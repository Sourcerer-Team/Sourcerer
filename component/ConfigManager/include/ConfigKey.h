#pragma once

namespace ConfigManager {

enum class ConfigKey {
  Unknown = 0,
  RecentlyOpenedRepos,

  // Main window
  TitleBarHeight,
  TitleBarMenuMinimumSize,

  // Home container

  last
};

}
