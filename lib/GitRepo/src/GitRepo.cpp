#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include "../../../component/GitRepoManager/include/GitRepoManagerStructs.h"
#include "GitLfs.h"

#include <CloudCredentialStorage.h>
#include <ConfigManager.h>
#include <EventManager.h>
#include <JWTDecoder.h>
#include <LockGuard.h>
#include <Log.h>

#include <git2/sys/errors.h>

#include <QByteArray>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QTextStream>
#include <QTimeZone>

#include <atomic>
#include <cstring>
#include <string>
#include <thread>
#include <tuple>

#ifdef _WIN32
#include <applink.c>
#endif

#define THREAD_ID "GitRepo"

namespace GitRepo {

using namespace Private;

Util::Mutex gErrorMutex;

QMap<std::thread::id, GitErrorContext> gErrorMap;

static QString capitalizeFirst(const QString &str) {
  QString capitalized = str;
  if (!capitalized.isEmpty()) {
    capitalized[0] = capitalized[0].toUpper();
  }

  return capitalized;
}

namespace Private {

QString buildCommitMessage(const QString &summary, const QString &message,
                           const QString &author, const QString &email,
                           bool signoff) {
  QString commitMessage = summary;

  if (!message.isEmpty()) {
    commitMessage += "\n\n" + message;
  }

  if (!signoff) {
    return commitMessage;
  }

  const QString signoffLine =
      QString("Signed-off-by: %1 <%2>").arg(author, email);

  while (!commitMessage.isEmpty() &&
         (commitMessage.endsWith('\n') || commitMessage.endsWith('\r'))) {
    commitMessage.chop(1);
  }

  // Like `git commit -s`, don't repeat a sign-off that ends the message.
  if (commitMessage.section('\n', -1).trimmed() == signoffLine) {
    return commitMessage;
  }

  if (!commitMessage.isEmpty()) {
    commitMessage += "\n\n";
  }

  commitMessage += signoffLine;
  return commitMessage;
}

QString commitSignatureDateToString(const git_signature *signature) {
  if (!signature) {
    return {};
  }

  const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(
      signature->when.time,
      QTimeZone::fromSecondsAheadOfUtc(signature->when.offset * 60));
  return QLocale::system().toString(dateTime, QLocale::ShortFormat);
}

} // namespace Private

static int encodeLibgitError(const int error, const int klass) {
  return (klass & 0x00ff) << 16 | (error & 0xffff);
}

namespace Private {

QString gitErrorName(const GitError errorType) {
  switch (errorType) {
  case GitError::Unknown:
    return QCoreApplication::translate("GitRepoError",
                                       "An unknown Git error occurred.");
  case GitError::MarkConflictedFilesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToCreateAncestorBlobFromWorkdir:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create ancestor blob from workdir while "
                        "marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToCreateOursBlobFromWorkdir:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create ours blob from workdir while "
                        "marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToCreateTheirsBlobFromWorkdir:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create theirs blob from workdir while "
                        "marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToAddConflictToIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add conflict to index while marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index while marking conflicted files.");
  case GitError::
      CollectConflictedFilePathsFromResultIndexFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while collecting "
                        "conflicted file paths from the result index.");
  case GitError::CollectConflictedFilePathsFailedToPeelHeadToCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel head to commit while collecting "
                        "conflicted file paths.");
  case GitError::CollectConflictedFilePathsFromResultIndexFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get commit tree while collecting "
                        "conflicted file paths from the result index.");
  case GitError::
      CollectConflictedFilePathsFromResultIndexFailedToDiffTreeAgainstIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to diff tree against index while collecting "
                        "conflicted file paths from the result index.");
  case GitError::
      CollectConflictedFilePathsFromResultIndexFailedToCreateConflictIterator:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create conflict iterator while collecting "
                        "conflicted file paths from the result index.");
  case GitError::CreateInitialCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create signature from profile while "
                        "creating the initial commit.");
  case GitError::CreateInitialCommitFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToReadNextBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read next branch while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToRenameBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to rename branch while creating the initial commit.");
  case GitError::CreateInitialCommitWithoutStagingFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature from profile while creating the initial "
        "commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToCreateTreeBuilder:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create tree builder while creating the "
                        "initial commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToWriteTreeBuilder:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write tree builder while creating the "
                        "initial commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while creating the initial "
                        "commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit while creating the initial "
                        "commit without staging.");
  case GitError::GetLastCommitFailedToResolveReferenceNameToId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve reference name to id while "
                        "getting the last commit.");
  case GitError::GetLastCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while getting the last commit.");
  case GitError::ApplyStashFailedToApplyStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to apply stash while applying the stash.");
  case GitError::ApplyStashFailedToDiffIndexAgainstWorkdir:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to diff index against workdir while applying the stash.");
  case GitError::HasConflictsFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while checking for conflicts.");
  case GitError::CheckoutBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup branch while checking out the branch.");
  case GitError::CheckoutBranchFailedToPeelLocalBranchToCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel local branch to commit while checking out the branch.");
  case GitError::CheckoutBranchFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToCheckoutLocalBranchTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout local branch tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToGetReferenceName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference name while checking out the branch.");
  case GitError::CheckoutBranchFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup reference while checking out the branch.");
  case GitError::CheckoutBranchFailedToPeelRemoteBranchToCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel remote branch to commit while "
                        "checking out the branch.");
  case GitError::CheckoutBranchFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while checking out the branch.");
  case GitError::CheckoutBranchFailedToCreateLocalBranchFromRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create local branch from remote while "
                        "checking out the branch.");
  case GitError::CheckoutBranchFailedToSetBranchUpstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set branch upstream while checking out the branch.");
  case GitError::CheckoutBranchFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToCheckoutTagTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout tag tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToSetHeadToTagReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set head to tag reference while checking out the branch.");
  case GitError::CheckoutBranchFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while checking out the branch.");
  case GitError::CheckoutBranchFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToSetHeadDetachedAtTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set head detached at tag while checking out the branch.");
  case GitError::OpenRepositoryFailedToOpenRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to open repository while opening the repository.");
  case GitError::ListCheckoutBranchesFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToReadNextBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read next branch while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToCheckIfBranchIsCheckedOut:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to check if branch is checked out while "
                        "listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToListTags:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list tags while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while listing checkout branches.");
  case GitError::ListBranchesFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while listing branches.");
  case GitError::ListBranchesFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while listing branches.");
  case GitError::ListBranchesFailedToReadNextBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read next branch while listing branches.");
  case GitError::ListBranchesFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while listing branches.");
  case GitError::ListRemoteBranchesWithStatusFailedToCalculateAheadBehind:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to calculate ahead behind while listing "
                        "remote branches with status.");
  case GitError::ListStashesFailedToListStashes:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list stashes while listing stashes.");
  case GitError::GetStashStatusFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while getting stash status.");
  case GitError::GetStashStatusFailedToPeelObjectToCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel object to commit while getting stash status.");
  case GitError::GetStashStatusFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToGetBaseParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get base parent commit while getting stash status.");
  case GitError::GetStashStatusFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToGetThirdCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get third commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToGetFourthCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get fourth commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToCreateStashDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create stash diff while getting stash status.");
  case GitError::GetStashStatusFailedToWalkUntrackedStashTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to walk untracked stash tree while getting stash status.");
  case GitError::ListTagsFailedToListTags:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list tags while listing tags.");
  case GitError::ListSubmodulesFailedToIterateSubmodules:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to iterate submodules while listing submodules.");
  case GitError::CreateBranchFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while creating a branch from a commit.");
  case GitError::CreateBranchFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while creating a branch from a commit.");
  case GitError::CreateBranchFromCommitFailedToCreateBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch while creating a branch from a commit.");
  case GitError::DeleteStashFailedToDropStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to drop stash while delete stash.");
  case GitError::DeleteAllStashesFailedToReadObjectId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read object id while deleting all stashes.");
  case GitError::DeleteAllStashesFailedToDropStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to drop stash while deleting all stashes.");
  case GitError::InitRepositoryFailedToInitializeRepositoryOptions:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to initialize repository options while "
                        "initializing the repository.");
  case GitError::InitRepositoryFailedToInitializeRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to initialize repository while initializing the repository.");
  case GitError::CredentialsAcquireCallbackFailedToCreatePlaintextCredentials:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create plaintext credentials while acquiring credentials.");
  case GitError::CheckoutCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while checking out the commit.");
  case GitError::CheckoutCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while checking out the commit.");
  case GitError::CheckoutCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while checking out the commit.");
  case GitError::CheckoutCommitFailedToCheckoutCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout commit tree while checking out the commit.");
  case GitError::CheckoutCommitFailedToDetachHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to detach head while checking out the commit.");
  case GitError::CloneRepositoryFailedToCloneRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to clone repository while cloning the repository.");
  case GitError::CloneRepositoryGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Cloning git@ repositories requires a commercial plan.");
  case GitError::GetBranchHistoryFailedToCreateRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToConfigureRevwalkSorting:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to configure revwalk sorting while getting branch history.");
  case GitError::GetBranchHistoryFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while getting branch history.");
  case GitError::GetBranchHistoryFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddReferenceToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add reference to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddLocalBranchToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add local branch to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddRemoteBranchToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add remote branch to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddTagToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add tag to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while getting branch history.");
  case GitError::GetCommitBranchesFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while getting commit branches.");
  case GitError::GetCommitBranchesFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while getting commit branches.");
  case GitError::GetCommitBranchesFailedToResolveReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve reference while getting commit branches.");
  case GitError::GetCommitBranchesFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel reference while getting commit branches.");
  case GitError::GetCommitBranchesFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while getting commit branches.");
  case GitError::GetCommitTagsFailedToListMatchingTags:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list matching tags while get commit tags.");
  case GitError::GetCommitHeadsFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while getting commit heads.");
  case GitError::GetCommitHeadsFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while getting commit heads.");
  case GitError::GetCommitHeadsFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while getting commit heads.");
  case GitError::GetCommitHeadsFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while getting commit heads.");
  case GitError::GetCommitIsHeadFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create branch iterator while checking "
                        "whether the commit is a branch head.");
  case GitError::GetCommitIsHeadFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while checking whether the "
                        "commit is a branch head.");
  case GitError::GetCommitIsHeadFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while checking whether "
                        "the commit is a branch head.");
  case GitError::GetCommitIsHeadFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to parse object id while checking whether "
                        "the commit is a branch head.");
  case GitError::GetStatusFailedToCreateStatusList:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create status list while get status.");
  case GitError::GetCommitStatusFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup commit while getting commit status.");
  case GitError::GetCommitStatusFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while getting commit status.");
  case GitError::GetCommitStatusFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while getting commit status.");
  case GitError::GetCommitStatusFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while getting commit status.");
  case GitError::GetCommitStatusFailedToCreateCommitDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit diff while getting commit status.");
  case GitError::GetCommitStatusFailedToDetectRenames:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to detect renames while getting commit status.");
  case GitError::StageFilesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository index while stage files.");
  case GitError::StageFilesFailedToRemovePathFromIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to remove path from index while stage files.");
  case GitError::StageFilesFailedToAddSubmoduleToIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add submodule to index while stage files.");
  case GitError::StageFilesFailedToAddPathToIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add path to index while stage files.");
  case GitError::StageFilesFailedToAddMissingPathToIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add missing path to index while stage files.");
  case GitError::StageFilesFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index while stage files.");
  case GitError::UnstageFilesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while unstage files.");
  case GitError::UnstageFilesFailedToResetPathsToHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to reset paths to head while unstage files.");
  case GitError::CommitStagedFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while committing staged changes.");
  case GitError::CommitStagedFailedToAbortRebaseSession:
    return QCoreApplication::translate("GitRepoError",
                                       "Failed to abort rebase session.");
  case GitError::CommitStagedFailedToOpenRebaseSession:
    return QCoreApplication::translate("GitRepoError",
                                       "Failed to open rebase session.");
  case GitError::CommitStagedFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree while committing staged changes.");
  case GitError::CommitStagedFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree while committing staged changes.");
  case GitError::CommitStagedFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while committing staged changes.");
  case GitError::CommitStagedFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while committing staged changes.");
  case GitError::CommitStagedFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while committing staged changes.");
  case GitError::CommitStagedFailedToResolveMergeHeadReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve merge head reference while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToGetSecondReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get second reference target while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToLoadSigningConfiguration:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to load signing configuration while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToSignCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to sign commit while committing staged changes.");
  case GitError::CommitStagedFailedToCreateCommitFromStagedChanges:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit from staged changes while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit while committing staged changes.");
  case GitError::CommitStagedFailedToCleanupRepositoryState:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to cleanup repository state while committing staged changes.");
  case GitError::DiffFilesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while diffing files.");
  case GitError::DiffFilesFailedToGetHeadObjectId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get head object id while diffing files.");
  case GitError::DiffFilesFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get commit tree while diffing files.");
  case GitError::DiffFilesFailedToDiffTreeAgainstWorkdir:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to diff tree against workdir while diffing files.");
  case GitError::DiffFilesFailedToCreatePatchFromDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create patch from diff while diffing files.");
  case GitError::DiffFilesFailedToReadPatchHunk:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read patch hunk while diffing files.");
  case GitError::DiffFilesFailedToReadPatchLine:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read patch line while diffing files.");
  case GitError::DiffFilesFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while diffing files.");
  case GitError::DiffFilesFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while diffing files.");
  case GitError::DiffFilesFailedToReadHeadCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read head commit while diffing files.");
  case GitError::DiffFilesFailedToLookupTreeEntryByPath:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree entry by path while diffing files.");
  case GitError::DiffFilesFailedToReadTreeEntryId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read tree entry id while diffing files.");
  case GitError::DiffFilesFailedToLookupBlobByPrefix:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup blob by prefix while diffing files.");
  case GitError::DiscardFilesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while discarding files.");
  case GitError::DiscardFilesFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout tree while discarding files.");
  case GitError::DiscardFilesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while discarding files.");
  case GitError::DiscardFilesFailedToResolveHeadRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve head revision while discarding files.");
  case GitError::DiscardFilesFailedToGetHeadTreeObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get head tree object id while discarding files.");
  case GitError::DiscardFilesFailedToLookupTreeEntryByPath:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree entry by path while discarding files.");
  case GitError::DiscardFilesFailedToReadTreeEntryId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read tree entry id while discarding files.");
  case GitError::DiscardFilesFailedToRemoveIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to remove index entry while discarding files.");
  case GitError::DiscardFilesFailedToAddIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add index entry while discarding files.");
  case GitError::DiscardFilesFailedToRemoveDeletedIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to remove deleted index entry while discarding files.");
  case GitError::DiscardFilesFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index while discarding files.");
  case GitError::PullFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while pulling changes.");
  case GitError::PullFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while pulling changes.");
  case GitError::PullNoUpstreamConfigured:
    return QCoreApplication::translate(
        "GitRepoError", "No upstream is configured while pulling changes.");
  case GitError::PullFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pulling changes.");
  case GitError::PullFailedToFetchRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to fetch remote while pulling changes.");
  case GitError::PullFailedToFetchRemoteWithCredentials:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to fetch remote with credentials while pulling changes.");
  case GitError::PullFailedToGetRepositoryHeadAfterFetch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head after fetch while pulling changes.");
  case GitError::PullFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup reference while pulling changes.");
  case GitError::PullFailedToCreateAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create annotated commit from reference "
                        "while pulling changes.");
  case GitError::PullFailedToGetDefaultSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get default signature while pulling changes.");
  case GitError::PullFailedToStartRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to start rebase while pulling changes.");
  case GitError::PullFailedToCommitRebaseStep:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to commit rebase step while pulling changes.");
  case GitError::PullFailedToFinishRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to finish rebase while pulling changes.");
  case GitError::PullFailedToAbortRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to abort rebase while pulling changes.");
  case GitError::PullFailedToCreateAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create annotated commit while pulling changes.");
  case GitError::PullFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup commit while pulling changes.");
  case GitError::PullFailedToPeelFetchedTargetCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel fetched target commit while pulling changes.");
  case GitError::PullFailedToSetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set reference target while pulling changes.");
  case GitError::PullFailedToCreateMergeAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create merge annotated commit while pulling changes.");
  case GitError::PullGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pulling git@ repositories requires a commercial plan.");
  case GitError::FetchRemoteInternalFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while fetching from the remote.");
  case GitError::FetchRemoteGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Fetching git@ repositories requires a commercial plan.");
  case GitError::FetchRemoteInternalFailedToFetchRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to fetch remote while fetching from the remote.");
  case GitError::FetchRemoteInternalFailedToFetchRemoteWithCredentials:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to fetch remote with credentials while "
                        "fetching from the remote.");
  case GitError::PushFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while pushing changes.");
  case GitError::PushGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pushing git@ repositories requires a commercial plan.");
  case GitError::PushFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while pushing changes.");
  case GitError::PushFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pushing changes.");
  case GitError::PushFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while pushing changes.");
  case GitError::PushTagFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup reference while pushing the tag.");
  case GitError::PushTagGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pushing tags to git@ repositories requires a commercial plan.");
  case GitError::PushTagFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pushing the tag.");
  case GitError::PushTagFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while pushing the tag.");
  case GitError::PushAllTagsFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pushing all tags.");
  case GitError::PushAllTagsGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pushing all tags to git@ repositories requires a commercial plan.");
  case GitError::PushAllTagsFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while pushing all tags.");
  case GitError::StashFailedToGetDefaultSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get default signature while creating a stash.");
  case GitError::StashFailedToCreateStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create stash while creating a stash.");
  case GitError::SetUpstreamFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while setting upstream.");
  case GitError::SetUpstreamFailedToSetBranchUpstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set branch upstream while setting upstream.");
  case GitError::GetUpstreamFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while getting upstream.");
  case GitError::GetUpstreamFailedToGetBranchUpstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get branch upstream while getting upstream.");
  case GitError::GetUpstreamFailedToOpenGitConfig:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to open git config while getting upstream.");
  case GitError::ResetToCommitFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while resetting to the commit.");
  case GitError::ResetToCommitFailedToResetRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to reset repository while resetting to the commit.");
  case GitError::RevertCommitFailedToLookupTargetCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup target commit while reverting the commit.");
  case GitError::RevertCommitFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while reverting the commit.");
  case GitError::RevertCommitFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while reverting the commit.");
  case GitError::RevertCommitFailedToPrepareRevertCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to prepare revert commit while reverting the commit.");
  case GitError::RevertCommitFailedToCheckoutIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout index while reverting the commit.");
  case GitError::RevertCommitFailedToWriteIndexTreeToRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree to repository while reverting the commit.");
  case GitError::RevertCommitFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while reverting the commit.");
  case GitError::RevertCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while reverting the commit.");
  case GitError::RevertCommitFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit while reverting the commit.");
  case GitError::ApplyPatchFailedToParsePatchBuffer:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse patch buffer while applying the patch.");
  case GitError::ApplyPatchFailedToApplyPatch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to apply patch while applying the patch.");
  case GitError::CherryPickCommitFailedToLookupTargetCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup target commit while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToPrepareCherryPickCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to prepare cherry pick commit while "
                        "cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToCheckoutIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout index while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToWriteIndexTreeToRepository:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index tree to repository while "
                        "cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit while cherry-picking the commit.");
  case GitError::RenameBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while renaming the branch.");
  case GitError::RenameBranchFailedToRenameBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to rename branch while renaming the branch.");
  case GitError::DeleteBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while deleting the branch.");
  case GitError::DeleteBranchFailedToDeleteBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete branch while deleting the branch.");
  case GitError::DeleteRemoteTagFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while deleting the remote tag.");
  case GitError::DeleteRemoteTagGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError", "Deleting remote tags from git@ repositories requires "
                        "a commercial plan.");
  case GitError::DeleteRemoteTagFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while deleting the remote tag.");
  case GitError::CreateTagFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while creating a tag from a commit.");
  case GitError::CreateTagFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while creating a tag from a commit.");
  case GitError::CreateTagFromCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while creating a tag from a commit.");
  case GitError::CreateTagFromCommitFailedToPeelCommitForTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel commit for tag while creating a tag from a commit.");
  case GitError::CreatePatchFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get second commit tree while creating a "
                        "patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToCreateCommitDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit diff while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToCreatePatchFromDiff:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create patch from diff while creating a "
                        "patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToRenderPatchToBuffer:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to render patch to buffer while creating a "
                        "patch from a commit.");
  case GitError::RenameTagFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup reference while renaming the tag.");
  case GitError::RenameTagFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while renaming the tag.");
  case GitError::RenameTagFailedToReadTagTarget:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read tag target while renaming the tag.");
  case GitError::RenameTagFailedToCreateAnnotatedTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create annotated tag while renaming the tag.");
  case GitError::RenameTagFailedToCreateLightweightTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create lightweight tag while renaming the tag.");
  case GitError::RenameTagFailedToDeleteTag:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete tag while renaming the tag.");
  case GitError::DeleteTagFailedToDeleteTag:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete tag while deleting the tag.");
  case GitError::DeleteRemoteBranchFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while deleting the remote branch.");
  case GitError::DeleteRemoteBranchGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError", "Deleting remote branches from git@ repositories "
                        "requires a commercial plan.");
  case GitError::DeleteRemoteBranchFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to push remote while deleting the remote branch.");
  case GitError::GetSubmodulePathFailedToLookupSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup submodule while getting the submodule path.");
  case GitError::DeleteSubmoduleFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while deleting the submodule.");
  case GitError::DeleteSubmoduleFailedToRemoveIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to remove index entry while deleting the submodule.");
  case GitError::DeleteSubmoduleFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index while deleting the submodule.");
  case GitError::CheckoutSubmoduleBranchFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToFetchRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to fetch remote while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup reference while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToPeelReferenceToCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference to commit while checking "
                        "out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToCreateLocalBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create local branch while checking out "
                        "the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToReadCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read commit id while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToSetHeadToBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set head to branch while checking out the "
                        "submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToCheckoutHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout head while checking out the submodule branch.");
  case GitError::CheckoutLocalSubmoduleBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while checking out the "
                        "local submodule branch.");
  case GitError::CheckoutLocalSubmoduleBranchFailedToSetHeadToBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set head to branch while checking out the "
                        "local submodule branch.");
  case GitError::CheckoutLocalSubmoduleBranchFailedToCheckoutHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout head while checking out the "
                        "local submodule branch.");
  case GitError::UpdateSubmoduleFailedToUpdateSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to update submodule while updating the submodule.");
  case GitError::UpdateSubmoduleFailedToAddSubmoduleToIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add submodule to index while updating the submodule.");
  case GitError::AddSubmoduleFailedToSetupSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to setup submodule while adding the submodule.");
  case GitError::AddSubmoduleGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Adding submodules to git@ repositories requires a commercial plan.");
  case GitError::AddSubmoduleFailedToCloneSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to clone submodule while adding the submodule.");
  case GitError::AddSubmoduleFailedToFinalizeSubmoduleAdd:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to finalize submodule add while adding the submodule.");
  case GitError::DiffFilesFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetSecondParentCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get second parent commit while diffing "
                        "files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetThirdCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get third commit tree while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToCreateCommitDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit diff while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToCreatePatchFromDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create patch from diff while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToReadPatchHunk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read patch hunk while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToReadPatchLine:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read patch line while diffing files from a commit.");
  case GitError::GetFileSizeFromCommitFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToGetReferenceName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference name while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToLookupTreeEntryByPath:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree entry by path while getting "
                        "file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToLookupBlob:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup blob while getting file size from a commit.");
  case GitError::MergeFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while merging branches.");
  case GitError::MergeFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while merging branches.");
  case GitError::MergeFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while merging branches.");
  case GitError::MergeFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout tree while merging branches.");
  case GitError::MergeFailedToGetReferenceName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get reference name while merging branches.");
  case GitError::MergeFailedToCreateAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create annotated commit from reference "
                        "while merging branches.");
  case GitError::MergeFailedToCreateSecondAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create second annotated commit from "
                        "reference while merging branches.");
  case GitError::MergeFailedToAnalyzeMerge:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to analyze merge while merging branches.");
  case GitError::MergeFailedToReadAnnotatedCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read annotated commit id while merging branches.");
  case GitError::MergeFailedToReadCommitId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read commit id while merging branches.");
  case GitError::MergeFailedToReadSecondCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read second commit id while merging branches.");
  case GitError::MergeFailedToCheckoutDestinationTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout destination tree while merging branches.");
  case GitError::MergeFailedToMerge:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to merge while merging branches.");
  case GitError::MergeFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while merging branches.");
  case GitError::MergeFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index tree while merging branches.");
  case GitError::MergeFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while merging branches.");
  case GitError::MergeFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create signature while merging branches.");
  case GitError::MergeFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while merging branches.");
  case GitError::MergeFailedToGetSecondReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second reference target while merging branches.");
  case GitError::MergeFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit while merging branches.");
  case GitError::RebaseBranchesFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while rebasing branches.");
  case GitError::RebaseBranchesFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while rebasing branches.");
  case GitError::RebaseBranchesFailedToCreateAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create annotated commit from reference "
                        "while rebasing branches.");
  case GitError::RebaseBranchesFailedToCreateSecondAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create second annotated commit from "
                        "reference while rebasing branches.");
  case GitError::RebaseBranchesFailedToReadAnnotatedCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read annotated commit id while rebasing branches.");
  case GitError::RebaseBranchesFailedToLookupAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup annotated commit while rebasing branches.");
  case GitError::RebaseBranchesFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create signature while rebasing branches.");
  case GitError::RebaseBranchesFailedToStartRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to start rebase while rebasing branches.");
  case GitError::RebaseBranchesFailedToCommitRebaseStep:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to commit rebase step while rebasing branches.");
  case GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to commit rebase step while rebasing branches "
                        "due to conflicts in files.");
  case GitError::RebaseBranchesFailedToFinishRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to finish rebase while rebasing branches.");
  case GitError::RebaseBranchesFailedToAbortRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to abort rebase while rebasing branches.");
  case GitError::SquashBranchesInvalidArguments:
    return QCoreApplication::translate(
        "GitRepoError",
        "Source and destination branches must be different non-empty "
        "branches while squashing branches.");
  case GitError::SquashBranchesRepositoryBusy:
    return QCoreApplication::translate(
        "GitRepoError",
        "Another operation (merge, rebase, etc.) is in progress. Finish or "
        "abort it before squashing branches.");
  case GitError::SquashBranchesFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while squashing branches.");
  case GitError::SquashBranchesFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while squashing branches.");
  case GitError::SquashBranchesFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel destination reference while squashing branches.");
  case GitError::SquashBranchesFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout destination branch while squashing branches.");
  case GitError::SquashBranchesFailedToSetHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set repository HEAD while squashing branches.");
  case GitError::SquashBranchesFailedToCreateAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create annotated commit while squashing branches.");
  case GitError::SquashBranchesFailedToAnalyzeMerge:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to analyze merge while squashing branches.");
  case GitError::SquashBranchesFailedToMerge:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to merge changes while squashing branches.");
  case GitError::SquashBranchesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while squashing branches.");
  case GitError::SquashBranchesFailedWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError",
        "Squash has conflicts. Resolve the conflicts in files, stage them and "
        "commit the result.");
  case GitError::SquashBranchesFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index tree while squashing branches.");
  case GitError::SquashBranchesFailedToLookupHeadCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup HEAD commit while squashing branches.");
  case GitError::SquashBranchesFailedToCleanupRepositoryState:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to cleanup repository state while squashing branches.");
  case GitError::BisectInvalidArguments:
    return QCoreApplication::translate("GitRepoError",
                                       "Invalid bisect arguments.");
  case GitError::BisectRepositoryBusy:
    return QCoreApplication::translate(
        "GitRepoError",
        "Cannot start bisect while another operation is in progress.");
  case GitError::BisectNotInProgress:
    return QCoreApplication::translate("GitRepoError",
                                       "Bisect is not in progress.");
  case GitError::BisectFailedToResolveCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve the commit while bisecting.");
  case GitError::BisectFailedToReadState:
    return QCoreApplication::translate("GitRepoError",
                                       "Failed to read the bisect state.");
  case GitError::BisectFailedToWriteState:
    return QCoreApplication::translate("GitRepoError",
                                       "Failed to write the bisect state.");
  case GitError::BisectFailedToWalkHistory:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to walk the commit history while bisecting.");
  case GitError::BisectInvalidRange:
    return QCoreApplication::translate(
        "GitRepoError",
        "The good commit is not older than the bad commit. Mark another "
        "commit as good or bad, or reset bisect and start again.");
  case GitError::BisectFailedToCheckout:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout the commit while bisecting. Commit or stash "
        "your local changes and try again.");
  case GitError::BisectFailedToResetRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to restore the original state while resetting bisect.");
  case GitError::ListConflictsForAFileFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while listing conflicts for a file.");
  case GitError::ListConflictsForAFileFailedToCreateConflictIterator:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create conflict iterator while listing "
                        "conflicts for a file.");
  case GitError::AbortMergeFailedToCleanupRepositoryState:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to cleanup repository state while aborting the merge.");
  case GitError::AbortMergeFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while aborting the merge.");
  case GitError::AbortMergeFailedToResetRepository:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to reset repository while aborting the merge.");
  case GitError::ListRemotesFailedToListRemotes:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list remotes while listing remotes.");
  case GitError::SetDefaultRemoteFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while setting the default remote.");
  case GitError::SetDefaultRemoteFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while setting the default remote.");
  case GitError::SetDefaultRemoteFailedToWriteDefaultRemoteConfig:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write default remote config while setting "
                        "the default remote.");
  case GitError::AddRemoteFailedToCreateRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create remote while adding the remote.");
  case GitError::AddRemoteFailedToSetPushUrl:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set push url while adding the remote.");
  case GitError::RemoveRemoteFailedToDeleteRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete remote while removing the remote.");
  case GitError::UpdateRemoteFailedToRenameRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to rename remote while updating the remote.");
  case GitError::UpdateRemoteFailedToSetFetchUrl:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set fetch url while updating the remote.");
  case GitError::UpdateRemoteFailedToSetPushUrl:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set push url while updating the remote.");
  case GitError::AddTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while adding the target.");
  case GitError::AddTargetFailedToSetFetchUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set fetch url config while adding the target.");
  case GitError::AddTargetFailedToSetPushUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set push url config while adding the target.");
  case GitError::EditTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while editing the target.");
  case GitError::EditTargetFailedToSetFetchUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set fetch url config while editing the target.");
  case GitError::EditTargetFailedToSetPushUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set push url config while editing the target.");
  case GitError::DeleteTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while deleting the target.");
  case GitError::DeleteTargetFailedToDeleteFetchUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to delete fetch url config while deleting the target.");
  case GitError::DeleteTargetFailedToDeletePushUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to delete push url config while deleting the target.");
  case GitError::SetDefaultTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while setting the default target.");
  case GitError::SetDefaultTargetFailedToCreateConfigIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create config iterator while setting the default target.");
  case GitError::SetDefaultTargetFailedToSetDefaultFetchUrl:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set default fetch url while setting the default target.");
  case GitError::AmendFailedToResolveReferenceNameToId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve reference name to id while amending the commit.");
  case GitError::AmendFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup commit while amending the commit.");
  case GitError::AmendFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while amending the commit.");
  case GitError::AmendFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree while amending the commit.");
  case GitError::AmendFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while amending the commit.");
  case GitError::AmendFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while amending the commit.");
  case GitError::AmendFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while amending the commit.");
  case GitError::AmendFailedToLoadSigningConfiguration:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to load signing configuration while amending the commit.");
  case GitError::AmendFailedToSignCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to sign commit while amending the commit.");
  case GitError::AmendFailedToCreateSignedAmendCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signed amend commit while amending the commit.");
  case GitError::AmendFailedToAmendCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to amend commit while amending the commit.");
  case GitError::ListWorktreesFailedToListWorktrees:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list worktrees while listing worktrees.");
  case GitError::ListWorktreesFailedToOpenWorktree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to open worktree while listing worktrees.");
  case GitError::AddWorktreeBranchRequired:
    return QCoreApplication::translate(
        "GitRepoError", "A branch is required while adding the worktree.");
  case GitError::AddWorktreeFailedToResolveHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve HEAD while adding the worktree.");
  case GitError::AddWorktreeFailedToCreateBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create branch while adding the worktree.");
  case GitError::AddWorktreeFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while adding the worktree.");
  case GitError::AddWorktreeFailedToCreateDirectory:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create directory while adding the worktree.");
  case GitError::AddWorktreeFailedToAddWorktree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add worktree while adding the worktree.");
  case GitError::RemoveWorktreeFailedToLookupWorktree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup worktree while removing the worktree.");
  case GitError::RemoveWorktreeMainNotAllowed:
    return QCoreApplication::translate("GitRepoError",
                                       "The main worktree cannot be removed.");
  case GitError::RemoveWorktreeFailedToPrune:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to remove worktree while removing the worktree.");
  case GitError::LockWorktreeFailedToLookupWorktree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup worktree while locking the worktree.");
  case GitError::LockWorktreeMainNotAllowed:
    return QCoreApplication::translate("GitRepoError",
                                       "The main worktree cannot be locked.");
  case GitError::LockWorktreeFailedToLock:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lock worktree while locking the worktree.");
  case GitError::UnlockWorktreeFailedToLookupWorktree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup worktree while unlocking the worktree.");
  case GitError::UnlockWorktreeMainNotAllowed:
    return QCoreApplication::translate("GitRepoError",
                                       "The main worktree cannot be unlocked.");
  case GitError::UnlockWorktreeFailedToUnlock:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to unlock worktree while unlocking the worktree.");
  case GitError::GitLabFlowBranchNameEmpty:
    return QCoreApplication::translate("GitRepoError",
                                       "GitLab Flow branch name is empty.");
  case GitError::GitLabFlowFailedToLookupBaseBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup the GitLab Flow base branch.");
  case GitError::GitLabFlowPromoteNotDownstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "The current branch is not an environment branch that can be "
        "promoted downstream.");
  case GitError::GitLabFlowFailedToLookupReleaseBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup the GitLab Flow release branch.");
  case GitError::GitHubFlowBranchNameEmpty:
    return QCoreApplication::translate("GitRepoError",
                                       "GitHub Flow branch name is empty.");
  case GitError::GitHubFlowFailedToLookupMainBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup the GitHub Flow main branch.");
  case GitError::GitHubFlowMainBranchNotAllowed:
    return QCoreApplication::translate(
        "GitRepoError",
        "The GitHub Flow main branch can't be updated or finished.");
  case GitError::GitFlowNextFailedToReadConfig:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read the git-flow-next configuration.");
  case GitError::GitFlowNextFailedToWriteConfig:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write the git-flow-next configuration.");
  case GitError::GitFlowNextNotInitialized:
    return QCoreApplication::translate(
        "GitRepoError", "git-flow-next is not initialized in this repository.");
  case GitError::GitFlowNextUnknownBranchType:
    return QCoreApplication::translate(
        "GitRepoError", "The git-flow-next branch type is not configured.");
  case GitError::GitFlowNextNotATopicBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "The branch doesn't belong to a git-flow-next topic branch type.");
  case GitError::GitFlowNextBranchNameEmpty:
    return QCoreApplication::translate("GitRepoError",
                                       "git-flow-next branch name is empty.");
  case GitError::GitFlowNextFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "A branch of the git-flow-next configuration doesn't exist locally.");
  case GitError::GitFlowNextUnsupportedStrategy:
    return QCoreApplication::translate(
        "GitRepoError",
        "The git-flow-next merge strategy is not supported. Use merge, rebase "
        "or squash.");
  case GitError::GitFlowNextInvalidWorkflow:
    return QCoreApplication::translate(
        "GitRepoError", "The custom GitFlow-Next workflow is not valid.");
  case GitError::GitFlowNextTagExists:
    return QCoreApplication::translate(
        "GitRepoError",
        "The tag for this git-flow-next branch already exists.");
  case GitError::SetWorkflowTypeFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while setting the workflow.");
  case GitError::SetWorkflowTypeFailedToWriteConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write workflow config while setting the workflow.");
  case GitError::MergeFailedWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError",
        "The merge stopped with conflicts. Resolve them and commit.");
  case GitError::RemoveWorktreeHasChanges:
    return QCoreApplication::translate(
        "GitRepoError",
        "The worktree has uncommitted changes. Remove it with force to "
        "discard them.");
  case GitError::RenameBranchInvalidName:
    return QCoreApplication::translate("GitRepoError",
                                       "The new branch name is not valid.");
  case GitError::AddTargetRemoteNotFound:
    return QCoreApplication::translate(
        "GitRepoError", "The remote of the target does not exist.");
  case GitError::CherryPickCommitFailedWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError",
        "The cherry-pick stopped with conflicts. Resolve them and commit.");
  case GitError::CherryPickCommitIsEmpty:
    return QCoreApplication::translate(
        "GitRepoError",
        "The cherry-pick changes nothing, the commit is applied already.");
  case GitError::RevertCommitFailedWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError",
        "The revert stopped with conflicts. Resolve them and commit.");
  case GitError::RevertCommitIsEmpty:
    return QCoreApplication::translate(
        "GitRepoError",
        "The revert changes nothing, the commit is reverted already.");
  case GitError::MergeUnrelatedHistories:
    return QCoreApplication::translate(
        "GitRepoError", "The branches have no common history.");
  case GitError::DeleteBranchNotFullyMerged:
    return QCoreApplication::translate(
        "GitRepoError",
        "The branch is not fully merged. Delete it with force to lose its "
        "commits.");
  case GitError::AddTargetAlreadyExists:
    return QCoreApplication::translate("GitRepoError",
                                       "A target with this name exists.");
  case GitError::EditTargetNotFound:
    return QCoreApplication::translate("GitRepoError",
                                       "The target does not exist.");
  case GitError::DeleteTargetNotFound:
    return QCoreApplication::translate("GitRepoError",
                                       "The target does not exist.");
  case GitError::SetUpstreamRemoteNotFound:
    return QCoreApplication::translate("GitRepoError",
                                       "The remote does not exist.");
  case GitError::DeleteSubmoduleFailedToUpdateConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to update the configuration while deleting the submodule.");
  case GitError::AddSubmoduleAlreadyExists:
    return QCoreApplication::translate(
        "GitRepoError",
        "A submodule with this name exists.");
  case GitError::PushTagExistsOnRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "The tag exists on the remote with another target.");
  case GitError::PullBranchNotCheckedOutCannotFastForward:
    return QCoreApplication::translate(
        "GitRepoError",
        "The branch is not checked out and can't be fast-forwarded. Check it "
        "out to merge.");
  case GitError::PullRebaseFailedWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError",
        "The rebase stopped with conflicts and was aborted.");
  case GitError::PushNoUpstreamConfigured:
    return QCoreApplication::translate(
        "GitRepoError",
        "The branch has no upstream. Choose a remote branch to push to.");
  case GitError::CloneRepositoryFailedToCreateDirectory:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create the destination directory.");
  case GitError::GitFlowUnknownBranchType:
    return QCoreApplication::translate(
        "GitRepoError",
        "The branch type is not a Git Flow branch type.");
  case GitError::GitFlowTagExists:
    return QCoreApplication::translate(
        "GitRepoError",
        "The tag for this Git Flow branch already exists.");
  case GitError::DeleteSubmoduleInvalidName:
    return QCoreApplication::translate("GitRepoError",
                                       "The submodule name is not valid.");
  case GitError::AbortMergeNothingToAbort:
    return QCoreApplication::translate(
        "GitRepoError", "No merge, cherry-pick or revert is in progress.");
  case GitError::AbortRebaseNothingToAbort:
    return QCoreApplication::translate("GitRepoError",
                                       "No rebase is in progress.");
  case GitError::GetUpstreamNotConfigured:
    return QCoreApplication::translate("GitRepoError",
                                       "The branch has no upstream.");
  case GitError::UpdateRemoteNotFound:
    return QCoreApplication::translate("GitRepoError",
                                       "The remote does not exist.");
  case GitError::UpdateRemoteInvalidArguments:
    return QCoreApplication::translate(
        "GitRepoError", "The remote name and the fetch URL can't be empty.");
  case GitError::EditTargetInvalidArguments:
    return QCoreApplication::translate(
        "GitRepoError",
        "The remote, the target name and the fetch URL can't be empty, and "
        "the Default target is changed with the remote.");
  case GitError::Last:
    return QCoreApplication::translate("GitRepoError",
                                       "Invalid Git error marker.");
  }

  return QCoreApplication::translate("GitRepoError",
                                     "An unknown Git error occurred.");
}

