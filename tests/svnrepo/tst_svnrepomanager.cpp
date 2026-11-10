// MIT License
// Copyright (c) 2026 Sourcerer

// End to end tests of the SvnRepoManager component: requests are sent as
// events and answered by the component's worker threads, as the Subversion
// bridge does. See docs/SVN_TEST_PLAN.md section 5; QEXPECT_FAIL markers name
// the suspected defect (section 7.3) that makes the check fail today.

#include "SvnTestHelpers.h"

#include <SvnRepoManager.h>
#include <SvnRepoManagerStructs.h>

#include <EventManager.h>
#include <GitRepoManagerStructs.h>

#include <QDir>
#include <QElapsedTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <memory>

using namespace SvnTest;
using namespace SvnRepoManager::Structs;
using EventManager::EventType;

namespace {

QString newUuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

struct Reply {
  bool answered = false;
  bool success = false;
  QVariant value;
  QString error;
  int errorCode = -1;
  SvnRepo::SvnError svnError = SvnRepo::SvnError::Unknown;
};

// Every request type the component handles (SvnRepoManager<Name>), except
// CloseRepo, which has its own tests.
const QStringList kRequests = {
    "AbortMerge", "AddRemote", "AddSubmodule", "AddTarget", "ApplyPatch",
    "ApplyStash", "Blame", "CheckoutBranch", "CheckoutCommit", "CherryPick",
    "CloneRepo", "CommitStaged", "CreateBranchFromCommit",
    "CreatePatchFromCommit", "CreateTagFromCommit", "DeleteAllStashes",
    "DeleteBranch", "DeleteRemoteBranch", "DeleteRemoteTag", "DeleteStash",
    "DeleteSubmodule", "DeleteTag", "DeleteTarget", "DiffFiles",
    "DiffFilesFromCommit", "DiscardFiles", "EditTarget", "FetchAll",
    "FetchRemote", "GetActiveBranch", "GetBranchHistory", "GetCommitBranches",
    "GetCommitHeads", "GetCommitIsHead", "GetCommitStatus", "GetCommitTags",
    "GetFileSizeFromCommit", "GetIsMergeInProgress", "GetStashStatus",
    "GetSubmodulePath",
    "GetUpstream", "IgnoreFiles", "InitRepo", "ListConflicts",
    "ListLocalBranches", "ListRemoteBranches", "ListRemoteBranchesWithStatus",
    "ListRemoteTags", "ListRemotes", "ListStashes", "ListSubmodules",
    "ListSubmodulesWithStatus", "ListTags", "Merge", "OpenRepo", "Pull",
    "Push", "PushAllTags", "PushTag", "Rebase", "RemoveRemote", "RenameBranch",
    "RenameTag", "Reset", "ResolveTreeConflict", "Revert", "SetDefaultRemote",
    "SetDefaultTarget", "SetUpstream", "StageFiles", "Stash", "Status",
    "SubmoduleUpdate", "UnstageFiles", "UpdateRemote"};

} // namespace

class SvnRepoManagerTest : public QObject {
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
  void requestsAfterCloseAreRejected();
  void unknownUuidDoesNotPoisonTheMap();
  void twoContainersAreIndependent();
  void requestsOnOneUuidAreFifo();
  void everyRequestIsAnswered_data();
  void everyRequestIsAnswered();
  void queriesWithEmptyResults_data();
  void queriesWithEmptyResults();
  void errorsCarryTheSvnError();
  void mergeErrorIsReported();
  void gitOnlyRequestsAreUnsupported();
  void applyStashResultMap();
  void listConflictsReportsTreeConflict();
  void cloneReportsProgress();
  void initThenOpenImmediately();
  void closeAnswersQueuedRequests();
  void closeDuringLongHistory();
  // Stops the manager, keep it last.
  void stopWithOpenContexts();

private:
  template <typename Command>
  Reply request(EventType requestType, EventType resultType,
                EventType errorType, Command command, int timeout = 60000);
  Reply open(const QString &path, const QString &uuid = QString());
  Reply close(int timeout = 10000);
  Reply sendRequest(const QString &name, int timeout);
  // A working copy at _wc with a.txt committed, opened in _uuid.
  void openFixture();

  std::unique_ptr<SvnRepoManager::SvnRepoManager> _manager;
  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _wc;
  QString _repoDir;
  QString _rootUrl;
  QString _head;
  QString _uuid;
  QString _cwd;
};

template <typename Command>
Reply SvnRepoManagerTest::request(EventType requestType, EventType resultType,
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
    reply.value = answer.template data<SvnRepoResult>().value;
  } else {
    const auto error = answer.template data<SvnRepoError>();
    reply.error = error.message;
    reply.errorCode = error.error;
    reply.svnError = error.gitError;
  }
  return reply;
}

