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
  GitRepoStashSplitterPos,
  GitRepoBranchRecyclerViewItemsSplitterPos,
  GitRepoHistoryViewColumnWidths,
  GitRepoHistoryViewHiddenColumns,
  GitRepoSpinnerOverlayTimerTimeout,
  GitRepoDirWatcherHoldbackTimeout,
  GitRepoMaxDiffFileSize,
  GitRepoMonospaceFontName,
  GitRepoMonospaceFontSize,
  GitRepoGitFlowMainBranchName,
  GitRepoGitFlowDevelopmentBranchName,
  GitRepoGitLabFlowMainBranchName,
  GitRepoGitLabFlowEnvironmentBranches,
  GitRepoGitLabFlowReleaseBranchSuffix,
  GitRepoGitHubFlowMainBranchName,
  GitRepoDiffViewType,
  GitRepoCommitSignOff,
  GitRepoCommitSign,

  // SvnRepo container, kept apart from GitRepo since its layout differs
  SvnRepoSplitterPos,
  SvnRepoStashSplitterPos,
  SvnRepoHistoryViewColumnWidths,
  SvnRepoHistoryViewHiddenColumns,

  // HgRepo container, kept apart from GitRepo and SvnRepo
  HgRepoSplitterPos,
  HgRepoStashSplitterPos,
  HgRepoHistoryViewColumnWidths,
  HgRepoHistoryViewHiddenColumns,

  // GitRepo library
  GitRepoGitCredentialsExpirationTime,
  GitRepoServerConnectTimeout,
  GitRepoServerTimeout,
  GitRepoSigningEnabled,
  GitRepoSigningFormat,
  GitRepoSigningKey,
  GitRepoSigningProgram,
  GitRepoCredentialsMasterKey,
  GitRepoHistoryPageSize,

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

  // News tracker
  NewsTrackerUrl,
  NewsTrackerPollInterval,
  NewsTrackerSeenNewsIds,

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

  // Github issue tracker
  GitHubOAuthClientId,

  // Azure issue tracker
  AzureOAuthClientId,
  AzureOAuthClientSecret,
  AzureOAuthTenantId,
  AzureOAuthAuthorizeUrl,
  AzureOAuthTokenUrl,
  AzureOAuthRedirectUrl,
  AzureOAuthScope,

  // Bitbucket issue tracker
  BitbucketOAuthClientId,
  BitbucketOAuthClientSecret,
  BitbucketOAuthAuthorizeUrl,
  BitbucketOAuthTokenUrl,
  BitbucketOAuthRedirectUrl,
  BitbucketOAuthScope,

  IssueTrackerToolbarPosition,

  // Testing
  TestColor,
  TestDateTime,

  last
};
}
