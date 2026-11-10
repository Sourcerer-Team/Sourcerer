// MIT License
// Copyright (c) 2026 Sourcerer

// End to end tests of the GitRepoManager component: requests are sent as
// events and answered by the component's worker threads, as RepoContainer
// does. See docs/GIT_TEST_PLAN.md, section 4.

#include "FakeCredentialsOverlay.h"
#include "GitHttpServer.h"
#include "GitTestHelpers.h"

#include <GitCredentialStorage.h>
#include <GitRepoManager.h>
#include <GitRepoManagerStructs.h>

#include <EventManager.h>

#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <algorithm>
#include <memory>

using namespace GitTest;
using namespace GitRepoManager::Structs;
using EventManager::EventType;

namespace {

QString newUuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

struct Reply {
  bool answered = false;
  bool success = false;
  QVariant value;
  QString error;
  int errorCode = -1;
  GitRepo::GitError gitError = GitRepo::GitError::Unknown;
};

// Every request type the component handles (GitRepoManager<Name>), except
// CloseRepo, which has its own tests.
const QStringList kRequests = {
    "AbortMerge", "AbortRebase", "AddRemote", "AddSubmodule", "AddTarget",
    "AddWorktree", "Amend", "ApplyPatch", "ApplyStash", "Bisect", "Blame",
    "CheckoutBranch", "CheckoutCommit", "CherryPick", "CloneRepo",
    "CommitRebase", "CommitStaged", "CreateBranchFromCommit",
    "CreatePatchFromCommit", "CreateTagFromCommit", "DeleteAllStashes",
    "DeleteBranch", "DeleteRemoteBranch", "DeleteRemoteTag", "DeleteStash",
    "DeleteSubmodule", "DeleteTag", "DeleteTarget", "DiffFiles",
    "DiffFilesFromCommit", "DiscardFiles", "EditTarget", "FetchAll",
    "FetchRemote", "GetActiveBranch", "GetBranchHistory", "GetCommitBranches",
    "GetCommitHeads", "GetCommitIsHead", "GetCommitStatus", "GetCommitTags",
    "GetFileSizeFromCommit", "GetIsMergeInProgress", "GetIsRebaseInProgress",
    "GetLfsStorageStatus", "GetStashStatus", "GetSubmodulePath", "GetUpstream",
    "GitFlow", "InitRepo", "ListConflicts", "ListLocalBranches",
    "ListRemoteBranches", "ListRemoteBranchesWithStatus", "ListRemotes",
    "ListStashes", "ListSubmodules", "ListSubmodulesWithStatus", "ListTags",
    "ListWorktrees", "LockWorktree", "Merge", "OpenRepo", "Pull", "Push",
    "PushAllTags", "PushTag", "Rebase", "RemoveRemote", "RemoveWorktree",
    "RenameBranch", "RenameTag", "Reset", "Revert", "SetDefaultRemote",
    "SetDefaultTarget", "SetUpstream", "Squash", "Stash", "StageFiles",
    "Status", "SubmoduleUpdate", "UnlockWorktree", "UnstageFiles",
    "UpdateRemote", "Workflow"};

} // namespace

class GitRepoManagerTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void cleanup();

  void requestsBeforeOpenFail();
  void initOpenCommitAndClose();
  void reopenAfterClose();
  void openFailures();
  void rejectedRequestsKeepTheOpenRepository_data();
  void rejectedRequestsKeepTheOpenRepository();
  void twoContainersAreIndependent();
  void everyRequestIsAnswered_data();
  void everyRequestIsAnswered();
  void errorsCarryTheGitError();
  void cloneReportsProgress();
  void fetchPullPushReportProgress();
  void closeAnswersQueuedRequests();
  void httpCredentials();
  void credentialPromptsFinishOncePerUrl();
  void closeDuringClone();
  // Stops the manager, keep it last.
  void stopWithOpenContexts();

private:
  template <typename Command>
  Reply request(EventType requestType, EventType resultType,
                EventType errorType, Command command, int timeout = 60000);
  Reply open(const QString &path);
  Reply sendRequest(const QString &name, int timeout);
  // A repository at _base/repo with one commit of a.txt, opened in _uuid.
  void openFixture();
  QString git(const QStringList &args, bool *ok = nullptr) const {
    return gitIn(_config, _root, args, ok);
  }

  std::unique_ptr<GitRepoManager::GitRepoManager> _manager;
  IsolatedConfig _config;
  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _root;
  QString _head;
  QString _uuid;
};

template <typename Command>
Reply GitRepoManagerTest::request(EventType requestType, EventType resultType,
                                  EventType errorType, Command command,
                                  int timeout) {
  auto &eventManager = EventManager::EventManager::getInstance();

  if (command.uuid.isEmpty()) {
    command.uuid = _uuid;
  }
  command.syncUuid = newUuid();

  const int token = eventManager.startWait();
  EventManager::Event event(requestType, command.syncUuid);
  event.addData(command);
  eventManager.send(event);

  const auto [answered, answer] = eventManager.waitForAnswer(
      token, command.syncUuid, {resultType, errorType}, timeout);

  Reply reply;
  reply.answered = answered;
  if (!answered) {
    return reply;
  }
  reply.success = answer.type() == resultType;
  if (reply.success) {
    reply.value = answer.template data<GitRepoResult>().value;
  } else {
    const auto error = answer.template data<GitRepoError>();
    reply.error = error.message;
    reply.errorCode = error.error;
    reply.gitError = error.gitError;
  }
  return reply;
}