#define REQUEST(name, ...)                                                     \
  request(EventType::SvnRepoManager##name,                                     \
          EventType::SvnRepoManager##name##Result,                             \
          EventType::SvnRepoManager##name##Error, __VA_ARGS__)

Reply SvnRepoManagerTest::open(const QString &path, const QString &uuid) {
  SvnRepoOpenCommand command;
  command.uuid = uuid;
  command.path = path;
  return REQUEST(OpenRepo, command);
}

Reply SvnRepoManagerTest::close(int timeout) {
  return REQUEST(CloseRepo, SvnRepoCloseCommand(), timeout);
}

void SvnRepoManagerTest::openFixture() {
  QVERIFY2(SvnRepo::initRepository(QStringLiteral("repo"), _wc,
                                   QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  _rootUrl = SvnRepo::listRemotes(_wc).first().fetchUrl;
  QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("A")));
  bool ok = false;
  svnIn(_wc, {QStringLiteral("update")}, &ok);
  _head = head(_repoDir);
  const Reply opened = open(_wc);
  QVERIFY2(opened.success, qPrintable(opened.error));
}

void SvnRepoManagerTest::initTestCase() {
  if (!toolsAvailable()) {
    QSKIP("svn, svnadmin or svnlook was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  useTestCredentialsKey();
  QVERIFY(SvnRepo::init());
  SvnRepo::initForCurrentThread();
  SvnRepo::Testing::installMalfunctionHandler();
  _cwd = QDir::currentPath();
  _manager = std::make_unique<SvnRepoManager::SvnRepoManager>();
  _manager->start();
}

void SvnRepoManagerTest::cleanupTestCase() {
  if (_manager) {
    QElapsedTimer timer;
    timer.start();
    _manager->stop();
    _manager.reset();
    QVERIFY2(timer.elapsed() < 30000,
             qPrintable(QString::number(timer.elapsed())));
  }
  SvnRepo::Testing::setConfigDirectory(QString());
}

void SvnRepoManagerTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  setConfigDir(_base + QStringLiteral("/svnconfig"));
  _wc = _base + QStringLiteral("/wc");
  _repoDir = _base + QStringLiteral("/repo-svnrepo");
  _head.clear();
  // A fresh container for every test (M2: a closed one can't be reopened).
  _uuid = newUuid();
}

void SvnRepoManagerTest::cleanup() {
  QDir::setCurrent(_cwd);
  if (_manager) {
    close();
  }
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
}

void SvnRepoManagerTest::requestsBeforeOpenFail() {
  const Reply status = REQUEST(Status, SvnRepoStatusCommand());
  QVERIFY(status.answered);
  QVERIFY(!status.success);
  QVERIFY(!status.error.isEmpty());
  // The message is about Subversion, not Git.
  QVERIFY2(!status.error.contains(QStringLiteral("Git")),
           qPrintable(status.error));

  const Reply closed = close();
  QVERIFY(closed.answered);
  QVERIFY(!closed.success);
}

void SvnRepoManagerTest::initOpenCommitAndClose() {
  SvnRepoInitRepoCommand init;
  init.repoName = QStringLiteral("repo");
  init.workingDir = _wc;
  init.defaultBranch = QStringLiteral("main");
  init.author = kAuthor;
  init.email = kEmail;
  const Reply initReply = REQUEST(InitRepo, init);
  QVERIFY2(initReply.success, qPrintable(initReply.error));
  // The working copy is checked out into workingDir itself.
  QCOMPARE(initReply.value.toString(), _wc);
  QVERIFY(QFileInfo(_wc + QStringLiteral("/.svn")).isDir());

  // The container that made the repository can open it.
  const Reply opened = open(_wc);
  QVERIFY2(opened.success, qPrintable(opened.error));

  QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), "a\n"));
  const Reply status = REQUEST(Status, SvnRepoStatusCommand());
  QVERIFY(status.success);
  QCOMPARE(status.value.value<SvnRepo::SvnStatus>().newFiles,
           QStringList{QStringLiteral("a.txt")});

  SvnRepoStageFilesCommand stage;
  stage.files = {QStringLiteral("a.txt")};
  QVERIFY(REQUEST(StageFiles, stage).success);
  SvnRepoCommitStagedCommand commit;
  commit.summary = QStringLiteral("Add a");
  commit.author = kAuthor;
  commit.email = kEmail;
  commit.cleanup = true;
  const Reply committed = REQUEST(CommitStaged, commit);
  QVERIFY2(committed.success, qPrintable(committed.error));
  QCOMPARE(changedPaths(_base + QStringLiteral("/repo-svnrepo"),
                        QStringLiteral("2")),
           QStringList{QStringLiteral("A trunk/a.txt")});

  SvnRepoGetBranchHistoryCommand history;
  history.branchName = QStringLiteral("trunk");
  history.branchType = SvnRepo::BranchType::Local;
  history.page = -1;
  const Reply page = REQUEST(GetBranchHistory, history);
  QVERIFY2(page.success, qPrintable(page.error));
  QCOMPARE(page.value.value<SvnRepo::SvnHistory>().commits.size(), 2);

  // Errors carry SvnRepo's error.
  SvnRepoCheckoutBranchCommand checkout;
  checkout.branchName = QStringLiteral("missing");
  checkout.branchType = SvnRepo::BranchType::Local;
  const Reply failed = REQUEST(CheckoutBranch, checkout);
  QVERIFY(failed.answered);
  QVERIFY(!failed.success);
  QVERIFY(!failed.error.isEmpty());
  QCOMPARE(failed.svnError, SvnRepo::SvnError::SwitchFailed);

  const Reply closed = close();
  QVERIFY2(closed.success, qPrintable(closed.error));

  // A request after the close is answered with an error.
  SvnRepoBlameCommand blame;
  blame.files = {QStringLiteral("a.txt")};
  const Reply afterClose = REQUEST(Blame, blame, 10000);
  QVERIFY(afterClose.answered);
  QVERIFY(!afterClose.success);
}

