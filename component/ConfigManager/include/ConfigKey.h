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
  MainWindowGeometry,
  MainWindowMinimumHeight,
  MainWindowMinimumWidth,

  // GitRepo container
  GitRepoSplitterPos,
  GitRepoBranchRecyclerViewItemsSplitterPos,
  GitRepoHistoryViewColumnWidths,
  GitRepoHistoryViewHiddenColumns,
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
  GitRepoSigningEnabled,
  GitRepoSigningFormat,
  GitRepoSigningKey,
  GitRepoSigningProgram,

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
  TooltipHistoryTimeout,

  // Testing
  TestColor,
  TestDateTime,

  last
};

}