#define REQUEST(name, ...)                                                     \
  request(EventType::GitRepoManager##name,                                     \
          EventType::GitRepoManager##name##Result,                             \
          EventType::GitRepoManager##name##Error, __VA_ARGS__)

Reply GitRepoManagerTest::open(const QString &path) {
  GitRepoOpenCommand command;
  command.path = path;
  command.author = kAuthor;
  command.email = kEmail;
  return REQUEST(OpenRepo, command);
}

void GitRepoManagerTest::openFixture() {
  QVERIFY(GitRepo::initRepository(QStringLiteral("repo"), _base,
                                  QStringLiteral("main"), kAuthor, kEmail));
  QVERIFY(writeFile(_root + QStringLiteral("/a.txt"), "a\n"));
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("A")});
  _head = git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
  const Reply opened = open(_root);
  QVERIFY2(opened.success, qPrintable(opened.error));
}

void GitRepoManagerTest::initTestCase() {
  if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty()) {
    QSKIP("git was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  useTestCredentialsKey();
  GitRepo::initForCurrentThread();
  _manager = std::make_unique<GitRepoManager::GitRepoManager>();
  _manager->start();
}

void GitRepoManagerTest::cleanupTestCase() {
  if (_manager) {
    // Every closed repository still has its worker thread, stop joins them.
    QElapsedTimer timer;
    timer.start();
    _manager->stop();
    _manager.reset();
    QVERIFY2(timer.elapsed() < 30000, qPrintable(QString::number(timer.elapsed())));
  }
}

void GitRepoManagerTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  _config.setUp(_base + QStringLiteral("/home"));
  _root = _base + QStringLiteral("/repo");
  _head.clear();
  _uuid = newUuid();
}

void GitRepoManagerTest::cleanup() {
  if (_manager) {
    REQUEST(CloseRepo, GitRepoCloseCommand(), 10000);
  }
  GitRepo::Testing::setCommercialPlanOverride(-1);
  GitRepo::GitCredentialStorage::getInstance().clearAll();
  GitRepo::clearCachedCredentials(QString());
  makeWritable(_tmp->path());
  _tmp.reset();
}

void GitRepoManagerTest::requestsBeforeOpenFail() {
  const Reply status = REQUEST(Status, GitRepoStatusCommand());
  QVERIFY(status.answered);
  QVERIFY(!status.success);
  QCOMPARE(status.error, QStringLiteral("Git repository has not been opened."));

  const Reply close = REQUEST(CloseRepo, GitRepoCloseCommand());
  QVERIFY(close.answered);
  QVERIFY(!close.success);
}

void GitRepoManagerTest::initOpenCommitAndClose() {
  GitRepoInitRepoCommand init;
  init.repoName = QStringLiteral("repo");
  init.workingDir = _base;
  init.defaultBranch = QStringLiteral("main");
  init.author = kAuthor;
  init.email = kEmail;
  const Reply initReply = REQUEST(InitRepo, init);
  QVERIFY2(initReply.success, qPrintable(initReply.error));
  QCOMPARE(initReply.value.toString(), _root);
  QVERIFY(QFileInfo(_root + QStringLiteral("/.git")).isDir());

  // The container that made the repository can open it right away.
  const Reply opened = open(_root);
  QVERIFY2(opened.success, qPrintable(opened.error));

  QVERIFY(writeFile(_root + QStringLiteral("/a.txt"), "a\n"));
  const Reply status = REQUEST(Status, GitRepoStatusCommand());
  QVERIFY(status.success);
  QCOMPARE(status.value.value<GitRepo::GitStatus>().newFiles,
           QStringList{QStringLiteral("a.txt")});

  GitRepoStageFilesCommand stage;
  stage.files = {QStringLiteral("a.txt")};
  QVERIFY(REQUEST(StageFiles, stage).success);
  GitRepoCommitStagedCommand commit;
  commit.summary = QStringLiteral("Add a");
  commit.author = kAuthor;
  commit.email = kEmail;
  commit.cleanup = true;
  const Reply committed = REQUEST(CommitStaged, commit);
  QVERIFY2(committed.success, qPrintable(committed.error));

  GitRepoGetBranchHistoryCommand history;
  history.branchName = QStringLiteral("main");
  history.branchType = GitRepo::BranchType::Local;
  const Reply page = REQUEST(GetBranchHistory, history);
  QVERIFY(page.success);
  const auto commits = page.value.value<GitRepo::GitHistory>();
  QCOMPARE(commits.commits.size(), 2);
  QCOMPARE(commits.commitInfos[commits.commits.first()].summary,
           QStringLiteral("Add a"));

  const Reply branches = REQUEST(ListLocalBranches, GitRepoListLocalBranchesCommand());
  QVERIFY(branches.success);
  QCOMPARE(branches.value.toStringList(), QStringList{QStringLiteral("main")});

  // Errors carry GitRepo's error.
  GitRepoCheckoutBranchCommand checkout;
  checkout.branchName = QStringLiteral("missing");
  checkout.branchType = GitRepo::BranchType::Local;
  const Reply failed = REQUEST(CheckoutBranch, checkout);
  QVERIFY(failed.answered);
  QVERIFY(!failed.success);
  QVERIFY(!failed.error.isEmpty());
  QVERIFY(failed.gitError != GitRepo::GitError::Unknown);

  const Reply closed = REQUEST(CloseRepo, GitRepoCloseCommand());
  QVERIFY2(closed.success, qPrintable(closed.error));

  // A request after the close is answered with an error. (Status is not
  // used: its worker passes the closed repository on to GitRepo, which
  // asserts in debug builds.)
  GitRepoBlameCommand blame;
  blame.files = {QStringLiteral("a.txt")};
  const Reply afterClose = REQUEST(Blame, blame, 10000);
  QVERIFY(afterClose.answered);
  QVERIFY(!afterClose.success);
}