void SvnRepoManagerTest::reopenAfterClose() {
  openFixture();
  QVERIFY(close().success);

  // The container opens the working copy again.
  const Reply reopened = open(_wc);
  QVERIFY2(reopened.success, qPrintable(reopened.error));
}

void SvnRepoManagerTest::openFailures() {
  // Not a working copy: an error with SvnRepo's error, nothing stays open.
  const QString empty = _base + QStringLiteral("/empty");
  QVERIFY(QDir().mkpath(empty));
  const Reply failed = open(empty);
  QVERIFY(failed.answered);
  QVERIFY(!failed.success);
  QVERIFY(!failed.error.isEmpty());
  QCOMPARE(failed.svnError, SvnRepo::SvnError::ErrorWhileOpening);

  // The container can open a working copy after that.
  QTRY_VERIFY_WITH_TIMEOUT(
      !REQUEST(Status, SvnRepoStatusCommand(), 2000).success, 5000);
  openFixture();
  QVERIFY(REQUEST(Status, SvnRepoStatusCommand()).success);
}

void SvnRepoManagerTest::rejectedRequestsKeepTheOpenRepository_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("open") << QStringLiteral("open");
  QTest::newRow("clone") << QStringLiteral("clone");
  QTest::newRow("init") << QStringLiteral("init");
}

void SvnRepoManagerTest::rejectedRequestsKeepTheOpenRepository() {
  QFETCH(QString, kind);
  openFixture();

  // A second open, clone or init in the same container is refused.
  Reply refused;
  if (kind == QLatin1String("open")) {
    refused = open(_wc);
  } else if (kind == QLatin1String("clone")) {
    SvnRepoCloneCommand clone;
    clone.url = _rootUrl + QStringLiteral("/trunk");
    clone.destinationDir = _base + QStringLiteral("/clone");
    refused = REQUEST(CloneRepo, clone);
  } else {
    SvnRepoInitRepoCommand init;
    init.repoName = QStringLiteral("other");
    init.workingDir = _base + QStringLiteral("/other");
    init.defaultBranch = QStringLiteral("main");
    init.author = kAuthor;
    init.email = kEmail;
    refused = REQUEST(InitRepo, init);
  }
  QVERIFY(refused.answered);
  QVERIFY(!refused.success);

  // The working copy that was open stays open.
  QTest::qWait(500);
  const Reply status = REQUEST(Status, SvnRepoStatusCommand(), 10000);
  QVERIFY2(status.success, qPrintable(status.error));
}

void SvnRepoManagerTest::requestsAfterCloseAreRejected() {
  openFixture();
  // A sentinel working copy as the process's current directory, with a
  // local edit: a request run with an empty path would reach it.
  const QString sentinel = _base + QStringLiteral("/sentinel");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), sentinel));
  QVERIFY(writeFile(sentinel + QStringLiteral("/a.txt"), "precious\n"));
  QVERIFY(QDir::setCurrent(sentinel));
  QVERIFY(close().success);

  SvnRepoDiscardFilesCommand discard;
  discard.files = {QStringLiteral("a.txt")};
  const Reply discarded = REQUEST(DiscardFiles, discard, 10000);
  QVERIFY(discarded.answered);
  QVERIFY(!discarded.success);
  QCOMPARE(readFile(sentinel + QStringLiteral("/a.txt")),
           QByteArray("precious\n"));
  QDir::setCurrent(_cwd);
}

void SvnRepoManagerTest::unknownUuidDoesNotPoisonTheMap() {
  const QString unknown = newUuid();
  SvnRepoStatusCommand status;
  status.uuid = unknown;
  const Reply refused = REQUEST(Status, status);
  QVERIFY(refused.answered);
  QVERIFY(!refused.success);

  // The uuid is still free for an open, and nothing crashed.
  openFixture();
  QVERIFY(close().success);
  SvnRepoOpenCommand openCommand;
  openCommand.uuid = unknown;
  openCommand.path = _wc;
  const Reply opened = REQUEST(OpenRepo, openCommand);
  QVERIFY2(opened.success, qPrintable(opened.error));
  status.uuid = unknown;
  QVERIFY(REQUEST(Status, status).success);
  SvnRepoCloseCommand closeCommand;
  closeCommand.uuid = unknown;
  QVERIFY(REQUEST(CloseRepo, closeCommand).success);
}

void SvnRepoManagerTest::twoContainersAreIndependent() {
  openFixture();
  const QString first = _uuid;
  const QString otherWc = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), otherWc));
  QVERIFY(writeFile(otherWc + QStringLiteral("/b.txt"), "b\n"));
  const QString second = newUuid();
  QVERIFY(open(otherWc, second).success);

  QVERIFY(writeFile(_wc + QStringLiteral("/c.txt"), "c\n"));
  for (int i = 0; i < 3; ++i) {
    SvnRepoStatusCommand statusFirst;
    statusFirst.uuid = first;
    SvnRepoStatusCommand statusSecond;
    statusSecond.uuid = second;
    const Reply a = REQUEST(Status, statusFirst);
    const Reply b = REQUEST(Status, statusSecond);
    QCOMPARE(a.value.value<SvnRepo::SvnStatus>().newFiles,
             QStringList{QStringLiteral("c.txt")});
    QCOMPARE(b.value.value<SvnRepo::SvnStatus>().newFiles,
             QStringList{QStringLiteral("b.txt")});
  }

  // Closing one leaves the other working.
  QVERIFY(close().success);
  SvnRepoStatusCommand statusSecond;
  statusSecond.uuid = second;
  QVERIFY(REQUEST(Status, statusSecond).success);
  SvnRepoCloseCommand closeSecond;
  closeSecond.uuid = second;
  QVERIFY(REQUEST(CloseRepo, closeSecond).success);
}

