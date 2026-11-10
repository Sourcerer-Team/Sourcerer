// MIT License
// Copyright (c) 2026 Sourcerer

// End to end tests of the HgRepoManager component: requests are sent as
// events and answered by the component's worker threads, as RepoContainer
// does.

#include "HgAuthServer.h"

#include <EventManager.h>
#include <GitCredentialStorage.h>
#include <HgRepo.h>
#include <HgRepoManager.h>
#include <HgRepoManagerStructs.h>
#include <SshKnownHosts.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <memory>

using namespace HgRepoManager::Structs;
using EventManager::EventType;

namespace {

const QString kAuthor = QStringLiteral("Tester");
const QString kEmail = QStringLiteral("tester@example.com");

bool writeFile(const QString &path, const QByteArray &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(content) >= 0;
}

// Runs a command line tool in dir, for setting up repositories.
bool runTool(const QString &program, const QStringList &args,
             const QString &dir, QByteArray *output = nullptr) {
  QProcess process;
  process.setWorkingDirectory(dir);
  process.start(program, args);
  if (!process.waitForFinished(120 * 1000) ||
      process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    qWarning() << program << args << process.readAllStandardError();
    return false;
  }
  if (output) {
    *output = process.readAllStandardOutput();
  }
  return true;
}

struct Reply {
  bool answered = false;
  bool success = false;
  QVariant value;
  QString error;
};

} // namespace

class HgRepoManagerTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void cleanupTestCase();
  void init();
  void cleanup();

  void requestsBeforeOpenFail();
  void initOpenCommitAndClose();
  void openTwiceFails();
  void cloneReportsProgress();
  void gitSubrepo();
  void subversionSubrepo();
  void closeRefusesQueuedRequests();
  void credentialsOverlay();
  void sshKeyPassphrase();
  void knownHosts();

private:
  // Sends command as HgRepoManager<Name> and waits for its answer.
  template <typename Command>
  Reply request(EventType requestType, EventType resultType,
                EventType errorType, Command command);
  Reply open(const QString &path);
  Reply commitAll(const QString &summary);

  std::unique_ptr<HgRepoManager::HgRepoManager> _manager;
  std::unique_ptr<QTemporaryDir> _tmp;
  QString _uuid;
};

template <typename Command>
Reply HgRepoManagerTest::request(EventType requestType, EventType resultType,
                                 EventType errorType, Command command) {
  auto &eventManager = EventManager::EventManager::getInstance();

  command.uuid = _uuid;
  command.syncUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);

  const int token = eventManager.startWait();
  EventManager::Event event(requestType, command.syncUuid);
  event.addData(command);
  eventManager.send(event);

  const auto [answered, answer] = eventManager.waitForAnswer(
      token, command.syncUuid, {resultType, errorType}, 120 * 1000);

  Reply reply;
  reply.answered = answered;
  if (!answered) {
    return reply;
  }
  reply.success = answer.type() == resultType;
  if (reply.success) {
    reply.value = answer.template data<HgRepoResult>().value;
  } else {
    reply.error = answer.template data<HgRepoError>().message;
  }
  return reply;
}

#define REQUEST(name, command)                                                 \
  request(EventType::HgRepoManager##name,                                      \
          EventType::HgRepoManager##name##Result,                              \
          EventType::HgRepoManager##name##Error, command)

Reply HgRepoManagerTest::open(const QString &path) {
  HgRepoOpenRepoCommand command;
  command.path = path;
  command.author = kAuthor;
  command.email = kEmail;
  return REQUEST(OpenRepo, command);
}

Reply HgRepoManagerTest::commitAll(const QString &summary) {
  const Reply status = REQUEST(Status, HgRepoStatusCommand());
  if (!status.success) {
    return status;
  }
  const auto files = status.value.value<GitRepo::GitStatus>();
  HgRepoStageFilesCommand stage;
  stage.files = files.newFiles + files.modifiedFiles + files.deletedFiles;
  if (!stage.files.isEmpty() && !REQUEST(StageFiles, stage).success) {
    return Reply();
  }
  HgRepoCommitStagedCommand commit;
  commit.summary = summary;
  commit.author = kAuthor;
  commit.email = kEmail;
  return REQUEST(CommitStaged, commit);
}

void HgRepoManagerTest::initTestCase() {
  if (HgRepo::findExecutable().isEmpty()) {
    QSKIP("hg was not found");
  }
  _manager = std::make_unique<HgRepoManager::HgRepoManager>();
  _manager->start();
}

void HgRepoManagerTest::cleanupTestCase() {
  if (_manager) {
    _manager->stop();
    _manager.reset();
  }
}

void HgRepoManagerTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
}

