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
  MainWindowMinimumHeight,
  MainWindowMinimumWidth,

  // GitRepo container
  GitRepoSplitterPos,
  GitRepoBranchRecyclerViewItemsSplitterPos,
  GitRepoSpinnerOverlayTimerTimeout,
  GitRepoDirWatcherHoldbackTimeout,
  GitRepoMaxDiffFileSize,
  GitRepoMonospaceFontName,
  GitRepoMonospaceFontSize,

  // GitRepo library
  GitRepoGitCredentialsExpirationTime,
  GitRepoServerConnectTimeout,
  GitRepoServerTimeout,

  // HttpClient
  HttpClientCaCertFile,
  HttpClientTimeout,

  // Cloud authentication
  CloudJwtExpirationMinutes,
  CloudSigninUrl,
  CloudSignupUrl,

  // Settings container
  SettingsSplitterPos,

  // Toasts
  ToastAnimationDuration,
  ToastHeight,
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