void SvnRepoManagerTest::requestsOnOneUuidAreFifo() {
  openFixture();
  auto &eventManager = EventManager::EventManager::getInstance();

  // Stage, unstage, stage, then status, sent without waiting: they run in
  // order, so the file ends up staged.
  QVERIFY(writeFile(_wc + QStringLiteral("/n.txt"), "n\n"));
  QVector<QPair<int, QString>> waits;
  const auto send = [&](auto command, EventType type) {
    command.uuid = _uuid;
    command.syncUuid = newUuid();
    const int token = eventManager.startWait();
    EventManager::Event event(type, command.syncUuid);
    event.addData(command);
    eventManager.send(event);
    waits << qMakePair(token, command.syncUuid);
  };
  SvnRepoStageFilesCommand stage;
  stage.files = {QStringLiteral("n.txt")};
  SvnRepoUnstageFilesCommand unstage;
  unstage.files = {QStringLiteral("n.txt")};
  send(stage, EventType::SvnRepoManagerStageFiles);
  send(unstage, EventType::SvnRepoManagerUnstageFiles);
  send(stage, EventType::SvnRepoManagerStageFiles);
  for (const auto &[token, syncUuid] : waits) {
    const auto [ok, answer] = eventManager.waitForAnswer(
        token, syncUuid,
        {EventType::SvnRepoManagerStageFilesResult,
         EventType::SvnRepoManagerStageFilesError,
         EventType::SvnRepoManagerUnstageFilesResult,
         EventType::SvnRepoManagerUnstageFilesError},
        20000);
    QVERIFY(ok);
    Q_UNUSED(answer);
  }
  const Reply status = REQUEST(Status, SvnRepoStatusCommand());
  QCOMPARE(status.value.value<SvnRepo::SvnStatus>().newStagedFiles,
           QStringList{QStringLiteral("n.txt")});
}

