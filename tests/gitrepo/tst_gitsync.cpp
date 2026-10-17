// Tests for the parts of lib/GitRepo that talk to other repositories: clone,
// fetch, pull, push, remote tags and branches, submodules and Git LFS. See
// docs/GIT_TEST_PLAN.md, sections 3.15 to 3.17. Remotes are local bare
// repositories, LFS objects are served by LfsStubServer. Every transfer waits
// 1.5 s for the progress toast (finishFetchProgress), so scenarios are few and
// big.

#include "GitTestHelpers.h"
#include "LfsStubServer.h"

#include "../../component/GitRepoManager/include/GitRepoManagerStructs.h"

#include <GitCredentialStorage.h>
#include <GitLfs.h>

#include <EventManager.h>

#include <QMutex>
#include <QMutexLocker>
#include <QNetworkProxyFactory>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>
#include <QtTest>

#include <memory>

using namespace GitTest;
using GitRepo::GitError;

namespace {

QString newUuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

// Collects the clone/fetch progress events of one operation.
class ProgressRecorder {
public:
  explicit ProgressRecorder(QString uuid)
      : _uuid(std::move(uuid)),
        _handler([this](EventManager::Event event) { record(event); }) {
    EventManager::EventManager::getInstance().subscribe(
        {EventManager::EventType::GitRepoManagerCloneRepoProgress, this,
         _handler});
  }
  ~ProgressRecorder() {
    EventManager::EventManager::getInstance().unsubscribe(
        {EventManager::EventType::GitRepoManagerCloneRepoProgress, this,
         _handler});
  }

  QVector<GitRepo::CloneProgress> events() const {
    QMutexLocker locker(&_mutex);
    return _events;
  }

private:
  void record(const EventManager::Event &event) {
    const auto result = event.data<GitRepoManager::Structs::GitRepoResult>();
    if (result.uuid != _uuid) {
      return;
    }
    QMutexLocker locker(&_mutex);
    _events += result.value.value<GitRepo::CloneProgress>();
  }

  QString _uuid;
  mutable QMutex _mutex;
  QVector<GitRepo::CloneProgress> _events;
  // EventManager tells subscriptions apart by the callback, the same object
  // must be used to subscribe and to unsubscribe.
  const EventManager::CallbackType _handler;
};

// Stands in for the credentials overlay: records every prompt and cancels it.
class CredentialPrompts {
public:
  CredentialPrompts()
      : _handler([this](EventManager::Event event) { answer(event); }) {
    EventManager::EventManager::getInstance().subscribe(
        {EventManager::EventType::GitRepoManagerGetCredentials, this,
         _handler});
  }
  ~CredentialPrompts() {
    EventManager::EventManager::getInstance().unsubscribe(
        {EventManager::EventType::GitRepoManagerGetCredentials, this,
         _handler});
  }

  QStringList urls() const {
    QMutexLocker locker(&_mutex);
    return _urls;
  }

private:
  void answer(const EventManager::Event &event) {
    const auto request = event.data<GitRepo::GitCredentialsRequest>();
    {
      QMutexLocker locker(&_mutex);
      _urls += request.url;
    }
    EventManager::Event cancel(
        EventManager::EventType::GitRepoManagerGetCredentialsCancel);
    cancel.addData(GitRepo::GitCredentialsErrorResponse{request.id});
    EventManager::EventManager::getInstance().send(cancel);
  }

  mutable QMutex _mutex;
  QStringList _urls;
  const EventManager::CallbackType _handler;
};

const QByteArray kLfsAttributes = "*.bin filter=lfs diff=lfs merge=lfs -text\n";

} // namespace

class GitSyncTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  // Clone.
  void cloneFromLocalPathAndFileUrl_data();
  void cloneFromLocalPathAndFileUrl();
  void cloneIntoExistingDirectory();
  void cloneFailureRemovesCreatedDirectory();
  void cloneEmptyRemote();
  void cloneReportsProgress();
  void commercialPlanGatesSshUrls_data();
  void commercialPlanGatesSshUrls();

  // Fetch.
  void fetchRemoteAndAll();
  void fetchErrors();
  void fetchPrunesOnlyWhenConfigured();
  void fetchDoesNotClobberLocalTags();

  // Pull.
  void pullFastForward();
  void pullFastForwardRefusesConflictingChanges();
  void pullWithoutUpstream();
  void pullNamedBranchNotCheckedOut();
  void pullDivergedLeavesMergeToCommit();
  void pullDivergedWithConflicts();
  void pullRebase();
  void pullRebaseConflictRestoresBranch();

  // Push.
  void pushFastForwardAndNothing();
  void pushWithoutUpstreamSetsError();
  void pushRejectedAndForced();
  void pushToDifferentlyNamedBranch();
  void pushNewBranch();
  void pushToCheckedOutBranchOfNonBareRemote();
  void tagsOnRemote();
  void deleteRemoteBranch();
  void remoteStatusAfterFetch();

  // Submodules.
  void addSubmoduleFromLocalRepo();
  void addSubmoduleWithBranchAndSpaces();
  void addSubmoduleFailuresCleanUp_data();
  void addSubmoduleFailuresCleanUp();
  void addSubmoduleKeepsExistingSubmodule();
  void submoduleStatuses();
  void updateSubmoduleRecordsCurrentCommit();
  void deleteSubmoduleRemovesEverything();
  void deleteSubmoduleKeepsOtherSubmodules();
  void deleteSubmoduleWhoseNameDiffersFromPath();
  void addSubmoduleWithNestedSubmodules();
  void deleteSubmoduleRefusesOtherDirectories();

  // LFS.
  void lfsPointerParse_data();
  void lfsPointerParse();
  void lfsObjectStore();
  void lfsStageAndSmudgeFromLocalStore();
  void lfsSmudgeDownloadsMissingObject();
  void lfsSmudgeRejectsWrongSize();
  void lfsDownloadErrors_data();
  void lfsDownloadErrors();
  void lfsUploadLocalObjects();
  void lfsPushToLocalRemote();
  void lfsEndpointDiscovery_data();
  void lfsEndpointDiscovery();

private:
  QString git(const QStringList &args, bool *ok = nullptr) const {
    return gitIn(_config, _root, args, ok);
  }
  QString gitAt(const QString &dir, const QStringList &args,
                bool *ok = nullptr) const {
    return gitIn(_config, dir, args, ok);
  }
  QString file(const QString &name) const { return _root + '/' + name; }
  QString head() const {
    return git({QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
  }
  QString rev(const QString &revision) const {
    bool ok = false;
    const QString id =
        git({QStringLiteral("rev-parse"), QStringLiteral("-q"),
             QStringLiteral("--verify"), revision},
            &ok);
    return ok ? id : QString();
  }
  QString remoteRev(const QString &revision) const {
    bool ok = false;
    const QString id =
        gitAt(_remote, {QStringLiteral("rev-parse"), QStringLiteral("-q"),
                        QStringLiteral("--verify"), revision},
              &ok);
    return ok ? id : QString();
  }
  // Commits every change in the working copy with git, returns HEAD.
  QString commitAll(const QString &summary) const;
  // Commits `name` in another clone and pushes it, returns the new commit.
  QString theirsCommit(const QString &name, const QByteArray &content,
                       const QString &summary,
                       const QString &branch = QStringLiteral("main"));
  // A new bare repository at `path` with the history of the seed.
  void makeBareCopy(const QString &path) const;
  // A repository with a few commits on main and dev, for submodules.
  QString makeSubmoduleSource(const QString &name) const;
  // Sets an error unrelated to the operation under test, to see whether the
  // operation reports its own.
  void setUnrelatedError();
  void reopen();
  // Makes the LFS endpoint the stub: it is derived from the fetch URL of
  // origin. lfs.url would be simpler, but it is not read, see
  // lfsEndpointDiscovery. git can't push to origin any more: libgit2's local
  // transport pushes to the fetch URL and ignores the push URL.
  void useLfsServer(const LfsStubServer &server) const {
    git({QStringLiteral("remote"), QStringLiteral("set-url"),
         QStringLiteral("origin"),
         server.origin() + QStringLiteral("/team/repo.git")});
    git({QStringLiteral("remote"), QStringLiteral("set-url"),
         QStringLiteral("--push"), QStringLiteral("origin"), _remote});
  }
  GitRepo::GitStatus status() { return GitRepo::getStatus(_repo.get(), _quit); }

  IsolatedConfig _config;
  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _seed;
  QString _remote;
  QString _theirs;
  QString _root;
  RepoPtr _repo;
  bool _quit = false;
};

QString GitSyncTest::commitAll(const QString &summary) const {
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("commit"), QStringLiteral("-q"),
       QStringLiteral("--allow-empty"), QStringLiteral("-m"), summary});
  return head();
}

QString GitSyncTest::theirsCommit(const QString &name, const QByteArray &content,
                                  const QString &summary,
                                  const QString &branch) {
  if (_theirs.isEmpty()) {
    _theirs = _base + QStringLiteral("/theirs");
    gitAt(_base, {QStringLiteral("clone"), QStringLiteral("-q"), _remote,
                  _theirs});
  }
  gitAt(_theirs, {QStringLiteral("fetch"), QStringLiteral("-q"),
                  QStringLiteral("origin")});
  bool exists = false;
  gitAt(_theirs,
        {QStringLiteral("rev-parse"), QStringLiteral("-q"),
         QStringLiteral("--verify"), QStringLiteral("origin/") + branch},
        &exists);
  gitAt(_theirs, {QStringLiteral("checkout"), QStringLiteral("-q"),
                  QStringLiteral("-B"), branch,
                  exists ? QStringLiteral("origin/") + branch
                         : QStringLiteral("origin/main")});
  writeFile(_theirs + '/' + name, content);
  gitAt(_theirs, {QStringLiteral("add"), QStringLiteral("-A")});
  gitAt(_theirs, {QStringLiteral("commit"), QStringLiteral("-q"),
                  QStringLiteral("-m"), summary});
  gitAt(_theirs, {QStringLiteral("push"), QStringLiteral("-q"),
                  QStringLiteral("origin"), branch});
  return gitAt(_theirs, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});
}

