#pragma once

#include <QHash>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <git2/repository.h>
#include <git2/status.h>

// Pseudo branch name that stands for the history of all branches.
#ifndef ALL_BRANCHES
#define ALL_BRANCHES "*"
#endif

// Types used by the GitRepo functions, see GitRepo.h.
namespace GitRepo {

namespace Lfs {
struct StorageStatus {
  QString repositoryPath;
  qint64 objectCount = 0;
  qint64 totalBytes = 0;
};
} // namespace Lfs

enum class GitError : int {
  Unknown = 0,
  MarkConflictedFilesFailedToGetRepositoryIndex,
  MarkConflictedFilesFailedToCreateAncestorBlobFromWorkdir,
  MarkConflictedFilesFailedToCreateOursBlobFromWorkdir,
  MarkConflictedFilesFailedToCreateTheirsBlobFromWorkdir,
  MarkConflictedFilesFailedToAddConflictToIndex,
  MarkConflictedFilesFailedToWriteIndex,
  CollectConflictedFilePathsFromResultIndexFailedToGetRepositoryHead,
  CollectConflictedFilePathsFailedToPeelHeadToCommit,
  CollectConflictedFilePathsFromResultIndexFailedToGetCommitTree,
  CollectConflictedFilePathsFromResultIndexFailedToDiffTreeAgainstIndex,
  CollectConflictedFilePathsFromResultIndexFailedToCreateConflictIterator,
  CreateInitialCommitFailedToCreateSignature,
  CreateInitialCommitFailedToGetRepositoryIndex,
  CreateInitialCommitFailedToWriteIndexTree,
  CreateInitialCommitFailedToLookupTree,
  CreateInitialCommitFailedToCreateCommit,
  CreateInitialCommitFailedToCreateBranchIterator,
  CreateInitialCommitFailedToReadNextBranch,
  CreateInitialCommitFailedToReadBranchName,
  CreateInitialCommitFailedToRenameBranch,
  CreateInitialCommitWithoutStagingFailedToCreateSignature,
  CreateInitialCommitWithoutStagingFailedToCreateTreeBuilder,
  CreateInitialCommitWithoutStagingFailedToWriteTreeBuilder,
  CreateInitialCommitWithoutStagingFailedToLookupTree,
  CreateInitialCommitWithoutStagingFailedToCreateCommit,
  GetLastCommitFailedToResolveReferenceNameToId,
  GetLastCommitFailedToLookupCommit,
  ApplyStashFailedToApplyStash,
  ApplyStashFailedToDiffIndexAgainstWorkdir,
  HasConflictsFailedToGetRepositoryIndex,
  CheckoutBranchFailedToLookupBranch,
  CheckoutBranchFailedToPeelLocalBranchToCommit,
  CheckoutBranchFailedToGetCommitTree,
  CheckoutBranchFailedToCheckoutLocalBranchTree,
  CheckoutBranchFailedToGetReferenceName,
  CheckoutBranchFailedToLookupReference,
  CheckoutBranchFailedToPeelRemoteBranchToCommit,
  CheckoutBranchFailedToLookupDestinationBranch,
  CheckoutBranchFailedToCreateLocalBranchFromRemote,
  CheckoutBranchFailedToSetBranchUpstream,
  CheckoutBranchFailedToGetSecondCommitTree,
  CheckoutBranchFailedToCheckoutTagTree,
  CheckoutBranchFailedToSetHeadToTagReference,
  CheckoutBranchFailedToResolveRevision,
  CheckoutBranchFailedToCheckoutTree,
  CheckoutBranchFailedToSetHeadDetachedAtTag,
  OpenRepositoryFailedToOpenRepository,
  ListCheckoutBranchesFailedToCreateBranchIterator,
  ListCheckoutBranchesFailedToReadNextBranch,
  ListCheckoutBranchesFailedToReadBranchName,
  ListCheckoutBranchesFailedToCheckIfBranchIsCheckedOut,
  ListCheckoutBranchesFailedToGetRepositoryHead,
  ListCheckoutBranchesFailedToListTags,
  ListCheckoutBranchesFailedToResolveRevision,
  ListBranchesFailedToGetRepositoryHead,
  ListBranchesFailedToCreateBranchIterator,
  ListBranchesFailedToReadNextBranch,
  ListBranchesFailedToReadBranchName,
  ListRemoteBranchesWithStatusFailedToCalculateAheadBehind,
  ListStashesFailedToListStashes,
  GetStashStatusFailedToResolveRevision,
  GetStashStatusFailedToPeelObjectToCommit,
  GetStashStatusFailedToGetCommitTree,
  GetStashStatusFailedToGetBaseParentCommit,
  GetStashStatusFailedToGetSecondCommitTree,
  GetStashStatusFailedToGetThirdCommitTree,
  GetStashStatusFailedToGetFourthCommitTree,
  GetStashStatusFailedToCreateStashDiff,
  GetStashStatusFailedToWalkUntrackedStashTree,
  ListTagsFailedToListTags,
  ListSubmodulesFailedToIterateSubmodules,
  CreateBranchFromCommitFailedToParseObjectId,
  CreateBranchFromCommitFailedToLookupCommit,
  CreateBranchFromCommitFailedToCreateBranch,
  DeleteStashFailedToDropStash,
  DeleteAllStashesFailedToReadObjectId,
  DeleteAllStashesFailedToDropStash,
  InitRepositoryFailedToInitializeRepositoryOptions,
  InitRepositoryFailedToInitializeRepository,
  CredentialsAcquireCallbackFailedToCreatePlaintextCredentials,
  CheckoutCommitFailedToParseObjectId,
  CheckoutCommitFailedToLookupCommit,
  CheckoutCommitFailedToGetCommitTree,
  CheckoutCommitFailedToCheckoutCommitTree,
  CheckoutCommitFailedToDetachHead,
  CloneRepositoryFailedToCloneRepository,
  CloneRepositoryGitSshUrlRequiresCommercialPlan,
  GetBranchHistoryFailedToCreateRevwalk,
  GetBranchHistoryFailedToConfigureRevwalkSorting,
  GetBranchHistoryFailedToCreateBranchIterator,
  GetBranchHistoryFailedToReadBranchName,
  GetBranchHistoryFailedToAddReferenceToRevwalk,
  GetBranchHistoryFailedToAddLocalBranchToRevwalk,
  GetBranchHistoryFailedToAddRemoteBranchToRevwalk,
  GetBranchHistoryFailedToAddTagToRevwalk,
  GetBranchHistoryFailedToLookupCommit,
  GetCommitBranchesFailedToCreateBranchIterator,
  GetCommitBranchesFailedToReadBranchName,
  GetCommitBranchesFailedToResolveReference,
  GetCommitBranchesFailedToPeelReference,
  GetCommitBranchesFailedToParseObjectId,
  GetCommitTagsFailedToListMatchingTags,
  GetCommitHeadsFailedToCreateBranchIterator,
  GetCommitHeadsFailedToPeelReference,
  GetCommitHeadsFailedToReadBranchName,
  GetCommitHeadsFailedToParseObjectId,
  GetCommitIsHeadFailedToCreateBranchIterator,
  GetCommitIsHeadFailedToPeelReference,
  GetCommitIsHeadFailedToReadBranchName,
  GetCommitIsHeadFailedToParseObjectId,
  GetStatusFailedToCreateStatusList,
  GetCommitStatusFailedToLookupCommit,
  GetCommitStatusFailedToGetCommitTree,
  GetCommitStatusFailedToGetParentCommit,
  GetCommitStatusFailedToGetSecondCommitTree,
  GetCommitStatusFailedToCreateCommitDiff,
  GetCommitStatusFailedToDetectRenames,
  StageFilesFailedToGetRepositoryIndex,
  StageFilesFailedToRemovePathFromIndex,
  StageFilesFailedToAddSubmoduleToIndex,
  StageFilesFailedToAddPathToIndex,
  StageFilesFailedToAddMissingPathToIndex,
  StageFilesFailedToWriteIndex,
  UnstageFilesFailedToResolveRevision,
  UnstageFilesFailedToResetPathsToHead,
  CommitStagedFailedToGetRepositoryIndex,
  CommitStagedFailedToAbortRebaseSession,
  CommitStagedFailedToOpenRebaseSession,
  CommitStagedFailedToWriteIndexTree,
  CommitStagedFailedToLookupTree,
  CommitStagedFailedToCreateSignature,
  CommitStagedFailedToGetRepositoryHead,
  CommitStagedFailedToGetReferenceTarget,
  CommitStagedFailedToResolveMergeHeadReference,
  CommitStagedFailedToGetSecondReferenceTarget,
  CommitStagedFailedToLoadSigningConfiguration,
  CommitStagedFailedToSignCommit,
  CommitStagedFailedToCreateCommitFromStagedChanges,
  CommitStagedFailedToCreateCommit,
  CommitStagedFailedToCleanupRepositoryState,
  DiffFilesFailedToResolveRevision,
  DiffFilesFailedToGetHeadObjectId,
  DiffFilesFailedToGetCommitTree,
  DiffFilesFailedToDiffTreeAgainstWorkdir,
  DiffFilesFailedToCreatePatchFromDiff,
  DiffFilesFailedToReadPatchHunk,
  DiffFilesFailedToReadPatchLine,
  DiffFilesFailedToGetRepositoryHead,
  DiffFilesFailedToPeelReference,
  DiffFilesFailedToReadHeadCommit,
  DiffFilesFailedToLookupTreeEntryByPath,
  DiffFilesFailedToReadTreeEntryId,
  DiffFilesFailedToLookupBlobByPrefix,
  DiscardFilesFailedToResolveRevision,
  DiscardFilesFailedToCheckoutTree,
  DiscardFilesFailedToGetRepositoryIndex,
  DiscardFilesFailedToResolveHeadRevision,
  DiscardFilesFailedToGetHeadTreeObjectId,
  DiscardFilesFailedToLookupTreeEntryByPath,
  DiscardFilesFailedToReadTreeEntryId,
  DiscardFilesFailedToRemoveIndexEntry,
  DiscardFilesFailedToAddIndexEntry,
  DiscardFilesFailedToRemoveDeletedIndexEntry,
  DiscardFilesFailedToWriteIndex,
  PullFailedToGetRepositoryHead,
  PullFailedToReadBranchName,
  PullNoUpstreamConfigured,
  PullFailedToLookupRemote,
  PullFailedToFetchRemote,
  PullFailedToFetchRemoteWithCredentials,
  PullFailedToGetRepositoryHeadAfterFetch,
  PullFailedToLookupReference,
  PullFailedToCreateAnnotatedCommitFromReference,
  PullFailedToGetDefaultSignature,
  PullFailedToStartRebase,
  PullFailedToCommitRebaseStep,
  PullFailedToFinishRebase,
  PullFailedToAbortRebase,
  PullFailedToCreateAnnotatedCommit,
  PullFailedToLookupCommit,
  PullFailedToPeelFetchedTargetCommit,
  PullFailedToSetReferenceTarget,
  PullFailedToCreateMergeAnnotatedCommit,
  PullGitSshUrlRequiresCommercialPlan,
  FetchRemoteInternalFailedToLookupRemote,
  FetchRemoteGitSshUrlRequiresCommercialPlan,
  FetchRemoteInternalFailedToFetchRemote,
  FetchRemoteInternalFailedToFetchRemoteWithCredentials,
  PushFailedToGetRepositoryHead,
  PushGitSshUrlRequiresCommercialPlan,
  PushFailedToReadBranchName,
  PushFailedToLookupRemote,
  PushFailedToPushRemote,
  PushTagFailedToLookupReference,
  PushTagGitSshUrlRequiresCommercialPlan,
  PushTagFailedToLookupRemote,
  PushTagFailedToPushRemote,
  PushAllTagsFailedToLookupRemote,
  PushAllTagsGitSshUrlRequiresCommercialPlan,
  PushAllTagsFailedToPushRemote,
  StashFailedToGetDefaultSignature,
  StashFailedToCreateStash,
  SetUpstreamFailedToLookupBranch,
  SetUpstreamFailedToSetBranchUpstream,
  GetUpstreamFailedToLookupBranch,
  GetUpstreamFailedToGetBranchUpstream,
  GetUpstreamFailedToOpenGitConfig,
  ResetToCommitFailedToResolveRevision,
  ResetToCommitFailedToResetRepository,
  RevertCommitFailedToLookupTargetCommit,
  RevertCommitFailedToGetRepositoryHead,
  RevertCommitFailedToGetReferenceTarget,
  RevertCommitFailedToPrepareRevertCommit,
  RevertCommitFailedToCheckoutIndex,
  RevertCommitFailedToWriteIndexTreeToRepository,
  RevertCommitFailedToLookupTree,
  RevertCommitFailedToCreateSignature,
  RevertCommitFailedToCreateCommit,
  ApplyPatchFailedToParsePatchBuffer,
  ApplyPatchFailedToApplyPatch,
  CherryPickCommitFailedToLookupTargetCommit,
  CherryPickCommitFailedToGetRepositoryHead,
  CherryPickCommitFailedToGetReferenceTarget,
  CherryPickCommitFailedToPrepareCherryPickCommit,
  CherryPickCommitFailedToCheckoutIndex,
  CherryPickCommitFailedToWriteIndexTreeToRepository,
  CherryPickCommitFailedToLookupTree,
  CherryPickCommitFailedToCreateSignature,
  CherryPickCommitFailedToCreateCommit,
  RenameBranchFailedToLookupBranch,
  RenameBranchFailedToRenameBranch,
  DeleteBranchFailedToLookupBranch,
  DeleteBranchFailedToDeleteBranch,
  DeleteRemoteBranchFailedToLookupRemote,
  DeleteRemoteBranchGitSshUrlRequiresCommercialPlan,
  DeleteRemoteBranchFailedToPushRemote,
  CreateTagFromCommitFailedToParseObjectId,
  CreateTagFromCommitFailedToLookupCommit,
  CreateTagFromCommitFailedToCreateSignature,
  CreateTagFromCommitFailedToPeelCommitForTag,
  CreatePatchFromCommitFailedToParseObjectId,
  CreatePatchFromCommitFailedToLookupCommit,
  CreatePatchFromCommitFailedToGetCommitTree,
  CreatePatchFromCommitFailedToGetParentCommit,
  CreatePatchFromCommitFailedToGetSecondCommitTree,
  CreatePatchFromCommitFailedToCreateCommitDiff,
  CreatePatchFromCommitFailedToCreatePatchFromDiff,
  CreatePatchFromCommitFailedToRenderPatchToBuffer,
  RenameTagFailedToLookupReference,
  RenameTagFailedToPeelReference,
  RenameTagFailedToReadTagTarget,
  RenameTagFailedToCreateAnnotatedTag,
  RenameTagFailedToCreateLightweightTag,
  RenameTagFailedToDeleteTag,
  DeleteTagFailedToDeleteTag,
  DeleteRemoteTagFailedToLookupRemote,
  DeleteRemoteTagGitSshUrlRequiresCommercialPlan,
  DeleteRemoteTagFailedToPushRemote,
  GetSubmodulePathFailedToLookupSubmodule,
  DeleteSubmoduleFailedToGetRepositoryIndex,
  DeleteSubmoduleFailedToRemoveIndexEntry,
  DeleteSubmoduleFailedToWriteIndex,
  CheckoutSubmoduleBranchFailedToLookupRemote,
  CheckoutSubmoduleBranchFailedToFetchRemote,
  CheckoutSubmoduleBranchFailedToLookupReference,
  CheckoutSubmoduleBranchFailedToPeelReferenceToCommit,
  CheckoutSubmoduleBranchFailedToCreateLocalBranch,
  CheckoutSubmoduleBranchFailedToReadCommitId,
  CheckoutSubmoduleBranchFailedToSetHeadToBranch,
  CheckoutSubmoduleBranchFailedToCheckoutHead,
  CheckoutLocalSubmoduleBranchFailedToLookupBranch,
  CheckoutLocalSubmoduleBranchFailedToSetHeadToBranch,
  CheckoutLocalSubmoduleBranchFailedToCheckoutHead,
  UpdateSubmoduleFailedToUpdateSubmodule,
  UpdateSubmoduleFailedToAddSubmoduleToIndex,
  AddSubmoduleFailedToSetupSubmodule,
  AddSubmoduleGitSshUrlRequiresCommercialPlan,
  AddSubmoduleFailedToCloneSubmodule,
  AddSubmoduleFailedToFinalizeSubmoduleAdd,
  DiffFilesFromCommitFailedToParseObjectId,
  DiffFilesFromCommitFailedToLookupCommit,
  DiffFilesFromCommitFailedToGetCommitTree,
  DiffFilesFromCommitFailedToGetParentCommit,
  DiffFilesFromCommitFailedToGetSecondCommitTree,
  DiffFilesFromCommitFailedToGetSecondParentCommit,
  DiffFilesFromCommitFailedToGetThirdCommitTree,
  DiffFilesFromCommitFailedToCreateCommitDiff,
  DiffFilesFromCommitFailedToCreatePatchFromDiff,
  DiffFilesFromCommitFailedToReadPatchHunk,
  DiffFilesFromCommitFailedToReadPatchLine,
  GetFileSizeFromCommitFailedToGetRepositoryHead,
  GetFileSizeFromCommitFailedToGetReferenceName,
  GetFileSizeFromCommitFailedToParseObjectId,
  GetFileSizeFromCommitFailedToLookupCommit,
  GetFileSizeFromCommitFailedToGetCommitTree,
  GetFileSizeFromCommitFailedToLookupTreeEntryByPath,
  GetFileSizeFromCommitFailedToLookupBlob,
  MergeFailedToLookupBranch,
  MergeFailedToLookupDestinationBranch,
  MergeFailedToPeelReference,
  MergeFailedToCheckoutTree,
  MergeFailedToGetReferenceName,
  MergeFailedToCreateAnnotatedCommitFromReference,
  MergeFailedToCreateSecondAnnotatedCommitFromReference,
  MergeFailedToAnalyzeMerge,
  MergeFailedToReadAnnotatedCommitId,
  MergeFailedToReadCommitId,
  MergeFailedToReadSecondCommitId,
  MergeFailedToCheckoutDestinationTree,
  MergeFailedToMerge,
  MergeFailedToGetRepositoryIndex,
  MergeFailedToWriteIndexTree,
  MergeFailedToLookupTree,
  MergeFailedToCreateSignature,
  MergeFailedToGetReferenceTarget,
  MergeFailedToGetSecondReferenceTarget,
  MergeFailedToCreateCommit,
  RebaseBranchesFailedToLookupBranch,
  RebaseBranchesFailedToLookupDestinationBranch,
  RebaseBranchesFailedToCreateAnnotatedCommitFromReference,
  RebaseBranchesFailedToCreateSecondAnnotatedCommitFromReference,
  RebaseBranchesFailedToReadAnnotatedCommitId,
  RebaseBranchesFailedToLookupAnnotatedCommit,
  RebaseBranchesFailedToCreateSignature,
  RebaseBranchesFailedToStartRebase,
  RebaseBranchesFailedToCommitRebaseStep,
  RebaseBranchesFailedToCommitRebaseStepWithConflicts,
  RebaseBranchesFailedToFinishRebase,
  RebaseBranchesFailedToAbortRebase,
  SquashBranchesInvalidArguments,
  SquashBranchesRepositoryBusy,
  SquashBranchesFailedToLookupBranch,
  SquashBranchesFailedToLookupDestinationBranch,
  SquashBranchesFailedToPeelReference,
  SquashBranchesFailedToCheckoutTree,
  SquashBranchesFailedToSetHead,
  SquashBranchesFailedToCreateAnnotatedCommit,
  SquashBranchesFailedToAnalyzeMerge,
  SquashBranchesFailedToMerge,
  SquashBranchesFailedToGetRepositoryIndex,
  SquashBranchesFailedWithConflicts,
  SquashBranchesFailedToWriteIndexTree,
  SquashBranchesFailedToLookupHeadCommit,
  SquashBranchesFailedToCleanupRepositoryState,
  BisectInvalidArguments,
  BisectRepositoryBusy,
  BisectNotInProgress,
  BisectFailedToResolveCommit,
  BisectFailedToReadState,
  BisectFailedToWriteState,
  BisectFailedToWalkHistory,
  BisectInvalidRange,
  BisectFailedToCheckout,
  BisectFailedToResetRepository,
  ListConflictsForAFileFailedToGetRepositoryIndex,
  ListConflictsForAFileFailedToCreateConflictIterator,
  AbortMergeFailedToCleanupRepositoryState,
  AbortMergeFailedToResolveRevision,
  AbortMergeFailedToResetRepository,
  ListRemotesFailedToListRemotes,
  SetDefaultRemoteFailedToLookupRemote,
  SetDefaultRemoteFailedToGetRepositoryConfig,
  SetDefaultRemoteFailedToWriteDefaultRemoteConfig,
  AddRemoteFailedToCreateRemote,
  AddRemoteFailedToSetPushUrl,
  RemoveRemoteFailedToDeleteRemote,
  UpdateRemoteFailedToRenameRemote,
  UpdateRemoteFailedToSetFetchUrl,
  UpdateRemoteFailedToSetPushUrl,
  AddTargetFailedToGetRepositoryConfig,
  AddTargetFailedToSetFetchUrlConfig,
  AddTargetFailedToSetPushUrlConfig,
  EditTargetFailedToGetRepositoryConfig,
  EditTargetFailedToSetFetchUrlConfig,
  EditTargetFailedToSetPushUrlConfig,
  DeleteTargetFailedToGetRepositoryConfig,
  DeleteTargetFailedToDeleteFetchUrlConfig,
  DeleteTargetFailedToDeletePushUrlConfig,
  SetDefaultTargetFailedToGetRepositoryConfig,
  SetDefaultTargetFailedToCreateConfigIterator,
  SetDefaultTargetFailedToSetDefaultFetchUrl,
  AmendFailedToResolveReferenceNameToId,
  AmendFailedToLookupCommit,
  AmendFailedToGetRepositoryIndex,
  AmendFailedToWriteIndexTree,
  AmendFailedToLookupTree,
  AmendFailedToCreateSignature,
  AmendFailedToGetParentCommit,
  AmendFailedToLoadSigningConfiguration,
  AmendFailedToSignCommit,
  AmendFailedToCreateSignedAmendCommit,
  AmendFailedToAmendCommit,
  ListWorktreesFailedToListWorktrees,
  ListWorktreesFailedToOpenWorktree,
  AddWorktreeBranchRequired,
  AddWorktreeFailedToResolveHead,
  AddWorktreeFailedToCreateBranch,
  AddWorktreeFailedToLookupBranch,
  AddWorktreeFailedToCreateDirectory,
  AddWorktreeFailedToAddWorktree,
  RemoveWorktreeFailedToLookupWorktree,
  RemoveWorktreeMainNotAllowed,
  RemoveWorktreeFailedToPrune,
  LockWorktreeFailedToLookupWorktree,
  LockWorktreeMainNotAllowed,
  LockWorktreeFailedToLock,
  UnlockWorktreeFailedToLookupWorktree,
  UnlockWorktreeMainNotAllowed,
  UnlockWorktreeFailedToUnlock,
  GitLabFlowBranchNameEmpty,
  GitLabFlowFailedToLookupBaseBranch,
  GitLabFlowPromoteNotDownstream,
  GitLabFlowFailedToLookupReleaseBranch,
  GitHubFlowBranchNameEmpty,
  GitHubFlowFailedToLookupMainBranch,
  GitHubFlowMainBranchNotAllowed,
  GitFlowNextFailedToReadConfig,
  GitFlowNextFailedToWriteConfig,
  GitFlowNextNotInitialized,
  GitFlowNextUnknownBranchType,
  GitFlowNextNotATopicBranch,
  GitFlowNextBranchNameEmpty,
  GitFlowNextFailedToLookupBranch,
  GitFlowNextUnsupportedStrategy,
  GitFlowNextTagExists,
  GitFlowNextInvalidWorkflow,
  SetWorkflowTypeFailedToGetRepositoryConfig,
  SetWorkflowTypeFailedToWriteConfig,
  MergeFailedWithConflicts,
  RemoveWorktreeHasChanges,
  RenameBranchInvalidName,
  AddTargetRemoteNotFound,
  CherryPickCommitFailedWithConflicts,
  CherryPickCommitIsEmpty,
  RevertCommitFailedWithConflicts,
  RevertCommitIsEmpty,
  MergeUnrelatedHistories,
  DeleteBranchNotFullyMerged,
  AddTargetAlreadyExists,
  EditTargetNotFound,
  DeleteTargetNotFound,
  SetUpstreamRemoteNotFound,
  DeleteSubmoduleFailedToUpdateConfig,
  AddSubmoduleAlreadyExists,
  PushTagExistsOnRemote,
  PullBranchNotCheckedOutCannotFastForward,
  PullRebaseFailedWithConflicts,
  PushNoUpstreamConfigured,
  CloneRepositoryFailedToCreateDirectory,
  GitFlowUnknownBranchType,
  GitFlowTagExists,
  DeleteSubmoduleInvalidName,
  AbortMergeNothingToAbort,
  AbortRebaseNothingToAbort,
  GetUpstreamNotConfigured,
  UpdateRemoteNotFound,
  UpdateRemoteInvalidArguments,
  EditTargetInvalidArguments,
  Last
};

struct GitErrorContext {
  GitError error = GitError::Unknown;
  int errorCode = -1;
  QString errorMessage;
};

enum class CloneProgressStageType : int {
  Unknown = 0,
  Fetch,
  Checkout,