void GitRepoManagerTest::reopenAfterClose() {
  openFixture();
  QVERIFY(REQUEST(CloseRepo, GitRepoCloseCommand()).success);

  // The container opens the repository again, as the Mercurial manager does.
  const Reply reopened = open(_root);
  QVERIFY2(reopened.success, qPrintable(reopened.error));
}

void GitRepoManagerTest::openFailures() {
  // Not a repository: an error, and nothing stays open.
  const QString empty = _base + QStringLiteral("/empty");
  QVERIFY(QDir().mkpath(empty));
  const Reply failed = open(empty);
  QVERIFY(failed.answered);
  QVERIFY(!failed.success);
  QVERIFY(!failed.error.isEmpty());
  const Reply status = REQUEST(Status, GitRepoStatusCommand());
  QCOMPARE(status.error, QStringLiteral("Git repository has not been opened."));

  // The container can open a repository after that.
  openFixture();
  QVERIFY(REQUEST(Status, GitRepoStatusCommand()).success);
}

void GitRepoManagerTest::rejectedRequestsKeepTheOpenRepository_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("open") << QStringLiteral("open");
  QTest::newRow("clone") << QStringLiteral("clone");
  QTest::newRow("init") << QStringLiteral("init");
}

void GitRepoManagerTest::rejectedRequestsKeepTheOpenRepository() {
  QFETCH(QString, kind);
  openFixture();

  // A second open, clone or init in the same container is refused.
  Reply refused;
  if (kind == QStringLiteral("open")) {
    refused = open(_root);
    QCOMPARE(refused.error,
             QStringLiteral("Git repository has already been opened."));
  } else if (kind == QStringLiteral("clone")) {
    GitRepoCloneCommand clone;
    clone.url = _root;
    clone.destinationDir = _base + QStringLiteral("/clone");
    refused = REQUEST(CloneRepo, clone);
    QCOMPARE(refused.error, QStringLiteral("Git repository is already in use."));
  } else {
    GitRepoInitRepoCommand init;
    init.repoName = QStringLiteral("other");
    init.workingDir = _base;
    init.defaultBranch = QStringLiteral("main");
    init.author = kAuthor;
    init.email = kEmail;
    refused = REQUEST(InitRepo, init);
    QCOMPARE(refused.error, QStringLiteral("Git repository is already in use."));
  }
  QVERIFY(!refused.success);

  // The repository that was open stays open.
  const Reply status = REQUEST(Status, GitRepoStatusCommand(), 10000);
  QVERIFY2(status.success, qPrintable(status.error));
}

void GitRepoManagerTest::twoContainersAreIndependent() {
  openFixture();
  const QString first = _uuid;

  const QString otherRoot = _base + QStringLiteral("/other");
  QVERIFY(GitRepo::initRepository(QStringLiteral("other"), _base,
                                  QStringLiteral("main"), kAuthor, kEmail));
  QVERIFY(writeFile(otherRoot + QStringLiteral("/b.txt"), "b\n"));
  const QString second = newUuid();
  GitRepoOpenCommand openSecond;
  openSecond.uuid = second;
  openSecond.path = otherRoot;
  openSecond.author = kAuthor;
  openSecond.email = kEmail;
  QVERIFY(REQUEST(OpenRepo, openSecond).success);

  QVERIFY(writeFile(_root + QStringLiteral("/c.txt"), "c\n"));
  for (int i = 0; i < 3; ++i) {
    GitRepoStatusCommand statusFirst;
    statusFirst.uuid = first;
    GitRepoStatusCommand statusSecond;
    statusSecond.uuid = second;
    const Reply a = REQUEST(Status, statusFirst);
    const Reply b = REQUEST(Status, statusSecond);
    QCOMPARE(a.value.value<GitRepo::GitStatus>().newFiles,
             QStringList{QStringLiteral("c.txt")});
    QCOMPARE(b.value.value<GitRepo::GitStatus>().newFiles,
             QStringList{QStringLiteral("b.txt")});
  }

  // Closing one leaves the other working.
  QVERIFY(REQUEST(CloseRepo, GitRepoCloseCommand()).success);
  GitRepoStatusCommand statusSecond;
  statusSecond.uuid = second;
  QVERIFY(REQUEST(Status, statusSecond).success);
  GitRepoCloseCommand closeSecond;
  closeSecond.uuid = second;
  QVERIFY(REQUEST(CloseRepo, closeSecond).success);
}

