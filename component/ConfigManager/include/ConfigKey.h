#pragma once

namespace ConfigManager {

enum class ConfigKey {
  Unknown = 0,
  RecentlyOpenedRepos,
  UiTheme,

  // Main window
  TitleBarHeight,
  TitleBarMaxTabWidth,
  TitleBarMenuMinimumSize,
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
  GitRepoDiffViewType,

  // GitRepo library
  GitRepoGitCredentialsExpirationTime,
  GitRepoServerConnectTimeout,
  GitRepoServerTimeout,

  // HttpClient
  HttpClientCaCertFile,
  HttpClientTimeout,

  // Cloud authentication
  CloudJwtExpirationMinutes,
  CloudJwtPublicKeyPath,
  CloudSigninUrl,
  CloudSignupUrl,
  CloudSignoutUrl,
  CloudUploadBugReportUrl,

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