void HgRepoManagerTest::cleanup() {
  REQUEST(CloseRepo, HgRepoCloseRepoCommand());

  // Git and Subversion keep read-only files, which QTemporaryDir can't
  // remove on Windows.
  QDirIterator it(_tmp->path(), QDir::Files | QDir::Hidden | QDir::System,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    QFile::setPermissions(it.next(), QFile::ReadOwner | QFile::WriteOwner);
  }
  _tmp.reset();
}

void HgRepoManagerTest::requestsBeforeOpenFail() {
  const Reply reply = REQUEST(Status, HgRepoStatusCommand());
  QVERIFY(reply.answered);
  QVERIFY(!reply.success);
  QVERIFY(reply.error.contains(QStringLiteral("not open")));
}

void HgRepoManagerTest::initOpenCommitAndClose() {
  HgRepoInitRepoCommand init;
  init.repoName = QStringLiteral("repo");
  init.workingDir = _tmp->path();
  init.author = kAuthor;
  init.email = kEmail;
  const Reply initReply = REQUEST(InitRepo, init);
  QVERIFY2(initReply.success, qPrintable(initReply.error));
  const QString root = initReply.value.toString();
  QVERIFY(QFileInfo(root + QStringLiteral("/.hg")).isDir());

  const Reply opened = open(root);
  QVERIFY2(opened.success, qPrintable(opened.error));
  QCOMPARE(opened.value.toString(), QDir::cleanPath(root));

  QVERIFY(writeFile(root + QStringLiteral("/a.txt"), "a\n"));
  const Reply status = REQUEST(Status, HgRepoStatusCommand());
  QVERIFY(status.success);
  QCOMPARE(status.value.value<GitRepo::GitStatus>().newFiles,
           QStringList{QStringLiteral("a.txt")});

  const Reply committed = commitAll(QStringLiteral("Add a"));
  QVERIFY2(committed.success, qPrintable(committed.error));

  HgRepoGetBranchHistoryCommand history;
  history.branchName = QStringLiteral(ALL_BRANCHES);
  history.branchType = GitRepo::BranchType::All;
  const Reply page = REQUEST(GetBranchHistory, history);
  QVERIFY(page.success);
  const auto commits = page.value.value<GitRepo::GitHistory>();
  QCOMPARE(commits.commits.size(), 1);
  QCOMPARE(commits.commitInfos.first().summary, QStringLiteral("Add a"));

  HgRepoListLocalBranchesCommand branches;
  const Reply list = REQUEST(ListLocalBranches, branches);
  QVERIFY(list.success);
  const auto infos = list.value.value<QVector<HgRepo::HgBranchInfo>>();
  QCOMPARE(infos.size(), 1);
  QCOMPARE(infos[0].name, QStringLiteral("default"));

  // Errors carry HgRepo's message.
  HgRepoCheckoutBranchCommand checkout;
  checkout.branchName = QStringLiteral("missing");
  checkout.branchType = GitRepo::BranchType::Local;
  const Reply failed = REQUEST(CheckoutBranch, checkout);
  QVERIFY(failed.answered);
  QVERIFY(!failed.success);
  QVERIFY(!failed.error.isEmpty());

  const Reply closed = REQUEST(CloseRepo, HgRepoCloseRepoCommand());
  QVERIFY2(closed.success, qPrintable(closed.error));
  QVERIFY(!REQUEST(Status, HgRepoStatusCommand()).success);

  // The same container can open the repository again right away.
  QVERIFY2(open(root).success, "reopen");
  QVERIFY(REQUEST(Status, HgRepoStatusCommand()).success);
}

