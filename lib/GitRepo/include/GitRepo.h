#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

#include <git2/repository.h>
#include <git2/status.h>

#include <tuple>

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

struct GitHistory {
  bool valid = false;
  QVector<QString> commits;
  QMap<QString, GitCommitInfo> commitInfos;

  bool operator==(const GitHistory &other) const {
    return valid == other.valid && commits == other.commits &&
           commitInfos == other.commitInfos;
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
  QString commit;
  QString author;
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

GitErrorContext getLastError();

void initForCurrentThread();

void shutdownForCurrentThread();

Lfs::StorageStatus getLfsStorageStatus(git_repository *repo);

QString getBasicAuthHeader(const QString &repoPath, const QString &url);
QString getBasicAuthHeader(git_repository *repo, const QString &url);
GitSshAuthInfo getSshAuthInfo(const QString &repoPath, const QString &url,
                              const QString &usernameFromUrl = QString());

// Clear cached credentials for a given repository path
// This should be called after updating credentials to ensure fresh credentials
// are used
void clearCachedCredentials(const QString &repoPath);

bool amend(git_repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit);

bool applyStash(git_repository *repo, int index);

bool hasConflicts(git_repository *repo);

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    BranchType type);

bool checkoutCommit(git_repository *repo, const QString &commit);

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid);

void closeRepository(git_repository *repo);

bool createBranchFromCommit(git_repository *repo, const QString &commit,
                            const QString &branchName);

bool startGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                        const QString &branchName,
                        const QString &developmentBranchName = {});

bool finishGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                         const QString &branchName, const QString &author,
                         const QString &email,
                         const QString &developmentBranchName = {});

bool deleteStash(git_repository *repo, int stashIndex);
// Delete all stashes in the repository
bool deleteAllStashes(git_repository *repo);
QString getActiveBranch(git_repository *repo, BranchType type,
                        const bool &quitFlag);

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email);

QString listConflictsForAFile(git_repository *repo, const QString &filePath);
QString listConflictsForStashFile(git_repository *repo, int stashIndex,
                                  const QString &filePath);

QStringList listLocalBranches(git_repository *repo, const bool &quitFlag);

QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag);

QVector<BranchInfo> listRemoteBranchesWithStatus(git_repository *repo,
                                                 const bool &quitFlag);

QStringList listStashes(git_repository *repo, const bool &quitFlag);

GitCommitStatus getStashStatus(git_repository *repo, int stashIndex);

QStringList listSubmodules(git_repository *repo);
QVector<SubmoduleInfo> listSubmodulesWithStatus(git_repository *repo,
                                                const bool &quitFlag);

QStringList listTags(git_repository *repo);

bool merge(git_repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email);
bool rebaseBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit);

git_repository *openRepository(const QString &path, const QString &author,
                               const QString &email);

GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            BranchType type, int page, const bool &quitFlag);

QStringList getCommitBranches(git_repository *repo, const QString &commitOid,
                              const bool &quitFlag);

QStringList getCommitTags(git_repository *repo, const QString &commitOid,
                          const bool &quitFlag);

QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag);

bool getCommitIsHead(git_repository *repo, const QString &commitOid,
                     const bool &quitFlag);

GitStatus getStatus(git_repository *repo, const bool &quitFlag);

GitCommitStatus getCommitStatus(git_repository *repo, const QString &commitOid);

bool stageFiles(git_repository *repo, const QStringList &files);

bool unstageFiles(git_repository *repo, const QStringList &files);

bool commitStaged(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);

bool commitRebase(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit);

FilesDiff diffFiles(git_repository *repo, const QStringList &files);

FilesDiff diffFilesFromCommit(git_repository *repo, const QStringList &files,
                              const QString &commit);

BlameReport blame(git_repository *repo, const QStringList &files);

qint64 getFileSizeFromCommit(git_repository *repo, const QString &filePath,
                             const QString &commit);

bool discardFiles(git_repository *repo, const QStringList &files);

bool pull(git_repository *rep, const QString &activeBranchName = QString(),
          bool rebase = false);

bool fetchAll(git_repository *repo);
bool fetchRemote(git_repository *repo, const QString &remoteName);
bool rebaseBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email);

bool push(git_repository *repo, const QString &activeBranchName = QString(),
          bool force = false);

bool push(git_repository *rep);

bool pushTag(git_repository *repo, const QString &tagName);

bool pushAllTags(git_repository *repo);

bool stash(git_repository *repo);

bool setUpstream(git_repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName);

std::tuple<bool, QString, QString> getUpstream(git_repository *repo,
                                               const QString &localBranchName);

bool resetToCommit(git_repository *repo, const QString &commit, ResetType type);

bool revertCommit(git_repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message);

bool cherryPickCommit(git_repository *repo, const QString &commit,
                      const QString &author, const QString &email);

bool applyPatch(git_repository *repo, const QString &patchContent);

bool renameBranch(git_repository *repo, const QString &oldName,
                  const QString &newName);

bool deleteBranch(git_repository *repo, const QString &branchName);

bool deleteRemoteBranch(git_repository *repo, const QString &branchName);

bool createTagFromCommit(git_repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message);

QString createPatchFromCommit(git_repository *repo, const QString &commit);

bool renameTag(git_repository *repo, const QString &oldName,
               const QString &newName);

bool deleteTag(git_repository *repo, const QString &tagName);

bool deleteRemoteTag(git_repository *repo, const QString &tagName);

QString getSubmodulePath(git_repository *repo, const QString &submoduleName);

bool deleteSubmodule(git_repository *repo, const QString &submoduleName,
                     const QString &submodulePath);

bool updateSubmodule(git_repository *repo, const QString &submoduleName);

bool addSubmodule(git_repository *repo, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid);

bool getIsMergeInProgress(git_repository *repo);

bool getIsRebaseInProgress(git_repository *repo);

bool abortMerge(git_repository *repo);

bool abortRebase(git_repository *repo);

// Remote management functions
QVector<RemoteInfo> listRemotes(git_repository *repo);

QString getDefaultRemote(git_repository *repo);

bool setDefaultRemote(git_repository *repo, const QString &remoteName);

bool addRemote(git_repository *repo, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl);

bool removeRemote(git_repository *repo, const QString &remoteName);

bool updateRemote(git_repository *repo, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl);

// Target management functions (multiple targets per remote)
bool addTarget(git_repository *repo, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl);

bool editTarget(git_repository *repo, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl);

bool deleteTarget(git_repository *repo, const QString &remoteName,
                  const QString &targetName);

bool setDefaultTarget(git_repository *repo, const QString &remoteName,
                      const QString &targetName);

} // namespace GitRepo
