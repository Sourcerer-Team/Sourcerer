#pragma once

namespace ConfigManager {

enum class ConfigKey {
  Unknown = 0,
  RecentlyOpenedRepos,

  // Main window
  TitleBarHeight,
  TitleBarMaxTabWidth,
  TitleBarMenuMinimumSize,

  // GitRepo container
  GitRepoSplitterPos,

  // Settings container
  SettingsSplitterPos,

  // Toasts
  ToastSpacing,
  ToastWidth,
  InfoToastDisplayTime,

  // Testing
  TestColor,
  TestDateTime,

  last
};

}