  last
};

struct CloneProgress {
  QString uuid;
  size_t completedSteps = 0;
  size_t totalSteps = 0;
  int lastStepPercentage = -1;
  bool started = false;
  bool finished = false;
  bool error = false;
  bool cloning = true;
  CloneProgressStageType stage = CloneProgressStageType::Unknown;
};

struct GitCredentialsRequest {
  size_t id = -1;
  QString url;
  QString username;
  int allowedTypes;
  QString repoPath;
  bool needsPassphrase = false; // True for SSH key passphrase requests
};

struct GitCredentialsResponse {
  size_t id = -1;
  QString username;
  QString password;
  QString passphrase; // For SSH key passphrases
  bool needPassphrase = false;
  bool rememberCredentials = false;
};

struct GitCredentialsErrorResponse {
  size_t id = -1;
};

// A server certificate or SSH host key trusted on first use, see
// getPinnedCertificates().
enum class PinnedCertificateType { Tls, Ssh };

struct PinnedCertificate {
  PinnedCertificateType type = PinnedCertificateType::Tls;
  QString host;
  // SHA-256, "AB:CD:..." for TLS, "SHA256:<base64>" for SSH like OpenSSH.
  QString fingerprint;
  // Milliseconds since the epoch, 0 if unknown.
  qint64 trustedSince = 0;
  // A different certificate the server presented since, which was refused.
  // Empty if there was none.
  QString rejectedFingerprint;
  qint64 rejectedAt = 0;
};

struct GitSshAuthInfo {
  QString username;
  QString privateKeyPath;
  QString publicKeyPath;
  QString passphrase;
  bool passphrasePresent = false;
  bool valid = false;
};

struct GitCommitInfo {
  git_oid oid;
  QString message;
  QString summary;
  QString authorName;
  QString authorEmail;
  qint64 timeStamp = -1;
  bool signedCommit = false;
  QString signatureType;
  QString signature;
  QVector<QString> branches;
  QVector<QString> parents;
  QVector<QString> children;