void GitSyncTest::makeBareCopy(const QString &path) const {
  gitAt(_base, {QStringLiteral("clone"), QStringLiteral("-q"),
                QStringLiteral("--bare"), _seed, path});
}

QString GitSyncTest::makeSubmoduleSource(const QString &name) const {
  const QString dir = _base + '/' + name;
  QDir().mkpath(dir);
  gitAt(dir, {QStringLiteral("init"), QStringLiteral("-q")});
  writeFile(dir + QStringLiteral("/s.txt"), "s\n");
  gitAt(dir, {QStringLiteral("add"), QStringLiteral("-A")});
  gitAt(dir, {QStringLiteral("commit"), QStringLiteral("-qm"),
              QStringLiteral("S1")});
  gitAt(dir, {QStringLiteral("branch"), QStringLiteral("dev")});
  writeFile(dir + QStringLiteral("/s.txt"), "s2\n");
  gitAt(dir, {QStringLiteral("commit"), QStringLiteral("-qam"),
              QStringLiteral("S2")});
  return dir;
}

void GitSyncTest::setUnrelatedError() {
  GitRepo::resetToCommit(_repo.get(), QStringLiteral("no-such-revision"),
                         GitRepo::ResetType::Soft);
  QCOMPARE(lastError(), GitError::ResetToCommitFailedToResolveRevision);
}

void GitSyncTest::reopen() {
  _repo.reset();
  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY2(_repo, qPrintable(lastErrorMessage()));
}