QString gitErrorMessage(const GitError errorType) {
  return gitErrorName(errorType);
}

void setErrorContext(const GitError errorType, const QString &gitErrorMessage,
                     const int errorCode) {
  LOCKGUARD(gErrorMutex);
  gErrorMap[std::this_thread::get_id()] = {
      errorType, errorCode, gitErrorMessage, /*gitErrorMessage(errorType)*/};
}

void setError(const GitError errorType, const int error) {
  const git_error *gitError = git_error_last();
  const int klass = gitError ? gitError->klass : 0;
  QString libgitMessage =
      gitError && gitError->message && klass != GIT_ERROR_NONE
          ? capitalizeFirst(QString::fromUtf8(gitError->message))
          : QString();
  // Errors that libgit2 didn't report itself still say what went wrong.
  if (libgitMessage.trimmed().isEmpty()) {
    libgitMessage = gitErrorMessage(errorType);
  }
  // A refused certificate: our reason, libgit2 has a generic one for SSH.
  if (error == GIT_ECERTIFICATE) {
    const QString rejection = takeCertificateRejection();
    if (!rejection.isEmpty()) {
      libgitMessage = rejection;
    }
  }

  setErrorContext(errorType, libgitMessage, encodeLibgitError(error, klass));

  LOG_ERROR(
      Log::ErrorId::Git,
      "Error type is {}, error is {}, error klass is {}, error message is "
      "'{}', libgit message is '{}'",
      static_cast<int>(errorType), error, klass,
      gitErrorMessage(errorType).toStdString(), libgitMessage.toStdString());
}