#define ROW(Name, Type, ...)                                                   \
  if (name == QLatin1String(#Name)) {                                          \
    Type command;                                                              \
    __VA_ARGS__;                                                               \
    return REQUEST(Name, command, timeout);                                    \
  }

Reply GitRepoManagerTest::sendRequest(const QString &name, int timeout) {
  const QString a = QStringLiteral("a.txt");
  ROW(AbortMerge, GitRepoAbortMergeCommand)
  ROW(AbortRebase, GitRepoAbortRebaseCommand)
  ROW(AddRemote, GitRepoAddRemoteCommand, command.remoteName = "r1",
      command.fetchUrl = _base + "/missing.git")
  ROW(AddSubmodule, GitRepoAddSubmoduleCommand,
      command.url = _base + "/missing", command.name = "sub")
  ROW(AddTarget, GitRepoAddTargetCommand, command.remoteName = "nope",
      command.targetName = "t", command.fetchUrl = "https://example.invalid/x")
  ROW(AddWorktree, GitRepoAddWorktreeCommand, command.name = "wt",
      command.path = _base + "/wt", command.branch = "wt",
      command.createBranch = true)
  ROW(Amend, GitRepoAmendCommand, command.author = kAuthor,
      command.email = kEmail, command.summary = "Amended")
  ROW(ApplyPatch, GitRepoApplyPatchCommand)
  ROW(ApplyStash, GitRepoApplyStashCommand, command.index = 0)
  ROW(Bisect, GitRepoBisectCommand)
  ROW(Blame, GitRepoBlameCommand, command.files = {a})
  ROW(CheckoutBranch, GitRepoCheckoutBranchCommand, command.branchName = "main",
      command.branchType = GitRepo::BranchType::Local)
  ROW(CheckoutCommit, GitRepoCheckoutCommitCommand, command.commit = _head)
  ROW(CherryPick, GitRepoCherryPickCommand, command.commit = _head,
      command.author = kAuthor, command.email = kEmail)
  ROW(CloneRepo, GitRepoCloneCommand, command.url = _base + "/missing.git",
      command.destinationDir = _base + "/clone")
  ROW(CommitRebase, GitRepoCommitRebaseCommand, command.author = kAuthor,
      command.email = kEmail, command.summary = "S")
  ROW(CommitStaged, GitRepoCommitStagedCommand, command.author = kAuthor,
      command.email = kEmail, command.summary = "S")
  ROW(CreateBranchFromCommit, GitRepoCreateBranchFromCommitCommand,
      command.branchName = "b1", command.commit = _head)
  ROW(CreatePatchFromCommit, GitRepoCreatePatchFromCommitCommand,
      command.commit = _head)
  ROW(CreateTagFromCommit, GitRepoCreateTagFromCommitCommand,
      command.tagName = "t1", command.commit = _head, command.author = kAuthor,
      command.email = kEmail, command.message = "Tag")
  ROW(DeleteAllStashes, GitRepoDeleteAllStashesCommand)
  ROW(DeleteBranch, GitRepoDeleteBranchCommand, command.branchName = "nope")
  ROW(DeleteRemoteBranch, GitRepoDeleteRemoteBranchCommand,
      command.branchName = "nope")
  ROW(DeleteRemoteTag, GitRepoDeleteRemoteTagCommand, command.tagName = "nope")
  ROW(DeleteStash, GitRepoDeleteStashCommand, command.index = 0)
  // A path inside the test directory: deleteSubmodule removes it.
  ROW(DeleteSubmodule, GitRepoDeleteSubmoduleCommand,
      command.submoduleName = "nope", command.submodulePath = _base + "/nope")
  ROW(DeleteTag, GitRepoDeleteTagCommand, command.tagName = "nope")
  ROW(DeleteTarget, GitRepoDeleteTargetCommand, command.remoteName = "nope",
      command.targetName = "t")
  ROW(DiffFiles, GitRepoDiffFilesCommand, command.files = {a})
  ROW(DiffFilesFromCommit, GitRepoDiffFilesFromCommitCommand,
      command.files = {a}, command.commit = _head)
  ROW(DiscardFiles, GitRepoDiscardFilesCommand, command.files = {a})
  ROW(EditTarget, GitRepoEditTargetCommand, command.remoteName = "nope",
      command.targetName = "t")
  ROW(FetchAll, GitRepoFetchAllCommand)
  ROW(FetchRemote, GitRepoFetchRemoteCommand, command.remoteName = "nope")
  ROW(GetActiveBranch, GitRepoGetActiveBranchCommand,
      command.branchType = GitRepo::BranchType::Local)
  ROW(GetBranchHistory, GitRepoGetBranchHistoryCommand,
      command.branchName = "main",
      command.branchType = GitRepo::BranchType::Local)
  ROW(GetCommitBranches, GitRepoGetCommitBranchesCommand,
      command.commitOid = _head)
  ROW(GetCommitHeads, GitRepoGetCommitHeadsCommand, command.commitOid = _head)
  ROW(GetCommitIsHead, GitRepoGetCommitIsHeadCommand, command.commitOid = _head)
  ROW(GetCommitStatus, GitRepoGetCommitStatusCommand, command.commitOid = _head)
  ROW(GetCommitTags, GitRepoGetCommitTagsCommand, command.commitOid = _head)
  ROW(GetFileSizeFromCommit, GitRepoGetFileSizeFromCommitCommand,
      command.filePath = a, command.commit = _head)
  ROW(GetIsMergeInProgress, GitRepoGetIsMergeInProgressCommand)
  ROW(GetIsRebaseInProgress, GitRepoGetIsRebaseInProgressCommand)
  ROW(GetLfsStorageStatus, GitRepoGetLfsStorageStatusCommand)
  ROW(GetStashStatus, GitRepoGetStashStatusCommand, command.index = 0)
  ROW(GetSubmodulePath, GitRepoGetSubmodulePathCommand,
      command.submoduleName = "nope")
  ROW(GetUpstream, GitRepoGetUpstreamCommand, command.localBranchName = "main")
  ROW(GitFlow, GitRepoGitFlowCommand)
  ROW(InitRepo, GitRepoInitRepoCommand, command.repoName = "other",
      command.workingDir = _base, command.defaultBranch = "main",
      command.author = kAuthor, command.email = kEmail)
  ROW(ListConflicts, GitRepoListConflictsCommand, command.filePath = a)
  ROW(ListLocalBranches, GitRepoListLocalBranchesCommand)
  ROW(ListRemoteBranches, GitRepoListRemoteBranchesCommand)
  ROW(ListRemoteBranchesWithStatus, GitRepoListRemoteBranchesWithStatusCommand)
  ROW(ListRemotes, GitRepoListRemotesCommand)
  ROW(ListStashes, GitRepoListStashesCommand)
  ROW(ListSubmodules, GitRepoListSubmodulesCommand)
  ROW(ListSubmodulesWithStatus, GitRepoListSubmodulesWithStatusCommand)
  ROW(ListTags, GitRepoListTagsCommand)
  ROW(ListWorktrees, GitRepoListWorktreesCommand)
  ROW(LockWorktree, GitRepoLockWorktreeCommand, command.name = "nope")
  ROW(Merge, GitRepoMergeCommand, command.sourceBranch = "main",
      command.destinationBranch = "main", command.author = kAuthor,
      command.email = kEmail)
  ROW(OpenRepo, GitRepoOpenCommand, command.path = _root,
      command.author = kAuthor, command.email = kEmail)
  ROW(Pull, GitRepoPullCommand)
  ROW(Push, GitRepoPushCommand)
  ROW(PushAllTags, GitRepoPushAllTagsCommand)
  ROW(PushTag, GitRepoPushTagCommand, command.tagName = "nope")
  ROW(Rebase, GitRepoRebaseCommand, command.sourceBranch = "main",
      command.destinationBranch = "main", command.author = kAuthor,
      command.email = kEmail)
  ROW(RemoveRemote, GitRepoRemoveRemoteCommand, command.remoteName = "nope")
  ROW(RemoveWorktree, GitRepoRemoveWorktreeCommand, command.name = "nope")
  ROW(RenameBranch, GitRepoRenameBranchCommand, command.oldBranchName = "nope",
      command.newBranchName = "x")
  ROW(RenameTag, GitRepoRenameTagCommand, command.oldTagName = "nope",
      command.newTagName = "x")
  ROW(Reset, GitRepoResetCommand, command.commit = "HEAD",
      command.resetType = GitRepo::ResetType::Mixed)
  ROW(Revert, GitRepoRevertCommand, command.commit = _head,
      command.author = kAuthor, command.email = kEmail,
      command.message = "Revert")
  ROW(SetDefaultRemote, GitRepoSetDefaultRemoteCommand,
      command.remoteName = "nope")
  ROW(SetDefaultTarget, GitRepoSetDefaultTargetCommand,
      command.remoteName = "nope", command.targetName = "t")
  ROW(SetUpstream, GitRepoSetUpstreamCommand, command.remoteName = "origin",
      command.remoteBranchName = "main", command.localBranchName = "main")
  ROW(Squash, GitRepoSquashCommand, command.sourceBranch = "main",
      command.destinationBranch = "main", command.author = kAuthor,
      command.email = kEmail)
  ROW(Stash, GitRepoStashCommand, command.note = "note")
  ROW(StageFiles, GitRepoStageFilesCommand, command.files = {a})
  ROW(Status, GitRepoStatusCommand)
  ROW(SubmoduleUpdate, GitRepoSubmoduleUpdateCommand,
      command.submoduleName = "nope")
  ROW(UnlockWorktree, GitRepoUnlockWorktreeCommand, command.name = "nope")
  ROW(UnstageFiles, GitRepoUnstageFilesCommand, command.files = {a})
  ROW(UpdateRemote, GitRepoUpdateRemoteCommand, command.remoteName = "nope",
      command.newRemoteName = "x")
  ROW(Workflow, GitRepoWorkflowCommand)
  qWarning() << "No request for" << name;
  return {};
}

void GitRepoManagerTest::everyRequestIsAnswered_data() {
  QTest::addColumn<QString>("name");
  for (const QString &name : kRequests) {
    QTest::newRow(qPrintable(name)) << name;
  }
}

void GitRepoManagerTest::everyRequestIsAnswered() {
  QFETCH(QString, name);
  openFixture();

  // Every request gets a result or an error, never silence (the bridge would
  // wait until its timeout).
  const Reply reply = sendRequest(name, 20000);
  QVERIFY(reply.answered);

  // A commit without tags is not an error.
  if (name == QLatin1String("GetCommitTags")) {
    QVERIFY(reply.success);
  }

  // Errors say what went wrong.
  QVERIFY2(reply.success || !reply.error.isEmpty(), "an error without a message");
}

void GitRepoManagerTest::errorsCarryTheGitError() {
  openFixture();
  GitRepoCheckoutCommitCommand checkout;
  checkout.commit = QStringLiteral("not a sha");
  const Reply failed = REQUEST(CheckoutCommit, checkout);
  QVERIFY(!failed.success);
  QCOMPARE(failed.gitError, GitRepo::GitError::CheckoutCommitFailedToParseObjectId);
  QVERIFY(!failed.error.isEmpty());
  QVERIFY(failed.errorCode != -1);
  QVERIFY(!translateGitError(failed.errorCode).isEmpty());

  // Errors of the component itself have no GitRepo error.
  const Reply notOpen = request(EventType::GitRepoManagerStatus,
                                EventType::GitRepoManagerStatusResult,
                                EventType::GitRepoManagerStatusError,
                                [] {
                                  GitRepoStatusCommand status;
                                  status.uuid = newUuid();
                                  return status;
                                }());
  QCOMPARE(notOpen.gitError, GitRepo::GitError::Unknown);
  QCOMPARE(notOpen.errorCode, -1);
}

void GitRepoManagerTest::cloneReportsProgress() {
  openFixture();
  REQUEST(CloseRepo, GitRepoCloseCommand());
  const QString source = _base + QStringLiteral("/source.git");
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"),
                         QStringLiteral("--bare"), _root, source});

  QMutex mutex;
  QVector<GitRepo::CloneProgress> progress;
  const QString cloneUuid = newUuid();
  auto &eventManager = EventManager::EventManager::getInstance();
  const EventManager::CallbackType handler = [&](EventManager::Event event) {
    const auto result = event.data<GitRepoResult>();
    QMutexLocker locker(&mutex);
    if (result.uuid == cloneUuid) {
      progress << result.value.value<GitRepo::CloneProgress>();
    }
  };
  eventManager.subscribe({EventType::GitRepoManagerCloneRepoProgress, this, handler});
  const auto unsubscribe = qScopeGuard([&] {
    eventManager.unsubscribe(
        {EventType::GitRepoManagerCloneRepoProgress, this, handler});
  });

  GitRepoCloneCommand clone;
  clone.uuid = cloneUuid;
  clone.url = source;
  clone.destinationDir = _base + QStringLiteral("/clone");
  const Reply cloned = REQUEST(CloneRepo, clone);
  QVERIFY2(cloned.success, qPrintable(cloned.error));
  QCOMPARE(cloned.value.toString(), clone.destinationDir);
  QVERIFY(QFileInfo::exists(clone.destinationDir + QStringLiteral("/a.txt")));
  QTRY_VERIFY([&] {
    QMutexLocker locker(&mutex);
    return !progress.isEmpty() && progress.last().finished;
  }());
  {
    QMutexLocker locker(&mutex);
    QVERIFY(progress.first().started);
    QVERIFY(!progress.last().error);
    QVERIFY(progress.last().cloning);
  }

  // The clone's container is free again: it can clone once more, and a
  // failed clone is reported with an error.
  clone.destinationDir = _base + QStringLiteral("/clone2");
  QVERIFY(REQUEST(CloneRepo, clone).success);
  clone.url = _base + QStringLiteral("/missing.git");
  clone.destinationDir = _base + QStringLiteral("/clone3");
  const Reply failed = REQUEST(CloneRepo, clone);
  QVERIFY(!failed.success);
  QCOMPARE(failed.gitError, GitRepo::GitError::CloneRepositoryFailedToCloneRepository);
  QVERIFY(!QFileInfo::exists(clone.destinationDir));
}