void GitSyncTest::initTestCase() {
  if (QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty()) {
    QSKIP("git was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  useTestCredentialsKey();
  // The LFS stub runs on 127.0.0.1, a system proxy must not get in between.
  QNetworkProxyFactory::setUseSystemConfiguration(false);
  GitRepo::initForCurrentThread();
}

void GitSyncTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  _config.setUp(_base + QStringLiteral("/home"));
  _seed = _base + QStringLiteral("/seed");
  _remote = _base + QStringLiteral("/remote.git");
  _theirs.clear();
  _root = _base + QStringLiteral("/repo");

  QVERIFY(QDir().mkpath(_seed));
  bool ok = false;
  gitAt(_seed, {QStringLiteral("init"), QStringLiteral("-q")}, &ok);
  QVERIFY(ok);
  QVERIFY(writeFile(_seed + QStringLiteral("/a.txt"), "a\n"));
  gitAt(_seed, {QStringLiteral("add"), QStringLiteral("-A")});
  gitAt(_seed, {QStringLiteral("commit"), QStringLiteral("-qm"),
                QStringLiteral("Base")});
  makeBareCopy(_remote);
  gitAt(_base, {QStringLiteral("clone"), QStringLiteral("-q"), _remote, _root},
        &ok);
  QVERIFY(ok);
  // Rebases and stashes take the committer from the repository config.
  git({QStringLiteral("config"), QStringLiteral("user.name"), kAuthor});
  git({QStringLiteral("config"), QStringLiteral("user.email"), kEmail});

  _repo.reset(GitRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY2(_repo, qPrintable(lastErrorMessage()));
}

void GitSyncTest::cleanup() {
  GitRepo::Testing::setCommercialPlanOverride(-1);
  GitRepo::GitCredentialStorage::getInstance().clearMemoryCredentials();
  _repo.reset();
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  _quit = false;
}

// Clone.

void GitSyncTest::cloneFromLocalPathAndFileUrl_data() {
  QTest::addColumn<QString>("name");
  QTest::addColumn<bool>("fileUrl");
  QTest::newRow("path") << QStringLiteral("plain.git") << false;
  QTest::newRow("file url") << QStringLiteral("url.git") << true;
  QTest::newRow("spaces") << QStringLiteral("with spaces.git") << false;
  QTest::newRow("unicode") << QString::fromUtf16(u"ünïçödé.git")
                           << false;
  QTest::newRow("unicode file url")
      << QString::fromUtf16(u"日本.git") << true;
}

void GitSyncTest::cloneFromLocalPathAndFileUrl() {
  QFETCH(QString, name);
  QFETCH(bool, fileUrl);

  const QString source = _base + '/' + name;
  makeBareCopy(source);
  const QString url =
      fileUrl ? QUrl::fromLocalFile(source).toString(QUrl::FullyEncoded) : source;
  const QString destination = _base + QStringLiteral("/clone");

  QVERIFY2(GitRepo::cloneRepository(url, destination, newUuid()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(destination + QStringLiteral("/a.txt")), QByteArray("a\n"));

  RepoPtr clone(GitRepo::openRepository(destination, kAuthor, kEmail));
  QVERIFY(clone);
  QCOMPARE(GitRepo::getActiveBranch(clone.get(), GitRepo::BranchType::Local,
                                    _quit),
           QStringLiteral("main"));
  const auto [hasUpstream, remote, branch] =
      GitRepo::getUpstream(clone.get(), QStringLiteral("main"));
  QVERIFY(hasUpstream);
  QCOMPARE(remote, QStringLiteral("origin"));
  QCOMPARE(branch, QStringLiteral("main"));
  const QVector<GitRepo::RemoteInfo> remotes = GitRepo::listRemotes(clone.get());
  QCOMPARE(remotes.size(), 1);
  QCOMPARE(remotes[0].fetchUrl, url);
  QVERIFY(!GitRepo::getStatus(clone.get(), _quit).hasChanges());
}

void GitSyncTest::cloneIntoExistingDirectory() {
  // A directory with files in it is refused and left as it was.
  const QString full = _base + QStringLiteral("/full");
  QVERIFY(writeFile(full + QStringLiteral("/keep.txt"), "keep\n"));
  QVERIFY(!GitRepo::cloneRepository(_remote, full, newUuid()));
  QCOMPARE(lastError(), GitError::CloneRepositoryFailedToCloneRepository);
  QCOMPARE(readFile(full + QStringLiteral("/keep.txt")), QByteArray("keep\n"));
  QVERIFY(!QFileInfo::exists(full + QStringLiteral("/.git")));

  // An empty directory is fine, and stays when the clone fails.
  const QString empty = _base + QStringLiteral("/empty");
  QVERIFY(QDir().mkpath(empty));
  QVERIFY(!GitRepo::cloneRepository(_base + QStringLiteral("/missing.git"),
                                    empty, newUuid()));
  QVERIFY(QFileInfo(empty).isDir());
  QVERIFY2(GitRepo::cloneRepository(_remote, empty, newUuid()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(empty + QStringLiteral("/a.txt")), QByteArray("a\n"));
}

void GitSyncTest::cloneFailureRemovesCreatedDirectory() {
  const QString destination = _base + QStringLiteral("/new/deeper");
  QVERIFY(!GitRepo::cloneRepository(_base + QStringLiteral("/missing.git"),
                                    destination, newUuid()));
  QCOMPARE(lastError(), GitError::CloneRepositoryFailedToCloneRepository);
  QVERIFY(!lastErrorMessage().isEmpty());
  QVERIFY(!QFileInfo::exists(destination));
  // The parent the clone created on the way goes as well.
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/new")));
  QVERIFY(QFileInfo(_base).isDir());
}

void GitSyncTest::cloneEmptyRemote() {
  const QString empty = _base + QStringLiteral("/empty.git");
  bool ok = false;
  gitAt(_base, {QStringLiteral("init"), QStringLiteral("-q"),
                QStringLiteral("--bare"), empty},
        &ok);
  QVERIFY(ok);
  const QString destination = _base + QStringLiteral("/clone");

  QVERIFY2(GitRepo::cloneRepository(empty, destination, newUuid()),
           qPrintable(lastErrorMessage()));
  RepoPtr clone(GitRepo::openRepository(destination, kAuthor, kEmail));
  QVERIFY(clone);
  // Opening made the first commit, it can be pushed to the empty remote.
  QCOMPARE(gitAt(destination, {QStringLiteral("rev-list"),
                               QStringLiteral("--count"), QStringLiteral("HEAD")}),
           QStringLiteral("1"));
}

void GitSyncTest::cloneReportsProgress() {
  for (int i = 0; i < 200; ++i) {
    QVERIFY(writeFile(_seed + QStringLiteral("/f%1.txt").arg(i),
                      QByteArray::number(i).repeated(100)));
  }
  gitAt(_seed, {QStringLiteral("add"), QStringLiteral("-A")});
  gitAt(_seed, {QStringLiteral("commit"), QStringLiteral("-qm"),
                QStringLiteral("Many")});
  const QString source = _base + QStringLiteral("/many.git");
  makeBareCopy(source);

  {
    const QString uuid = newUuid();
    ProgressRecorder recorder(uuid);
    QVERIFY(GitRepo::cloneRepository(QUrl::fromLocalFile(source).toString(),
                                     _base + QStringLiteral("/clone"), uuid));
    QTRY_VERIFY(!recorder.events().isEmpty() &&
                recorder.events().last().finished);
    const QVector<GitRepo::CloneProgress> events = recorder.events();
    QVERIFY(events.first().started);
    QVERIFY(!events.last().error);
    QVERIFY(events.last().cloning);
    for (int i = 0; i + 1 < events.size(); ++i) {
      QVERIFY2(!events[i].finished, qPrintable(QString::number(i)));
      if (events[i].stage == events[i + 1].stage &&
          events[i + 1].lastStepPercentage >= 0) {
        QVERIFY(events[i + 1].lastStepPercentage >=
                events[i].lastStepPercentage);
      }
    }
  }

  {
    // A failed clone ends with an error.
    const QString uuid = newUuid();
    ProgressRecorder recorder(uuid);
    QVERIFY(!GitRepo::cloneRepository(_base + QStringLiteral("/missing.git"),
                                      _base + QStringLiteral("/clone2"), uuid));
    QTRY_VERIFY(!recorder.events().isEmpty() &&
                recorder.events().last().finished);
    QVERIFY(recorder.events().last().error);
  }
}

void GitSyncTest::commercialPlanGatesSshUrls_data() {
  QTest::addColumn<QString>("url");
  QTest::newRow("scp") << QStringLiteral("git@example.invalid:team/repo.git");
  QTest::newRow("git") << QStringLiteral("git://example.invalid/team/repo.git");
  QTest::newRow("ssh")
      << QStringLiteral("ssh://git@example.invalid/team/repo.git");
}

void GitSyncTest::commercialPlanGatesSshUrls() {
  QFETCH(QString, url);
  GitRepo::Testing::setCommercialPlanOverride(0);
  git({QStringLiteral("remote"), QStringLiteral("set-url"),
       QStringLiteral("origin"), url});
  git({QStringLiteral("tag"), QStringLiteral("v1")});
  reopen();

  // Without the plan these are refused before connecting.
  QVERIFY(!GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")) &&
          lastError() == GitError::FetchRemoteGitSshUrlRequiresCommercialPlan);
  QVERIFY(!GitRepo::pull(_repo.get()) &&
          lastError() == GitError::PullGitSshUrlRequiresCommercialPlan);
  QVERIFY(!GitRepo::push(_repo.get(), QString()) &&
          lastError() == GitError::PushGitSshUrlRequiresCommercialPlan);
  QVERIFY(!GitRepo::pushTag(_repo.get(), QStringLiteral("v1")) &&
          lastError() == GitError::PushTagGitSshUrlRequiresCommercialPlan);
  QVERIFY(!GitRepo::pushAllTags(_repo.get()) &&
          lastError() == GitError::PushAllTagsGitSshUrlRequiresCommercialPlan);
  QVERIFY(!GitRepo::deleteRemoteBranch(_repo.get(), QStringLiteral("main")) &&
          lastError() ==
              GitError::DeleteRemoteBranchGitSshUrlRequiresCommercialPlan);
  QVERIFY(!GitRepo::deleteRemoteTag(_repo.get(), QStringLiteral("v1")) &&
          lastError() == GitError::DeleteRemoteTagGitSshUrlRequiresCommercialPlan);

  // Adding a submodule is refused as well, without leaving anything behind.
  QVERIFY(!GitRepo::addSubmodule(_repo.get(), url, QStringLiteral("sub"),
                                 QString(), newUuid()) &&
          lastError() == GitError::AddSubmoduleGitSshUrlRequiresCommercialPlan);
  QVERIFY(!QFileInfo::exists(file(".gitmodules")) &&
          !QFileInfo::exists(file("sub")));

  // Nothing was sent.
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/v1")), QString());
}

// Fetch.

void GitSyncTest::fetchRemoteAndAll() {
  const QString feature = theirsCommit(QStringLiteral("f.txt"), "f\n",
                                       QStringLiteral("Feature"),
                                       QStringLiteral("feature"));
  gitAt(_theirs, {QStringLiteral("tag"), QStringLiteral("-a"),
                  QStringLiteral("-m"), QStringLiteral("One"),
                  QStringLiteral("v1")});
  gitAt(_theirs, {QStringLiteral("push"), QStringLiteral("-q"),
                  QStringLiteral("origin"), QStringLiteral("v1")});

  QVERIFY2(GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/feature")), feature);
  QVERIFY(GitRepo::listRemoteBranches(_repo.get(), _quit)
              .contains(QStringLiteral("origin/feature")));
  QVERIFY(GitRepo::listTags(_repo.get()).contains(QStringLiteral("v1")));
  // Fetching moves no local branch and touches no file.
  QCOMPARE(GitRepo::listLocalBranches(_repo.get(), _quit),
           QStringList{QStringLiteral("main")});
  QVERIFY(!status().hasChanges());

  // fetchAll fetches every remote, not only the ones branches track.
  const QString second = _base + QStringLiteral("/second.git");
  makeBareCopy(second);
  gitAt(_seed, {QStringLiteral("checkout"), QStringLiteral("-q"),
                QStringLiteral("-b"), QStringLiteral("other")});
  QVERIFY(writeFile(_seed + QStringLiteral("/o.txt"), "o\n"));
  gitAt(_seed, {QStringLiteral("add"), QStringLiteral("-A")});
  gitAt(_seed, {QStringLiteral("commit"), QStringLiteral("-qm"),
                QStringLiteral("Other")});
  gitAt(_seed, {QStringLiteral("push"), QStringLiteral("-q"), second,
                QStringLiteral("other")});
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("second"), second,
                             QString()));
  const QString again = theirsCommit(QStringLiteral("f.txt"), "f2\n",
                                     QStringLiteral("Feature 2"),
                                     QStringLiteral("feature"));
  QVERIFY(GitRepo::fetchAll(_repo.get()));
  QVERIFY(!rev(QStringLiteral("refs/remotes/second/other")).isEmpty());
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/feature")), again);
}

void GitSyncTest::fetchErrors() {
  QVERIFY(!GitRepo::fetchRemote(_repo.get(), QString()));
  QVERIFY(!GitRepo::fetchRemote(_repo.get(), QStringLiteral("nope")));
  QCOMPARE(lastError(), GitError::FetchRemoteInternalFailedToLookupRemote);

  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("broken"),
                             _base + QStringLiteral("/missing.git"), QString()));
  QVERIFY(!GitRepo::fetchRemote(_repo.get(), QStringLiteral("broken")));
  QCOMPARE(lastError(),
           GitError::FetchRemoteInternalFailedToFetchRemoteWithCredentials);
  QVERIFY(!lastErrorMessage().isEmpty());

  // One broken remote doesn't stop the others.
  const QString tip = theirsCommit(QStringLiteral("b.txt"), "b\n",
                                   QStringLiteral("B"));
  const bool fetched = GitRepo::fetchAll(_repo.get());
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/main")), tip);
  QVERIFY(!fetched);
}

void GitSyncTest::fetchPrunesOnlyWhenConfigured() {
  theirsCommit(QStringLiteral("f.txt"), "f\n", QStringLiteral("F"),
               QStringLiteral("feature"));
  QVERIFY(GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")));
  QVERIFY(!rev(QStringLiteral("refs/remotes/origin/feature")).isEmpty());
  gitAt(_theirs, {QStringLiteral("push"), QStringLiteral("-q"),
                  QStringLiteral("origin"), QStringLiteral("--delete"),
                  QStringLiteral("feature")});

  // Like `git fetch`, a deleted remote branch stays without fetch.prune.
  QVERIFY(GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")));
  QVERIFY(!rev(QStringLiteral("refs/remotes/origin/feature")).isEmpty());

  git({QStringLiteral("config"), QStringLiteral("fetch.prune"),
       QStringLiteral("true")});
  reopen();
  QVERIFY(GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")));
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/feature")), QString());
  QVERIFY(!GitRepo::listRemoteBranches(_repo.get(), _quit)
               .contains(QStringLiteral("origin/feature")));
}

void GitSyncTest::fetchDoesNotClobberLocalTags() {
  // A local tag, and a tag of the same name on another commit of the remote.
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  const QString local = commitAll(QStringLiteral("Local"));
  git({QStringLiteral("tag"), QStringLiteral("v1")});
  theirsCommit(QStringLiteral("r.txt"), "r\n", QStringLiteral("Remote"));
  gitAt(_theirs, {QStringLiteral("tag"), QStringLiteral("v1")});
  gitAt(_theirs, {QStringLiteral("push"), QStringLiteral("-q"),
                  QStringLiteral("origin"), QStringLiteral("v1")});

  GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin"));
  // Like `git fetch`, an existing tag is not clobbered.
  QCOMPARE(rev(QStringLiteral("v1^{commit}")), local);
}

// Pull.

void GitSyncTest::pullFastForward() {
  QVERIFY(writeFile(file("a.txt"), "local, not committed\n"));
  const QString tip = theirsCommit(QStringLiteral("b.txt"), "b\n",
                                   QStringLiteral("B"));

  QVERIFY2(GitRepo::pull(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(head(), tip);
  QCOMPARE(rev(QStringLiteral("main")), tip);
  QCOMPARE(readFile(file("b.txt")), QByteArray("b\n"));
  // Local changes the pull doesn't touch are kept.
  QCOMPARE(readFile(file("a.txt")), QByteArray("local, not committed\n"));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));

  // Up to date: nothing changes.
  QVERIFY(GitRepo::pull(_repo.get(), QStringLiteral("main")));
  QCOMPARE(head(), tip);
}

void GitSyncTest::pullFastForwardRefusesConflictingChanges() {
  const QString before = head();
  QVERIFY(writeFile(file("a.txt"), "local, not committed\n"));
  const QString tip = theirsCommit(QStringLiteral("a.txt"), "theirs\n",
                                   QStringLiteral("Theirs"));

  QVERIFY(!GitRepo::pull(_repo.get()));
  QCOMPARE(lastError(), GitError::PullFailedToPeelFetchedTargetCommit);
  QCOMPARE(readFile(file("a.txt")), QByteArray("local, not committed\n"));
  QCOMPARE(head(), before);
  // The fetch itself happened.
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/main")), tip);
}

void GitSyncTest::pullWithoutUpstream() {
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("lonely")});
  QVERIFY(!GitRepo::pull(_repo.get()));
  QCOMPARE(lastError(), GitError::PullNoUpstreamConfigured);

  git({QStringLiteral("checkout"), QStringLiteral("-q"),
       QStringLiteral("--detach")});
  QVERIFY(!GitRepo::pull(_repo.get()));
  QCOMPARE(lastError(), GitError::PullFailedToReadBranchName);
}

void GitSyncTest::pullNamedBranchNotCheckedOut() {
  theirsCommit(QStringLiteral("o.txt"), "o\n", QStringLiteral("Other"),
               QStringLiteral("other"));
  git({QStringLiteral("fetch"), QStringLiteral("-q")});
  git({QStringLiteral("branch"), QStringLiteral("--track"),
       QStringLiteral("other"), QStringLiteral("origin/other")});
  const QString otherTip = theirsCommit(QStringLiteral("o.txt"), "o2\n",
                                        QStringLiteral("Other 2"),
                                        QStringLiteral("other"));
  const QString mainBefore = rev(QStringLiteral("main"));

  // Pulling a branch that isn't checked out updates that branch.
  const bool pulled = GitRepo::pull(_repo.get(), QStringLiteral("other"));
  QVERIFY(pulled);
  QCOMPARE(rev(QStringLiteral("main")), mainBefore);
  QCOMPARE(rev(QStringLiteral("other")), otherTip);
}

void GitSyncTest::pullDivergedLeavesMergeToCommit() {
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  const QString local = commitAll(QStringLiteral("Local"));
  const QString remote = theirsCommit(QStringLiteral("r.txt"), "r\n",
                                      QStringLiteral("Remote"));

  // Unlike `git pull`, the merge is left for the user to commit.
  QVERIFY2(GitRepo::pull(_repo.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(head(), local);
  QVERIFY(GitRepo::getIsMergeInProgress(_repo.get()));
  QVERIFY(!GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(readFile(file("r.txt")), QByteArray("r\n"));

  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Merge"), QString(),
                                kAuthor, kEmail, true, false, false));
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%P")}),
           local + ' ' + remote);
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitSyncTest::pullDivergedWithConflicts() {
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  const QString local = commitAll(QStringLiteral("Local"));
  theirsCommit(QStringLiteral("a.txt"), "remote\n", QStringLiteral("Remote"));

  // The pull itself succeeds and leaves the conflict to resolve.
  QVERIFY(GitRepo::pull(_repo.get()));
  QCOMPARE(head(), local);
  QVERIFY(GitRepo::hasConflicts(_repo.get()));
  QCOMPARE(status().conflictedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(readFile(file("a.txt")).contains("<<<<<<<"));

  QVERIFY(GitRepo::abortMerge(_repo.get()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QVERIFY(!GitRepo::getIsMergeInProgress(_repo.get()));
}

void GitSyncTest::pullRebase() {
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  git({QStringLiteral("add"), QStringLiteral("-A")});
  git({QStringLiteral("-c"), QStringLiteral("user.name=") + kOtherAuthor,
       QStringLiteral("-c"), QStringLiteral("user.email=") + kOtherEmail,
       QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("Local")});
  const QString remote = theirsCommit(QStringLiteral("r.txt"), "r\n",
                                      QStringLiteral("Remote"));

  QVERIFY2(GitRepo::pull(_repo.get(), QString(), true),
           qPrintable(lastErrorMessage()));
  // The local commit is replayed on top of the remote one, no merge commit.
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral("HEAD~1")}), remote);
  QCOMPARE(git({QStringLiteral("log"), QStringLiteral("-1"),
                QStringLiteral("--format=%s|%an|%cn|%P")}),
           QStringLiteral("Local|Other Author|Tester|") + remote);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QVERIFY(!GitRepo::getIsRebaseInProgress(_repo.get()));
  QVERIFY(!status().hasChanges());
  QCOMPARE(readFile(file("r.txt")), QByteArray("r\n"));
  QCOMPARE(readFile(file("l.txt")), QByteArray("l\n"));
}

void GitSyncTest::pullRebaseConflictRestoresBranch() {
  QVERIFY(writeFile(file("a.txt"), "local\n"));
  const QString local = commitAll(QStringLiteral("Local"));
  theirsCommit(QStringLiteral("a.txt"), "remote\n", QStringLiteral("Remote"));
  setUnrelatedError();

  QVERIFY(!GitRepo::pull(_repo.get(), QString(), true));
  // The rebase was aborted, the branch is where it was.
  QCOMPARE(head(), local);
  QCOMPARE(git({QStringLiteral("symbolic-ref"), QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(readFile(file("a.txt")), QByteArray("local\n"));
  QVERIFY(!GitRepo::getIsRebaseInProgress(_repo.get()));
  QVERIFY(!status().hasChanges());
  // The caller learns why.
  QCOMPARE(lastError(), GitError::PullRebaseFailedWithConflicts);
}

// Push.

void GitSyncTest::pushFastForwardAndNothing() {
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  const QString local = commitAll(QStringLiteral("Local"));

  QVERIFY2(GitRepo::push(_repo.get(), QString()), qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("main")), local);
  // The remote-tracking branch follows.
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/main")), local);

  // Nothing new to push.
  QVERIFY2(GitRepo::push(_repo.get(), QStringLiteral("main")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("main")), local);
}

void GitSyncTest::pushWithoutUpstreamSetsError() {
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("lonely")});
  setUnrelatedError();
  QVERIFY(!GitRepo::push(_repo.get(), QString()));
  QCOMPARE(remoteRev(QStringLiteral("lonely")), QString());
  QCOMPARE(lastError(), GitError::PushNoUpstreamConfigured);
}

void GitSyncTest::pushRejectedAndForced() {
  const QString remote = theirsCommit(QStringLiteral("r.txt"), "r\n",
                                      QStringLiteral("Remote"));
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  const QString local = commitAll(QStringLiteral("Local"));

  QVERIFY(!GitRepo::push(_repo.get(), QString()));
  QCOMPARE(lastError(), GitError::PushFailedToPushRemote);
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(remoteRev(QStringLiteral("main")), remote);

  QVERIFY2(GitRepo::push(_repo.get(), QString(), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("main")), local);
}

void GitSyncTest::pushToDifferentlyNamedBranch() {
  git({QStringLiteral("checkout"), QStringLiteral("-q"), QStringLiteral("-b"),
       QStringLiteral("work")});
  QVERIFY(writeFile(file("w.txt"), "w\n"));
  const QString work = commitAll(QStringLiteral("Work"));
  git({QStringLiteral("config"), QStringLiteral("branch.work.remote"),
       QStringLiteral("origin")});
  git({QStringLiteral("config"), QStringLiteral("branch.work.merge"),
       QStringLiteral("refs/heads/release")});
  reopen();

  QVERIFY2(GitRepo::push(_repo.get(), QString()), qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("release")), work);
  QCOMPARE(remoteRev(QStringLiteral("work")), QString());
  QCOMPARE(remoteRev(QStringLiteral("main")), rev(QStringLiteral("main")));
}

void GitSyncTest::pushNewBranch() {
  QVERIFY(GitRepo::createBranchFromCommit(_repo.get(), head(),
                                          QStringLiteral("feature/new")));
  QVERIFY(GitRepo::checkoutBranch(_repo.get(), QStringLiteral("feature/new"),
                                  GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("n.txt"), "n\n"));
  const QString tip = commitAll(QStringLiteral("New"));

  // The new branch tracks a remote branch of the same name, which the first
  // push creates.
  QVERIFY2(GitRepo::push(_repo.get(), QString()), qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("feature/new")), tip);
  QVERIFY(GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")));
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/feature/new")), tip);
}

void GitSyncTest::pushToCheckedOutBranchOfNonBareRemote() {
  // The seed is a working copy with main checked out.
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("seed"), _seed,
                             QString()));
  QVERIFY(GitRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("main"), QStringLiteral("main")));
  git({QStringLiteral("fetch"), QStringLiteral("-q"), QStringLiteral("seed")});
  git({QStringLiteral("config"), QStringLiteral("branch.main.remote"),
       QStringLiteral("seed")});
  reopen();
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  commitAll(QStringLiteral("Local"));
  const QString seedHead =
      gitAt(_seed, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});

  // Like `git push` (receive.denyCurrentBranch), the seed's working copy
  // would no longer match its HEAD.
  QVERIFY(!GitRepo::push(_repo.get(), QString()));
  QCOMPARE(lastError(), GitError::PushFailedToPushRemote);
  QCOMPARE(gitAt(_seed, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")}),
           seedHead);
}