  bool operator==(const GitCommitInfo &other) const {
    return git_oid_equal(&oid, &other.oid) == 1 && message == other.message &&
           summary == other.summary && authorName == other.authorName &&
           authorEmail == other.authorEmail && timeStamp == other.timeStamp &&
           signedCommit == other.signedCommit &&
           signatureType == other.signatureType &&
           signature == other.signature && branches == other.branches &&
           parents == other.parents && children == other.children;
  }
};

// How a commit takes part in a bisect session in progress.
enum class BisectMark : int {
  None = 0,
  Good,
  Bad,
  Skip,
  // Checked out for testing and not marked yet.
  Current,

  last
};

// One page of the repository history. Commits are ordered newest first and
// every commit's parents are listed in commitInfos[sha].parents (parents may
// live on a later page).
struct GitHistory {
  bool valid = false;
  QVector<QString> commits;
  QMap<QString, GitCommitInfo> commitInfos;
  // Commit SHA to BisectMark. Empty if bisect is not in progress. Deliberately
  // not part of operator==, see GitHistoryModel::updateHistoryAndStatus.
  // Filled on page 0 only.
  QHash<QString, int> bisectMarks;
  // True if there are more commits after this page.
  bool hasMore = false;
  // Total number of commits in the history being paged.
  qint64 total = 0;
  // Filled on page 0 only: SHA of HEAD and, per tip commit SHA, the names of
  // the refs pointing to it (local branches first, then remote branches
  // without the remote prefix, then tags). The names are used as stable keys
  // to choose graph colors, they are not part of operator==.
  QString headCommit;
  QHash<QString, QStringList> refTips;
  // Filled on page 0 only: per tip commit SHA, the full names of the refs
  // pointing to it (refs/heads/..., refs/remotes/..., refs/tags/...), used to
  // show branch and tag badges. Not part of operator==.
  QHash<QString, QStringList> refLabels;