void GitRepoManagerTest::fetchPullPushReportProgress() {
  // A clone of a bare remote, opened in the container.
  const QString seed = _base + QStringLiteral("/seed");
  QVERIFY(QDir().mkpath(seed));
  gitIn(_config, seed, {QStringLiteral("init"), QStringLiteral("-q")});
  QVERIFY(writeFile(seed + QStringLiteral("/a.txt"), "a\n"));
  gitIn(_config, seed, {QStringLiteral("add"), QStringLiteral("-A")});
  gitIn(_config, seed, {QStringLiteral("commit"), QStringLiteral("-qm"),
                        QStringLiteral("A")});
  const QString remote = _base + QStringLiteral("/remote.git");
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"),
                         QStringLiteral("--bare"), seed, remote});
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"), remote,
                         _root});
  QVERIFY(open(_root).success);

  QMutex mutex;
  int finished = 0;
  auto &eventManager = EventManager::EventManager::getInstance();
  const EventManager::CallbackType handler = [&](EventManager::Event event) {
    const auto progress =
        event.data<GitRepoResult>().value.value<GitRepo::CloneProgress>();
    QMutexLocker locker(&mutex);
    if (progress.finished && !progress.cloning) {
      ++finished;
    }
  };
  eventManager.subscribe({EventType::GitRepoManagerCloneRepoProgress, this, handler});
  const auto unsubscribe = qScopeGuard([&] {
    eventManager.unsubscribe(
        {EventType::GitRepoManagerCloneRepoProgress, this, handler});
  });

  GitRepoFetchRemoteCommand fetch;
  fetch.remoteName = QStringLiteral("origin");
  QVERIFY(REQUEST(FetchRemote, fetch).success);
  QVERIFY(REQUEST(Pull, GitRepoPullCommand()).success);
  QVERIFY(writeFile(_root + QStringLiteral("/b.txt"), "b\n"));
  gitIn(_config, _root, {QStringLiteral("add"), QStringLiteral("-A")});
  gitIn(_config, _root, {QStringLiteral("commit"), QStringLiteral("-qm"),
                         QStringLiteral("B")});
  const Reply pushed = REQUEST(Push, GitRepoPushCommand());
  QVERIFY2(pushed.success, qPrintable(pushed.error));

  // Each transfer ends its progress toast.
  QTRY_COMPARE([&] {
    QMutexLocker locker(&mutex);
    return finished;
  }(), 3);
}