void GitSyncTest::tagsOnRemote() {
  const QString base = head();
  QVERIFY(GitRepo::createTagFromCommit(_repo.get(), base,
                                       QStringLiteral("annotated"), kAuthor,
                                       kEmail, QStringLiteral("Annotated")));
  git({QStringLiteral("tag"), QStringLiteral("light")});
  git({QStringLiteral("tag"), QStringLiteral("release/1.0")});
  git({QStringLiteral("tag"), QStringLiteral("1.0")});

  // One tag.
  QVERIFY2(GitRepo::pushTag(_repo.get(), QStringLiteral("annotated")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/annotated")),
           rev(QStringLiteral("refs/tags/annotated")));
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/light")), QString());

  QVERIFY(!GitRepo::pushTag(_repo.get(), QStringLiteral("missing")));
  QCOMPARE(lastError(), GitError::PushTagFailedToLookupReference);

  // All tags.
  QVERIFY2(GitRepo::pushAllTags(_repo.get()), qPrintable(lastErrorMessage()));
  for (const QString &tag :
       {QStringLiteral("light"), QStringLiteral("release/1.0"),
        QStringLiteral("1.0")}) {
    QCOMPARE(remoteRev(QStringLiteral("refs/tags/") + tag), base);
  }

  // A tag that points elsewhere on the remote is not overwritten.
  QVERIFY(writeFile(file("l.txt"), "l\n"));
  commitAll(QStringLiteral("Local"));
  git({QStringLiteral("tag"), QStringLiteral("-f"), QStringLiteral("light")});
  const bool moved = GitRepo::pushTag(_repo.get(), QStringLiteral("light"));
  // `git push` refuses to move an existing tag without --force.
  QVERIFY(!moved);
  QCOMPARE(lastError(), GitError::PushTagExistsOnRemote);
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/light")), base);
  gitAt(_remote, {QStringLiteral("tag"), QStringLiteral("-f"),
                  QStringLiteral("light"), base});

  // Deleting a remote tag keeps the others.
  QVERIFY2(GitRepo::deleteRemoteTag(_repo.get(), QStringLiteral("annotated")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/annotated")), QString());
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/light")), base);
  // The local tag stays.
  QVERIFY(!rev(QStringLiteral("refs/tags/annotated")).isEmpty());

  // A tag name with a slash.
  QVERIFY(GitRepo::deleteRemoteTag(_repo.get(), QStringLiteral("release/1.0")));
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/release/1.0")), QString());
  QCOMPARE(remoteRev(QStringLiteral("refs/tags/1.0")), base);
}

void GitSyncTest::deleteRemoteBranch() {
  theirsCommit(QStringLiteral("f.txt"), "f\n", QStringLiteral("F"),
               QStringLiteral("feature"));
  theirsCommit(QStringLiteral("g.txt"), "g\n", QStringLiteral("G"),
               QStringLiteral("other"));
  git({QStringLiteral("fetch"), QStringLiteral("-q")});
  git({QStringLiteral("branch"), QStringLiteral("--track"),
       QStringLiteral("feature"), QStringLiteral("origin/feature")});
  reopen();

  QVERIFY2(GitRepo::deleteRemoteBranch(_repo.get(), QStringLiteral("feature")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("refs/heads/feature")), QString());
  // The local branch stays, its remote-tracking branch goes.
  QVERIFY(!rev(QStringLiteral("refs/heads/feature")).isEmpty());
  QCOMPARE(rev(QStringLiteral("refs/remotes/origin/feature")), QString());

  // As shown in the remote branches list.
  QVERIFY(GitRepo::deleteRemoteBranch(_repo.get(), QStringLiteral("origin/other")));
  QCOMPARE(remoteRev(QStringLiteral("refs/heads/other")), QString());

  // A branch of another remote.
  const QString upstream = _base + QStringLiteral("/upstream.git");
  makeBareCopy(upstream);
  gitAt(_seed, {QStringLiteral("push"), QStringLiteral("-q"), upstream,
                QStringLiteral("main:topic")});
  QVERIFY(GitRepo::addRemote(_repo.get(), QStringLiteral("upstream"), upstream,
                             QString()));
  QVERIFY(GitRepo::fetchRemote(_repo.get(), QStringLiteral("upstream")));
  GitRepo::deleteRemoteBranch(_repo.get(), QStringLiteral("upstream/topic"));
  QCOMPARE(gitIn(_config, upstream,
                 {QStringLiteral("rev-parse"), QStringLiteral("-q"),
                  QStringLiteral("--verify"), QStringLiteral("refs/heads/topic")},
                 nullptr),
           QString());
}

void GitSyncTest::remoteStatusAfterFetch() {
  const auto statusOf = [this](const QString &name) {
    for (const GitRepo::BranchInfo &info :
         GitRepo::listRemoteBranchesWithStatus(_repo.get(), _quit)) {
      if (info.name == name) {
        return info.statusInfo;
      }
    }
    return GitRepo::BranchStatus{};
  };

  QCOMPARE(statusOf(QStringLiteral("origin/main")).status,
           GitRepo::BranchStatusType::UpToDate);

  QVERIFY(writeFile(file("l.txt"), "l\n"));
  commitAll(QStringLiteral("Local 1"));
  QVERIFY(writeFile(file("l.txt"), "l2\n"));
  commitAll(QStringLiteral("Local 2"));
  GitRepo::BranchStatus s = statusOf(QStringLiteral("origin/main"));
  QCOMPARE(s.status, GitRepo::BranchStatusType::Ahead);
  QCOMPARE(s.aheadCount, size_t(2));

  // The remote moved, only a fetch shows it.
  theirsCommit(QStringLiteral("r.txt"), "r\n", QStringLiteral("Remote"));
  QCOMPARE(statusOf(QStringLiteral("origin/main")).status,
           GitRepo::BranchStatusType::Ahead);
  QVERIFY(GitRepo::fetchRemote(_repo.get(), QStringLiteral("origin")));
  s = statusOf(QStringLiteral("origin/main"));
  QCOMPARE(s.status, GitRepo::BranchStatusType::Diverged);
  QCOMPARE(s.aheadCount, size_t(2));
  QCOMPARE(s.behindCount, size_t(1));

  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD~2"),
                                 GitRepo::ResetType::Hard));
  s = statusOf(QStringLiteral("origin/main"));
  QCOMPARE(s.status, GitRepo::BranchStatusType::Behind);
  QCOMPARE(s.behindCount, size_t(1));
}

// Submodules.

void GitSyncTest::addSubmoduleFromLocalRepo() {
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  const QString sourceHead =
      gitAt(source, {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});

  QVERIFY2(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("sub"),
                                 QString(), newUuid()),
           qPrintable(lastErrorMessage()));
  // .gitmodules and the gitlink are staged together.
  const GitRepo::GitStatus st = status();
  QVERIFY2(st.newStagedFiles.contains(QStringLiteral(".gitmodules")),
           qPrintable(st.newStagedFiles.join(',')));
  QVERIFY(st.newStagedFiles.contains(QStringLiteral("sub")));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("-f"),
                QStringLiteral(".gitmodules"),
                QStringLiteral("submodule.sub.path")}),
           QStringLiteral("sub"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("-f"),
                QStringLiteral(".gitmodules"),
                QStringLiteral("submodule.sub.url")}),
           source);
  // The submodule tracks the branch its remote has checked out.
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("-f"),
                QStringLiteral(".gitmodules"),
                QStringLiteral("submodule.sub.branch")}),
           QStringLiteral("main"));
  QCOMPARE(gitAt(file("sub"), {QStringLiteral("symbolic-ref"),
                               QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/main"));
  QCOMPARE(gitAt(file("sub"), {QStringLiteral("rev-parse"),
                               QStringLiteral("HEAD")}),
           sourceHead);
  QCOMPARE(gitAt(file("sub"), {QStringLiteral("rev-parse"),
                               QStringLiteral("--abbrev-ref"),
                               QStringLiteral("@{upstream}")}),
           QStringLiteral("origin/main"));
  QCOMPARE(readFile(file("sub/s.txt")), QByteArray("s2\n"));

  QCOMPARE(GitRepo::listSubmodules(_repo.get()), QStringList{QStringLiteral("sub")});
  QCOMPARE(GitRepo::getSubmodulePath(_repo.get(), QStringLiteral("sub")),
           QStringLiteral("sub"));
  QVERIFY(GitRepo::getSubmodulePath(_repo.get(), QStringLiteral("nope")).isEmpty());
  QCOMPARE(lastError(), GitError::GetSubmodulePathFailedToLookupSubmodule);

  // git understands what was written.
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add sub"),
                                QString(), kAuthor, kEmail, true, false, false));
  QVERIFY(git({QStringLiteral("submodule"), QStringLiteral("status")})
              .contains(sourceHead + QStringLiteral(" sub")));
}

