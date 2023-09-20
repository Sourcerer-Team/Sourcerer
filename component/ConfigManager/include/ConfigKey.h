#pragma once

namespace ConfigManager {

enum class ConfigKey {
  Unknown = 0,
  RecentlyOpenedRepos,

  // Main window
  TitleBarHeight,
  TitleBarMaxTabWidth,
  TitleBarMenuMinimumSize,
  TitleBarMinimizeButtonColor,
  TitleBarMaximizeButtonColor,
  TitleBarCloseButtonColor,

  // GitRepo container
  GitRepoSplitterPos,

  // GitRepo library
  GitRepoGitCredentialsExpirationTime,

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