  bool operator==(const GitHistory &other) const {
    return valid == other.valid && commits == other.commits &&
           commitInfos == other.commitInfos && hasMore == other.hasMore &&
           total == other.total;
  }
};

struct GitStatus {
  QString path;
  QStringList newFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
  QStringList typeChangedFiles;
  QStringList renamedFiles;
  QStringList unreadableFiles;
  QStringList newStagedFiles;
  QStringList modifiedStagedFiles;
  QStringList deletedStagedFiles;
  QStringList typeChangedStagedFiles;
  QStringList renamedStagedFiles;
  QStringList ignoredFiles;
  QStringList conflictedFiles;

  bool hasChanges() const {
    return !newFiles.isEmpty() || !modifiedFiles.isEmpty() ||
           !deletedFiles.isEmpty() || !typeChangedFiles.isEmpty() ||
           !renamedFiles.isEmpty() || !unreadableFiles.isEmpty() ||
           !newStagedFiles.isEmpty() || !modifiedStagedFiles.isEmpty() ||
           !deletedStagedFiles.isEmpty() || !typeChangedStagedFiles.isEmpty() ||
           !renamedStagedFiles.isEmpty() /*|| !ignoredFiles.isEmpty()*/ ||
           !conflictedFiles.isEmpty();
  }