void GitSyncTest::addSubmoduleWithBranchAndSpaces() {
  const QString source = makeSubmoduleSource(QStringLiteral("sub source"));
  const QString devHead =
      gitAt(source, {QStringLiteral("rev-parse"), QStringLiteral("dev")});

  QVERIFY2(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("my sub"),
                                 QStringLiteral("dev"), newUuid()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(gitAt(file("my sub"), {QStringLiteral("symbolic-ref"),
                                  QStringLiteral("HEAD")}),
           QStringLiteral("refs/heads/dev"));
  QCOMPARE(gitAt(file("my sub"), {QStringLiteral("rev-parse"),
                                  QStringLiteral("HEAD")}),
           devHead);
  // The files are the ones of dev, the submodule has no local changes.
  QCOMPARE(readFile(file("my sub/s.txt")), QByteArray("s\n"));
  QCOMPARE(gitAt(file("my sub"), {QStringLiteral("status"),
                                  QStringLiteral("--porcelain")}),
           QString());
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("-f"),
                QStringLiteral(".gitmodules"),
                QStringLiteral("submodule.my sub.branch")}),
           QStringLiteral("dev"));
  // The gitlink records the branch tip.
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral(":my sub")}),
           devHead);
}

void GitSyncTest::addSubmoduleFailuresCleanUp_data() {
  QTest::addColumn<QString>("kind");
  QTest::newRow("missing url") << QStringLiteral("url");
  QTest::newRow("missing branch") << QStringLiteral("branch");
}

void GitSyncTest::addSubmoduleFailuresCleanUp() {
  QFETCH(QString, kind);
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  const QString url =
      kind == QStringLiteral("url") ? _base + QStringLiteral("/missing") : source;
  const QString branch =
      kind == QStringLiteral("branch") ? QStringLiteral("nope") : QString();

  QVERIFY(!GitRepo::addSubmodule(_repo.get(), url, QStringLiteral("sub"),
                                 branch, newUuid()));
  // Nothing of the submodule is left.
  QVERIFY(!QFileInfo::exists(file("sub")));
  QVERIFY(!QFileInfo::exists(file(".git/modules/sub")));
  QVERIFY2(!readFile(file(".gitmodules")).contains("sub"),
           readFile(file(".gitmodules")).constData());
  QVERIFY(!readFile(file(".git/config")).contains("submodule"));
  QVERIFY(git({QStringLiteral("ls-files"), QStringLiteral("--stage")})
              .indexOf(QStringLiteral("160000")) < 0);
  QVERIFY(GitRepo::listSubmodules(_repo.get()).isEmpty());
}