void setOwnError(const GitError errorType, const int error) {
  setErrorContext(errorType, gitErrorMessage(errorType),
                  encodeLibgitError(error, GIT_ERROR_INVALID));
  LOG_ERROR(Log::ErrorId::Git, "Error type is {}, error message is '{}'",
            static_cast<int>(errorType),
            gitErrorMessage(errorType).toStdString());
}

bool markConflictedFiles(git_repository *repo, const QStringList &filePaths) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (filePaths.isEmpty()) {
    LOG_DEBUG("No conflicted files to mark in the index");
    LOG_PERF(0, "Exit");
    return true;
  }

  git_index *index = nullptr;
  int error = git_repository_index(&index, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get repository index when marking conflicts: "
              "error={}",
              error);
    setError(GitError::MarkConflictedFilesFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  for (const auto &filePath : filePaths) {
    if (filePath.isEmpty()) {
      continue;
    }

    const QByteArray data = filePath.toUtf8();

    LOG_INFO("Marking conflict in: {}", filePath);

    // Add conflict entry manually
    git_index_entry ancestor = {0}, ours = {0}, theirs = {0};
    ancestor.path = data.constData();
    ancestor.mode = GIT_FILEMODE_BLOB;

    error = git_blob_create_fromworkdir(&ancestor.id, repo, ancestor.path);
    if (error) {
      setError(
          GitError::MarkConflictedFilesFailedToCreateAncestorBlobFromWorkdir,
          error);
    }

    ours.path = data.constData();
    ours.mode = GIT_FILEMODE_BLOB;

    error = git_blob_create_fromworkdir(&ours.id, repo, ours.path);
    if (error) {
      setError(GitError::MarkConflictedFilesFailedToCreateOursBlobFromWorkdir,
               error);
    }

    theirs.path = data.constData();
    theirs.mode = GIT_FILEMODE_BLOB;

    error = git_blob_create_fromworkdir(&theirs.id, repo, theirs.path);
    if (error) {
      setError(GitError::MarkConflictedFilesFailedToCreateTheirsBlobFromWorkdir,
               error);
    }

    error = git_index_conflict_add(index, &ancestor, &ours, &theirs);
    if (error) {
      setError(GitError::MarkConflictedFilesFailedToAddConflictToIndex, error);
    }
  }

  error = git_index_write(index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to write conflicted files to the index: error={}", error);
    setError(GitError::MarkConflictedFilesFailedToWriteIndex, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  LOG_DEBUG("Marked {} conflicted files in the index", filePaths.size());
  LOG_PERF(0, "Exit");
  return true;
}

bool file_has_conflict_marker_triplet(const QString &path,
                                      const QString &startMarker,
                                      const QString &middleMarker,
                                      const QString &endMarker) {
  if (path.isEmpty()) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  bool foundStart = false;
  bool foundMiddle = false;
  bool foundEnd = false;

  while (!stream.atEnd()) {
    const QString line = stream.readLine();
    foundStart |= line.startsWith(startMarker, Qt::CaseSensitive);
    foundMiddle |= line.startsWith(middleMarker, Qt::CaseSensitive);
    foundEnd |= line.startsWith(endMarker, Qt::CaseSensitive);

    if (foundStart && foundMiddle && foundEnd) {
      break;
    }
  }

  return foundStart && foundMiddle && foundEnd;
}

void git_strarray_destroy(git_strarray &arr) {
  for (int i = 0; i < arr.count; ++i) {
    free(arr.strings[i]);
  }

  free(arr.strings);

  arr.strings = nullptr;
  arr.count = 0;
}

git_strarray qstringlist_to_git_strarray(const QStringList &list) {
  git_strarray arr;
  arr.count = static_cast<size_t>(list.length());
  arr.strings = static_cast<char **>(malloc(arr.count * sizeof(char *)));

  for (size_t i = 0; i < arr.count; ++i) {
    arr.strings[i] = strdup(list[i].toStdString().c_str()); // allocates a copy
  }

  return arr;
}

} // namespace Private

static bool createProfileSignature(git_signature **sig, GitError errorId,
                                   const QString &author,
                                   const QString &email) {
  if (author.isEmpty() || email.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot create signature for initial commit: author or email "
              "is empty.");
    setError(errorId, GIT_EINVALID);
    return false;
  }

  const std::string authorStd = author.toStdString();
  const std::string emailStd = email.toStdString();

  const int error = git_signature_now(sig, authorStd.c_str(), emailStd.c_str());
  if (error) {
    setError(errorId, error);
    return false;
  }

  return true;
}