void HgRepoManagerTest::openTwiceFails() {
  const QString root = _tmp->path() + QStringLiteral("/repo");
  QVERIFY(HgRepo::initRepository(QStringLiteral("repo"), _tmp->path(),
                                 QString(), kAuthor, kEmail));
  QVERIFY(open(root).success);
  const Reply again = open(root);
  QVERIFY(!again.success);
  QVERIFY(REQUEST(CloseRepo, HgRepoCloseRepoCommand()).success);

  QTemporaryDir notARepository;
  _uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const Reply failed = open(notARepository.path());
  QVERIFY(!failed.success);
  QVERIFY(failed.error.contains(QStringLiteral("not a Mercurial repository")));
  // A failed open leaves nothing open.
  QVERIFY(!REQUEST(Status, HgRepoStatusCommand()).success);
}

void HgRepoManagerTest::cloneReportsProgress() {
  const QString source = _tmp->path() + QStringLiteral("/source");
  QVERIFY(HgRepo::initRepository(QStringLiteral("source"), _tmp->path(),
                                 QString(), kAuthor, kEmail));
  for (int i = 0; i < 3; ++i) {
    QVERIFY(writeFile(source + QStringLiteral("/f%1.txt").arg(i), "x\n"));
  }
  QVERIFY(runTool(HgRepo::findExecutable(),
                  {QStringLiteral("commit"), QStringLiteral("-A"),
                   QStringLiteral("-m"), QStringLiteral("Files")},
                  source));

  QMutex mutex;
  QVector<GitRepo::CloneProgress> progress;
  auto &eventManager = EventManager::EventManager::getInstance();
  const EventManager::EventCallback callback{
      EventType::HgRepoManagerCloneRepoProgress, this,
      [&](const EventManager::Event &event) {
        const auto result = event.data<HgRepoResult>();
        QMutexLocker locker(&mutex);
        if (result.uuid == _uuid) {
          progress << result.value.value<GitRepo::CloneProgress>();
        }
      }};
  eventManager.subscribe(callback);

  HgRepoCloneRepoCommand clone;
  clone.url = source;
  clone.destinationDir = _tmp->path() + QStringLiteral("/clone");
  const Reply cloned = REQUEST(CloneRepo, clone);

  // Progress events are delivered before the result on the same queue.
  QTest::qWait(200);
  eventManager.unsubscribe(callback);

  QVERIFY2(cloned.success, qPrintable(cloned.error));
  QVERIFY(QFileInfo::exists(clone.destinationDir + QStringLiteral("/f2.txt")));

  QMutexLocker locker(&mutex);
  QVERIFY(!progress.isEmpty());
  QVERIFY(progress.first().started);
  QVERIFY(progress.last().finished);
  QVERIFY(!progress.last().error);
  QCOMPARE(progress.last().uuid, _uuid);
}

