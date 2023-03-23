#pragma once

namespace ConfigManager {

enum class ConfigKey {
  Unknown = 0,
  RecentlyOpenedRepos,

  // Main window
  TitleBarHeight,
  TitleBarFontSize,
  TitleBarMenuMinimumSize,

  // Home container
  HomeContainerRecentTableNameFontSize,
  HomeContainerRecentTableUrlFontSize,
  HomeContainerRecentTableNameColor,
  HomeContainerRecentTableSelectedNameColor,
  HomeContainerRecentTableUrlColor,

  last
};

}