  bool operator==(const GitStatus &other) const {
    return path == other.path && newFiles == other.newFiles &&
           modifiedFiles == other.modifiedFiles &&
           deletedFiles == other.deletedFiles &&
           typeChangedFiles == other.typeChangedFiles &&
           renamedFiles == other.renamedFiles &&
           unreadableFiles == other.unreadableFiles &&
           newStagedFiles == other.newStagedFiles &&
           modifiedStagedFiles == other.modifiedStagedFiles &&
           deletedStagedFiles == other.deletedStagedFiles &&
           typeChangedStagedFiles == other.typeChangedStagedFiles &&
           renamedStagedFiles == other.renamedStagedFiles &&
           ignoredFiles == other.ignoredFiles &&
           conflictedFiles == other.conflictedFiles;
  }
};

struct GitCommitStatus {
  QString commit;
  QString authorName;
  QString authorEmail;
  QString committerDate;
  QString note;
  QString summary;
  QStringList parentShas;
  QStringList newFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
  QStringList typeChangedFiles;
  QStringList renamedFiles;
  QStringList untrackedFiles;
};

enum class BranchType {
  Unknown = 0,

  All,
  Local,
  Remote,
  Tag,

  last
};

struct GitHistoryWithStatus {
  ;
  GitHistory history;
  GitStatus status;
  BranchType branchType = BranchType::Unknown;
  QString branchName;
  QString uuid;
  int page = -1;