#define ROW(Name, Type, ...)                                                   \
  if (name == QLatin1String(#Name)) {                                          \
    Type command;                                                              \
    __VA_ARGS__;                                                               \
    return REQUEST(Name, command, timeout);                                    \
  }

Reply SvnRepoManagerTest::sendRequest(const QString &name, int timeout) {
  const QString a = QStringLiteral("a.txt");
  ROW(AbortMerge, SvnRepoAbortMergeCommand)
  ROW(AddRemote, SvnRepoAddRemoteCommand, command.remoteName = "r1",
      command.fetchUrl = _base + "/missing")
  // Inside the test folder: a missing repository.
  ROW(AddSubmodule, SvnRepoAddSubmoduleCommand,
      command.url = fileUrl(_base + "/missing"), command.name = "sub")
  ROW(AddTarget, SvnRepoAddTargetCommand, command.remoteName = "origin",
      command.targetName = "t", command.fetchUrl = "https://example.invalid/x")
  ROW(ApplyPatch, SvnRepoApplyPatchCommand)
  ROW(ApplyStash, SvnRepoApplyStashCommand, command.index = 0)
  ROW(Blame, SvnRepoBlameCommand, command.files = {a})
  ROW(CheckoutBranch, SvnRepoCheckoutBranchCommand,
      command.branchName = "trunk",
      command.branchType = SvnRepo::BranchType::Local)
  ROW(CheckoutCommit, SvnRepoCheckoutCommitCommand, command.commit = _head)
  ROW(CherryPick, SvnRepoCherryPickCommand, command.commit = _head,
      command.author = kAuthor, command.email = kEmail)
  ROW(CloneRepo, SvnRepoCloneCommand,
      command.url = fileUrl(_base + "/missing"),
      command.destinationDir = _base + "/clone")
  ROW(CommitStaged, SvnRepoCommitStagedCommand, command.author = kAuthor,
      command.email = kEmail, command.summary = "S")
  ROW(CreateBranchFromCommit, SvnRepoCreateBranchFromCommitCommand,
      command.branchName = "b1", command.commit = _head)
  ROW(CreatePatchFromCommit, SvnRepoCreatePatchFromCommitCommand,
      command.commit = _head)
  ROW(CreateTagFromCommit, SvnRepoCreateTagFromCommitCommand,
      command.tagName = "t1", command.commit = _head, command.author = kAuthor,
      command.email = kEmail, command.message = "Tag")
  ROW(DeleteAllStashes, SvnRepoDeleteAllStashesCommand)
  ROW(DeleteBranch, SvnRepoDeleteBranchCommand, command.branchName = "nope")
  ROW(DeleteRemoteBranch, SvnRepoDeleteRemoteBranchCommand,
      command.branchName = "nope")
  ROW(DeleteRemoteTag, SvnRepoDeleteRemoteTagCommand, command.tagName = "nope")
  ROW(DeleteStash, SvnRepoDeleteStashCommand, command.index = 0)
  // A path inside the test folder: deleteSubmodule removes it.
  ROW(DeleteSubmodule, SvnRepoDeleteSubmoduleCommand,
      command.submoduleName = "nope", command.submodulePath = _base + "/nope")
  ROW(DeleteTag, SvnRepoDeleteTagCommand, command.tagName = "nope")
  ROW(DeleteTarget, SvnRepoDeleteTargetCommand, command.remoteName = "origin",
      command.targetName = "t")
  ROW(DiffFiles, SvnRepoDiffFilesCommand, command.files = {a})
  ROW(DiffFilesFromCommit, SvnRepoDiffFilesFromCommitCommand,
      command.files = {a}, command.commit = _head)
  ROW(DiscardFiles, SvnRepoDiscardFilesCommand, command.files = {a})
  ROW(EditTarget, SvnRepoEditTargetCommand, command.remoteName = "origin",
      command.targetName = "t")
  ROW(FetchAll, SvnRepoFetchAllCommand)
  ROW(FetchRemote, SvnRepoFetchRemoteCommand, command.remoteName = "origin")
  ROW(GetActiveBranch, SvnRepoGetActiveBranchCommand,
      command.branchType = SvnRepo::BranchType::Local)
  ROW(GetBranchHistory, SvnRepoGetBranchHistoryCommand,
      command.branchName = "trunk",
      command.branchType = SvnRepo::BranchType::Local)
  ROW(GetCommitBranches, SvnRepoGetCommitBranchesCommand,
      command.commitOid = _head)
  ROW(GetCommitHeads, SvnRepoGetCommitHeadsCommand, command.commitOid = _head)
  ROW(GetCommitIsHead, SvnRepoGetCommitIsHeadCommand, command.commitOid = _head)
  ROW(GetCommitStatus, SvnRepoGetCommitStatusCommand, command.commitOid = _head)
  ROW(GetCommitTags, SvnRepoGetCommitTagsCommand, command.commitOid = _head)
  ROW(GetFileSizeFromCommit, SvnRepoGetFileSizeFromCommitCommand,
      command.filePath = a, command.commit = _head)
  ROW(GetIsMergeInProgress, SvnRepoGetIsMergeInProgressCommand)
  ROW(GetStashStatus, SvnRepoGetStashStatusCommand, command.index = 0)
  ROW(GetSubmodulePath, SvnRepoGetSubmodulePathCommand,
      command.submoduleName = "nope")
  ROW(GetUpstream, SvnRepoGetUpstreamCommand, command.localBranchName = "trunk")
  ROW(IgnoreFiles, SvnRepoIgnoreFilesCommand, command.files = {"x.log"})
  ROW(InitRepo, SvnRepoInitRepoCommand, command.repoName = "other",
      command.workingDir = _base + "/other", command.defaultBranch = "main",
      command.author = kAuthor, command.email = kEmail)
  ROW(ListConflicts, SvnRepoListConflictsCommand, command.filePath = a)
  ROW(ListLocalBranches, SvnRepoListLocalBranchesCommand)
  ROW(ListRemoteBranches, SvnRepoListRemoteBranchesCommand)
  ROW(ListRemoteBranchesWithStatus, SvnRepoListRemoteBranchesWithStatusCommand)
  ROW(ListRemoteTags, SvnRepoListRemoteTagsCommand)
  ROW(ListRemotes, SvnRepoListRemotesCommand)
  ROW(ListStashes, SvnRepoListStashesCommand)
  ROW(ListSubmodules, SvnRepoListSubmodulesCommand)
  ROW(ListSubmodulesWithStatus, SvnRepoListSubmodulesWithStatusCommand)
  ROW(ListTags, SvnRepoListTagsCommand)
  ROW(Merge, SvnRepoMergeCommand, command.sourceBranch = "trunk",
      command.destinationBranch = "trunk", command.author = kAuthor,
      command.email = kEmail)
  ROW(OpenRepo, SvnRepoOpenCommand, command.path = _wc)
  ROW(Pull, SvnRepoPullCommand)
  ROW(Push, SvnRepoPushCommand)
  ROW(PushAllTags, SvnRepoPushAllTagsCommand)
  ROW(PushTag, SvnRepoPushTagCommand, command.tagName = "nope")
  ROW(Rebase, SvnRepoRebaseCommand, command.sourceBranch = "trunk",
      command.destinationBranch = "trunk", command.author = kAuthor,
      command.email = kEmail)
  ROW(RemoveRemote, SvnRepoRemoveRemoteCommand, command.remoteName = "nope")
  ROW(RenameBranch, SvnRepoRenameBranchCommand, command.oldBranchName = "nope",
      command.newBranchName = "x")
  ROW(RenameTag, SvnRepoRenameTagCommand, command.oldTagName = "nope",
      command.newTagName = "x")
  ROW(Reset, SvnRepoResetCommand, command.commit = _head,
      command.resetType = SvnRepo::ResetType::Mixed)
  ROW(ResolveTreeConflict, SvnRepoResolveTreeConflictCommand,
      command.filePath = a, command.keepLocal = true)
  ROW(Revert, SvnRepoRevertCommand, command.commit = _head,
      command.author = kAuthor, command.email = kEmail,
      command.message = "Revert")
  ROW(SetDefaultRemote, SvnRepoSetDefaultRemoteCommand,
      command.remoteName = "nope")
  ROW(SetDefaultTarget, SvnRepoSetDefaultTargetCommand,
      command.remoteName = "origin", command.targetName = "t")
  ROW(SetUpstream, SvnRepoSetUpstreamCommand, command.remoteName = "origin",
      command.remoteBranchName = "trunk", command.localBranchName = "trunk")
  ROW(StageFiles, SvnRepoStageFilesCommand, command.files = {a})
  ROW(Stash, SvnRepoStashCommand, command.note = "note")
  ROW(Status, SvnRepoStatusCommand)
  ROW(SubmoduleUpdate, SvnRepoSubmoduleUpdateCommand,
      command.submoduleName = "nope")
  ROW(UnstageFiles, SvnRepoUnstageFilesCommand, command.files = {a})
  ROW(UpdateRemote, SvnRepoUpdateRemoteCommand, command.remoteName = "nope",
      command.newRemoteName = "x")
  qWarning() << "No request for" << name;
  return {};
}

void SvnRepoManagerTest::everyRequestIsAnswered_data() {
  QTest::addColumn<QString>("name");
  for (const QString &name : kRequests) {
    QTest::newRow(qPrintable(name)) << name;
  }
}

void SvnRepoManagerTest::everyRequestIsAnswered() {
  QFETCH(QString, name);
  openFixture();

  // Every request gets a result or an error, never silence (the bridge would
  // wait until its timeout), and an error says what went wrong.
  const Reply reply = sendRequest(name, 20000);
  QVERIFY(reply.answered);
  QVERIFY2(reply.success || !reply.error.isEmpty(),
           "an error without a message");
}

void SvnRepoManagerTest::queriesWithEmptyResults_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("GetCommitTags") << QStringLiteral("GetCommitTags");
  QTest::newRow("GetCommitBranches") << QStringLiteral("GetCommitBranches");
  QTest::newRow("GetCommitHeads") << QStringLiteral("GetCommitHeads");
}