void GitSyncTest::addSubmoduleKeepsExistingSubmodule() {
  const QString first = makeSubmoduleSource(QStringLiteral("first"));
  const QString second = makeSubmoduleSource(QStringLiteral("second"));
  QVERIFY(GitRepo::addSubmodule(_repo.get(), first, QStringLiteral("sub"),
                                QString(), newUuid()));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add sub"),
                                QString(), kAuthor, kEmail, true, false, false));
  // Work in progress inside the submodule.
  QVERIFY(writeFile(file("sub/precious.txt"), "precious\n"));

  // Adding another submodule under the same name is refused, and the
  // existing one is left alone.
  QVERIFY(!GitRepo::addSubmodule(_repo.get(), second, QStringLiteral("sub"),
                                 QString(), newUuid()));
  QCOMPARE(lastError(), GitError::AddSubmoduleAlreadyExists);
  QCOMPARE(readFile(file("sub/precious.txt")), QByteArray("precious\n"));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("-f"),
                QStringLiteral(".gitmodules"),
                QStringLiteral("submodule.sub.url")},
               nullptr),
           first);
}

void GitSyncTest::submoduleStatuses() {
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  QVERIFY(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("sub"),
                                QString(), newUuid()));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add sub"),
                                QString(), kAuthor, kEmail, true, false, false));
  const QString pinned = git({QStringLiteral("rev-parse"), QStringLiteral(":sub")});

  const auto statusOf = [this]() {
    const QVector<GitRepo::SubmoduleInfo> infos =
        GitRepo::listSubmodulesWithStatus(_repo.get(), _quit);
    return infos.size() == 1 ? infos[0] : GitRepo::SubmoduleInfo{};
  };

  GitRepo::SubmoduleInfo info = statusOf();
  QCOMPARE(info.name, QStringLiteral("sub"));
  QCOMPARE(info.commit, pinned);
  QCOMPARE(info.statusInfo.status, GitRepo::SubmoduleStatusType::UpToDate);

  // A new commit inside the submodule.
  QVERIFY(writeFile(file("sub/s.txt"), "s3\n"));
  gitAt(file("sub"), {QStringLiteral("commit"), QStringLiteral("-qam"),
                      QStringLiteral("S3")});
  info = statusOf();
  QCOMPARE(info.statusInfo.status, GitRepo::SubmoduleStatusType::Outdated);
  QCOMPARE(info.commit,
           gitAt(file("sub"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")}));

  // Cancelled.
  _quit = true;
  QVERIFY(GitRepo::listSubmodulesWithStatus(_repo.get(), _quit).isEmpty());
  _quit = false;

  // A clone without --recurse-submodules has the submodule, not checked out.
  const QString clone = _base + QStringLiteral("/clone");
  git({QStringLiteral("push"), QStringLiteral("-q"), QStringLiteral("origin"),
       QStringLiteral("main")});
  gitAt(_base, {QStringLiteral("clone"), QStringLiteral("-q"), _remote, clone});
  {
    RepoPtr cloned(GitRepo::openRepository(clone, kAuthor, kEmail));
    QVERIFY(cloned);
    const QVector<GitRepo::SubmoduleInfo> infos =
        GitRepo::listSubmodulesWithStatus(cloned.get(), _quit);
    QCOMPARE(infos.size(), 1);
    QCOMPARE(infos[0].statusInfo.status, GitRepo::SubmoduleStatusType::Error);
    QVERIFY(infos[0].commit.isEmpty());
  }

  // The checkout was deleted.
  makeWritable(file("sub"));
  QVERIFY(QDir(file("sub")).removeRecursively());
  const QVector<GitRepo::SubmoduleInfo> infos =
      GitRepo::listSubmodulesWithStatus(_repo.get(), _quit);
  QCOMPARE(infos.size(), 1);
  QCOMPARE(infos[0].statusInfo.status, GitRepo::SubmoduleStatusType::Error);
}

void GitSyncTest::updateSubmoduleRecordsCurrentCommit() {
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  QVERIFY(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("sub"),
                                QString(), newUuid()));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add sub"),
                                QString(), kAuthor, kEmail, true, false, false));
  QVERIFY(writeFile(file("sub/s.txt"), "s3\n"));
  gitAt(file("sub"), {QStringLiteral("commit"), QStringLiteral("-qam"),
                      QStringLiteral("S3")});
  const QString subHead =
      gitAt(file("sub"), {QStringLiteral("rev-parse"), QStringLiteral("HEAD")});

  // Updating records the submodule's current commit in the parent.
  QVERIFY2(GitRepo::updateSubmodule(_repo.get(), QStringLiteral("sub")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(git({QStringLiteral("rev-parse"), QStringLiteral(":sub")}), subHead);
  QCOMPARE(status().modifiedStagedFiles, QStringList{QStringLiteral("sub")});
  // The status compares with the commit pinned in HEAD, so it is up to date
  // once the new pin is committed.
  QCOMPARE(GitRepo::listSubmodulesWithStatus(_repo.get(), _quit)[0]
               .statusInfo.status,
           GitRepo::SubmoduleStatusType::Outdated);
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Bump sub"),
                                QString(), kAuthor, kEmail, true, false, false));
  QCOMPARE(GitRepo::listSubmodulesWithStatus(_repo.get(), _quit)[0]
               .statusInfo.status,
           GitRepo::SubmoduleStatusType::UpToDate);

  QVERIFY(!GitRepo::updateSubmodule(_repo.get(), QStringLiteral("nope")));
  QCOMPARE(lastError(), GitError::UpdateSubmoduleFailedToUpdateSubmodule);
}

void GitSyncTest::deleteSubmoduleRemovesEverything() {
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  QVERIFY(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("sub"),
                                QString(), newUuid()));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add sub"),
                                QString(), kAuthor, kEmail, true, false, false));

  QVERIFY2(GitRepo::deleteSubmodule(_repo.get(), QStringLiteral("sub"),
                                    file("sub")),
           qPrintable(lastErrorMessage()));
  QVERIFY(!QFileInfo::exists(file("sub")));
  QVERIFY(!QFileInfo::exists(file(".git/modules/sub")));
  QVERIFY(!readFile(file(".gitmodules")).contains("sub"));
  QVERIFY(!readFile(file(".git/config")).contains("submodule"));
  const GitRepo::GitStatus st = status();
  QVERIFY2(st.deletedStagedFiles.contains(QStringLiteral("sub")),
           qPrintable(st.deletedStagedFiles.join(',')));
  QVERIFY(GitRepo::listSubmodules(_repo.get()).isEmpty());

  // Deleting it again changes nothing and doesn't fail.
  QVERIFY(GitRepo::deleteSubmodule(_repo.get(), QStringLiteral("sub"),
                                   file("sub")));
}

void GitSyncTest::deleteSubmoduleKeepsOtherSubmodules() {
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  QVERIFY(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("sub"),
                                QString(), newUuid()));
  QVERIFY(GitRepo::addSubmodule(_repo.get(), source, QStringLiteral("sub2"),
                                QString(), newUuid()));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Add subs"),
                                QString(), kAuthor, kEmail, true, false, false));

  QVERIFY(GitRepo::deleteSubmodule(_repo.get(), QStringLiteral("sub"),
                                   file("sub")));
  QVERIFY(QFileInfo::exists(file("sub2/s.txt")));
  QCOMPARE(git({QStringLiteral("config"), QStringLiteral("-f"),
                QStringLiteral(".gitmodules"),
                QStringLiteral("submodule.sub2.path")}),
           QStringLiteral("sub2"));
  QVERIFY(readFile(file(".git/config")).contains("[submodule \"sub2\"]"));
}

void GitSyncTest::deleteSubmoduleWhoseNameDiffersFromPath() {
  const QString source = makeSubmoduleSource(QStringLiteral("subsource"));
  bool ok = false;
  git({QStringLiteral("submodule"), QStringLiteral("add"), QStringLiteral("-q"),
       QStringLiteral("--name"), QStringLiteral("library"), source,
       QStringLiteral("external/lib")},
      &ok);
  QVERIFY(ok);
  git({QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("Lib")});
  reopen();
  QCOMPARE(GitRepo::getSubmodulePath(_repo.get(), QStringLiteral("library")),
           QStringLiteral("external/lib"));

  QVERIFY(GitRepo::deleteSubmodule(_repo.get(), QStringLiteral("library"),
                                   file("external/lib")));
  QVERIFY(!QFileInfo::exists(file("external/lib/s.txt")));
  QCOMPARE(git({QStringLiteral("ls-files"), QStringLiteral("--stage"),
                QStringLiteral("external/lib")}),
           QString());
}

void GitSyncTest::addSubmoduleWithNestedSubmodules() {
  const QString inner = makeSubmoduleSource(QStringLiteral("inner"));
  const QString outer = makeSubmoduleSource(QStringLiteral("outer"));
  bool ok = false;
  gitAt(outer, {QStringLiteral("submodule"), QStringLiteral("add"),
                QStringLiteral("-q"), inner, QStringLiteral("nested")},
        &ok);
  QVERIFY(ok);
  gitAt(outer, {QStringLiteral("commit"), QStringLiteral("-qm"),
                QStringLiteral("Nested")});

  QVERIFY2(GitRepo::addSubmodule(_repo.get(), outer, QStringLiteral("outer"),
                                 QString(), newUuid()),
           qPrintable(lastErrorMessage()));
  // Like `git submodule add`, nested submodules are not checked out.
  QVERIFY(QFileInfo(file("outer/nested")).isDir());
  QVERIFY(!QFileInfo::exists(file("outer/nested/s.txt")));
  QCOMPARE(GitRepo::listSubmodules(_repo.get()),
           QStringList{QStringLiteral("outer")});
}