  bool operator==(const GitHistoryWithStatus &other) const {
    return history == other.history && status == other.status &&
           branchType == other.branchType && branchName == other.branchName &&
           uuid == other.uuid && page == other.page;
  }
};

enum class BranchStatusType {
  Unknown = 0,
  UpToDate,   // Local tracking branch is even with remote
  Ahead,      // Local tracking branch is ahead of remote
  Behind,     // Local tracking branch is behind remote
  Diverged,   // Local tracking branch has diverged from remote
  NoUpstream, // No tracking branch configured

  last
};

struct BranchStatus {
  BranchStatusType status = BranchStatusType::Unknown;
  size_t aheadCount = 0;  // Commits local is ahead
  size_t behindCount = 0; // Commits local is behind
};

struct BranchInfo {
  QString name;
  BranchStatus statusInfo;

  bool operator==(const BranchInfo &other) const {
    return name == other.name && statusInfo.status == other.statusInfo.status &&
           statusInfo.aheadCount == other.statusInfo.aheadCount &&
           statusInfo.behindCount == other.statusInfo.behindCount;
  }
};

enum class SubmoduleStatusType {
  Unknown = 0,
  UpToDate, // Submodule is at expected commit
  Outdated, // Submodule is at different commit than expected
  Error,    // Error occurred checking submodule status