void SvnRepoManagerTest::queriesWithEmptyResults() {
  QFETCH(QString, name);
  openFixture();

  // r1 (the layout) has no tag, touches no branch's files and heads nothing
  // once a.txt was committed: an empty result, not an error.
  Reply reply;
  if (name == QLatin1String("GetCommitTags")) {
    SvnRepoGetCommitTagsCommand command;
    command.commitOid = QStringLiteral("1");
    reply = REQUEST(GetCommitTags, command);
  } else if (name == QLatin1String("GetCommitBranches")) {
    SvnRepoGetCommitBranchesCommand command;
    command.commitOid = QStringLiteral("0");
    reply = REQUEST(GetCommitBranches, command);
  } else {
    SvnRepoGetCommitHeadsCommand command;
    command.commitOid = QStringLiteral("1");
    reply = REQUEST(GetCommitHeads, command);
  }
  QVERIFY(reply.answered);
  QVERIFY2(reply.success, qPrintable(reply.error));
  if (reply.success) {
    QVERIFY(reply.value.toStringList().isEmpty());
  }
}

void SvnRepoManagerTest::errorsCarryTheSvnError() {
  openFixture();
  SvnRepoCheckoutCommitCommand checkout;
  checkout.commit = QStringLiteral("not a revision");
  const Reply failed = REQUEST(CheckoutCommit, checkout);
  QVERIFY(!failed.success);
  QCOMPARE(failed.svnError, SvnRepo::SvnError::InvalidRevision);
  QVERIFY(!failed.error.isEmpty());

  // A Subversion error keeps its code, and there is a text for it.
  SvnRepoCheckoutBranchCommand branch;
  branch.branchName = QStringLiteral("missing");
  branch.branchType = SvnRepo::BranchType::Local;
  const Reply svnFailed = REQUEST(CheckoutBranch, branch);
  QVERIFY(!svnFailed.success);
  QVERIFY2(svnFailed.errorCode > 0, qPrintable(QString::number(svnFailed.errorCode)));
  QCOMPARE(decodeSvnError(svnFailed.errorCode), svnFailed.errorCode);
  QVERIFY2(!translateSvnError(svnFailed.errorCode)
                .startsWith(QStringLiteral("An unexpected error")),
           qPrintable(translateSvnError(svnFailed.errorCode)));

  // Errors of the component itself have no SvnRepo error.
  SvnRepoStatusCommand status;
  status.uuid = newUuid();
  const Reply notOpen = REQUEST(Status, status);
  QCOMPARE(notOpen.svnError, SvnRepo::SvnError::Unknown);
  QCOMPARE(notOpen.errorCode, -1);
}