void GitSyncTest::deleteSubmoduleRefusesOtherDirectories() {
  // Everything here is below the test's temporary directory: a directory
  // that stands in for the process's current directory, and the working copy.
  const QString current = _base + QStringLiteral("/current");
  QVERIFY(writeFile(current + QStringLiteral("/keep.txt"), "keep\n"));
  const QString previous = QDir::currentPath();
  QVERIFY(QDir::setCurrent(current));
  const auto restore = qScopeGuard([&] { QDir::setCurrent(previous); });

  // No submodule and no path: nothing may be deleted.
  GitRepo::deleteSubmodule(_repo.get(), QString(), QString());
  QVERIFY(QFileInfo::exists(current + QStringLiteral("/keep.txt")));

  // The bridge passes <repository>/<name>: an empty name is the working copy.
  QVERIFY(writeFile(file("precious.txt"), "precious\n"));
  GitRepo::deleteSubmodule(_repo.get(), QString(), _root);
  QVERIFY(QFileInfo::exists(file("precious.txt")));
  QVERIFY(QFileInfo::exists(file("a.txt")));
}

// LFS.

void GitSyncTest::lfsPointerParse_data() {
  QTest::addColumn<QByteArray>("data");
  QTest::addColumn<bool>("valid");
  QTest::addColumn<qint64>("size");

  const QByteArray oid = QByteArray(64, 'a');
  const QByteArray version = "version https://git-lfs.github.com/spec/v1\n";
  QTest::newRow("made") << GitRepo::Lfs::makePointer(QString::fromLatin1(oid), 12)
                        << true << qint64(12);
  QTest::newRow("extension keys")
      << version + "ext-0-foo sha256:" + QByteArray(64, 'b') + "\noid sha256:" +
             oid + "\nsize 5\n"
      << true << qint64(5);
  QTest::newRow("crlf") << QByteArray(version).replace("\n", "\r\n") +
                               "oid sha256:" + oid + "\r\nsize 7\r\n"
                        << true << qint64(7);
  QTest::newRow("zero size") << version + "oid sha256:" + oid + "\nsize 0\n"
                             << true << qint64(0);
  QTest::newRow("wrong version")
      << "version https://hawser.github.com/spec/v2\noid sha256:" + oid +
             "\nsize 5\n"
      << false << qint64(0);
  QTest::newRow("missing size") << version + "oid sha256:" + oid + "\n" << false
                                << qint64(0);
  QTest::newRow("negative size") << version + "oid sha256:" + oid + "\nsize -1\n"
                                 << false << qint64(0);
  QTest::newRow("size not a number")
      << version + "oid sha256:" + oid + "\nsize ten\n" << false << qint64(0);
  QTest::newRow("short oid") << version + "oid sha256:" + oid.left(63) +
                                    "\nsize 5\n"
                             << false << qint64(0);
  QTest::newRow("other hash") << version + "oid sha1:" + oid.left(40) + "\nsize 5\n"
                              << false << qint64(0);
  QTest::newRow("empty") << QByteArray() << false << qint64(0);
  QTest::newRow("plain text") << QByteArray("just a file\n") << false << qint64(0);
  QTest::newRow("pointer inside a bigger file")
      << "Some text before\n" + version + "oid sha256:" + oid +
             "\nsize 5\nand more after\n" + QByteArray(2000, 'x')
      << false << qint64(0);
}

void GitSyncTest::lfsPointerParse() {
  QFETCH(QByteArray, data);
  QFETCH(bool, valid);
  QFETCH(qint64, size);

  const GitRepo::Lfs::Pointer pointer = GitRepo::Lfs::parsePointer(data);
  QCOMPARE(pointer.valid, valid);
  if (valid) {
    QCOMPARE(pointer.oid, QString(64, QLatin1Char('a')));
    QCOMPARE(pointer.size, size);
  } else {
    QVERIFY(!pointer.errorMessage.isEmpty());
  }
}

void GitSyncTest::lfsObjectStore() {
  const QByteArray content = "large file content\n";
  const QString expectedOid = LfsStubServer::oidOf(content);

  QString oid;
  QVERIFY(GitRepo::Lfs::storeObject(_repo.get(), content, oid));
  QCOMPARE(oid, expectedOid);
  QCOMPARE(GitRepo::Lfs::objectPath(_repo.get(), oid),
           file(".git/lfs/objects/") + oid.left(2) + '/' + oid.mid(2, 2) + '/' +
               oid);
  QVERIFY(GitRepo::Lfs::objectPath(_repo.get(), QStringLiteral("abc")).isEmpty());
  QVERIFY(GitRepo::Lfs::objectExists(_repo.get(), oid));
  QVERIFY(!GitRepo::Lfs::objectExists(_repo.get(), QString(64, QLatin1Char('f'))));
  QByteArray read;
  QVERIFY(GitRepo::Lfs::readObject(_repo.get(), oid, read));
  QCOMPARE(read, content);
  QVERIFY(!GitRepo::Lfs::readObject(_repo.get(), QString(64, QLatin1Char('f')),
                                    read));

  // Storing again keeps one copy.
  QVERIFY(GitRepo::Lfs::storeObject(_repo.get(), content, oid));
  QString emptyOid;
  QVERIFY(GitRepo::Lfs::storeObject(_repo.get(), QByteArray(), emptyOid));
  const GitRepo::Lfs::StorageStatus storage =
      GitRepo::getLfsStorageStatus(_repo.get());
  QCOMPARE(storage.objectCount, qint64(2));
  QCOMPARE(storage.totalBytes, qint64(content.size()));
  QCOMPARE(storage.repositoryPath, _root);
  // No temporary files are left.
  QDirIterator it(file(".git/lfs"), QDir::Files, QDirIterator::Subdirectories);
  int files = 0;
  while (it.hasNext()) {
    it.next();
    ++files;
  }
  QCOMPARE(files, 2);
}

void GitSyncTest::lfsStageAndSmudgeFromLocalStore() {
  QVERIFY(writeFile(file(".gitattributes"), kLfsAttributes));
  commitAll(QStringLiteral("Attributes"));
  reopen();
  const QByteArray content = QByteArray("binary\0data", 11).repeated(1000);
  QVERIFY(writeFile(file("big.bin"), content));

  // Staging stores the content and puts a pointer in the index.
  QVERIFY2(GitRepo::stageFiles(_repo.get(), {QStringLiteral("big.bin")}),
           qPrintable(lastErrorMessage()));
  const QString oid = LfsStubServer::oidOf(content);
  QVERIFY(GitRepo::Lfs::objectExists(_repo.get(), oid));
  bool ok = false;
  const QString blob = git({QStringLiteral("cat-file"), QStringLiteral("-p"),
                            QStringLiteral(":big.bin")},
                           &ok);
  QVERIFY(ok);
  const GitRepo::Lfs::Pointer pointer = GitRepo::Lfs::parsePointer(blob.toUtf8());
  QVERIFY2(pointer.valid, qPrintable(blob));
  QCOMPARE(pointer.oid, oid);
  QCOMPARE(pointer.size, qint64(content.size()));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Big"), QString(),
                                kAuthor, kEmail, true, false, false));
  QVERIFY(!status().hasChanges());

  // Checking out writes the content back from the local store.
  QVERIFY(QFile::remove(file("big.bin")));
  QVERIFY2(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD"),
                                  GitRepo::ResetType::Hard),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("big.bin")), content);
  QVERIFY(!status().hasChanges());

  // Discarding a change does the same.
  QVERIFY(writeFile(file("big.bin"), "changed"));
  QVERIFY2(GitRepo::discardFiles(_repo.get(), {QStringLiteral("big.bin")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("big.bin")), content);
  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD"),
                                 GitRepo::ResetType::Hard));
  QCOMPARE(readFile(file("big.bin")), content);

  // The diff shows no pointer text as a change.
  QVERIFY(writeFile(file("big.bin"), content + "more"));
  QCOMPARE(status().modifiedFiles, QStringList{QStringLiteral("big.bin")});
}

void GitSyncTest::lfsSmudgeDownloadsMissingObject() {
  LfsStubServer server;
  QVERIFY(server.start());
  const QByteArray content = "downloaded content\n";
  server.addObject(content);
  const QString oid = LfsStubServer::oidOf(content);
  useLfsServer(server);

  // The pointer is committed as it is, git has no LFS filter here.
  QVERIFY(writeFile(file(".gitattributes"), kLfsAttributes));
  QVERIFY(writeFile(file("big.bin"),
                    GitRepo::Lfs::makePointer(oid, content.size())));
  commitAll(QStringLiteral("Pointer"));
  QVERIFY(QFile::remove(file("big.bin")));
  reopen();

  CredentialPrompts prompts;
  QVERIFY2(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD"),
                                  GitRepo::ResetType::Hard),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("big.bin")), content);
  QVERIFY(GitRepo::Lfs::objectExists(_repo.get(), oid));
  QCOMPARE(server.requests(),
           (QStringList{QStringLiteral("POST /team/repo.git/info/lfs/objects/batch"),
                        QStringLiteral("GET /objects/") + oid}));
  QCOMPARE(server.batches(), QStringList{QStringLiteral("download:") + oid});
  // git-lfs asks for credentials only when the server wants them.
  QVERIFY2(prompts.urls().isEmpty(), qPrintable(prompts.urls().join(',')));

  // The next checkout uses the local copy.
  server.clearLog();
  QVERIFY(QFile::remove(file("big.bin")));
  QVERIFY(GitRepo::resetToCommit(_repo.get(), QStringLiteral("HEAD"),
                                 GitRepo::ResetType::Hard));
  QCOMPARE(readFile(file("big.bin")), content);
  QVERIFY(server.requests().isEmpty());
}

