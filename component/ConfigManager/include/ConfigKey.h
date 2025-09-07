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
  MainWindowBorderWidth,

  // GitRepo container
  GitRepoSplitterPos,
  GitRepoBranchRecyclerViewItemsSplitterPos,
  GitRepoSpinnerOverlayTimerTimeout,

  // GitRepo library
  GitRepoGitCredentialsExpirationTime,

  // Settings container
  SettingsSplitterPos,

  // Toasts
  ToastSpacing,
  ToastWidth,
  InfoToastDisplayTime,

  // Tooltips
  TooltipDisplayTime,

  // Testing
  TestColor,
  TestDateTime,

  last
};

}