  last
};

struct SubmoduleStatus {
  SubmoduleStatusType status = SubmoduleStatusType::Unknown;
};

struct SubmoduleInfo {
  QString name;
  QString commit;
  SubmoduleStatus statusInfo;

  bool operator==(const SubmoduleInfo &other) const {
    return name == other.name && commit == other.commit &&
           statusInfo.status == other.statusInfo.status;
  }
};

struct WorktreeInfo {
  // Admin name under .git/worktrees; for the main worktree the directory name
  // of the main working directory.
  QString name;
  // Absolute working directory path, forward slashes, no trailing slash.
  QString path;
  // Short branch name (e.g. "feature/x"); empty if HEAD is detached.
  QString branch;
  // 7-char abbreviated HEAD commit sha (empty if unborn/unreadable).
  QString head;
  // The main (non-linked) working tree.
  bool isMain = false;
  // The worktree whose working directory backs the repository passed in.
  bool isCurrent = false;
  bool locked = false;
  // git_worktree_validate() == 0 (always true for the main worktree).
  bool valid = true;

  bool operator==(const WorktreeInfo &other) const {
    return name == other.name && path == other.path && branch == other.branch &&
           head == other.head && isMain == other.isMain &&
           isCurrent == other.isCurrent && locked == other.locked &&
           valid == other.valid;
  }
};

struct RemoteTarget {
  QString name; // e.g., "Default", "Backup", etc.
  QString fetchUrl;
  QString pushUrl;