void GitRepoManagerTest::closeAnswersQueuedRequests() {
  openFixture();
  // Something slow first: a fetch from a remote that doesn't exist takes
  // more than a second (retries and the progress toast delay).
  git({QStringLiteral("remote"), QStringLiteral("add"), QStringLiteral("broken"),
       _base + QStringLiteral("/missing.git")});

  auto &eventManager = EventManager::EventManager::getInstance();
  const auto send = [&](auto command, EventType type) {
    command.uuid = _uuid;
    command.syncUuid = newUuid();
    const int token = eventManager.startWait();
    EventManager::Event event(type, command.syncUuid);
    event.addData(command);
    eventManager.send(event);
    return qMakePair(token, command.syncUuid);
  };

  GitRepoFetchRemoteCommand fetch;
  fetch.remoteName = QStringLiteral("broken");
  const auto fetchWait = send(fetch, EventType::GitRepoManagerFetchRemote);
  QVector<QPair<int, QString>> statusWaits;
  for (int i = 0; i < 3; ++i) {
    statusWaits << send(GitRepoStatusCommand(), EventType::GitRepoManagerStatus);
  }
  const auto closeWait = send(GitRepoCloseCommand(), EventType::GitRepoManagerCloseRepo);

  const auto [closed, closeAnswer] = eventManager.waitForAnswer(
      closeWait.first, closeWait.second,
      {EventType::GitRepoManagerCloseRepoResult,
       EventType::GitRepoManagerCloseRepoError},
      60000);
  QVERIFY(closed);
  QCOMPARE(closeAnswer.type(), EventType::GitRepoManagerCloseRepoResult);
  const auto [fetched, fetchAnswer] = eventManager.waitForAnswer(
      fetchWait.first, fetchWait.second,
      {EventType::GitRepoManagerFetchRemoteResult,
       EventType::GitRepoManagerFetchRemoteError},
      1000);
  QVERIFY(fetched);
  Q_UNUSED(fetchAnswer);

  // The requests queued before the close are answered too, with a result
  // or an error.
  int answered = 0;
  for (const auto &[token, syncUuid] : statusWaits) {
    const auto [ok, answer] = eventManager.waitForAnswer(
        token, syncUuid,
        {EventType::GitRepoManagerStatusResult,
         EventType::GitRepoManagerStatusError},
        2000);
    Q_UNUSED(answer);
    answered += ok ? 1 : 0;
  }
  QCOMPARE(answered, 3);
}

