// MIT License
// Copyright (c) 2026 Sourcerer

// The work of every HgRepoManager command, run on the repository's worker
// thread (or a task thread for clone and init).

#include "HgRepoManagerWorkers.h"
#include "HgRepoManagerCredentials.h"

#include <EventManager.h>
#include <GitRepo.h>
#include <Log.h>
#include <SvnRepo.h>

#include <QDir>
#include <QFileInfo>

#define THREAD_ID "HgRepoManagerWorkers"

using namespace HgRepoManager::Structs;

namespace HgRepoManager::Workers {

namespace {

Outcome succeeded(const QVariant &value = QVariant(true)) {
  Outcome outcome;
  outcome.success = true;
  outcome.value = value;
  return outcome;
}

Outcome failed(HgRepo::HgError error, const QString &message, int code = -1) {
  Outcome outcome;
  outcome.error.message = message;
  outcome.error.error = code;
  outcome.error.hgError = error;
  return outcome;
}

// The error of the HgRepo call that just failed.
Outcome hgFailed() {
  const HgRepo::HgErrorContext error = HgRepo::getLastError();
  return failed(error.error, error.errorMessage, error.errorCode);
}

Outcome result(bool success, const QVariant &value = QVariant(true)) {
  return success ? succeeded(value) : hgFailed();
}

template <typename T> Outcome value(const T &value) {
  return succeeded(QVariant::fromValue(value));
}

Outcome notOpen() {
  return failed(HgRepo::HgError::InvalidArgument,
                QStringLiteral("The Mercurial repository is not open."));
}

Outcome gitFailed() {
  const GitRepo::GitErrorContext error = GitRepo::getLastError();
  return failed(HgRepo::HgError::SubrepoFailed,
                error.errorMessage.isEmpty()
                    ? QStringLiteral("The Git subrepo operation failed.")
                    : error.errorMessage,
                error.errorCode);
}

Outcome svnFailed() {
  const SvnRepo::SvnErrorContext error = SvnRepo::getLastError();
  return failed(HgRepo::HgError::SubrepoFailed,
                error.errorMessage.isEmpty()
                    ? QStringLiteral("The Subversion subrepo operation failed.")
                    : error.errorMessage,
                error.errorCode);
}

bool findSubrepo(HgRepo::Repository *repo, const QString &name,
                 HgRepo::HgSubrepo *subrepo) {
  const QString path = QDir::fromNativeSeparators(name);
  for (const HgRepo::HgSubrepo &candidate : HgRepo::listSubrepos(repo)) {
    if (candidate.path == path) {
      *subrepo = candidate;
      return true;
    }
  }
  return false;
}

// Git and Subversion subrepos are checked out with GitRepo and SvnRepo and
// then registered in .hgsub.
Outcome addGitSubrepo(Session &session, const HgRepoAddSubmoduleCommand &c) {
  const QString destination =
      HgRepo::repositoryPath(session.repo) + '/' + c.name;
  if (!GitRepo::cloneRepository(c.url, destination, c.uuid)) {
    return gitFailed();
  }

  if (!c.branch.isEmpty()) {
    git_repository *gitRepo =
        GitRepo::openRepository(destination, QString(), QString());
    if (!gitRepo) {
      return gitFailed();
    }
    const bool checkedOut =
        GitRepo::checkoutBranch(gitRepo, QStringLiteral("origin/") + c.branch,
                                GitRepo::BranchType::Remote);
    const Outcome failure = checkedOut ? Outcome() : gitFailed();
    GitRepo::closeRepository(gitRepo);
    if (!checkedOut) {
      return failure;
    }
  }

  return result(HgRepo::registerSubrepo(session.repo, c.name, c.url,
                                        HgRepo::HgSubrepoKind::Git));
}

Outcome addSvnSubrepo(Session &session, const HgRepoAddSubmoduleCommand &c) {
  QString url = c.url;
  if (!c.branch.isEmpty()) {
    while (url.endsWith('/')) {
      url.chop(1);
    }
    url += '/' + c.branch;
  }

  const QString destination =
      HgRepo::repositoryPath(session.repo) + '/' + c.name;
  if (!SvnRepo::cloneRepository(url, destination, c.uuid)) {
    return svnFailed();
  }
  return result(HgRepo::registerSubrepo(session.repo, c.name, url,
                                        HgRepo::HgSubrepoKind::Subversion));
}

Outcome updateGitSubrepo(Session &session, const HgRepo::HgSubrepo &subrepo) {
  const QString path =
      HgRepo::repositoryPath(session.repo) + '/' + subrepo.path;
  if (!QFileInfo::exists(path + QStringLiteral("/.git")) &&
      !GitRepo::cloneRepository(subrepo.source, path, session.uuid)) {
    return gitFailed();
  }

  git_repository *gitRepo = GitRepo::openRepository(path, QString(), QString());
  if (!gitRepo) {
    return gitFailed();
  }
  // The pinned commit may not have been fetched yet.
  GitRepo::fetchAll(gitRepo);
  const bool updated = GitRepo::checkoutCommit(gitRepo, subrepo.pinnedState);
  const Outcome failure = updated ? Outcome() : gitFailed();
  GitRepo::closeRepository(gitRepo);
  return updated ? succeeded() : failure;
}

Outcome updateSvnSubrepo(Session &session, const HgRepo::HgSubrepo &subrepo) {
  const QString path =
      HgRepo::repositoryPath(session.repo) + '/' + subrepo.path;
  if (!QFileInfo::exists(path + QStringLiteral("/.svn")) &&
      !SvnRepo::cloneRepository(subrepo.source, path, session.uuid)) {
    return svnFailed();
  }

  const QString workingCopy = SvnRepo::openRepository(path);
  if (workingCopy.isEmpty()) {
    return svnFailed();
  }
  const bool updated =
      SvnRepo::checkoutCommit(workingCopy, subrepo.pinnedState);
  const Outcome failure = updated ? Outcome() : svnFailed();
  SvnRepo::closeRepository(workingCopy);
  return updated ? succeeded() : failure;
}

bool runGitLabFlow(HgRepo::Repository *repo, const HgRepoGitFlowCommand &c,
                   bool *conflicted) {
  switch (c.gitLabFlowAction) {
  case GitRepo::GitLabFlowAction::StartFeature:
    return HgRepo::startGitLabFlowFeature(repo, c.branchName, c.mainBranchName);
  case GitRepo::GitLabFlowAction::FinishFeature:
    return HgRepo::finishGitLabFlowFeature(repo, c.branchName, c.mainBranchName,
                                           c.author, c.email);
  case GitRepo::GitLabFlowAction::Promote:
    return HgRepo::promoteGitLabFlowEnvironment(
        repo, c.environmentChain, c.branchName, c.author, c.email);
  case GitRepo::GitLabFlowAction::StartRelease:
    return HgRepo::startGitLabFlowRelease(repo, c.branchName, c.mainBranchName);
  case GitRepo::GitLabFlowAction::CherryPickToRelease:
    return HgRepo::cherryPickToGitLabFlowRelease(repo, c.commit, c.branchName,
                                                 c.author, c.email, conflicted);
  default:
    LOG_ERROR(Log::ErrorId::Git, "Unknown GitLab Flow action {}",
              static_cast<int>(c.gitLabFlowAction));
    return false;
  }
}

bool runGitHubFlow(HgRepo::Repository *repo, const HgRepoGitFlowCommand &c) {
  switch (c.gitHubFlowAction) {
  case GitRepo::GitHubFlowAction::StartBranch:
    return HgRepo::startGitHubFlowBranch(repo, c.branchName, c.mainBranchName);
  case GitRepo::GitHubFlowAction::UpdateBranch:
    return HgRepo::updateGitHubFlowBranch(repo, c.branchName, c.mainBranchName,
                                          c.author, c.email);
  case GitRepo::GitHubFlowAction::FinishBranch:
    return HgRepo::finishGitHubFlowBranch(repo, c.branchName, c.mainBranchName,
                                          c.author, c.email);
  default:
    LOG_ERROR(Log::ErrorId::Git, "Unknown GitHub Flow action {}",
              static_cast<int>(c.gitHubFlowAction));
    return false;
  }
}

bool runGitFlowNext(HgRepo::Repository *repo, const HgRepoGitFlowCommand &c) {
  switch (c.gitFlowNextAction) {
  case GitRepo::GitFlowNextAction::Init:
    return HgRepo::initGitFlowNext(repo, c.mainBranchName,
                                   c.developmentBranchName);
  case GitRepo::GitFlowNextAction::Start:
    return HgRepo::startGitFlowNextBranch(repo, c.topicType, c.branchName);
  case GitRepo::GitFlowNextAction::Finish:
    return HgRepo::finishGitFlowNextBranch(repo, c.branchName, c.author,
                                           c.email);
  case GitRepo::GitFlowNextAction::Update:
    return HgRepo::updateGitFlowNextBranch(repo, c.branchName, c.author,
                                           c.email);
  case GitRepo::GitFlowNextAction::Apply:
    return HgRepo::applyGitFlowNextWorkflow(repo, c.customWorkflow);
  default:
    LOG_ERROR(Log::ErrorId::Git, "Unknown git-flow-next action {}",
              static_cast<int>(c.gitFlowNextAction));
    return false;
  }
}

// Repository lifetime.

Outcome run(Session &session, const HgRepoOpenRepoCommand &c) {
  if (!HgRepo::checkExecutable()) {
    return hgFailed();
  }
  session.repo = HgRepo::openRepository(c.path, c.author, c.email);
  return session.repo ? value(HgRepo::repositoryPath(session.repo))
                      : hgFailed();
}

Outcome run(Session &session, const HgRepoCloseRepoCommand &) {
  HgRepo::closeRepository(session.repo);
  session.repo = nullptr;
  return succeeded();
}

Outcome run(Session &, const HgRepoCloneRepoCommand &c) {
  const QString uuid = c.uuid;
  return result(
      HgRepo::cloneRepository(
          c.url, c.destinationDir, uuid,
          [uuid](const GitRepo::CloneProgress &progress) {
            EventManager::Event event(
                EventManager::EventType::HgRepoManagerCloneRepoProgress, uuid);
            event.addData(HgRepoResult{uuid, HgRepoCommandType::CloneRepo,
                                       QVariant::fromValue(progress)});
            EventManager::EventManager::getInstance().send(event);
          }),
      QVariant(c.destinationDir));
}

Outcome run(Session &, const HgRepoInitRepoCommand &c) {
  if (!HgRepo::checkExecutable()) {
    return hgFailed();
  }
  return result(HgRepo::initRepository(c.repoName, c.workingDir,
                                       c.defaultBranch, c.author, c.email),
                QVariant(QDir::cleanPath(c.workingDir + '/' + c.repoName)));
}

// Working directory.

Outcome run(Session &s, const HgRepoStatusCommand &) {
  const GitRepo::GitStatus status = HgRepo::getStatus(s.repo, *s.quit);
  return status.path.isEmpty() ? hgFailed() : value(status);
}

Outcome run(Session &s, const HgRepoStageFilesCommand &c) {
  return result(HgRepo::stageFiles(s.repo, c.files));
}

Outcome run(Session &s, const HgRepoUnstageFilesCommand &c) {
  return result(HgRepo::unstageFiles(s.repo, c.files));
}

Outcome run(Session &s, const HgRepoDiscardFilesCommand &c) {
  return result(HgRepo::discardFiles(s.repo, c.files));
}

Outcome run(Session &s, const HgRepoMarkResolvedCommand &c) {
  return result(HgRepo::markResolved(s.repo, c.files));
}

Outcome run(Session &s, const HgRepoCommitStagedCommand &c) {
  return result(HgRepo::commitStaged(s.repo, c.summary, c.message, c.author,
                                     c.email, c.cleanup, c.signoff,
                                     c.signCommit));
}

Outcome run(Session &s, const HgRepoAmendCommand &c) {
  return result(HgRepo::amend(s.repo, c.summary, c.message, c.author, c.email,
                              c.signoff, c.signCommit));
}

Outcome run(Session &s, const HgRepoApplyPatchCommand &c) {
  return result(HgRepo::applyPatch(s.repo, c.patchContent));
}

Outcome run(Session &s, const HgRepoListConflictsCommand &c) {
  return value(HgRepo::listConflictsForAFile(s.repo, c.filePath));
}

Outcome run(Session &s, const HgRepoGetIsMergeInProgressCommand &) {
  return value(HgRepo::getIsMergeInProgress(s.repo));
}

Outcome run(Session &s, const HgRepoAbortMergeCommand &) {
  return result(HgRepo::abortMerge(s.repo));
}

// Diffs and blame.

Outcome run(Session &s, const HgRepoDiffFilesCommand &c) {
  const GitRepo::FilesDiff diff = HgRepo::diffFiles(s.repo, c.files);
  return diff.valid ? value(diff) : hgFailed();
}

Outcome run(Session &s, const HgRepoDiffFilesFromCommitCommand &c) {
  const GitRepo::FilesDiff diff =
      HgRepo::diffFilesFromCommit(s.repo, c.files, c.commit);
  return diff.valid ? value(diff) : hgFailed();
}

Outcome run(Session &s, const HgRepoBlameCommand &c) {
  const GitRepo::BlameReport report = HgRepo::blame(s.repo, c.files);
  return report.valid ? value(report) : hgFailed();
}

Outcome run(Session &s, const HgRepoGetFileSizeFromCommitCommand &c) {
  const qint64 size =
      HgRepo::getFileSizeFromCommit(s.repo, c.filePath, c.commit);
  return size >= 0 ? value(size) : hgFailed();
}

Outcome run(Session &s, const HgRepoCreatePatchFromCommitCommand &c) {
  const QString patch = HgRepo::createPatchFromCommit(s.repo, c.commit);
  return patch.isEmpty() ? hgFailed() : value(patch);
}

// History.

Outcome run(Session &s, const HgRepoGetBranchHistoryCommand &c) {
  const GitRepo::GitHistory history = HgRepo::getBranchHistory(
      s.repo, c.branchName, c.branchType, c.page, *s.quit);
  return history.valid ? value(history) : hgFailed();
}

Outcome run(Session &s, const HgRepoGetCommitStatusCommand &c) {
  const GitRepo::GitCommitStatus status =
      HgRepo::getCommitStatus(s.repo, c.commitOid);
  return status.commit.isEmpty() ? hgFailed() : value(status);
}

Outcome run(Session &s, const HgRepoGetCommitBranchesCommand &c) {
  return value(HgRepo::getCommitBranches(s.repo, c.commitOid, *s.quit));
}

Outcome run(Session &s, const HgRepoGetCommitTagsCommand &c) {
  return value(HgRepo::getCommitTags(s.repo, c.commitOid, *s.quit));
}

Outcome run(Session &s, const HgRepoGetCommitHeadsCommand &c) {
  return value(HgRepo::getCommitHeads(s.repo, c.commitOid, *s.quit));
}

Outcome run(Session &s, const HgRepoGetCommitIsHeadCommand &c) {
  return value(HgRepo::getCommitIsHead(s.repo, c.commitOid, *s.quit));
}

// Branches and bookmarks.

Outcome run(Session &s, const HgRepoGetActiveBranchCommand &c) {
  return value(HgRepo::getActiveBranch(s.repo, c.branchType, *s.quit));
}

Outcome run(Session &s, const HgRepoListLocalBranchesCommand &c) {
  return value(
      HgRepo::listLocalBranchesWithKind(s.repo, c.includeClosed, *s.quit));
}

Outcome run(Session &s, const HgRepoCheckoutBranchCommand &c) {
  return result(HgRepo::checkoutBranch(s.repo, c.branchName, c.branchType));
}

Outcome run(Session &s, const HgRepoCheckoutCommitCommand &c) {
  return result(HgRepo::checkoutCommit(s.repo, c.commit));
}

Outcome run(Session &s, const HgRepoCreateBranchFromCommitCommand &c) {
  return result(
      c.kind == HgRepo::HgBranchKind::NamedBranch
          ? HgRepo::createNamedBranchFromCommit(s.repo, c.commit, c.branchName)
          : HgRepo::createBranchFromCommit(s.repo, c.commit, c.branchName));
}

Outcome run(Session &s, const HgRepoCloseNamedBranchCommand &c) {
  return result(
      HgRepo::closeNamedBranch(s.repo, c.branchName, c.author, c.email));
}

Outcome run(Session &s, const HgRepoRenameBranchCommand &c) {
  return result(HgRepo::renameBranch(s.repo, c.oldBranchName, c.newBranchName));
}

Outcome run(Session &s, const HgRepoDeleteBranchCommand &c) {
  return result(HgRepo::deleteBranch(s.repo, c.branchName));
}

Outcome run(Session &s, const HgRepoDeleteRemoteBranchCommand &c) {
  return result(HgRepo::deleteRemoteBranch(s.repo, c.branchName));
}

// Tags.

Outcome run(Session &s, const HgRepoListTagsCommand &) {
  HgRepoTagsResult tags;
  tags.tags = HgRepo::listTags(s.repo);
  tags.localTags = HgRepo::listLocalTags(s.repo);
  return value(tags);
}

Outcome run(Session &s, const HgRepoCreateTagFromCommitCommand &c) {
  return result(HgRepo::createTagFromCommit(
      s.repo, c.commit, c.tagName, c.author, c.email, c.message, c.local));
}

Outcome run(Session &s, const HgRepoDeleteTagCommand &c) {
  return result(HgRepo::deleteTag(s.repo, c.tagName));
}

Outcome run(Session &s, const HgRepoRenameTagCommand &c) {
  return result(HgRepo::renameTag(s.repo, c.oldTagName, c.newTagName));
}

Outcome run(Session &s, const HgRepoPushTagCommand &c) {
  return result(HgRepo::pushTag(s.repo, c.tagName));
}

Outcome run(Session &s, const HgRepoPushAllTagsCommand &) {
  return result(HgRepo::pushAllTags(s.repo));
}

Outcome run(Session &s, const HgRepoDeleteRemoteTagCommand &c) {
  return result(HgRepo::deleteRemoteTag(s.repo, c.tagName));
}

// Merging and history editing.

// Like GitRepoManager, a merge, rebase or squash stopped by conflicts answers
// false rather than an error; the conflicts show in the status.
Outcome conflictsAsFalse(bool success) {
  if (!success &&
      HgRepo::getLastError().error == HgRepo::HgError::MergeConflicts) {
    return succeeded(QVariant(false));
  }
  return result(success);
}

Outcome run(Session &s, const HgRepoMergeCommand &c) {
  return conflictsAsFalse(HgRepo::merge(
      s.repo, c.sourceBranch, c.destinationBranch, c.author, c.email));
}

Outcome run(Session &s, const HgRepoRebaseCommand &c) {
  return conflictsAsFalse(HgRepo::rebaseBranches(s.repo, c.sourceBranch,
                                                 c.destinationBranch, c.author,
                                                 c.email, c.signCommit));
}

Outcome run(Session &s, const HgRepoSquashCommand &c) {
  return conflictsAsFalse(HgRepo::squashBranches(s.repo, c.sourceBranch,
                                                 c.destinationBranch, c.author,
                                                 c.email, c.signCommit));
}

Outcome run(Session &s, const HgRepoGetIsRebaseInProgressCommand &) {
  return value(HgRepo::getIsRebaseInProgress(s.repo));
}

Outcome run(Session &s, const HgRepoAbortRebaseCommand &) {
  return result(HgRepo::abortRebase(s.repo));
}

Outcome run(Session &s, const HgRepoCommitRebaseCommand &c) {
  return result(HgRepo::commitRebase(s.repo, c.summary, c.message, c.author,
                                     c.email, c.cleanup, c.signoff,
                                     c.signCommit));
}

Outcome run(Session &s, const HgRepoCherryPickCommand &c) {
  return result(HgRepo::cherryPickCommit(s.repo, c.commit, c.author, c.email));
}

Outcome run(Session &s, const HgRepoRevertCommand &c) {
  return result(
      HgRepo::revertCommit(s.repo, c.commit, c.author, c.email, c.message));
}

Outcome run(Session &s, const HgRepoStripCommand &c) {
  return result(HgRepo::strip(s.repo, c.commit, c.keepWorkingCopy, c.backup));
}

Outcome run(Session &s, const HgRepoResetCommand &c) {
  return result(HgRepo::resetToCommit(s.repo, c.commit, c.resetType));
}

Outcome run(Session &s, const HgRepoBisectCommand &c) {
  GitRepo::BisectStatus status;
  return HgRepo::bisect(s.repo, c.action, c.commit, status) ? value(status)
                                                            : hgFailed();
}

// Shelves.

Outcome run(Session &s, const HgRepoStashCommand &c) {
  return result(HgRepo::stash(s.repo, c.note));
}

Outcome run(Session &s, const HgRepoListStashesCommand &) {
  return value(HgRepo::listStashes(s.repo, *s.quit));
}

// {"success", "hasConflicts"}, as GitRepoManager answers.
Outcome run(Session &s, const HgRepoApplyStashCommand &c) {
  const bool applied = HgRepo::applyStash(s.repo, c.index);
  const bool conflicts = !applied && HgRepo::getLastError().error ==
                                         HgRepo::HgError::MergeConflicts;
  if (!applied && !conflicts) {
    return hgFailed();
  }

  QMap<QString, QVariant> resultData;
  resultData["success"] = true;
  resultData["hasConflicts"] = conflicts;
  return succeeded(QVariant(resultData));
}

Outcome run(Session &s, const HgRepoDeleteStashCommand &c) {
  return result(HgRepo::deleteStash(s.repo, c.index));
}

Outcome run(Session &s, const HgRepoDeleteAllStashesCommand &) {
  return result(HgRepo::deleteAllStashes(s.repo));
}

Outcome run(Session &s, const HgRepoGetStashStatusCommand &c) {
  const GitRepo::GitCommitStatus status =
      HgRepo::getStashStatus(s.repo, c.index);
  return status.commit.isEmpty() ? hgFailed() : value(status);
}

// Remotes and synchronization.

Outcome run(Session &s, const HgRepoListRemotesCommand &) {
  return value(HgRepo::listRemotes(s.repo));
}

Outcome run(Session &s, const HgRepoAddRemoteCommand &c) {
  return result(HgRepo::addRemote(s.repo, c.remoteName, c.fetchUrl, c.pushUrl));
}

Outcome run(Session &s, const HgRepoRemoveRemoteCommand &c) {
  return result(HgRepo::removeRemote(s.repo, c.remoteName));
}

Outcome run(Session &s, const HgRepoUpdateRemoteCommand &c) {
  return result(HgRepo::updateRemote(s.repo, c.remoteName, c.newRemoteName,
                                     c.fetchUrl, c.pushUrl));
}

Outcome run(Session &s, const HgRepoSetDefaultRemoteCommand &c) {
  return result(HgRepo::setDefaultRemote(s.repo, c.remoteName));
}

Outcome run(Session &s, const HgRepoAddTargetCommand &c) {
  return result(HgRepo::addTarget(s.repo, c.remoteName, c.targetName,
                                  c.fetchUrl, c.pushUrl));
}

Outcome run(Session &s, const HgRepoEditTargetCommand &c) {
  return result(HgRepo::editTarget(s.repo, c.remoteName, c.targetName,
                                   c.newFetchUrl, c.newPushUrl));
}

Outcome run(Session &s, const HgRepoDeleteTargetCommand &c) {
  return result(HgRepo::deleteTarget(s.repo, c.remoteName, c.targetName));
}

Outcome run(Session &s, const HgRepoSetDefaultTargetCommand &c) {
  return result(HgRepo::setDefaultTarget(s.repo, c.remoteName, c.targetName));
}

Outcome run(Session &s, const HgRepoPullCommand &c) {
  return result(HgRepo::pull(s.repo, c.branchName, c.rebase));
}

Outcome run(Session &s, const HgRepoPushCommand &c) {
  return result(HgRepo::push(s.repo, c.branchName, c.force));
}

Outcome run(Session &s, const HgRepoFetchAllCommand &) {
  return result(HgRepo::fetchAll(s.repo));
}

Outcome run(Session &s, const HgRepoFetchRemoteCommand &c) {
  return result(HgRepo::fetchRemote(s.repo, c.remoteName));
}

Outcome run(Session &s, const HgRepoListRemoteBranchesCommand &) {
  return value(HgRepo::listRemoteBranches(s.repo, *s.quit));
}

Outcome run(Session &s, const HgRepoListRemoteBranchesWithStatusCommand &) {
  return value(HgRepo::listRemoteBranchesWithStatus(s.repo, *s.quit));
}

Outcome run(Session &s, const HgRepoSetUpstreamCommand &c) {
  return result(HgRepo::setUpstream(s.repo, c.remoteName, c.remoteBranchName,
                                    c.localBranchName));
}

Outcome run(Session &s, const HgRepoGetUpstreamCommand &c) {
  const auto [configured, remote, remoteBranch] =
      HgRepo::getUpstream(s.repo, c.localBranchName);
  return value(HgRepoUpstreamResult{configured, remote, remoteBranch});
}

// Subrepos.

Outcome run(Session &s, const HgRepoListSubmodulesCommand &) {
  return value(HgRepo::listSubmodules(s.repo));
}

Outcome run(Session &s, const HgRepoListSubmodulesWithStatusCommand &) {
  HgRepoSubmodulesResult submodules;
  submodules.infos = HgRepo::listSubmodulesWithStatus(s.repo, *s.quit);
  submodules.subrepos = HgRepo::listSubrepos(s.repo);

  // HgRepo can't read Subversion checkouts; SvnRepo can.
  const QString root = HgRepo::repositoryPath(s.repo);
  for (int i = 0; i < submodules.subrepos.size() && i < submodules.infos.size();
       ++i) {
    const HgRepo::HgSubrepo &subrepo = submodules.subrepos[i];
    GitRepo::SubmoduleInfo &info = submodules.infos[i];
    if (subrepo.kind != HgRepo::HgSubrepoKind::Subversion ||
        info.statusInfo.status == GitRepo::SubmoduleStatusType::Error) {
      continue;
    }
    info.statusInfo.status =
        SvnRepo::getCommitIsHead(root + '/' + subrepo.path, subrepo.pinnedState,
                                 *s.quit)
            ? GitRepo::SubmoduleStatusType::UpToDate
            : GitRepo::SubmoduleStatusType::Outdated;
  }
  return value(submodules);
}

Outcome run(Session &s, const HgRepoGetSubmodulePathCommand &c) {
  const QString path = HgRepo::getSubmodulePath(s.repo, c.submoduleName);
  return path.isEmpty() ? hgFailed() : value(path);
}

Outcome run(Session &s, const HgRepoAddSubmoduleCommand &c) {
  switch (c.kind) {
  case HgRepo::HgSubrepoKind::Git:
    return addGitSubrepo(s, c);
  case HgRepo::HgSubrepoKind::Subversion:
    return addSvnSubrepo(s, c);
  default:
    return result(
        HgRepo::addSubmodule(s.repo, c.url, c.name, c.branch, c.uuid));
  }
}

Outcome run(Session &s, const HgRepoDeleteSubmoduleCommand &c) {
  return result(
      HgRepo::deleteSubmodule(s.repo, c.submoduleName, c.submodulePath));
}

Outcome run(Session &s, const HgRepoSubmoduleUpdateCommand &c) {
  HgRepo::HgSubrepo subrepo;
  if (!findSubrepo(s.repo, c.submoduleName, &subrepo)) {
    return failed(HgRepo::HgError::SubrepoFailed,
                  QStringLiteral("%1 is not a subrepo.").arg(c.submoduleName));
  }

  if (subrepo.kind != HgRepo::HgSubrepoKind::Mercurial &&
      subrepo.pinnedState.isEmpty()) {
    return failed(HgRepo::HgError::SubrepoFailed,
                  QStringLiteral("%1 has no pinned revision yet, commit it "
                                 "first.")
                      .arg(subrepo.path));
  }

  switch (subrepo.kind) {
  case HgRepo::HgSubrepoKind::Git:
    return updateGitSubrepo(s, subrepo);
  case HgRepo::HgSubrepoKind::Subversion:
    return updateSvnSubrepo(s, subrepo);
  default:
    return result(HgRepo::updateSubmodule(s.repo, c.submoduleName));
  }
}

// Worktrees.

Outcome run(Session &s, const HgRepoListWorktreesCommand &) {
  return value(HgRepo::listWorktrees(s.repo));
}

Outcome run(Session &s, const HgRepoAddWorktreeCommand &c) {
  return result(HgRepo::addWorktree(s.repo, c.name, c.path, c.branch,
                                    c.createBranch, c.baseBranch, c.kind));
}

Outcome run(Session &s, const HgRepoRemoveWorktreeCommand &c) {
  return result(HgRepo::removeWorktree(s.repo, c.name, c.force));
}

Outcome run(Session &s, const HgRepoLockWorktreeCommand &c) {
  return result(HgRepo::lockWorktree(s.repo, c.name, c.reason));
}

Outcome run(Session &s, const HgRepoUnlockWorktreeCommand &c) {
  return result(HgRepo::unlockWorktree(s.repo, c.name));
}

// LFS.

Outcome run(Session &s, const HgRepoEnableLfsCommand &) {
  return result(HgRepo::enableLfs(s.repo));
}

Outcome run(Session &s, const HgRepoAddFilesToLfsCommand &c) {
  return result(HgRepo::addFilesToLfs(s.repo, c.files));
}

Outcome run(Session &s, const HgRepoRemoveFilesFromLfsCommand &c) {
  return result(HgRepo::removeFilesFromLfs(s.repo, c.files));
}

Outcome run(Session &s, const HgRepoListLfsFilesCommand &) {
  return value(HgRepo::listLfsFiles(s.repo));
}

Outcome run(Session &s, const HgRepoGetLfsStorageStatusCommand &) {
  return value(HgRepo::getLfsStorageStatus(s.repo));
}

// Workflows.

Outcome run(Session &s, const HgRepoGitFlowCommand &c) {
  HgRepoGitFlowResult flow;
  bool success = false;
  switch (c.workflow) {
  case GitRepo::WorkflowType::GitLabFlow:
    success = runGitLabFlow(s.repo, c, &flow.conflicted);
    break;
  case GitRepo::WorkflowType::GitHubFlow:
    success = runGitHubFlow(s.repo, c);
    break;
  case GitRepo::WorkflowType::GitFlowNext:
    success = runGitFlowNext(s.repo, c);
    break;
  default:
    success =
        c.start ? HgRepo::startGitFlowBranch(s.repo, c.branchType, c.branchName,
                                             c.developmentBranchName)
                : HgRepo::finishGitFlowBranch(s.repo, c.branchType,
                                              c.branchName, c.author, c.email,
                                              c.developmentBranchName);
    break;
  }
  return success ? value(flow) : hgFailed();
}

Outcome run(Session &s, const HgRepoWorkflowCommand &c) {
  if (c.write && !HgRepo::setWorkflowType(s.repo, c.workflow)) {
    return hgFailed();
  }
  if (c.writeFlowBranchKind &&
      !HgRepo::setFlowBranchKind(s.repo, c.flowBranchKind)) {
    return hgFailed();
  }

  HgRepoWorkflowResult workflow;
  workflow.state.type = HgRepo::getWorkflowType(s.repo);
  // Read for every workflow, so it can be saved as a custom workflow from
  // any of them, like GitRepoManager does.
  HgRepo::readGitFlowNextConfig(s.repo, &workflow.state.gitFlowNext);
  if (workflow.state.type == GitRepo::WorkflowType::GitFlowNext) {
    HgRepo::getGitFlowNextWorkflowReference(
        s.repo, &workflow.state.customWorkflowId,
        &workflow.state.customWorkflowRevision);
  }
  workflow.flowBranchKind = HgRepo::getFlowBranchKind(s.repo);
  return value(workflow);
}

Outcome run(Session &, const HgRepoQuitCommand &) {
  return succeeded();
}

} // namespace

Outcome execute(Session &session, const HgRepoCommand &command) {
  LOG_PERF(0, "Enter");

  const bool needsRepository = command.type != HgRepoCommandType::OpenRepo &&
                               command.type != HgRepoCommandType::CloneRepo &&
                               command.type != HgRepoCommandType::InitRepo &&
                               command.type != HgRepoCommandType::Quit;
  if (needsRepository && !session.repo) {
    LOG_PERF(0, "Exit");
    return notOpen();
  }

  Outcome outcome;
  switch (command.type) {
#define HG_REPO_EXECUTE(X, name)                                               \
  case HgRepoCommandType::name:                                                \
    outcome =                                                                  \
        run(session, static_cast<const HgRepo##name##Command &>(command));     \
    break;
    HG_REPO_MANAGER_COMMAND_LIST(HG_REPO_EXECUTE, unused)
#undef HG_REPO_EXECUTE
  case HgRepoCommandType::Quit:
    outcome = succeeded();
    break;
  default:
    outcome = failed(HgRepo::HgError::InvalidArgument,
                     QStringLiteral("Unknown command %1")
                         .arg(static_cast<int>(command.type)));
    break;
  }

  // No credentials overlay outlives the command that raised it.
  GitRepo::finishPendingCredentialPrompts();
  SvnRepo::finishPendingCredentialPrompts();

  outcome.error.uuid = command.uuid;
  LOG_PERF(0, "Exit");
  return outcome;
}

EventManager::EventType resultEventType(HgRepoCommandType type) {
  switch (type) {
#define HG_REPO_RESULT_EVENT(X, name)                                          \
  case HgRepoCommandType::name:                                                \
    return EventManager::EventType::HgRepoManager##name##Result;
    HG_REPO_MANAGER_COMMAND_LIST(HG_REPO_RESULT_EVENT, unused)
#undef HG_REPO_RESULT_EVENT
  default:
    return EventManager::EventType::Null;
  }
}

EventManager::EventType errorEventType(HgRepoCommandType type) {
  switch (type) {
#define HG_REPO_ERROR_EVENT(X, name)                                           \
  case HgRepoCommandType::name:                                                \
    return EventManager::EventType::HgRepoManager##name##Error;
    HG_REPO_MANAGER_COMMAND_LIST(HG_REPO_ERROR_EVENT, unused)
#undef HG_REPO_ERROR_EVENT
  default:
    return EventManager::EventType::Null;
  }
}

void reply(const HgRepoCommand &command, const Outcome &outcome) {
  if (command.type == HgRepoCommandType::Quit) {
    return;
  }

  if (outcome.success) {
    EventManager::Event event(resultEventType(command.type), command.syncUuid);
    event.addData(HgRepoResult{command.uuid, command.type, outcome.value});
    EventManager::EventManager::getInstance().send(event);
  } else {
    HgRepoError error = outcome.error;
    error.uuid = command.uuid;
    if (error.message.isEmpty()) {
      error.message = QStringLiteral("The Mercurial operation failed.");
    }
    LOG_WARN("Mercurial command {} failed: {}", static_cast<int>(command.type),
             error.message.toStdString());
    EventManager::Event event(errorEventType(command.type), command.syncUuid);
    event.addData(error);
    EventManager::EventManager::getInstance().send(event);
  }
}

void initThread(const bool *quit) {
  // HTTP credentials are asked through the Git credentials overlay.
  thread_local EventCredentialsProvider credentialsProvider;

  HgRepo::initForCurrentThread();
  HgRepo::setCancelFlag(quit);
  HgRepo::setCredentialsProvider(&credentialsProvider);
  GitRepo::initForCurrentThread();
  SvnRepo::initForCurrentThread();
}

void shutdownThread() {
  SvnRepo::shutdownForCurrentThread();
  GitRepo::shutdownForCurrentThread();
  HgRepo::setCredentialsProvider(nullptr);
  HgRepo::shutdownForCurrentThread();
}

} // namespace HgRepoManager::Workers