void SvnRepoManagerTest::mergeErrorIsReported() {
  openFixture();
  // Conflicting changes on trunk and branches/feature.
  bool ok = false;
  svn({QStringLiteral("copy"), QStringLiteral("-m"), QStringLiteral("Branch"),
       _rootUrl + QStringLiteral("/trunk"),
       _rootUrl + QStringLiteral("/branches/feature")},
      &ok);
  const QString feature = _base + QStringLiteral("/feature");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/branches/feature"), feature));
  QVERIFY(writeFile(feature + QStringLiteral("/a.txt"), "feature\n"));
  QVERIFY(svnCommitAll(feature, QStringLiteral("Feature")));
  QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), "main\n"));
  QVERIFY(svnCommitAll(_wc, QStringLiteral("Main")));

  SvnRepoMergeCommand merge;
  merge.sourceBranch = QStringLiteral("feature");
  merge.destinationBranch = QStringLiteral("trunk");
  merge.author = kAuthor;
  merge.email = kEmail;
  const Reply reply = REQUEST(Merge, merge);
  QVERIFY(reply.answered);
  QVERIFY(!reply.success);
  QCOMPARE(reply.svnError, SvnRepo::SvnError::MergeFailedWithConflicts);
}

void SvnRepoManagerTest::gitOnlyRequestsAreUnsupported() {
  openFixture();
  SvnRepoRebaseCommand rebase;
  rebase.sourceBranch = QStringLiteral("trunk");
  rebase.destinationBranch = QStringLiteral("trunk");
  const Reply rebased = REQUEST(Rebase, rebase);
  QVERIFY(!rebased.success);
  QCOMPARE(rebased.svnError, SvnRepo::SvnError::UnsupportedOperation);
  QVERIFY(!rebased.error.isEmpty());

  SvnRepoResetCommand reset;
  reset.commit = _head;
  reset.resetType = SvnRepo::ResetType::Hard;
  const Reply wasReset = REQUEST(Reset, reset);
  QVERIFY(!wasReset.success);
  QCOMPARE(wasReset.svnError, SvnRepo::SvnError::UnsupportedOperation);
  QVERIFY(!wasReset.error.isEmpty());
}

void SvnRepoManagerTest::applyStashResultMap() {
  openFixture();
  QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), "stashed\n"));
  SvnRepoStashCommand stash;
  stash.note = QStringLiteral("note");
  QVERIFY(REQUEST(Stash, stash).success);
  QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), "mine\n"));
  QVERIFY(SvnRepo::stageFiles(_wc, {QStringLiteral("a.txt")}));
  QVERIFY(SvnRepo::commitStaged(_wc, QStringLiteral("Mine"), QString(),
                                kAuthor, kEmail, true, false, false));

  SvnRepoApplyStashCommand apply;
  apply.index = 0;
  const Reply applied = REQUEST(ApplyStash, apply);
  QVERIFY2(applied.success, qPrintable(applied.error));
  const QVariantMap result = applied.value.toMap();
  QVERIFY(result.value(QStringLiteral("success")).toBool());
  QVERIFY(result.value(QStringLiteral("hasConflicts")).toBool());
  QVERIFY(result.value(QStringLiteral("conflictedFiles"))
              .toMap()
              .value(QStringLiteral("a.txt"))
              .toString()
              .contains(QStringLiteral("<<<<<<<")));
}

void SvnRepoManagerTest::listConflictsReportsTreeConflict() {
  openFixture();
  const QString other = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), other));
  bool ok = false;
  svnIn(other, {QStringLiteral("delete"), QStringLiteral("a.txt")}, &ok);
  QVERIFY(svnCommitAll(other, QStringLiteral("Delete")));
  QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), "mine\n"));
  svnIn(_wc, {QStringLiteral("update"), QStringLiteral("--accept"),
              QStringLiteral("postpone")});

  SvnRepoListConflictsCommand list;
  list.filePath = QStringLiteral("a.txt");
  const Reply reply = REQUEST(ListConflicts, list);
  QVERIFY2(reply.success, qPrintable(reply.error));
  QVERIFY(reply.value.value<SvnRepoListConflictsCommandResult>().treeConflict);
}

void SvnRepoManagerTest::cloneReportsProgress() {
  openFixture();
  close();

  QMutex mutex;
  QVector<GitRepo::CloneProgress> progress;
  const QString cloneUuid = newUuid();
  auto &eventManager = EventManager::EventManager::getInstance();
  const EventManager::CallbackType handler = [&](EventManager::Event event) {
    const auto result = event.data<GitRepoManager::Structs::GitRepoResult>();
    QMutexLocker locker(&mutex);
    if (result.uuid == cloneUuid) {
      progress << result.value.value<GitRepo::CloneProgress>();
    }
  };
  eventManager.subscribe(
      {EventType::SvnRepoManagerCloneRepoProgress, this, handler});
  const auto unsubscribe = qScopeGuard([&] {
    eventManager.unsubscribe(
        {EventType::SvnRepoManagerCloneRepoProgress, this, handler});
  });

  SvnRepoCloneCommand clone;
  clone.uuid = cloneUuid;
  clone.url = _rootUrl + QStringLiteral("/trunk");
  clone.destinationDir = _base + QStringLiteral("/clone");
  const Reply cloned = REQUEST(CloneRepo, clone);
  QVERIFY2(cloned.success, qPrintable(cloned.error));
  QCOMPARE(cloned.value.toString(), clone.destinationDir);
  QVERIFY(QFileInfo::exists(clone.destinationDir + QStringLiteral("/a.txt")));
  QTRY_VERIFY([&] {
    QMutexLocker locker(&mutex);
    return !progress.isEmpty() && progress.last().finished;
  }());

  // The clone's container is free again, and a failed clone is an error
  // that leaves nothing behind.
  clone.url = fileUrl(_base + QStringLiteral("/missing"));
  clone.destinationDir = _base + QStringLiteral("/clone2");
  const Reply failed = REQUEST(CloneRepo, clone);
  QVERIFY(!failed.success);
  QCOMPARE(failed.svnError, SvnRepo::SvnError::CheckoutFailed);
  QVERIFY(!QFileInfo::exists(clone.destinationDir));
}