void GitRepoManagerTest::httpCredentials() {
  // A bare repository served over HTTP with Basic authentication.
  openFixture();
  REQUEST(CloseRepo, GitRepoCloseCommand());
  const QString served = _base + QStringLiteral("/served");
  QVERIFY(QDir().mkpath(served));
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"),
                         QStringLiteral("--bare"), _root,
                         served + QStringLiteral("/remote.git")});
  const QString username = QStringLiteral("alice");
  const QString password = QStringLiteral("s3cret=!");
  GitHttpServer server(served, username, password, _config.environment());
  QVERIFY(server.start());
  const QString url = server.url(QStringLiteral("remote.git"));
  // Credentials are only asked for on the commercial plan.
  GitRepo::Testing::setCommercialPlanOverride(1);

  FakeCredentialsOverlay overlay;
  const auto clone = [&](const QString &name) {
    GitRepoCloneCommand command;
    command.uuid = newUuid();
    command.url = url;
    command.destinationDir = _base + '/' + name;
    return REQUEST(CloneRepo, command);
  };

  // The right credentials.
  overlay.setAnswers({{username, password}});
  const Reply cloned = clone(QStringLiteral("clone"));
  QVERIFY2(cloned.success, qPrintable(cloned.error));
  QVERIFY(QFileInfo::exists(_base + QStringLiteral("/clone/a.txt")));
  QTRY_COMPARE(overlay.events(),
               (QStringList{QStringLiteral("prompt"), QStringLiteral("finished")}));
  QCOMPARE(overlay.requests().value(0).repoPath, _base + QStringLiteral("/clone"));
  QCOMPARE(overlay.requests().value(0).url, url);
  QVERIFY(server.refused() >= 1);

  // A wrong answer is asked again, like `git clone` does.
  GitRepo::clearCachedCredentials(QString());
  overlay.setAnswers({{username, QStringLiteral("wrong")}, {username, password}});
  const Reply retried = clone(QStringLiteral("retried"));
  QVERIFY2(retried.success, qPrintable(retried.error));

  // Cancelled: the clone fails after one prompt and leaves nothing behind.
  GitRepo::clearCachedCredentials(QString());
  overlay.setAnswers({});
  const Reply cancelled = clone(QStringLiteral("cancelled"));
  QVERIFY(cancelled.answered);
  QVERIFY(!cancelled.success);
  QVERIFY(!cancelled.error.isEmpty());
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 1);
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/cancelled")));

  // Stored credentials for the destination are used without asking.
  GitRepo::clearCachedCredentials(QString());
  const QString stored = _base + QStringLiteral("/stored");
  GitRepo::GitCredentialStorage::getInstance().storeCredentials(
      stored, username, password, false);
  overlay.setAnswers({});
  const Reply withStored = clone(QStringLiteral("stored"));
  QVERIFY2(withStored.success, qPrintable(withStored.error));
  QVERIFY(overlay.requests().isEmpty());

  // Fetching in the clone: a wrong answer is replaced by asking again
  // (fetch retries).
  GitRepo::clearCachedCredentials(QString());
  GitRepo::GitCredentialStorage::getInstance().clearAll();
  // A container that was never used: closed ones can't open again.
  _uuid = newUuid();
  QVERIFY(open(_base + QStringLiteral("/clone")).success);
  overlay.setAnswers({{username, QStringLiteral("wrong")}, {username, password}});
  GitRepoFetchRemoteCommand fetch;
  fetch.remoteName = QStringLiteral("origin");
  const Reply fetched = REQUEST(FetchRemote, fetch);
  QVERIFY2(fetched.success, qPrintable(fetched.error));
  QTRY_VERIFY(overlay.events().contains(QStringLiteral("invalid")));
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 2);

  // Stored credentials that expired are asked for again.
  GitRepo::clearCachedCredentials(QString());
  GitRepo::GitCredentialStorage::getInstance().storeCredentials(
      _base + QStringLiteral("/clone"), username, password, false);
  const ConfigOverride expiry(
      ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime, 1);
  QThread::msleep(20);
  overlay.setAnswers({{username, password}});
  QVERIFY(REQUEST(FetchRemote, fetch).success);
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 1);
}