void GitSyncTest::lfsSmudgeRejectsWrongSize() {
  const QByteArray content = "real content\n";
  QString oid;
  QVERIFY(GitRepo::Lfs::storeObject(_repo.get(), content, oid));
  QVERIFY(writeFile(file(".gitattributes"), kLfsAttributes));
  // The pointer claims another size than the object has.
  QVERIFY(writeFile(file("big.bin"),
                    GitRepo::Lfs::makePointer(oid, content.size() + 1)));
  commitAll(QStringLiteral("Pointer"));
  QVERIFY(QFile::remove(file("big.bin")));
  reopen();

  QVERIFY(!GitRepo::discardFiles(_repo.get(), {QStringLiteral("big.bin")}));
  QVERIFY(readFile(file("big.bin")) != content);
}

void GitSyncTest::lfsDownloadErrors_data() {
  QTest::addColumn<int>("fault");
  QTest::addColumn<int>("status");
  QTest::addColumn<bool>("onServer");
  using Fault = LfsStubServer::Fault;
  QTest::newRow("unauthorized") << int(Fault::BatchStatus) << 401 << true;
  QTest::newRow("batch not found") << int(Fault::BatchStatus) << 404 << true;
  QTest::newRow("server error") << int(Fault::BatchStatus) << 500 << true;
  QTest::newRow("malformed batch") << int(Fault::MalformedBatch) << 0 << true;
  QTest::newRow("no actions") << int(Fault::NoActions) << 0 << true;
  QTest::newRow("download fails") << int(Fault::DownloadStatus) << 503 << true;
  QTest::newRow("object missing") << int(Fault::None) << 0 << false;
  QTest::newRow("wrong content") << int(Fault::WrongContent) << 0 << true;
}

void GitSyncTest::lfsDownloadErrors() {
  QFETCH(int, fault);
  QFETCH(int, status);
  QFETCH(bool, onServer);

  LfsStubServer server;
  QVERIFY(server.start());
  const QByteArray content = "xcontent\n";
  const QString oid = LfsStubServer::oidOf(content);
  if (onServer) {
    server.addObject(content);
  }
  server.setFault(static_cast<LfsStubServer::Fault>(fault), status);
  useLfsServer(server);
  reopen();

  CredentialPrompts prompts;
  QByteArray data;
  QVERIFY(!GitRepo::Lfs::downloadObject(_repo.get(), oid, content.size(), data));
  // Nothing is stored under the requested id.
  QVERIFY(!GitRepo::Lfs::objectExists(_repo.get(), oid));
  QCOMPARE(GitRepo::getLfsStorageStatus(_repo.get()).objectCount,
           qint64(0) + (QTest::currentDataTag() == QByteArray("wrong content")
                            ? 1
                            : 0));
}

void GitSyncTest::lfsUploadLocalObjects() {
  LfsStubServer server;
  QVERIFY(server.start());
  QVERIFY(writeFile(file(".gitattributes"), kLfsAttributes));
  commitAll(QStringLiteral("Attributes"));
  reopen();
  const QString endpoint = server.origin() + QStringLiteral("/team/repo.git");

  // Nothing in LFS yet: no request at all.
  QVERIFY(GitRepo::Lfs::uploadLocalObjects(_repo.get(), _root,
                                           QStringLiteral("origin"), endpoint,
                                           QStringLiteral("refs/heads/main")));
  QVERIFY(server.requests().isEmpty());

  const QByteArray content = "to upload\n";
  QVERIFY(writeFile(file("big.bin"), content));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("big.bin")}));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Big"), QString(),
                                kAuthor, kEmail, true, false, false));

  CredentialPrompts prompts;
  QVERIFY2(GitRepo::Lfs::uploadLocalObjects(_repo.get(), _root,
                                            QStringLiteral("origin"), endpoint,
                                            QStringLiteral("refs/heads/main")),
           qPrintable(lastErrorMessage()));
  const QString oid = LfsStubServer::oidOf(content);
  QVERIFY(server.hasObject(oid));
  QCOMPARE(server.requests(),
           (QStringList{QStringLiteral("POST /team/repo.git/info/lfs/objects/batch"),
                        QStringLiteral("PUT /objects/") + oid,
                        QStringLiteral("POST /verify")}));
  QCOMPARE(server.batches(), QStringList{QStringLiteral("upload:") + oid});

  // The server has it now: asked, but not sent again.
  server.clearLog();
  QVERIFY(GitRepo::Lfs::uploadLocalObjects(_repo.get(), _root,
                                           QStringLiteral("origin"), endpoint,
                                           QStringLiteral("refs/heads/main")));
  QVERIFY(server.requests().filter(QStringLiteral("PUT ")).isEmpty());

  // Failures of the server fail the upload, which stops the push.
  QVERIFY(writeFile(file("big.bin"), "changed\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("big.bin")}));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Big 2"), QString(),
                                kAuthor, kEmail, true, false, false));
  server.setFault(LfsStubServer::Fault::UploadStatus, 507);
  QVERIFY(!GitRepo::Lfs::uploadLocalObjects(_repo.get(), _root,
                                            QStringLiteral("origin"), endpoint,
                                            QStringLiteral("refs/heads/main")));
  server.setFault(LfsStubServer::Fault::BatchStatus, 403);
  QVERIFY(!GitRepo::Lfs::uploadLocalObjects(_repo.get(), _root,
                                            QStringLiteral("origin"), endpoint,
                                            QStringLiteral("refs/heads/main")));
  QCOMPARE(server.objectCount(), 1);

  useLfsServer(server);
  reopen();
  QVERIFY(!GitRepo::push(_repo.get(), QString()));
  QCOMPARE(lastError(), GitError::PushFailedToPushRemote);
  QVERIFY(remoteRev(QStringLiteral("main")) != head());
}
void GitSyncTest::lfsPushToLocalRemote() {
  QVERIFY(writeFile(file(".gitattributes"), kLfsAttributes));
  commitAll(QStringLiteral("Attributes"));
  reopen();
  QVERIFY(writeFile(file("big.bin"), "content\n"));
  QVERIFY(GitRepo::stageFiles(_repo.get(), {QStringLiteral("big.bin")}));
  QVERIFY(GitRepo::commitStaged(_repo.get(), QStringLiteral("Big"), QString(),
                                kAuthor, kEmail, true, false, false));

  // git-lfs copies the objects into a local remote's lfs/objects.
  const bool pushed = GitRepo::push(_repo.get(), QString());
  QVERIFY2(pushed, qPrintable(lastErrorMessage()));
  QCOMPARE(remoteRev(QStringLiteral("main")), head());
}

void GitSyncTest::lfsEndpointDiscovery_data() {
  QTest::addColumn<QString>("remoteName");
  QTest::addColumn<QString>("remotePath");
  QTest::addColumn<QStringList>("config");
  QTest::addColumn<QString>("expected");

  QTest::newRow("derived from .git url")
      << QStringLiteral("origin") << QStringLiteral("/team/repo.git")
      << QStringList() << QStringLiteral("/team/repo.git/info/lfs/objects/batch");
  QTest::newRow("derived without .git")
      << QStringLiteral("origin") << QStringLiteral("/team/repo") << QStringList()
      << QStringLiteral("/team/repo.git/info/lfs/objects/batch");
  QTest::newRow("trailing slash")
      << QStringLiteral("origin") << QStringLiteral("/team/repo.git/")
      << QStringList() << QStringLiteral("/team/repo.git/info/lfs/objects/batch");
  QTest::newRow("remote lfsurl")
      << QStringLiteral("origin") << QStringLiteral("/team/repo.git")
      << QStringList{QStringLiteral("remote.origin.lfsurl=<server>/custom")}
      << QStringLiteral("/custom/objects/batch");
  QTest::newRow("lfs.url first")
      << QStringLiteral("origin") << QStringLiteral("/team/repo.git")
      << QStringList{QStringLiteral("remote.origin.lfsurl=<server>/custom"),
                     QStringLiteral("lfs.url=<server>/global")}
      << QStringLiteral("/global/objects/batch");
  QTest::newRow("lfspushurl is for pushing")
      << QStringLiteral("origin") << QStringLiteral("/team/repo.git")
      << QStringList{QStringLiteral("remote.origin.lfsurl=<server>/custom"),
                     QStringLiteral("remote.origin.lfspushurl=<server>/push")}
      << QStringLiteral("/custom/objects/batch");
  QTest::newRow("no origin")
      << QStringLiteral("upstream") << QStringLiteral("/team/repo.git")
      << QStringList() << QStringLiteral("/team/repo.git/info/lfs/objects/batch");
}

void GitSyncTest::lfsEndpointDiscovery() {
  QFETCH(QString, remoteName);
  QFETCH(QString, remotePath);
  QFETCH(QStringList, config);
  QFETCH(QString, expected);

  LfsStubServer server;
  QVERIFY(server.start());
  const QByteArray content = "content\n";
  server.addObject(content);

  git({QStringLiteral("remote"), QStringLiteral("remove"),
       QStringLiteral("origin")});
  git({QStringLiteral("remote"), QStringLiteral("add"), remoteName,
       server.origin() + remotePath});
  for (QString entry : config) {
    entry.replace(QStringLiteral("<server>"), server.origin());
    const int equals = entry.indexOf('=');
    git({QStringLiteral("config"), entry.left(equals), entry.mid(equals + 1)});
  }
  reopen();

  CredentialPrompts prompts;
  QByteArray data;
  GitRepo::Lfs::downloadObject(_repo.get(), LfsStubServer::oidOf(content),
                               content.size(), data);
  QVERIFY(!server.requests().isEmpty());
  QCOMPARE(server.requests().first(), QStringLiteral("POST ") + expected);
}

QTEST_GUILESS_MAIN(GitSyncTest)

#include "tst_gitsync.moc"