static bool createInitialCommit(git_repository *repo,
                                const QString &defaultBranchName,
                                const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  git_signature *sig = nullptr;

  if (!createProfileSignature(
          &sig, GitError::CreateInitialCommitFailedToCreateSignature, author,
          email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = 0;

  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid tree_id;

  error = git_index_write_tree(&tree_id, index);
  if (error) {
    git_index_free(index);
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToWriteIndexTree, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  git_tree *tree = nullptr;

  error = git_tree_lookup(&tree, repo, &tree_id);
  if (error) {
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToLookupTree, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid commit_id;

  error = git_commit_create_v(&commit_id, repo, HEAD, sig, sig, NULL,
                              "Initial commit", tree, 0);
  if (error) {
    git_tree_free(tree);
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToCreateCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree_free(tree);
  git_signature_free(sig);

  // HEAD points at the branch libgit2 picked (init.defaultBranch, or master
  // when it is not set). Rename it if it differs from the requested one.
  git_reference *branchRef = nullptr;

  error = git_repository_head(&branchRef, repo);
  if (error) {
    setError(GitError::CreateInitialCommitFailedToReadBranchName, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (defaultBranchName.isEmpty() ||
      QString::fromUtf8(git_reference_shorthand(branchRef)) ==
          defaultBranchName) {
    git_reference_free(branchRef);
    LOG_PERF(0, "Exit");
    return true;
  }

  git_reference *newRef = nullptr;

  error = git_branch_move(&newRef, branchRef,
                          defaultBranchName.toStdString().c_str(), false);
  if (error) {
    setError(GitError::CreateInitialCommitFailedToRenameBranch, error);
    git_reference_free(branchRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(newRef);
  git_reference_free(branchRef);

  LOG_PERF(0, "Exit");
  return true;
}

static bool createInitialCommitWithoutStaging(git_repository *repo,
                                              const QString &author,
                                              const QString &email) {
  LOG_PERF(0, "Enter");

  git_signature *sig = nullptr;
  if (!createProfileSignature(
          &sig,
          GitError::CreateInitialCommitWithoutStagingFailedToCreateSignature,
          author, email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = 0;

  // Create an empty tree builder
  git_treebuilder *builder = nullptr;
  error = git_treebuilder_new(&builder, repo, nullptr);

  if (error) {
    git_signature_free(sig);
    setError(
        GitError::CreateInitialCommitWithoutStagingFailedToCreateTreeBuilder,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write the empty tree
  git_oid tree_id;
  error = git_treebuilder_write(&tree_id, builder);
  git_treebuilder_free(builder);

  if (error) {
    git_signature_free(sig);
    setError(
        GitError::CreateInitialCommitWithoutStagingFailedToWriteTreeBuilder,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Lookup the empty tree
  git_tree *tree = nullptr;
  error = git_tree_lookup(&tree, repo, &tree_id);

  if (error) {
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitWithoutStagingFailedToLookupTree,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create the initial commit with empty tree
  git_oid commit_id;
  error = git_commit_create_v(&commit_id, repo, HEAD, sig, sig, nullptr,
                              "Initial commit", tree, 0);

  git_tree_free(tree);
  git_signature_free(sig);

  if (error) {
    setError(GitError::CreateInitialCommitWithoutStagingFailedToCreateCommit,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

static git_commit *getLastCommit(git_repository *repo, const QString &name) {
  LOG_PERF(0, "Enter");

  git_commit *commit = nullptr;
  git_oid oid_parent_commit;

  int error = git_reference_name_to_id(&oid_parent_commit, repo,
                                       name.toStdString().c_str());
  if (error) {
    setError(GitError::GetLastCommitFailedToResolveReferenceNameToId, error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  error = git_commit_lookup(&commit, repo, &oid_parent_commit);
  if (error) {
    setError(GitError::GetLastCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  LOG_PERF(0, "Exit");
  return commit;
}

#ifdef SOURCERER_GITREPO_TESTING
static std::atomic<int> gCommercialPlanOverride{-1};

namespace Testing {

void setCommercialPlanOverride(int value) { gCommercialPlanOverride = value; }

} // namespace Testing
#endif

namespace Private {

bool isCommercialPlan() {
  LOG_PERF(0, "Enter");

#ifdef SOURCERER_GITREPO_TESTING
  if (const int forced = gCommercialPlanOverride; forced >= 0) {
    LOG_PERF(0, "Exit");
    return forced == 1;
  }
#endif

  Util::CloudCredentialtorage &credentialStorage =
      Util::CloudCredentialtorage::getInstance();

  QString username;
  qint64 creationTime = -1;
  qint64 expirationTime = -1;
  if (!credentialStorage.getCredentialsInfo(username, creationTime,
                                            expirationTime)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get creation time for checking commercial plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  QString jwt;
  if (!credentialStorage.getJWT(jwt)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get JWT for checking commercial plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify JWT signature using public key before processing claims
  const QString publicKeyPem = Util::JWTDecoder::getJWTPublicKey();
  if (publicKeyPem.isEmpty()) {
    LOG_WARN("JWT public key is not configured, skipping signature "
             "verification. This should be configured for security.");
  } else {
    if (!Util::JWTDecoder::verifySignature(jwt, publicKeyPem)) {
      LOG_ERROR(Log::ErrorId::Security,
                "JWT signature verification failed for commercial plan check.");
      LOG_PERF(0, "Exit");
      return false;
    }
    LOG_DEBUG("JWT signature verification successful");
  }

  const Util::JWTDecoder::Claims claims = Util::JWTDecoder::decodeClaims(jwt);
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const bool creationTimeInTheFuture = creationTime > now;
  const bool storageExpired = expirationTime < now;
  const bool jwtExpired = claims.exp < now;

  LOG_PERF(0, "Exit");

  return !creationTimeInTheFuture && !storageExpired && !jwtExpired &&
         (claims.plan == "Pro" || claims.plan == "Business");
}

bool isSshOrGitUrl(const QString &url) {
  return url.startsWith("git@") ||
         url.startsWith("git://", Qt::CaseInsensitive) ||
         url.startsWith("ssh://", Qt::CaseInsensitive);
}

} // namespace Private

GitErrorContext getLastError() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Retrieving last error for current thread");

  LOCKGUARD(gErrorMutex);

  const auto threadId = std::this_thread::get_id();
  if (gErrorMap.contains(threadId)) {
    const auto &error = gErrorMap[threadId];
    LOG_DEBUG("Thread error type: {}, code: {}, message: '{}'",
              static_cast<int>(error.error), error.errorCode,
              error.errorMessage.toStdString());
  } else {
    LOG_DEBUG("No error found for current thread");
  }

  LOG_PERF(0, "Exit");
  return gErrorMap[std::this_thread::get_id()];
}

void initForCurrentThread() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Initializing libgit2 for current thread");

  const int numberOfCalls = git_libgit2_init();
  addSystemRootCertificates();

  if (!GitRepo::Lfs::initialize()) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to initialize Git LFS support");
  }

  LOG_DEBUG("Number of initializations is {}", numberOfCalls);
  LOG_DEBUG("libgit2 initialized successfully, total initialization count: {}",
            numberOfCalls);

  // Get timeout values from ConfigManager
  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong connectTimeout = configManager.getLong(
      ConfigManager::ConfigKey::GitRepoServerConnectTimeout);
  const qlonglong serverTimeout =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoServerTimeout);

  LOG_DEBUG("Setting server connect timeout to {} ms", connectTimeout);
  LOG_DEBUG("Setting server timeout to {} ms", serverTimeout);

  // Set SSH connection timeout (applies to initial connection establishment)
  git_libgit2_opts(GIT_OPT_SET_SERVER_CONNECT_TIMEOUT,
                   static_cast<int>(connectTimeout));

  // Set SSH timeout (for libssh2 operations and data transfer)
  git_libgit2_opts(GIT_OPT_SET_SERVER_TIMEOUT, static_cast<int>(serverTimeout));

  LOG_PERF(0, "Exit");
}

void shutdownForCurrentThread() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Shutting down libgit2 for current thread");

  GitRepo::Lfs::shutdown();

  const int numberofRemaining = git_libgit2_shutdown();

  LOG_DEBUG("Number of remaining initializations is {}", numberofRemaining);
  LOG_DEBUG("libgit2 shutdown complete, remaining init count: {}",
            numberofRemaining);

  LOG_PERF(0, "Exit");
}

Lfs::StorageStatus getLfsStorageStatus(git_repository *repo) {
  return Lfs::getStorageStatus(repo);
}

void closeRepository(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return;
  }

  forgetHistoryCursor(repo);

  git_repository_free(repo);

  LOG_PERF(0, "Exit");
}

git_repository *openRepository(const QString &path, const QString &author,
                               const QString &email) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Opening repository at path: '{}'", path.toStdString());

  git_repository *repo = nullptr;

  if (const int error =
          git_repository_open(&repo, path.toStdString().c_str())) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open repository at '{}': error={}",
              path.toStdString(), error);
    setError(GitError::OpenRepositoryFailedToOpenRepository, error);
    return nullptr;
  }

  LOG_DEBUG("Repository opened successfully at '{}'", path.toStdString());

  // Check if repository has no commits (HEAD is unborn)
  if (repo && git_repository_head_unborn(repo) == 1) {
    LOG_DEBUG("Repository has unborn HEAD, creating initial commit");
    // Create empty initial commit without staging existing files
    createInitialCommitWithoutStaging(repo, author, email);
    LOG_DEBUG("Initial commit created for new repository");
  }

  LOG_PERF(0, "Exit");
  return repo;
}

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email) {
  LOG_PERF(0, "Enter");

  git_repository_init_options opts{};

  int error = git_repository_init_options_init(
      &opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
  if (error) {
    setError(GitError::InitRepositoryFailedToInitializeRepositoryOptions,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  opts.flags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;

  git_repository *repo = nullptr;

  const QString repoPath = workingDir + "/" + repoName;
  const bool repoPathExisted = QFileInfo::exists(repoPath);

  error = git_repository_init_ext(&repo, repoPath.toStdString().c_str(), &opts);
  if (error) {
    setError(GitError::InitRepositoryFailedToInitializeRepository, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Internal error. Can't initialize git repository.");
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created = createInitialCommit(repo, defaultBranch, author, email);
  if (!created) {
    closeRepository(repo);
    // Remove the half made repository so that a retry can succeed.
    if (repoPathExisted) {
      QDir(repoPath + QStringLiteral("/.git")).removeRecursively();
    } else {
      QDir(repoPath).removeRecursively();
    }
    LOG_ERROR(Log::ErrorId::Git, "Can't create initial commit.");
    LOG_PERF(0, "Exit");
    return false;
  }

  closeRepository(repo);

  LOG_PERF(0, "Exit");

  return true;
}

namespace Private {

thread_local PushReferenceStatus gPushReferenceStatus;

int pushUpdateReferenceCallback(const char *refname, const char *status,
                                void *data) {
  Q_UNUSED(data);

  if (!status) {
    return 0;
  }

  gPushReferenceStatus.rejected = true;
  gPushReferenceStatus.refname =
      refname ? QString::fromUtf8(refname) : QString();
  gPushReferenceStatus.status = QString::fromUtf8(status);

  LOG_ERROR(Log::ErrorId::Git, "Remote rejected push update for ref '{}': {}",
            gPushReferenceStatus.refname.toStdString(),
            gPushReferenceStatus.status.toStdString());

  git_error_set_str(GIT_ERROR_NET, status);
  return GIT_EUSER;
}

int pushSidebandProgressCallback(const char *str, int len, void *data) {
  Q_UNUSED(data);

  if (!str || len <= 0) {
    return 0;
  }

  const QString message = QString::fromUtf8(str, len).trimmed();
  if (!message.isEmpty()) {
    LOG_DEBUG("Remote push output: {}", message.toStdString());
  }

  return 0;
}

int fetchProgressCallback(const git_indexer_progress *stats, void *payload) {
  if (!payload || !stats) {
    return 0;
  }

  std::tuple<CloneProgress, GitCredentialPayload> *callbackPayload =
      static_cast<std::tuple<CloneProgress, GitCredentialPayload> *>(payload);
  auto &[clonePayload, _] = *callbackPayload;

  clonePayload.completedSteps = stats->indexed_objects;
  clonePayload.totalSteps = stats->total_objects;
  clonePayload.started = true;
  clonePayload.finished = false;
  clonePayload.stage = CloneProgressStageType::Fetch;

  if (stats->total_objects > 0) {
    const int percentage =
        ((stats->indexed_objects * 1.0f) / stats->total_objects) * 100;
    if (percentage > clonePayload.lastStepPercentage) {
      clonePayload.lastStepPercentage = percentage;
      EventManager::Event progressEvent(
          EventManager::EventType::GitRepoManagerCloneRepoProgress);
      progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
          clonePayload.uuid,
          GitRepoManager::Structs::GitRepoCommandType::Unknown,
          QVariant::fromValue(clonePayload)});
      EventManager::EventManager::getInstance().send(progressEvent);
    }
  } else {
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        clonePayload.uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(clonePayload)});
    EventManager::EventManager::getInstance().send(progressEvent);
  }

  return 0;
}

void checkoutProgressCallback(const char *path, size_t completed_steps,
                              const size_t total_steps, void *payload) {
  if (!payload)
    return;

  const auto progress = static_cast<CloneProgress *>(payload);
  progress->completedSteps = completed_steps;
  progress->totalSteps = total_steps;
  progress->started = true;
  progress->finished = false;
  progress->stage = CloneProgressStageType::Checkout;

  if (completed_steps == 0)
    progress->lastStepPercentage = 0;

  if (total_steps <= 0)
    return;

  const int percentage = ((completed_steps * 1.0f) / total_steps) * 100;
  if (percentage > progress->lastStepPercentage) {
    progress->lastStepPercentage = percentage;
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        progress->uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(*progress)});
    EventManager::EventManager::getInstance().send(progressEvent);
  }
}

void finishFetchProgress(const QString &uuid, const bool error,
                         const bool cloning) {

  // Avoid race condition with progress toast creation
  std::this_thread::sleep_for(1500ms);

  CloneProgress progress;
  progress.uuid = uuid;
  progress.completedSteps = 100;
  progress.totalSteps = 100;
  progress.started = true;
  progress.finished = true;
  progress.error = error;
  progress.cloning = cloning;

  EventManager::Event progressEvent(
      EventManager::EventType::GitRepoManagerCloneRepoProgress);
  progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
      uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
      QVariant::fromValue(progress)});
  EventManager::EventManager::getInstance().send(progressEvent);
}

} // namespace Private

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting repository clone operation");
  LOG_DEBUG("  URL: '{}'", url.toStdString());
  LOG_DEBUG("  Destination directory: '{}'", destinationDir.toStdString());
  LOG_DEBUG("  UUID: '{}'", uuid.toStdString());

  const bool gitSchema = isSshOrGitUrl(url);
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::CloneRepositoryGitSshUrlRequiresCommercialPlan,
             GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Cloning git@ repositories is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QDir workingDir;
  const bool workingDirExisted = workingDir.exists(destinationDir);
  LOG_DEBUG("Destination directory already existed: {}", workingDirExisted);

  // The topmost directory the clone creates, removed again if it fails.
  QString createdRoot = QDir::cleanPath(QDir(destinationDir).absolutePath());
  while (!workingDirExisted) {
    const QString parent = QFileInfo(createdRoot).absolutePath();
    if (parent == createdRoot || QFileInfo::exists(parent)) {
      break;
    }
    createdRoot = parent;
  }

  if (!workingDir.mkpath(destinationDir)) {
    LOG_ERROR(Log::ErrorId::Git, "can't create destination dir '{}'",
              destinationDir.toStdString());
    setError(GitError::CloneRepositoryFailedToCreateDirectory, GIT_EDIRECTORY);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Destination directory created/validated successfully");

  git_repository *repo = nullptr;
  git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
  git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = true;

  GitCredentialPayload credPayload;
  credPayload.repoPath = destinationDir;
  credPayload.uuid = uuid;
  credPayload.cloning = true;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  checkout_opts.progress_payload = &progressData;
  checkout_opts.progress_cb = &checkoutProgressCallback;
  fetch_opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetch_opts.callbacks.transfer_progress = &fetchProgressCallback;
  fetch_opts.callbacks.certificate_check = &certificateCheckCallback;
  fetch_opts.callbacks.payload = &callbackPayload;
  clone_opts.checkout_opts = checkout_opts;
  clone_opts.fetch_opts = fetch_opts;

  LOG_DEBUG("Initialized clone options, starting git_clone operation");
  const int error =
      git_clone(&repo, url.toStdString().c_str(),
                destinationDir.toStdString().c_str(), &clone_opts);

  LOG_DEBUG("git_clone returned with error code: {}", error);

  finishCredentialsAcquiring(url);

  finishFetchProgress(uuid, error != 0, true);

  if (error || !repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Clone failed: error code={}, repo pointer null={}", error,
              !repo);
    invalidateCredentials(url);

    if (!workingDirExisted) {
      LOG_DEBUG("Cleaning up destination directory as clone failed");
      QDir dirToRemove(createdRoot);
      if (!dirToRemove.removeRecursively()) {
        LOG_WARN(
            "Failed to clean up destination directory after clone failure");
      }
    }

    setError(GitError::CloneRepositoryFailedToCloneRepository, error);
    LOG_ERROR(Log::ErrorId::Git, "Repository clone failed for URL: {}",
              url.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Repository cloned successfully");
  git_repository_free(repo);
  LOG_INFO("Clone operation completed successfully for URL: {}",
           url.toStdString());

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace GitRepo