void GitRepoManagerTest::credentialPromptsFinishOncePerUrl() {
  // Two remotes on two servers: FetchAll asks once per URL, and every
  // prompt is closed exactly once.
  openFixture();
  REQUEST(CloseRepo, GitRepoCloseCommand());
  const QString served = _base + QStringLiteral("/served");
  QVERIFY(QDir().mkpath(served));
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"),
                         QStringLiteral("--bare"), _root,
                         served + QStringLiteral("/remote.git")});
  const QString username = QStringLiteral("alice");
  const QString password = QStringLiteral("s3cret=!");
  GitHttpServer first(served, username, password, _config.environment());
  GitHttpServer second(served, username, password, _config.environment());
  QVERIFY(first.start());
  QVERIFY(second.start());
  GitRepo::Testing::setCommercialPlanOverride(1);

  const QString clone = _base + QStringLiteral("/clone");
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"),
                         _base + QStringLiteral("/served/remote.git"), clone});
  gitIn(_config, clone, {QStringLiteral("remote"), QStringLiteral("set-url"),
                         QStringLiteral("origin"),
                         first.url(QStringLiteral("remote.git"))});
  gitIn(_config, clone, {QStringLiteral("remote"), QStringLiteral("add"),
                         QStringLiteral("second"),
                         second.url(QStringLiteral("remote.git"))});
  _uuid = newUuid();
  QVERIFY(open(clone).success);

  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{username, password}, {username, password}});
  const Reply fetched = REQUEST(FetchAll, GitRepoFetchAllCommand());
  QVERIFY2(fetched.success, qPrintable(fetched.error));

  const QVector<GitRepo::GitCredentialsRequest> prompts = overlay.requests();
  QCOMPARE(prompts.size(), 2);
  QVERIFY(prompts.at(0).url != prompts.at(1).url);
  QVERIFY(prompts.at(0).id != prompts.at(1).id);
  // Finished events may arrive just after the answer.
  QTRY_COMPARE(overlay.finishedIds().size(), 2);
  QThread::msleep(200);
  QVector<size_t> finished = overlay.finishedIds();
  QVector<size_t> asked = {prompts.at(0).id, prompts.at(1).id};
  std::sort(finished.begin(), finished.end());
  std::sort(asked.begin(), asked.end());
  QCOMPARE(finished, asked);

  // With the credentials cached nothing is asked, and nothing is closed.
  overlay.setAnswers({});
  const Reply again = REQUEST(FetchAll, GitRepoFetchAllCommand());
  QVERIFY2(again.success, qPrintable(again.error));
  QThread::msleep(200);
  QCOMPARE(overlay.events(), QStringList());
}

void GitRepoManagerTest::closeDuringClone() {
  // No cancellation: a close sent while a clone runs is answered once the
  // clone is done, nothing deadlocks and the container can be used again.
  openFixture();
  REQUEST(CloseRepo, GitRepoCloseCommand());
  const QString served = _base + QStringLiteral("/served");
  QVERIFY(QDir().mkpath(served));
  gitIn(_config, _base, {QStringLiteral("clone"), QStringLiteral("-q"),
                         QStringLiteral("--bare"), _root,
                         served + QStringLiteral("/remote.git")});
  GitHttpServer server(served, QString(), QString(), _config.environment());
  server.setDelay(300);
  QVERIFY(server.start());

  auto &eventManager = EventManager::EventManager::getInstance();
  _uuid = newUuid();
  GitRepoCloneCommand cloneCommand;
  cloneCommand.uuid = _uuid;
  cloneCommand.syncUuid = newUuid();
  cloneCommand.url = server.url(QStringLiteral("remote.git"));
  cloneCommand.destinationDir = _base + QStringLiteral("/clone");
  const int cloneToken = eventManager.startWait();
  EventManager::Event cloneEvent(EventType::GitRepoManagerCloneRepo,
                                 cloneCommand.syncUuid);
  cloneEvent.addData(cloneCommand);
  eventManager.send(cloneEvent);

  // The clone is talking to the server.
  QTRY_VERIFY_WITH_TIMEOUT(server.passed() > 0, 10000);
  QElapsedTimer timer;
  timer.start();
  const Reply closed = REQUEST(CloseRepo, GitRepoCloseCommand(), 60000);
  QVERIFY(closed.answered);
  QVERIFY2(closed.success, qPrintable(closed.error));
  QVERIFY(timer.elapsed() < 60000);

  const auto [cloneAnswered, cloneAnswer] = eventManager.waitForAnswer(
      cloneToken, cloneCommand.syncUuid,
      {EventType::GitRepoManagerCloneRepoResult,
       EventType::GitRepoManagerCloneRepoError},
      60000);
  QVERIFY(cloneAnswered);
  if (cloneAnswer.type() == EventType::GitRepoManagerCloneRepoResult) {
    QCOMPARE(readFile(_base + QStringLiteral("/clone/a.txt")),
             QByteArray("a\n"));
  }

  // The container is free again.
  const Reply reopened = open(_root);
  QVERIFY2(reopened.success, qPrintable(reopened.error));
}

void GitRepoManagerTest::stopWithOpenContexts() {
  // Last test: stops the manager with two open repositories and requests
  // in flight. stop() joins every worker thread without hanging.
  openFixture();
  const QString first = _uuid;
  QVERIFY(GitRepo::initRepository(QStringLiteral("other"), _base,
                                  QStringLiteral("main"), kAuthor, kEmail));
  _uuid = newUuid();
  QVERIFY(open(_base + QStringLiteral("/other")).success);

  auto &eventManager = EventManager::EventManager::getInstance();
  for (const QString &uuid : {first, _uuid}) {
    for (int i = 0; i < 3; ++i) {
      GitRepoStatusCommand status;
      status.uuid = uuid;
      status.syncUuid = newUuid();
      EventManager::Event event(EventType::GitRepoManagerStatus,
                                status.syncUuid);
      event.addData(status);
      eventManager.send(event);
    }
  }

  QElapsedTimer timer;
  timer.start();
  _manager->stop();
  _manager.reset();
  QVERIFY2(timer.elapsed() < 30000, qPrintable(QString::number(timer.elapsed())));

  // Nothing answers any more.
  QVERIFY(!REQUEST(Status, GitRepoStatusCommand(), 500).answered);
}

QTEST_GUILESS_MAIN(GitRepoManagerTest)

#include "tst_gitrepomanager.moc"