void SvnRepoManagerTest::initThenOpenImmediately() {
  // The bridge opens the working copy right after the init's answer.
  for (int i = 0; i < 3; ++i) {
    const QString uuid = newUuid();
    SvnRepoInitRepoCommand init;
    init.uuid = uuid;
    init.repoName = QStringLiteral("repo%1").arg(i);
    init.workingDir = _base + QStringLiteral("/wc%1").arg(i);
    init.defaultBranch = QStringLiteral("main");
    init.author = kAuthor;
    init.email = kEmail;
    QVERIFY(REQUEST(InitRepo, init).success);
    const Reply opened = open(init.workingDir, uuid);
    QVERIFY2(opened.success, qPrintable(opened.error));
    SvnRepoCloseCommand closeCommand;
    closeCommand.uuid = uuid;
    REQUEST(CloseRepo, closeCommand, 5000);
  }
}

void SvnRepoManagerTest::closeAnswersQueuedRequests() {
  openFixture();
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

  QVector<QPair<int, QString>> statusWaits;
  for (int i = 0; i < 5; ++i) {
    statusWaits << send(SvnRepoStatusCommand(), EventType::SvnRepoManagerStatus);
  }
  const auto closeWait =
      send(SvnRepoCloseCommand(), EventType::SvnRepoManagerCloseRepo);
  const auto [closed, closeAnswer] = eventManager.waitForAnswer(
      closeWait.first, closeWait.second,
      {EventType::SvnRepoManagerCloseRepoResult,
       EventType::SvnRepoManagerCloseRepoError},
      60000);
  QVERIFY(closed);
  QCOMPARE(closeAnswer.type(), EventType::SvnRepoManagerCloseRepoResult);

  // Every request queued before the close is answered too.
  int answered = 0;
  for (const auto &[token, syncUuid] : statusWaits) {
    const auto [ok, answer] = eventManager.waitForAnswer(
        token, syncUuid,
        {EventType::SvnRepoManagerStatusResult,
         EventType::SvnRepoManagerStatusError},
        2000);
    Q_UNUSED(answer);
    answered += ok ? 1 : 0;
  }
  QCOMPARE(answered, 5);
}

void SvnRepoManagerTest::closeDuringLongHistory() {
  openFixture();
  for (int i = 0; i < 30; ++i) {
    QVERIFY(writeFile(_wc + QStringLiteral("/a.txt"), QByteArray::number(i)));
    QVERIFY(svnCommitAll(_wc, QStringLiteral("C%1").arg(i)));
  }

  // A history request and a close right behind it: both answered, nothing
  // hangs.
  auto &eventManager = EventManager::EventManager::getInstance();
  SvnRepoGetBranchHistoryCommand history;
  history.uuid = _uuid;
  history.syncUuid = newUuid();
  history.branchName = QStringLiteral("trunk");
  history.branchType = SvnRepo::BranchType::Local;
  history.page = -1;
  const int token = eventManager.startWait();
  EventManager::Event event(EventType::SvnRepoManagerGetBranchHistory,
                            history.syncUuid);
  event.addData(history);
  eventManager.send(event);
  const Reply closed = close(60000);
  QVERIFY(closed.answered);
  const auto [answered, answer] = eventManager.waitForAnswer(
      token, history.syncUuid,
      {EventType::SvnRepoManagerGetBranchHistoryResult,
       EventType::SvnRepoManagerGetBranchHistoryError},
      10000);
  QVERIFY(answered);
  if (answer.type() == EventType::SvnRepoManagerGetBranchHistoryError) {
    QVERIFY(!answer.data<SvnRepoError>().message.isEmpty());
  }
}

void SvnRepoManagerTest::stopWithOpenContexts() {
  // Last test: stops the manager with two open working copies and requests
  // in flight. stop() joins every worker thread without hanging.
  openFixture();
  const QString first = _uuid;
  const QString otherWc = _base + QStringLiteral("/other");
  QVERIFY(secondWc(_rootUrl + QStringLiteral("/trunk"), otherWc));
  _uuid = newUuid();
  QVERIFY(open(otherWc).success);

  auto &eventManager = EventManager::EventManager::getInstance();
  for (const QString &uuid : {first, _uuid}) {
    for (int i = 0; i < 3; ++i) {
      SvnRepoStatusCommand status;
      status.uuid = uuid;
      status.syncUuid = newUuid();
      EventManager::Event event(EventType::SvnRepoManagerStatus,
                                status.syncUuid);
      event.addData(status);
      eventManager.send(event);
    }
  }

  QElapsedTimer timer;
  timer.start();
  _manager->stop();
  _manager.reset();
  QVERIFY2(timer.elapsed() < 30000,
           qPrintable(QString::number(timer.elapsed())));

  // Nothing answers any more.
  QVERIFY(!REQUEST(Status, SvnRepoStatusCommand(), 500).answered);
}

QTEST_GUILESS_MAIN(SvnRepoManagerTest)

#include "tst_svnrepomanager.moc"