  bool operator==(const RemoteTarget &other) const {
    return name == other.name && fetchUrl == other.fetchUrl &&
           pushUrl == other.pushUrl;
  }
};
struct RemoteInfo {
  QString name;
  QString fetchUrl; // Kept for backward compatibility
  QString pushUrl;  // Kept for backward compatibility
  bool isDefault = false;
  QVector<RemoteTarget> targets; // Multiple fetch/push URL combinations

  bool operator==(const RemoteInfo &other) const {
    return name == other.name && fetchUrl == other.fetchUrl &&
           pushUrl == other.pushUrl && targets == other.targets;
  }

  // Get the default target
  RemoteTarget getDefaultTarget() const {
    if (!targets.isEmpty()) {
      for (const auto &target : targets) {
        if (target.name == "Default") {
          return target;
        }
      }
      return targets.first();
    }
    return RemoteTarget{QStringLiteral("Default"), fetchUrl, pushUrl};
  }
};

enum class GitFlowBranchType {
  Unknown = 0,

  Feature,
  Release,
  Hotfix,

  last
};

enum class ResetType {
  Unknown = 0,

  Soft,
  Mixed,
  Hard,

  last
};

struct FilesDiffLine {
  QChar origin;
  QString content;
};

struct FilesDiffHunk {
  int oldStart = -1;
  int newStart = -1;
  int oldLines = -1;
  int newLines = -1;
  QVector<FilesDiffLine> lines;
};

struct FilesDiffDelta {
  QString oldFilePath;
  QString newFilePath;
  QVector<FilesDiffHunk> hunks;
};

struct FilesDiff {
  QString diff;
  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QVector<FilesDiffDelta> deltas;
  bool valid = false;
};

struct BlameLineInfo {
  QString filePath;
  qint64 lineNumber = -1;
  QString content;
  QString commit; // Full SHA, all zeros for uncommitted lines.
  QString author;
  QString email;
  QString summary;
};

struct BlameFileInfo {
  QString filePath;
  QVector<BlameLineInfo> lineInfos;
  QString errorMessage;
};

struct BlameReport {
  QVector<BlameFileInfo> fileInfos;
  bool valid = false;
};

enum class BisectAction : int {
  Good = 0,
  Bad,
  Skip,
  Reset,

  last
};

struct BisectStatus {
  // A bisect session exists after the operation.
  bool active = false;
  // The first bad commit has been found.
  bool finished = false;
  // Only skipped commits are left, the first bad commit is one of them.
  bool ambiguous = false;
  // Both a good and a bad commit are needed before a commit can be tested.
  bool waitingForGood = false;
  bool waitingForBad = false;
  // Commit checked out for testing.
  QString currentCommit;
  QString currentSummary;
  // First bad commit when finished.
  QString firstBadCommit;
  QString firstBadSummary;
  // Commits that are still candidates and the estimated steps left.
  int remaining = 0;
  int steps = 0;
};

} // namespace GitRepo