void HgRepoManagerTest::gitSubrepo() {
  const QString git = QStandardPaths::findExecutable(QStringLiteral("git"));
  if (git.isEmpty()) {
    QSKIP("git was not found, Mercurial needs it for Git subrepos");
  }

  // A Git repository to take the subrepo from.
  const QString source = _tmp->path() + QStringLiteral("/vendor");
  QDir().mkpath(source);
  QVERIFY(runTool(git, {QStringLiteral("init"), QStringLiteral("-q")}, source));
  QVERIFY(writeFile(source + QStringLiteral("/v.txt"), "v1\n"));
  const QStringList identity = {
      QStringLiteral("-c"), QStringLiteral("user.name=Tester"),
      QStringLiteral("-c"), QStringLiteral("user.email=tester@example.com")};
  QVERIFY(runTool(git, {QStringLiteral("add"), QStringLiteral(".")}, source));
  QVERIFY(runTool(
      git,
      identity + QStringList{QStringLiteral("commit"), QStringLiteral("-q"),
                             QStringLiteral("-m"), QStringLiteral("v1")},
      source));

  QVERIFY(HgRepo::initRepository(QStringLiteral("repo"), _tmp->path(),
                                 QString(), kAuthor, kEmail));
  const QString root = _tmp->path() + QStringLiteral("/repo");
  QVERIFY(writeFile(root + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY(open(root).success);
  QVERIFY(commitAll(QStringLiteral("Root")).success);

  HgRepoAddSubmoduleCommand add;
  add.url = QDir::fromNativeSeparators(source);
  add.name = QStringLiteral("vendor");
  add.kind = HgRepo::HgSubrepoKind::Git;
  const Reply added = REQUEST(AddSubmodule, add);
  QVERIFY2(added.success, qPrintable(added.error));
  QVERIFY(QFileInfo::exists(root + QStringLiteral("/vendor/v.txt")));

  HgRepoCommitStagedCommand commit;
  commit.summary = QStringLiteral("Add vendor");
  commit.author = kAuthor;
  commit.email = kEmail;
  const Reply committed = REQUEST(CommitStaged, commit);
  QVERIFY2(committed.success, qPrintable(committed.error));

  Reply list = REQUEST(ListSubmodulesWithStatus,
                       HgRepoListSubmodulesWithStatusCommand());
  QVERIFY(list.success);
  auto submodules = list.value.value<HgRepoSubmodulesResult>();
  QCOMPARE(submodules.subrepos.size(), 1);
  QCOMPARE(submodules.subrepos[0].kind, HgRepo::HgSubrepoKind::Git);
  QCOMPARE(submodules.infos[0].statusInfo.status,
           GitRepo::SubmoduleStatusType::UpToDate);
  const QString pinned = submodules.subrepos[0].pinnedState;
  QCOMPARE(pinned.size(), 40);

  // A new commit in the checkout makes it outdated until updated.
  QVERIFY(writeFile(root + QStringLiteral("/vendor/v.txt"), "v2\n"));
  QVERIFY(runTool(git,
                  identity +
                      QStringList{QStringLiteral("commit"),
                                  QStringLiteral("-q"), QStringLiteral("-a"),
                                  QStringLiteral("-m"), QStringLiteral("v2")},
                  root + QStringLiteral("/vendor")));
  list = REQUEST(ListSubmodulesWithStatus,
                 HgRepoListSubmodulesWithStatusCommand());
  QCOMPARE(
      list.value.value<HgRepoSubmodulesResult>().infos[0].statusInfo.status,
      GitRepo::SubmoduleStatusType::Outdated);

  HgRepoSubmoduleUpdateCommand update;
  update.submoduleName = QStringLiteral("vendor");
  const Reply updated = REQUEST(SubmoduleUpdate, update);
  QVERIFY2(updated.success, qPrintable(updated.error));
  list = REQUEST(ListSubmodulesWithStatus,
                 HgRepoListSubmodulesWithStatusCommand());
  QCOMPARE(
      list.value.value<HgRepoSubmodulesResult>().infos[0].statusInfo.status,
      GitRepo::SubmoduleStatusType::UpToDate);
}

void HgRepoManagerTest::subversionSubrepo() {
  const QString svn = QStandardPaths::findExecutable(QStringLiteral("svn"));
  const QString svnadmin =
      QStandardPaths::findExecutable(QStringLiteral("svnadmin"));
  if (svn.isEmpty() || svnadmin.isEmpty()) {
    QSKIP("svn was not found, Mercurial needs it for Subversion subrepos");
  }

  const QString server = _tmp->path() + QStringLiteral("/svnrepo");
  QVERIFY(runTool(svnadmin, {QStringLiteral("create"), server}, _tmp->path()));
  const QString imported = _tmp->path() + QStringLiteral("/import");
  QVERIFY(writeFile(imported + QStringLiteral("/legacy.txt"), "l\n"));
  const QString url =
      QUrl::fromLocalFile(QDir::fromNativeSeparators(server)).toString();
  QVERIFY(runTool(svn,
                  {QStringLiteral("import"), QStringLiteral("-q"),
                   QStringLiteral("-m"), QStringLiteral("Initial import"),
                   imported, url + QStringLiteral("/trunk")},
                  _tmp->path()));

  QVERIFY(HgRepo::initRepository(QStringLiteral("repo"), _tmp->path(),
                                 QString(), kAuthor, kEmail));
  const QString root = _tmp->path() + QStringLiteral("/repo");
  QVERIFY(writeFile(root + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY(open(root).success);
  QVERIFY(commitAll(QStringLiteral("Root")).success);

  HgRepoAddSubmoduleCommand add;
  add.url = url;
  add.name = QStringLiteral("legacy");
  add.branch = QStringLiteral("trunk");
  add.kind = HgRepo::HgSubrepoKind::Subversion;
  const Reply added = REQUEST(AddSubmodule, add);
  QVERIFY2(added.success, qPrintable(added.error));
  QVERIFY(QFileInfo::exists(root + QStringLiteral("/legacy/legacy.txt")));

  HgRepoCommitStagedCommand commit;
  commit.summary = QStringLiteral("Add legacy");
  commit.author = kAuthor;
  commit.email = kEmail;
  const Reply committed = REQUEST(CommitStaged, commit);
  QVERIFY2(committed.success, qPrintable(committed.error));

  const Reply list = REQUEST(ListSubmodulesWithStatus,
                             HgRepoListSubmodulesWithStatusCommand());
  QVERIFY(list.success);
  const auto submodules = list.value.value<HgRepoSubmodulesResult>();
  QCOMPARE(submodules.subrepos[0].kind, HgRepo::HgSubrepoKind::Subversion);
  QCOMPARE(submodules.subrepos[0].source, url + QStringLiteral("/trunk"));
  QVERIFY(!submodules.subrepos[0].pinnedState.isEmpty());
  QCOMPARE(submodules.infos[0].statusInfo.status,
           GitRepo::SubmoduleStatusType::UpToDate);

  HgRepoSubmoduleUpdateCommand update;
  update.submoduleName = QStringLiteral("legacy");
  const Reply updated = REQUEST(SubmoduleUpdate, update);
  QVERIFY2(updated.success, qPrintable(updated.error));
}

// Stands in for GitCredentialsOverlayWidget: answers the credential prompts
// from a list, cancels when it is used up, and records the prompt events.
class FakeCredentialsOverlay {
public:
  FakeCredentialsOverlay() {
    auto &eventManager = EventManager::EventManager::getInstance();
    for (const EventType type :
         {EventType::GitRepoManagerGetCredentials,
          EventType::GitRepoManagerGetCredentialsInvalid,
          EventType::GitRepoManagerGetCredentialsFinished}) {
      eventManager.subscribe({type, this, _handler});
    }
  }

  ~FakeCredentialsOverlay() {
    auto &eventManager = EventManager::EventManager::getInstance();
    for (const EventType type :
         {EventType::GitRepoManagerGetCredentials,
          EventType::GitRepoManagerGetCredentialsInvalid,
          EventType::GitRepoManagerGetCredentialsFinished}) {
      eventManager.unsubscribe({type, this, _handler});
    }
  }

  void setAnswers(const QVector<QPair<QString, QString>> &answers) {
    QMutexLocker locker(&_mutex);
    _answers = answers;
    _events.clear();
  }

  QStringList events() {
    QMutexLocker locker(&_mutex);
    return _events;
  }

  QStringList repoPaths() {
    QMutexLocker locker(&_mutex);
    return _repoPaths;
  }

private:
  void onEvent(const EventManager::Event &event) {
    QMutexLocker locker(&_mutex);
    std::size_t id = 0;
    switch (event.type()) {
    case EventType::GitRepoManagerGetCredentials: {
      const auto request = event.data<GitRepo::GitCredentialsRequest>();
      id = request.id;
      _repoPaths << request.repoPath;
      _events << QStringLiteral("prompt");
      break;
    }
    case EventType::GitRepoManagerGetCredentialsInvalid:
      id = event.data<std::size_t>();
      _events << QStringLiteral("invalid");
      break;
    default:
      _events << QStringLiteral("finished");
      return;
    }

    auto &eventManager = EventManager::EventManager::getInstance();
    if (_answers.isEmpty()) {
      EventManager::Event cancel(EventType::GitRepoManagerGetCredentialsCancel);
      cancel.addData(GitRepo::GitCredentialsErrorResponse{id});
      eventManager.send(cancel);
      return;
    }
    const auto [username, password] = _answers.takeFirst();
    GitRepo::GitCredentialsResponse response;
    response.id = id;
    response.username = username;
    response.password = password;
    response.rememberCredentials = false;
    EventManager::Event submit(EventType::GitRepoManagerGetCredentialsSubmit);
    submit.addData(response);
    eventManager.send(submit);
  }

  // One callback for subscribe and unsubscribe: EventManager tells
  // subscriptions apart by the callback's type, and every lambda expression
  // has its own.
  const EventManager::CallbackType _handler =
      [this](EventManager::Event event) { onEvent(event); };
  QMutex _mutex;
  QVector<QPair<QString, QString>> _answers;
  QStringList _events;
  QStringList _repoPaths;
};

void HgRepoManagerTest::credentialsOverlay() {
  const QString source = _tmp->path() + QStringLiteral("/source");
  QVERIFY(HgRepo::initRepository(QStringLiteral("source"), _tmp->path(),
                                 QString(), kAuthor, kEmail));
  QVERIFY(writeFile(source + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY(runTool(HgRepo::findExecutable(),
                  {QStringLiteral("commit"), QStringLiteral("-A"),
                   QStringLiteral("-m"), QStringLiteral("First")},
                  source));

  const QString username = QStringLiteral("alice");
  const QString password = QStringLiteral("s3cret");
  HgAuthServer server(source, username, password);
  QVERIFY(server.start());

  FakeCredentialsOverlay overlay;

  // A wrong answer leaves the overlay open with "invalid", the right one
  // closes it.
  overlay.setAnswers(
      {{username, QStringLiteral("wrong")}, {username, password}});
  HgRepoCloneRepoCommand clone;
  clone.url = server.url();
  clone.destinationDir =
      QDir::cleanPath(_tmp->path()) + QStringLiteral("/clone");
  const Reply cloned = REQUEST(CloneRepo, clone);
  QVERIFY2(cloned.success, qPrintable(cloned.error));
  QTRY_COMPARE(overlay.events(),
               (QStringList{QStringLiteral("prompt"), QStringLiteral("invalid"),
                            QStringLiteral("finished")}));
  QCOMPARE(overlay.repoPaths().value(0), clone.destinationDir);

  // Stored credentials are used without asking.
  auto &storage = GitRepo::GitCredentialStorage::getInstance();
  storage.storeCredentials(clone.destinationDir, username, password, false);
  const auto forget = qScopeGuard([&]() { storage.clearMemoryCredentials(); });

  overlay.setAnswers({});
  QVERIFY(open(clone.destinationDir).success);
  const Reply pulled = REQUEST(Pull, HgRepoPullCommand());
  QVERIFY2(pulled.success, qPrintable(pulled.error));
  QVERIFY(overlay.events().isEmpty());

  // Cancelling the prompt fails the command.
  storage.clearMemoryCredentials();
  HgRepoCloneRepoCommand second = clone;
  second.destinationDir =
      QDir::cleanPath(_tmp->path()) + QStringLiteral("/second");
  const Reply cancelled = REQUEST(CloneRepo, second);
  QVERIFY(cancelled.answered);
  QVERIFY(!cancelled.success);
  QVERIFY2(cancelled.error.contains(QStringLiteral("cancelled")),
           qPrintable(cancelled.error));
  QCOMPARE(overlay.events(), QStringList{QStringLiteral("prompt")});
}

void HgRepoManagerTest::sshKeyPassphrase() {
#ifndef Q_OS_WIN
  QSKIP("The fake ssh is a batch file");
#endif
  QString keygen = QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
  if (keygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }

  const QString dir = QDir::cleanPath(_tmp->path());
  const QString passphrase = QStringLiteral("c0rrect horse!");
  const QString key = dir + QStringLiteral("/id_test");
  QVERIFY(runTool(keygen,
                  {QStringLiteral("-q"), QStringLiteral("-t"),
                   QStringLiteral("ed25519"), QStringLiteral("-N"), passphrase,
                   QStringLiteral("-f"), QDir::toNativeSeparators(key)},
                  dir));

  QVERIFY(HgRepo::initRepository(QStringLiteral("repo"), dir, QString(),
                                 kAuthor, kEmail));
  const QString root = dir + QStringLiteral("/repo");

  // Stands in for ssh: asks the askpass program from hg's environment for the
  // passphrase of the key, as ssh does, and keeps what it prints.
  const QString answerFile = QDir::toNativeSeparators(dir + "/answer.txt");
  const QString fakeSsh = QDir::toNativeSeparators(dir + "/fake_ssh.cmd");
  QVERIFY(writeFile(fakeSsh,
                    "@echo off\r\n\"%SSH_ASKPASS%\" \"Enter passphrase for key "
                    "'" +
                        key.toUtf8() + "': \" > \"" + answerFile.toUtf8() +
                        "\"\r\nexit /b 255\r\n"));
  QVERIFY(writeFile(root + QStringLiteral("/.hg/hgrc"),
                    "[ui]\nssh = \"" + fakeSsh.toUtf8() +
                        "\"\n[paths]\nsshremote = "
                        "ssh://hg@example.invalid/repo\n"));
  QVERIFY(open(root).success);

  const auto askedAnswer = [&]() {
    QFile::remove(answerFile);
    HgRepoFetchRemoteCommand fetch;
    fetch.remoteName = QStringLiteral("sshremote");
    REQUEST(FetchRemote, fetch);
    QFile file(answerFile);
    return file.open(QIODevice::ReadOnly)
               ? QString::fromUtf8(file.readAll()).trimmed()
               : QStringLiteral("<no answer file>");
  };

  FakeCredentialsOverlay overlay;

  // A wrong passphrase is caught before ssh gets it.
  overlay.setAnswers(
      {{QString(), QStringLiteral("wrong")}, {QString(), passphrase}});
  QCOMPARE(askedAnswer(), passphrase);
  QTRY_COMPARE(overlay.events(),
               (QStringList{QStringLiteral("prompt"), QStringLiteral("invalid"),
                            QStringLiteral("finished")}));

  // It is remembered for the key.
  overlay.setAnswers({});
  QCOMPARE(askedAnswer(), passphrase);
  QVERIFY(overlay.events().isEmpty());

  // Another key, the user cancels: ssh gets no answer.
  const QString otherKey = dir + QStringLiteral("/id_other");
  QVERIFY(runTool(keygen,
                  {QStringLiteral("-q"), QStringLiteral("-t"),
                   QStringLiteral("ed25519"), QStringLiteral("-N"), passphrase,
                   QStringLiteral("-f"), QDir::toNativeSeparators(otherKey)},
                  dir));
  QVERIFY(writeFile(fakeSsh,
                    "@echo off\r\n\"%SSH_ASKPASS%\" \"Enter passphrase for key "
                    "'" +
                        otherKey.toUtf8() + "': \" > \"" + answerFile.toUtf8() +
                        "\"\r\nexit /b 255\r\n"));
  QCOMPARE(askedAnswer(), QString());
  QCOMPARE(overlay.events(), QStringList{QStringLiteral("prompt")});

  // Host key questions are declined, the key manager trusts hosts.
  QVERIFY(writeFile(fakeSsh,
                    "@echo off\r\n\"%SSH_ASKPASS%\" \"Are you sure you want "
                    "to continue connecting (yes/no/[fingerprint])? \" > \"" +
                        answerFile.toUtf8() + "\"\r\nexit /b 255\r\n"));
  QCOMPARE(askedAnswer(), QStringLiteral("no"));
}

void HgRepoManagerTest::knownHosts() {
  namespace KnownHosts = GitRepo::SshKnownHosts;
  const QString keygen =
      QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
  if (keygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }

  // A host key stands in for one ssh-keyscan would get.
  const QString dir = QDir::cleanPath(_tmp->path());
  QVERIFY(runTool(keygen,
                  {QStringLiteral("-q"), QStringLiteral("-t"),
                   QStringLiteral("ed25519"), QStringLiteral("-N"), QString(),
                   QStringLiteral("-f"),
                   QDir::toNativeSeparators(dir + "/host_key")},
                  dir));
  QFile publicKey(dir + "/host_key.pub");
  QVERIFY(publicKey.open(QIODevice::ReadOnly));
  const QString key =
      QString::fromUtf8(publicKey.readAll()).section(' ', 0, 1).trimmed();

  const QString file = dir + "/ssh/known_hosts";
  QVERIFY(!KnownHosts::isKnown(QStringLiteral("example.invalid"), file));

  KnownHosts::HostKey hostKey;
  hostKey.line = QStringLiteral("example.invalid ") + key;
  QString error;
  QVERIFY2(KnownHosts::add({hostKey}, &error, file), qPrintable(error));
  QVERIFY(KnownHosts::isKnown(QStringLiteral("example.invalid"), file));
  QVERIFY(!KnownHosts::isKnown(QStringLiteral("other.invalid"), file));

  // A real host, when the network allows: its keys come with fingerprints.
  if (qEnvironmentVariableIsSet("SOURCERER_TEST_SSH_HOST")) {
    const auto keys = KnownHosts::scan(
        qEnvironmentVariable("SOURCERER_TEST_SSH_HOST"), &error);
    QVERIFY2(!keys.isEmpty(), qPrintable(error));
    QVERIFY(keys.first().fingerprint.startsWith(QStringLiteral("SHA256:")));
  }

  // A host that doesn't answer gives an error and no keys.
  QVERIFY(
      KnownHosts::scan(QStringLiteral("127.0.0.1"), &error, 9, 2).isEmpty());
  QVERIFY(!error.isEmpty());
}

void HgRepoManagerTest::closeRefusesQueuedRequests() {
  QVERIFY(HgRepo::initRepository(QStringLiteral("repo"), _tmp->path(),
                                 QString(), kAuthor, kEmail));
  QVERIFY(open(_tmp->path() + QStringLiteral("/repo")).success);

  // Requests queued behind each other are all answered, the ones after the
  // close with an error.
  auto &eventManager = EventManager::EventManager::getInstance();
  QVector<QPair<int, QString>> waits;
  for (int i = 0; i < 5; ++i) {
    HgRepoStatusCommand status;
    status.uuid = _uuid;
    status.syncUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
    waits.append({eventManager.startWait(), status.syncUuid});
    EventManager::Event event(EventType::HgRepoManagerStatus, status.syncUuid);
    event.addData(status);
    eventManager.send(event);
  }
  HgRepoCloseRepoCommand close;
  close.uuid = _uuid;
  close.syncUuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const int closeToken = eventManager.startWait();
  EventManager::Event closeEvent(EventType::HgRepoManagerCloseRepo,
                                 close.syncUuid);
  closeEvent.addData(close);
  eventManager.send(closeEvent);

  const auto [closed, closeAnswer] =
      eventManager.waitForAnswer(closeToken, close.syncUuid,
                                 {EventType::HgRepoManagerCloseRepoResult,
                                  EventType::HgRepoManagerCloseRepoError},
                                 60 * 1000);
  QVERIFY(closed);
  QCOMPARE(closeAnswer.type(), EventType::HgRepoManagerCloseRepoResult);

  for (const auto &[token, syncUuid] : waits) {
    const auto [answered, answer] =
        eventManager.waitForAnswer(token, syncUuid,
                                   {EventType::HgRepoManagerStatusResult,
                                    EventType::HgRepoManagerStatusError},
                                   60 * 1000);
    QVERIFY(answered);
    Q_UNUSED(answer);
  }

  // After the close nothing is open.
  QVERIFY(!REQUEST(Status, HgRepoStatusCommand()).success);
}

QTEST_GUILESS_MAIN(HgRepoManagerTest)

#include "tst_hgrepomanager.moc"
