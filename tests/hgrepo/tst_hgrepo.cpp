// MIT License
// Copyright (c) 2026 Sourcerer

// End to end tests of HgRepo against local repositories, using the hg
// executable installed on the machine.

#include "HgAuthServer.h"

#include <ConfigManager.h>
#include <HgRepo.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QScopeGuard>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

namespace {

const QString kAuthor = QStringLiteral("Tester");
const QString kEmail = QStringLiteral("tester@example.com");
const QString kUser = QStringLiteral("Tester <tester@example.com>");
const QString kNullCommit =
    QStringLiteral("0000000000000000000000000000000000000000");

bool writeFile(const QString &path, const QByteArray &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly) && file.write(content) >= 0;
}

QByteArray readFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString lastErrorMessage() {
  return HgRepo::getLastError().errorMessage;
}

QStringList changedLines(const GitRepo::FilesDiffDelta &delta) {
  QStringList lines;
  for (const GitRepo::FilesDiffHunk &hunk : delta.hunks) {
    for (const GitRepo::FilesDiffLine &line : hunk.lines) {
      if (line.origin != ' ') {
        lines.append(line.origin + line.content);
      }
    }
  }
  return lines;
}

struct RepoCloser {
  void operator()(HgRepo::Repository *repo) const {
    HgRepo::closeRepository(repo);
  }
};

using RepoPtr = std::unique_ptr<HgRepo::Repository, RepoCloser>;

// Answers credential requests from a list, cancels when it is used up.
class ScriptedCredentials final : public HgRepo::HgCredentialsProvider {
public:
  bool credentials(const HgRepo::HgAuthRequest &request,
                   HgRepo::HgCredentials *credentials) override {
    requests << request;
    if (answers.isEmpty()) {
      return false;
    }
    *credentials = answers.takeFirst();
    return true;
  }

  void finished(const HgRepo::HgAuthRequest &request,
                const HgRepo::HgAuthResult result) override {
    Q_UNUSED(request);
    results << result;
  }

  QVector<HgRepo::HgCredentials> answers;
  QVector<HgRepo::HgAuthRequest> requests;
  QVector<HgRepo::HgAuthResult> results;

  QVector<HgRepo::HgSshKey> sshKeys() override { return keys; }
  QVector<HgRepo::HgSshKey> keys;
};

const HgRepo::HgCredentials kGoodCredentials{QStringLiteral("alice"),
                                             QStringLiteral("s3cret=!")};
const HgRepo::HgCredentials kBadCredentials{QStringLiteral("alice"),
                                            QStringLiteral("wrong")};

} // namespace

class HgRepoTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  void initWritesUsername();
  void openRejectsNonRepository();
  void openFromSubdirectoryFindsRoot();
  void stageUnstageAndCommit();
  void commitOnlyStagedFiles();
  void commitWithoutStagedFilesFails();
  void stageDirectoryAndMissingFiles();
  void renames();
  void discard();
  void amend();
  void historyPaging();
  void historyRefs();
  void commitStatus();
  void phases();
  void diffs();
  void blameAttributesLines();
  void fileSize();
  void mergeConflicts();
  void cancellationRestartsServer();
  void signingIsUnsupported();

  void namedBranchesAndBookmarks();
  void closedAndRenamedBranches();
  void commitBranchQueries();
  void tags();
  void mergeBranches();
  void mergeConflictAbort();
  void rebaseAndSquash();
  void rebaseConflictContinueAndAbort();
  void graftAndBackout();
  void graftConflictIsFinishedByCommit();
  void stripAndReset();
  void shelves();
  void shelveConflicts();
  void bisectFindsFirstBadCommit();
  void remotesAndTargets();
  void hgrcEditsKeepComments();
  void pullPushAndRemoteStatus();
  void remoteBookmarks();
  void pullMergesDivergedHistory();
  void tagsOnRemote();
  void httpAuthentication();
  void httpAuthenticationCancelled();
  void sshKeySelection();

  void mercurialSubrepos();
  void subrepoKinds();
  void worktrees();
  void lfs();
  void gitFlowWithNamedBranches();
  void gitFlowNextWithBookmarks();
  void applyCustomWorkflow();
  void gitHubFlow();
  void gitLabFlow();

private:
  QString file(const QString &name) const { return _root + "/" + name; }
  // Runs hg directly, for setting up states HgRepo doesn't create yet.
  QByteArray hg(const QStringList &args, bool *ok = nullptr) const;
  QByteArray hgIn(const QString &dir, const QStringList &args,
                  bool *ok = nullptr) const;
  QString commitAll(const QString &summary);
  QString commitIn(HgRepo::Repository *repo, const QString &summary);
  QString tip() const;
  QString nodeOf(const QString &dir, const QString &revision) const;
  // A clone of _root, opened, with remote "default" pointing at _root.
  RepoPtr cloneRoot(QString *clonePath);

  std::unique_ptr<QTemporaryDir> _tmp;
  QString _root;
  RepoPtr _repo;
  bool _quit = false;
};

QByteArray HgRepoTest::hg(const QStringList &args, bool *ok) const {
  return hgIn(_root, args, ok);
}

QByteArray HgRepoTest::hgIn(const QString &dir, const QStringList &args,
                            bool *ok) const {
  QProcess process;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("HGPLAIN"), QStringLiteral("1"));
  env.insert(QStringLiteral("HGENCODING"), QStringLiteral("UTF-8"));
  process.setProcessEnvironment(env);
  process.setWorkingDirectory(dir);
  process.start(HgRepo::findExecutable(),
                QStringList{QStringLiteral("--config"),
                            QStringLiteral("ui.username=") + kUser,
                            QStringLiteral("--config"),
                            QStringLiteral("ui.merge=internal:merge")} +
                    args);
  const bool finished = process.waitForFinished(60 * 1000);
  const bool success = finished &&
                       process.exitStatus() == QProcess::NormalExit &&
                       process.exitCode() == 0;
  if (ok) {
    *ok = success;
  } else if (!success) {
    qWarning() << "hg" << args << "failed:" << process.readAllStandardError();
  }
  return process.readAllStandardOutput();
}

QString HgRepoTest::commitAll(const QString &summary) {
  return commitIn(_repo.get(), summary);
}

QString HgRepoTest::commitIn(HgRepo::Repository *repo, const QString &summary) {
  const GitRepo::GitStatus status = HgRepo::getStatus(repo, _quit);
  QStringList files = status.newFiles + status.modifiedFiles +
                      status.deletedFiles + status.renamedFiles;
  if (!files.isEmpty() && !HgRepo::stageFiles(repo, files)) {
    return QString();
  }
  if (!HgRepo::commitStaged(repo, summary, QString(), kAuthor, kEmail, true,
                            false, false)) {
    return QString();
  }
  return nodeOf(HgRepo::repositoryPath(repo), QStringLiteral("."));
}

QString HgRepoTest::tip() const {
  return nodeOf(_root, QStringLiteral("."));
}

QString HgRepoTest::nodeOf(const QString &dir, const QString &revision) const {
  return QString::fromUtf8(
             hgIn(dir, {QStringLiteral("log"), QStringLiteral("-r"), revision,
                        QStringLiteral("-T"), QStringLiteral("{node}")}))
      .trimmed();
}

RepoPtr HgRepoTest::cloneRoot(QString *clonePath) {
  *clonePath = _tmp->path() + QStringLiteral("/clone");
  bool ok = false;
  hgIn(_tmp->path(),
       {QStringLiteral("clone"), QStringLiteral("--quiet"), _root, *clonePath},
       &ok);
  if (!ok) {
    return RepoPtr();
  }
  return RepoPtr(HgRepo::openRepository(*clonePath, kAuthor, kEmail));
}

void HgRepoTest::initTestCase() {
  if (HgRepo::findExecutable().isEmpty()) {
    QSKIP("hg was not found");
  }
  QVERIFY2(HgRepo::checkExecutable(), qPrintable(lastErrorMessage()));
  qInfo() << "Testing with Mercurial" << HgRepo::version();
  HgRepo::initForCurrentThread();
}

void HgRepoTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _root = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path())) +
          QStringLiteral("/repo");

  QVERIFY2(HgRepo::initRepository(QStringLiteral("repo"), _tmp->path(),
                                  QStringLiteral("main"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  _repo.reset(HgRepo::openRepository(_root, kAuthor, kEmail));
  QVERIFY2(_repo, qPrintable(lastErrorMessage()));
}

void HgRepoTest::cleanup() {
  _repo.reset();
  _tmp.reset();
  _quit = false;
}

void HgRepoTest::initWritesUsername() {
  QVERIFY(QFileInfo(file(".hg")).isDir());
  QVERIFY(readFile(file(".hg/hgrc")).contains(kUser.toUtf8()));
  QCOMPARE(HgRepo::repositoryPath(_repo.get()), _root);
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
}

void HgRepoTest::openRejectsNonRepository() {
  QTemporaryDir empty;
  QVERIFY(!HgRepo::openRepository(empty.path(), kAuthor, kEmail));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::NotARepository);
}

void HgRepoTest::openFromSubdirectoryFindsRoot() {
  QVERIFY(writeFile(file("dir/a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Add a")).isEmpty());

  // A subdirectory has no .hg of its own, so it is not a repository root.
  QVERIFY(!HgRepo::openRepository(file("dir"), kAuthor, kEmail));

  RepoPtr again(HgRepo::openRepository(_root + "/", kAuthor, kEmail));
  QVERIFY2(again, qPrintable(lastErrorMessage()));
  QCOMPARE(HgRepo::repositoryPath(again.get()), _root);
}

void HgRepoTest::stageUnstageAndCommit() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));

  GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(status.newStagedFiles.isEmpty());

  QVERIFY2(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newStagedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(status.newFiles.isEmpty());

  QVERIFY(HgRepo::unstageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(status.newStagedFiles.isEmpty());

  // Absolute paths are accepted too.
  QVERIFY(HgRepo::stageFiles(_repo.get(), {file("a.txt")}));
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Add a"),
                                QStringLiteral("Body"), kAuthor, kEmail, true,
                                true, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
  QVERIFY(!QFile::exists(file(".hg/sourcerer/staged")));

  const QString message = QString::fromUtf8(
      hg({QStringLiteral("log"), QStringLiteral("-r"), QStringLiteral("."),
          QStringLiteral("-T"), QStringLiteral("{desc}")}));
  QVERIFY(message.startsWith(QStringLiteral("Add a\n\nBody")));
  QVERIFY(message.contains(
      QStringLiteral("Signed-off-by: Tester <tester@example.com>")));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{user}")})),
           kUser);
}

void HgRepoTest::commitOnlyStagedFiles() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitAll(QStringLiteral("Add files")).isEmpty());

  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  QVERIFY(writeFile(file("b.txt"), "b2\n"));

  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.modifiedStagedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(status.modifiedFiles, QStringList{QStringLiteral("b.txt")});

  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Change a"),
                                QString(), QString(), QString(), true, false,
                                false),
           qPrintable(lastErrorMessage()));

  status = HgRepo::getStatus(_repo.get(), _quit);
  QVERIFY(status.modifiedStagedFiles.isEmpty());
  QCOMPARE(status.modifiedFiles, QStringList{QStringLiteral("b.txt")});
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{files}")})),
           QStringLiteral("a.txt"));
}

void HgRepoTest::commitWithoutStagedFilesFails() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!HgRepo::commitStaged(_repo.get(), QStringLiteral("Nothing"),
                                QString(), kAuthor, kEmail, true, false,
                                false));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::NothingToCommit);
}

void HgRepoTest::stageDirectoryAndMissingFiles() {
  QVERIFY(writeFile(file("dir/c.txt"), "c\n"));
  QVERIFY(writeFile(file("dir/sub/b.txt"), "b\n"));
  QVERIFY(writeFile(file("keep.txt"), "k\n"));

  GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newFiles, (QStringList{QStringLiteral("dir/c.txt"),
                                         QStringLiteral("dir/sub/b.txt"),
                                         QStringLiteral("keep.txt")}));

  // A directory stages every file below it.
  QVERIFY2(HgRepo::stageFiles(_repo.get(), {QStringLiteral("dir")}),
           qPrintable(lastErrorMessage()));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newStagedFiles,
           (QStringList{QStringLiteral("dir/c.txt"),
                        QStringLiteral("dir/sub/b.txt")}));
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("keep.txt")});

  QVERIFY(HgRepo::unstageFiles(_repo.get(), {QStringLiteral("dir/sub")}));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newStagedFiles, QStringList{QStringLiteral("dir/c.txt")});

  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("dir")}));
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Add dir"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));

  // A file deleted from disk is missing until it is staged.
  QVERIFY(QFile::remove(file("dir/c.txt")));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.deletedFiles, QStringList{QStringLiteral("dir/c.txt")});

  QVERIFY2(HgRepo::stageFiles(_repo.get(), {QStringLiteral("dir/c.txt")}),
           qPrintable(lastErrorMessage()));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.deletedStagedFiles, QStringList{QStringLiteral("dir/c.txt")});
  QVERIFY(status.deletedFiles.isEmpty());

  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Remove c"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QVERIFY(status.deletedStagedFiles.isEmpty());
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("keep.txt")});
}

void HgRepoTest::renames() {
  QVERIFY(writeFile(file("old.txt"), "content\n"));
  QVERIFY(!commitAll(QStringLiteral("Add old")).isEmpty());

  hg({QStringLiteral("mv"), QStringLiteral("old.txt"),
      QStringLiteral("new.txt")});

  GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.renamedFiles, QStringList{QStringLiteral("new.txt")});
  // The removal of the source is part of the rename.
  QVERIFY(status.deletedFiles.isEmpty());

  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("new.txt")}));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.renamedStagedFiles, QStringList{QStringLiteral("new.txt")});

  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Rename"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());

  const GitRepo::GitCommitStatus commit =
      HgRepo::getCommitStatus(_repo.get(), tip());
  QCOMPARE(commit.renamedFiles, QStringList{QStringLiteral("new.txt")});
  QVERIFY(commit.deletedFiles.isEmpty());
  QVERIFY(commit.newFiles.isEmpty());
}

void HgRepoTest::discard() {
  QVERIFY(writeFile(file("a.txt"), "one\n"));
  QVERIFY(!commitAll(QStringLiteral("Add a")).isEmpty());

  QVERIFY(writeFile(file("a.txt"), "changed\n"));
  QVERIFY(writeFile(file("added.txt"), "new\n"));
  QVERIFY(HgRepo::stageFiles(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("added.txt")}));

  QVERIFY2(HgRepo::discardFiles(_repo.get(), {QStringLiteral("a.txt"),
                                              QStringLiteral("added.txt")}),
           qPrintable(lastErrorMessage()));

  QCOMPARE(readFile(file("a.txt")), QByteArray("one\n"));
  // Like GitRepo, a discarded new file stays on disk, untracked.
  QVERIFY(QFileInfo::exists(file("added.txt")));
  const GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("added.txt")});
  QVERIFY(status.modifiedFiles.isEmpty());
  QVERIFY(status.newStagedFiles.isEmpty());
  QVERIFY(status.modifiedStagedFiles.isEmpty());
}

void HgRepoTest::amend() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString first = commitAll(QStringLiteral("First"));
  QVERIFY(!first.isEmpty());

  // Without staged files only the message changes, even with other changes in
  // the working directory.
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  hg({QStringLiteral("add"), QStringLiteral("b.txt")});
  QVERIFY2(HgRepo::amend(_repo.get(), QStringLiteral("First, reworded"),
                         QString(), kAuthor, kEmail, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{desc}|{files}")})),
           QStringLiteral("First, reworded|a.txt"));
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).newFiles,
           QStringList{QStringLiteral("b.txt")});

  // Staged files are added to the amended commit.
  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("b.txt")}));
  QVERIFY2(HgRepo::amend(_repo.get(), QStringLiteral("First, with b"),
                         QString(), kAuthor, kEmail, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{desc}|{files}")})),
           QStringLiteral("First, with b|a.txt b.txt"));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
  QCOMPARE(
      hg({QStringLiteral("log"), QStringLiteral("-T"), QStringLiteral("x")}),
      QByteArray("x"));
}

void HgRepoTest::historyPaging() {
  auto &config = ConfigManager::ConfigManager::getInstance();
  const qlonglong previousPageSize =
      config.getLong(ConfigManager::ConfigKey::GitRepoHistoryPageSize);
  config.setLong(ConfigManager::ConfigKey::GitRepoHistoryPageSize, 3);
  const auto restore = qScopeGuard([&]() {
    config.setLong(ConfigManager::ConfigKey::GitRepoHistoryPageSize,
                   previousPageSize);
  });

  QStringList commits;
  for (int i = 0; i < 7; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i) + "\n"));
    const QString commit = commitAll(QStringLiteral("Commit %1").arg(i));
    QVERIFY2(!commit.isEmpty(), qPrintable(lastErrorMessage()));
    commits.prepend(commit);
  }

  GitRepo::GitHistory page0 =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("default"),
                               GitRepo::BranchType::Local, 0, _quit);
  QVERIFY2(page0.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(page0.total, qint64(7));
  QVERIFY(page0.hasMore);
  QCOMPARE(page0.commits, commits.mid(0, 3));
  QCOMPARE(page0.headCommit, commits.first());

  const GitRepo::GitCommitInfo &newest = page0.commitInfos[commits.first()];
  QCOMPARE(newest.summary, QStringLiteral("Commit 6"));
  QCOMPARE(newest.authorName, kAuthor);
  QCOMPARE(newest.authorEmail, kEmail);
  QCOMPARE(newest.parents, QVector<QString>{commits.at(1)});
  QVERIFY(newest.timeStamp > 0);
  QCOMPARE(
      QByteArray(reinterpret_cast<const char *>(newest.oid.id), 20).toHex(),
      commits.first().toLatin1());
  QVERIFY(page0.commitInfos[commits.last()].parents.isEmpty());

  const GitRepo::GitHistory page2 =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("default"),
                               GitRepo::BranchType::Local, 2, _quit);
  QVERIFY(page2.valid);
  QCOMPARE(page2.commits, commits.mid(6));
  QVERIFY(!page2.hasMore);
  // Ref tips are filled on page 0 only.
  QVERIFY(page2.refTips.isEmpty());

  const GitRepo::GitHistory all =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral(ALL_BRANCHES),
                               GitRepo::BranchType::All, -1, _quit);
  QCOMPARE(all.commits, commits);
  QVERIFY(!all.hasMore);
}

void HgRepoTest::historyRefs() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(!root.isEmpty());
  hg({QStringLiteral("tag"), QStringLiteral("-r"), root,
      QStringLiteral("v1.0")});
  const QString tagCommit = tip();
  hg({QStringLiteral("bookmark"), QStringLiteral("-r"), root,
      QStringLiteral("feature")});

  hg({QStringLiteral("branch"), QStringLiteral("stable")});
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  const QString stable = commitAll(QStringLiteral("On stable"));
  QVERIFY(!stable.isEmpty());

  const GitRepo::GitHistory history =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral(ALL_BRANCHES),
                               GitRepo::BranchType::All, 0, _quit);
  QVERIFY2(history.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(history.total, qint64(3));
  QCOMPARE(history.headCommit, stable);
  QCOMPARE(history.refLabels.value(stable),
           QStringList{QStringLiteral("refs/heads/stable")});
  QCOMPARE(history.refLabels.value(tagCommit),
           QStringList{QStringLiteral("refs/heads/default")});
  QCOMPARE(history.refLabels.value(root),
           (QStringList{QStringLiteral("refs/bookmarks/feature"),
                        QStringLiteral("refs/tags/v1.0")}));
  QCOMPARE(history.refTips.value(root),
           (QStringList{QStringLiteral("@feature"), QStringLiteral("v1.0")}));

  // Named branch, bookmark and tag histories.
  const GitRepo::GitHistory branch =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("default"),
                               GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(branch.commits, (QVector<QString>{tagCommit, root}));

  const GitRepo::GitHistory bookmark =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("@feature"),
                               GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(bookmark.commits, QVector<QString>{root});

  const GitRepo::GitHistory tag = HgRepo::getBranchHistory(
      _repo.get(), QStringLiteral("v1.0"), GitRepo::BranchType::Tag, 0, _quit);
  QCOMPARE(tag.commits, QVector<QString>{root});

  const GitRepo::GitHistory head =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("HEAD"),
                               GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(head.commits, (QVector<QString>{stable, tagCommit, root}));

  const GitRepo::GitHistory unknown =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("no-such-branch"),
                               GitRepo::BranchType::Local, 0, _quit);
  QVERIFY(!unknown.valid);
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::HistoryFailed);
}

void HgRepoTest::commitStatus() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(writeFile(file("gone.txt"), "g\n"));
  const QString first = commitAll(QStringLiteral("First"));
  QVERIFY(!first.isEmpty());

  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(QFile::remove(file("gone.txt")));
  const QString second = commitAll(QStringLiteral("Second\n\nWith a body"));
  QVERIFY2(!second.isEmpty(), qPrintable(lastErrorMessage()));

  const GitRepo::GitCommitStatus status =
      HgRepo::getCommitStatus(_repo.get(), second);
  QCOMPARE(status.commit, second);
  QCOMPARE(status.summary, QStringLiteral("Second"));
  QCOMPARE(status.authorName, kAuthor);
  QCOMPARE(status.authorEmail, kEmail);
  QCOMPARE(status.parentShas, QStringList{first});
  QVERIFY(!status.committerDate.isEmpty());
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("b.txt")});
  QCOMPARE(status.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(status.deletedFiles, QStringList{QStringLiteral("gone.txt")});

  QVERIFY(HgRepo::getCommitStatus(_repo.get(), QStringLiteral("deadbeef"))
              .commit.isEmpty());
}

void HgRepoTest::phases() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString first = commitAll(QStringLiteral("First"));
  QVERIFY(!first.isEmpty());
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString second = commitAll(QStringLiteral("Second"));
  QVERIFY(!second.isEmpty());
  QVERIFY(writeFile(file("a.txt"), "a3\n"));
  const QString third = commitAll(QStringLiteral("Third"));
  QVERIFY(!third.isEmpty());

  bool ok = false;
  hg({QStringLiteral("phase"), QStringLiteral("--public"), first}, &ok);
  QVERIFY(ok);
  hg({QStringLiteral("phase"), QStringLiteral("--secret"),
      QStringLiteral("--force"), third},
     &ok);
  QVERIFY(ok);

  const GitRepo::GitHistory history =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("default"),
                               GitRepo::BranchType::Local, 0, _quit);
  QVERIFY2(history.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(history.commitInfos.value(first).phase,
           GitRepo::CommitPhase::Public);
  QCOMPARE(history.commitInfos.value(second).phase,
           GitRepo::CommitPhase::Draft);
  QCOMPARE(history.commitInfos.value(third).phase,
           GitRepo::CommitPhase::Secret);

  QCOMPARE(HgRepo::getCommitStatus(_repo.get(), first).phase,
           GitRepo::CommitPhase::Public);
  QCOMPARE(HgRepo::getCommitStatus(_repo.get(), third).phase,
           GitRepo::CommitPhase::Secret);
}

void HgRepoTest::diffs() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));
  const QString first = commitAll(QStringLiteral("Add a"));
  QVERIFY(!first.isEmpty());

  QVERIFY(writeFile(file("a.txt"), "one\nTWO\nthree\n"));
  QVERIFY(writeFile(file("new.txt"), "fresh\n"));

  const GitRepo::FilesDiff diff = HgRepo::diffFiles(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("new.txt")});
  QVERIFY2(diff.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(diff.deltas.size(), 2);
  QCOMPARE(diff.deltas[0].newFilePath, QStringLiteral("a.txt"));
  QCOMPARE(diff.deltas[0].hunks.size(), 1);
  QCOMPARE(changedLines(diff.deltas[0]),
           (QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")}));
  QCOMPARE(diff.deltas[1].newFilePath, QStringLiteral("new.txt"));
  QCOMPARE(changedLines(diff.deltas[1]), QStringList{QStringLiteral("+fresh")});
  QCOMPARE(diff.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(diff.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\nTWO\nthree\n"));
  QVERIFY(diff.repoContent.value(QStringLiteral("new.txt")).isEmpty());
  QVERIFY(diff.diff.contains(QStringLiteral("diff --git a/a.txt b/a.txt")));

  const QString second = commitAll(QStringLiteral("Change a"));
  QVERIFY(!second.isEmpty());

  const GitRepo::FilesDiff fromCommit = HgRepo::diffFilesFromCommit(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("new.txt")},
      second);
  QVERIFY2(fromCommit.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(fromCommit.deltas.size(), 2);
  QCOMPARE(changedLines(fromCommit.deltas[0]),
           (QStringList{QStringLiteral("-two"), QStringLiteral("+TWO")}));
  QCOMPARE(changedLines(fromCommit.deltas[1]),
           QStringList{QStringLiteral("+fresh")});
  QCOMPARE(fromCommit.repoContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\ntwo\nthree\n"));
  QCOMPARE(fromCommit.filesContent.value(QStringLiteral("a.txt")),
           QStringLiteral("one\nTWO\nthree\n"));
  QVERIFY(fromCommit.repoContent.value(QStringLiteral("new.txt")).isEmpty());

  // The first commit has no parent.
  const GitRepo::FilesDiff rootDiff = HgRepo::diffFilesFromCommit(
      _repo.get(), {QStringLiteral("a.txt")}, first);
  QVERIFY(rootDiff.valid);
  QCOMPARE(changedLines(rootDiff.deltas.value(0)),
           (QStringList{QStringLiteral("+one"), QStringLiteral("+two"),
                        QStringLiteral("+three")}));
}

void HgRepoTest::blameAttributesLines() {
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\n"));
  const QString first = commitAll(QStringLiteral("Add a"));
  QVERIFY(!first.isEmpty());
  QVERIFY(writeFile(file("a.txt"), "one\ntwo\nthree\n"));
  QVERIFY(writeFile(file("untracked.txt"), "u\n"));

  const GitRepo::BlameReport report = HgRepo::blame(
      _repo.get(), {QStringLiteral("a.txt"), QStringLiteral("untracked.txt"),
                    QStringLiteral("missing.txt")});
  QVERIFY(report.valid);
  QCOMPARE(report.fileInfos.size(), 3);

  const GitRepo::BlameFileInfo &blamed = report.fileInfos[0];
  QVERIFY(blamed.errorMessage.isEmpty());
  QCOMPARE(blamed.lineInfos.size(), 3);
  QCOMPARE(blamed.lineInfos[0].content, QStringLiteral("one"));
  QCOMPARE(blamed.lineInfos[0].lineNumber, qint64(1));
  QCOMPARE(blamed.lineInfos[0].commit, first);
  QCOMPARE(blamed.lineInfos[0].author, kAuthor);
  QCOMPARE(blamed.lineInfos[0].email, kEmail);
  QCOMPARE(blamed.lineInfos[0].summary, QStringLiteral("Add a"));
  QCOMPARE(blamed.lineInfos[2].content, QStringLiteral("three"));
  QCOMPARE(blamed.lineInfos[2].commit, kNullCommit);

  // Every line of an untracked file is uncommitted.
  const GitRepo::BlameFileInfo &untracked = report.fileInfos[1];
  QVERIFY(untracked.errorMessage.isEmpty());
  QCOMPARE(untracked.lineInfos.size(), 1);
  QCOMPARE(untracked.lineInfos[0].commit, kNullCommit);

  QVERIFY(!report.fileInfos[2].errorMessage.isEmpty());
  QVERIFY(report.fileInfos[2].lineInfos.isEmpty());
}

void HgRepoTest::fileSize() {
  QVERIFY(writeFile(file("a.txt"), "12345"));
  const QString first = commitAll(QStringLiteral("Add a"));
  QVERIFY(!first.isEmpty());
  QVERIFY(writeFile(file("a.txt"), "1234567890"));
  const QString second = commitAll(QStringLiteral("Grow a"));

  QCOMPARE(HgRepo::getFileSizeFromCommit(_repo.get(), QStringLiteral("a.txt"),
                                         first),
           qint64(5));
  QCOMPARE(HgRepo::getFileSizeFromCommit(_repo.get(), QStringLiteral("a.txt"),
                                         QString()),
           qint64(10));
  QCOMPARE(HgRepo::getFileSizeFromCommit(_repo.get(),
                                         QStringLiteral("missing.txt"), second),
           qint64(-1));
}

void HgRepoTest::mergeConflicts() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  QVERIFY(!commitAll(QStringLiteral("Base")).isEmpty());

  hg({QStringLiteral("branch"), QStringLiteral("other")});
  QVERIFY(writeFile(file("a.txt"), "other\n"));
  QVERIFY(!commitAll(QStringLiteral("Other")).isEmpty());

  hg({QStringLiteral("update"), QStringLiteral("default")});
  QVERIFY(writeFile(file("a.txt"), "default\n"));
  QVERIFY(!commitAll(QStringLiteral("Default")).isEmpty());

  bool merged = true;
  hg({QStringLiteral("merge"), QStringLiteral("other")}, &merged);
  QVERIFY(!merged);

  QVERIFY(HgRepo::getIsMergeInProgress(_repo.get()));
  QVERIFY(HgRepo::hasConflicts(_repo.get()));
  GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.conflictedFiles, QStringList{QStringLiteral("a.txt")});
  QVERIFY(readFile(file("a.txt")).contains("<<<<<<<"));

  // Staging the edited file resolves it.
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY2(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::hasConflicts(_repo.get()));
  status = HgRepo::getStatus(_repo.get(), _quit);
  QVERIFY(status.conflictedFiles.isEmpty());

  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Merge other"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(HgRepo::getCommitStatus(_repo.get(), tip()).parentShas.size(), 2);
}

void HgRepoTest::cancellationRestartsServer() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));

  _quit = true;
  const GitRepo::GitStatus cancelled = HgRepo::getStatus(_repo.get(), _quit);
  QVERIFY(cancelled.path.isEmpty());
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::Cancelled);

  // The killed command server is started again for the next command.
  _quit = false;
  const GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("a.txt")});
}

void HgRepoTest::signingIsUnsupported() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY(!HgRepo::commitStaged(_repo.get(), QStringLiteral("Signed"),
                                QString(), kAuthor, kEmail, true, false, true));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::UnsupportedOperation);
}

void HgRepoTest::namedBranchesAndBookmarks() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(!root.isEmpty());

  QVERIFY2(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                               QStringLiteral("feature")),
           qPrintable(lastErrorMessage()));
  QVector<HgRepo::HgBranchInfo> branches =
      HgRepo::listLocalBranchesWithKind(_repo.get(), false, _quit);
  QCOMPARE(branches.size(), 2);
  QCOMPARE(branches[0].name, QStringLiteral("default"));
  QVERIFY(!branches[0].active);
  QCOMPARE(branches[1].name, QStringLiteral("feature"));
  QVERIFY(branches[1].isNew);
  QVERIFY(branches[1].active);
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("feature"));

  QVERIFY(writeFile(file("b.txt"), "b\n"));
  const QString onFeature = commitAll(QStringLiteral("On feature"));
  QVERIFY(!onFeature.isEmpty());
  branches = HgRepo::listLocalBranchesWithKind(_repo.get(), false, _quit);
  QVERIFY(!branches[1].isNew);
  QCOMPARE(branches[1].node, onFeature);

  // A bookmark is created without updating to it.
  QVERIFY2(
      HgRepo::createBranchFromCommit(_repo.get(), root, QStringLiteral("mark")),
      qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), onFeature);
  QCOMPARE(HgRepo::listLocalBranches(_repo.get(), _quit),
           (QStringList{QStringLiteral("default"), QStringLiteral("feature"),
                        QStringLiteral("@mark")}));
  branches = HgRepo::listLocalBranchesWithKind(_repo.get(), false, _quit);
  QCOMPARE(branches[2].kind, HgRepo::HgBranchKind::Bookmark);
  QVERIFY(!branches[2].active);

  QVERIFY2(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("@mark"),
                                  GitRepo::BranchType::Local),
           qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), root);
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@mark"));

  // Checking out a named branch deactivates the bookmark.
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("feature"),
                                 GitRepo::BranchType::Local));
  QCOMPARE(tip(), onFeature);
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("feature"));

  QVERIFY(HgRepo::checkoutCommit(_repo.get(), root));
  QCOMPARE(tip(), root);
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("default"));

  QVERIFY(!HgRepo::checkoutBranch(_repo.get(), QStringLiteral("missing"),
                                  GitRepo::BranchType::Local));
}

void HgRepoTest::closedAndRenamedBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(!root.isEmpty());

  QVERIFY(
      HgRepo::createBranchFromCommit(_repo.get(), root, QStringLiteral("old")));
  QVERIFY2(HgRepo::renameBranch(_repo.get(), QStringLiteral("@old"),
                                QStringLiteral("new")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(HgRepo::listLocalBranches(_repo.get(), _quit),
           (QStringList{QStringLiteral("default"), QStringLiteral("@new")}));
  QVERIFY(HgRepo::deleteBranch(_repo.get(), QStringLiteral("@new")));
  QCOMPARE(HgRepo::listLocalBranches(_repo.get(), _quit),
           QStringList{QStringLiteral("default")});

  // A named branch can be renamed until it has commits.
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                              QStringLiteral("fresh")));
  QVERIFY2(HgRepo::renameBranch(_repo.get(), QStringLiteral("fresh"),
                                QStringLiteral("renamed")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("renamed"));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  QVERIFY(!commitAll(QStringLiteral("On renamed")).isEmpty());
  QVERIFY(!HgRepo::renameBranch(_repo.get(), QStringLiteral("renamed"),
                                QStringLiteral("again")));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::BranchFailed);

  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));

  QVERIFY(writeFile(file("a.txt"), "dirty\n"));
  QVERIFY(!HgRepo::closeNamedBranch(_repo.get(), QStringLiteral("renamed"),
                                    kAuthor, kEmail));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::UncommittedChanges);
  QVERIFY(HgRepo::discardFiles(_repo.get(), {QStringLiteral("a.txt")}));

  QVERIFY2(HgRepo::closeNamedBranch(_repo.get(), QStringLiteral("renamed"),
                                    kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  // Back where it was.
  QCOMPARE(tip(), root);
  QCOMPARE(HgRepo::listLocalBranches(_repo.get(), _quit),
           QStringList{QStringLiteral("default")});
  const QVector<HgRepo::HgBranchInfo> all =
      HgRepo::listLocalBranchesWithKind(_repo.get(), true, _quit);
  QCOMPARE(all.size(), 2);
  QCOMPARE(all[1].name, QStringLiteral("renamed"));
  QVERIFY(all[1].closed);
}

void HgRepoTest::commitBranchQueries() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(HgRepo::createBranchFromCommit(_repo.get(), root,
                                         QStringLiteral("mark")));
  QVERIFY(writeFile(file("a.txt"), "b\n"));
  const QString second = commitAll(QStringLiteral("Second"));
  QVERIFY(!second.isEmpty());

  QStringList branches = HgRepo::getCommitBranches(_repo.get(), root, _quit);
  branches.sort();
  QCOMPARE(branches,
           (QStringList{QStringLiteral("@mark"), QStringLiteral("default")}));
  QCOMPARE(HgRepo::getCommitBranches(_repo.get(), second, _quit),
           QStringList{QStringLiteral("default")});

  QCOMPARE(HgRepo::getCommitHeads(_repo.get(), root, _quit),
           QStringList{QStringLiteral("@mark")});
  QCOMPARE(HgRepo::getCommitHeads(_repo.get(), second, _quit),
           QStringList{QStringLiteral("default")});
  QVERIFY(HgRepo::getCommitIsHead(_repo.get(), root, _quit));

  QVERIFY(HgRepo::deleteBranch(_repo.get(), QStringLiteral("@mark")));
  QVERIFY(!HgRepo::getCommitIsHead(_repo.get(), root, _quit));
}

void HgRepoTest::tags() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(!root.isEmpty());

  // A global tag is a commit, a local one is not.
  QVERIFY2(HgRepo::createTagFromCommit(_repo.get(), root, QStringLiteral("v1"),
                                       kAuthor, kEmail,
                                       QStringLiteral("Release 1")),
           qPrintable(lastErrorMessage()));
  const QString tagCommit = tip();
  QVERIFY(tagCommit != root);
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{desc}|{user}")})),
           QStringLiteral("Release 1|") + kUser);

  QVERIFY2(HgRepo::createTagFromCommit(_repo.get(), root,
                                       QStringLiteral("local1"), kAuthor,
                                       kEmail, QString(), true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), tagCommit);

  QStringList tags = HgRepo::listTags(_repo.get());
  tags.sort();
  QCOMPARE(tags, (QStringList{QStringLiteral("local1"), QStringLiteral("v1")}));
  QVERIFY(HgRepo::isLocalTag(_repo.get(), QStringLiteral("local1")));
  QVERIFY(!HgRepo::isLocalTag(_repo.get(), QStringLiteral("v1")));
  tags = HgRepo::getCommitTags(_repo.get(), root, _quit);
  tags.sort();
  QCOMPARE(tags, (QStringList{QStringLiteral("local1"), QStringLiteral("v1")}));

  QVERIFY(!HgRepo::createTagFromCommit(_repo.get(), root, QStringLiteral("v1"),
                                       kAuthor, kEmail, QString()));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::TagFailed);

  QVERIFY2(HgRepo::renameTag(_repo.get(), QStringLiteral("v1"),
                             QStringLiteral("v2")),
           qPrintable(lastErrorMessage()));
  QVERIFY2(HgRepo::renameTag(_repo.get(), QStringLiteral("local1"),
                             QStringLiteral("local2")),
           qPrintable(lastErrorMessage()));
  tags = HgRepo::getCommitTags(_repo.get(), root, _quit);
  tags.sort();
  QCOMPARE(tags, (QStringList{QStringLiteral("local2"), QStringLiteral("v2")}));
  QVERIFY(HgRepo::isLocalTag(_repo.get(), QStringLiteral("local2")));

  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("v2"),
                                 GitRepo::BranchType::Tag));
  QCOMPARE(tip(), root);
  const QString activeTag =
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Tag, _quit);
  QVERIFY(activeTag == QStringLiteral("v2") ||
          activeTag == QStringLiteral("local2"));

  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY2(HgRepo::deleteTag(_repo.get(), QStringLiteral("v2")),
           qPrintable(lastErrorMessage()));
  QVERIFY2(HgRepo::deleteTag(_repo.get(), QStringLiteral("local2")),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::listTags(_repo.get()).isEmpty());
}

void HgRepoTest::mergeBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(HgRepo::createBranchFromCommit(_repo.get(), root,
                                         QStringLiteral("target")));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  const QString second = commitAll(QStringLiteral("Second"));
  QVERIFY(HgRepo::createBranchFromCommit(_repo.get(), second,
                                         QStringLiteral("source")));

  // A bookmark is fast-forwarded.
  QVERIFY2(HgRepo::merge(_repo.get(), QStringLiteral("@source"),
                         QStringLiteral("@target"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(_root, QStringLiteral("target")), second);
  QCOMPARE(tip(), second);
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@target"));

  // Named branches get a merge commit.
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), second,
                                              QStringLiteral("feature")));
  QVERIFY(writeFile(file("c.txt"), "c\n"));
  QVERIFY(!commitAll(QStringLiteral("Feature")).isEmpty());
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("d.txt"), "d\n"));
  QVERIFY(!commitAll(QStringLiteral("Main")).isEmpty());

  QVERIFY2(HgRepo::merge(_repo.get(), QStringLiteral("feature"),
                         QStringLiteral("default"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  const QString merged = tip();
  const GitRepo::GitCommitStatus status =
      HgRepo::getCommitStatus(_repo.get(), merged);
  QCOMPARE(status.parentShas.size(), 2);
  QCOMPARE(status.summary, QStringLiteral("Merge branch 'feature' into "
                                          "'default'"));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
  QVERIFY(QFileInfo::exists(file("c.txt")));

  // Merging again changes nothing.
  QVERIFY(HgRepo::merge(_repo.get(), QStringLiteral("feature"),
                        QStringLiteral("default"), kAuthor, kEmail));
  QCOMPARE(tip(), merged);
}

void HgRepoTest::mergeConflictAbort() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  const QString root = commitAll(QStringLiteral("Base"));
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                              QStringLiteral("other")));
  QVERIFY(writeFile(file("a.txt"), "other\n"));
  QVERIFY(!commitAll(QStringLiteral("Other")).isEmpty());
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("a.txt"), "default\n"));
  const QString main = commitAll(QStringLiteral("Default"));

  QVERIFY(!HgRepo::merge(_repo.get(), QStringLiteral("other"),
                         QStringLiteral("default"), kAuthor, kEmail));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::MergeConflicts);
  QVERIFY(HgRepo::getIsMergeInProgress(_repo.get()));
  QVERIFY(HgRepo::listConflictsForAFile(_repo.get(), QStringLiteral("a.txt"))
              .contains(QStringLiteral("<<<<<<<")));

  QVERIFY2(HgRepo::abortMerge(_repo.get()), qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("default\n"));
  QCOMPARE(tip(), main);
  QVERIFY(HgRepo::listConflictsForAFile(_repo.get(), QStringLiteral("a.txt"))
              .isEmpty());
}

void HgRepoTest::rebaseAndSquash() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                              QStringLiteral("feature")));
  QVERIFY(writeFile(file("f1.txt"), "1\n"));
  QVERIFY(!commitAll(QStringLiteral("F1")).isEmpty());
  QVERIFY(writeFile(file("f2.txt"), "2\n"));
  QVERIFY(!commitAll(QStringLiteral("F2")).isEmpty());
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("d.txt"), "d\n"));
  const QString main = commitAll(QStringLiteral("D1"));

  QVERIFY2(HgRepo::rebaseBranches(_repo.get(), QStringLiteral("feature"),
                                  QStringLiteral("default"), kAuthor, kEmail,
                                  false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("feature"));
  const GitRepo::GitHistory history =
      HgRepo::getBranchHistory(_repo.get(), QStringLiteral("feature"),
                               GitRepo::BranchType::Local, 0, _quit);
  QCOMPARE(history.commits.size(), 4);
  QCOMPARE(history.commits[2], main);
  QCOMPARE(history.commitInfos[history.commits[0]].summary,
           QStringLiteral("F2"));

  QVERIFY2(HgRepo::squashBranches(_repo.get(), QStringLiteral("feature"),
                                  QStringLiteral("default"), kAuthor, kEmail,
                                  false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("default"));
  const GitRepo::GitCommitStatus squashed =
      HgRepo::getCommitStatus(_repo.get(), tip());
  QCOMPARE(squashed.parentShas, QStringList{main});
  QCOMPARE(squashed.summary,
           QStringLiteral("Squash merge branch 'feature' into 'default'"));
  QCOMPARE(squashed.newFiles,
           (QStringList{QStringLiteral("f1.txt"), QStringLiteral("f2.txt")}));
  const QString description = QString::fromUtf8(
      hg({QStringLiteral("log"), QStringLiteral("-r"), QStringLiteral("."),
          QStringLiteral("-T"), QStringLiteral("{desc}\n{branch}\n{user}")}));
  QVERIFY(description.contains(QStringLiteral("* F2\n* F1")));
  QVERIFY(description.endsWith(QStringLiteral("default\n") + kUser));
  // The source branch is kept.
  QVERIFY(HgRepo::listLocalBranches(_repo.get(), _quit)
              .contains(QStringLiteral("feature")));
}

void HgRepoTest::rebaseConflictContinueAndAbort() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  const QString root = commitAll(QStringLiteral("Base"));
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                              QStringLiteral("topic")));
  QVERIFY(writeFile(file("a.txt"), "topic\n"));
  QVERIFY(!commitAll(QStringLiteral("Topic")).isEmpty());
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  QVERIFY(!commitAll(QStringLiteral("Main")).isEmpty());

  QVERIFY(!HgRepo::rebaseBranches(_repo.get(), QStringLiteral("topic"),
                                  QStringLiteral("default"), kAuthor, kEmail,
                                  false));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::MergeConflicts);
  QVERIFY(HgRepo::getIsRebaseInProgress(_repo.get()));
  QVERIFY2(HgRepo::abortRebase(_repo.get()), qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsRebaseInProgress(_repo.get()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("main\n"));

  QVERIFY(!HgRepo::rebaseBranches(_repo.get(), QStringLiteral("topic"),
                                  QStringLiteral("default"), kAuthor, kEmail,
                                  false));
  QVERIFY(!HgRepo::commitRebase(_repo.get(), QString(), QString(), kAuthor,
                                kEmail, true, false, false));
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(HgRepo::commitRebase(_repo.get(), QString(), QString(), kAuthor,
                                kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsRebaseInProgress(_repo.get()));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("cat"), QStringLiteral("-r"),
                                 QStringLiteral("max(branch(topic))"),
                                 QStringLiteral("a.txt")})),
           QStringLiteral("resolved\n"));
}

void HgRepoTest::graftAndBackout() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                              QStringLiteral("other")));
  QVERIFY(writeFile(file("b.txt"), "b\n"));
  const QString other = commitAll(QStringLiteral("Other change"));
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("c.txt"), "c\n"));
  QVERIFY(!commitAll(QStringLiteral("Main")).isEmpty());

  QVERIFY2(HgRepo::cherryPickCommit(_repo.get(), other,
                                    QStringLiteral("Grafter"),
                                    QStringLiteral("grafter@example.com")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{desc}|{branch}|{user}")})),
           QStringLiteral("Other change|default|Grafter "
                          "<grafter@example.com>"));
  QVERIFY(QFileInfo::exists(file("b.txt")));

  QVERIFY2(HgRepo::revertCommit(_repo.get(), tip(), kAuthor, kEmail,
                                QStringLiteral("Back out the graft")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{desc}")})),
           QStringLiteral("Back out the graft"));
  QVERIFY(!QFileInfo::exists(file("b.txt")));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
}

void HgRepoTest::graftConflictIsFinishedByCommit() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  const QString root = commitAll(QStringLiteral("Base"));
  QVERIFY(HgRepo::createNamedBranchFromCommit(_repo.get(), root,
                                              QStringLiteral("other")));
  QVERIFY(writeFile(file("a.txt"), "other\n"));
  const QString other = commitAll(QStringLiteral("Other"));
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("a.txt"), "main\n"));
  const QString main = commitAll(QStringLiteral("Main"));

  // Like GitRepo, a conflicting cherry-pick succeeds without a commit.
  QVERIFY2(HgRepo::cherryPickCommit(_repo.get(), other, kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), main);
  QVERIFY(HgRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).conflictedFiles,
           QStringList{QStringLiteral("a.txt")});

  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}));
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Ignored"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                                 QStringLiteral("."), QStringLiteral("-T"),
                                 QStringLiteral("{desc}|{p1node}")})),
           QStringLiteral("Other|") + main);
}

void HgRepoTest::stripAndReset() {
  QStringList commits;
  for (int i = 0; i < 3; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i) + "\n"));
    commits << commitAll(QStringLiteral("Commit %1").arg(i));
    QVERIFY(!commits.last().isEmpty());
  }

  QVERIFY2(HgRepo::strip(_repo.get(), commits[2], false, true),
           qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), commits[1]);
  QVERIFY(!QDir(file(".hg/strip-backup"))
               .entryList({QStringLiteral("*.hg")}, QDir::Files)
               .isEmpty());
  QCOMPARE(
      hg({QStringLiteral("log"), QStringLiteral("-T"), QStringLiteral("x")}),
      QByteArray("xx"));

  // Soft keeps the changes of the stripped commits.
  QVERIFY2(
      HgRepo::resetToCommit(_repo.get(), commits[0], GitRepo::ResetType::Soft),
      qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), commits[0]);
  QCOMPARE(readFile(file("a.txt")), QByteArray("1\n"));
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).modifiedFiles,
           QStringList{QStringLiteral("a.txt")});

  const QString again = commitAll(QStringLiteral("Again"));
  QVERIFY(!again.isEmpty());
  QVERIFY(writeFile(file("a.txt"), "dirty\n"));
  QVERIFY2(
      HgRepo::resetToCommit(_repo.get(), commits[0], GitRepo::ResetType::Hard),
      qPrintable(lastErrorMessage()));
  QCOMPARE(tip(), commits[0]);
  QCOMPARE(readFile(file("a.txt")), QByteArray("0\n"));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
}

void HgRepoTest::shelves() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());

  QVERIFY(writeFile(file("a.txt"), "first\n"));
  QVERIFY(writeFile(file("n.txt"), "new\n"));
  QVERIFY2(HgRepo::stash(_repo.get(), QStringLiteral("First note")),
           qPrintable(lastErrorMessage()));
  // Unknown files are shelved too.
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
  QVERIFY(!QFileInfo::exists(file("n.txt")));

  QVERIFY(writeFile(file("a.txt"), "second\n"));
  QVERIFY2(HgRepo::stash(_repo.get(), QString()),
           qPrintable(lastErrorMessage()));

  const QStringList stashes = HgRepo::listStashes(_repo.get(), _quit);
  QCOMPARE(stashes.size(), 2);
  QCOMPARE(stashes[1], QStringLiteral("First note"));
  QVERIFY(stashes[0].startsWith(QStringLiteral("changes to")));
  QCOMPARE(HgRepo::listStashNames(_repo.get()).size(), 2);

  const GitRepo::GitCommitStatus status =
      HgRepo::getStashStatus(_repo.get(), 1);
  QCOMPARE(status.summary, QStringLiteral("First note"));
  QCOMPARE(status.modifiedFiles, QStringList{QStringLiteral("a.txt")});
  QCOMPARE(status.newFiles, QStringList{QStringLiteral("n.txt")});

  // Applying keeps the shelve.
  QVERIFY2(HgRepo::applyStash(_repo.get(), 1), qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("first\n"));
  QVERIFY(QFileInfo::exists(file("n.txt")));
  QCOMPARE(HgRepo::listStashes(_repo.get(), _quit).size(), 2);

  QVERIFY(HgRepo::deleteStash(_repo.get(), 0));
  QCOMPARE(HgRepo::listStashes(_repo.get(), _quit),
           QStringList{QStringLiteral("First note")});
  QVERIFY(HgRepo::deleteAllStashes(_repo.get()));
  QVERIFY(HgRepo::listStashes(_repo.get(), _quit).isEmpty());
  QVERIFY(!HgRepo::applyStash(_repo.get(), 0));

  hg({QStringLiteral("update"), QStringLiteral("--clean"),
      QStringLiteral(".")});
  QVERIFY(QFile::remove(file("n.txt")));
  QVERIFY(!HgRepo::stash(_repo.get(), QString()));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::ShelveFailed);
}

void HgRepoTest::shelveConflicts() {
  QVERIFY(writeFile(file("a.txt"), "base\n"));
  QVERIFY(!commitAll(QStringLiteral("Base")).isEmpty());
  QVERIFY(writeFile(file("a.txt"), "shelved\n"));
  QVERIFY(HgRepo::stash(_repo.get(), QStringLiteral("Shelved")));
  QVERIFY(writeFile(file("a.txt"), "other\n"));
  QVERIFY(!commitAll(QStringLiteral("Other")).isEmpty());

  QVERIFY(!HgRepo::applyStash(_repo.get(), 0));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::MergeConflicts);
  QVERIFY(HgRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).conflictedFiles,
           QStringList{QStringLiteral("a.txt")});

  // Staging the resolved file finishes the unshelve.
  QVERIFY(writeFile(file("a.txt"), "resolved\n"));
  QVERIFY2(HgRepo::stageFiles(_repo.get(), {QStringLiteral("a.txt")}),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsMergeInProgress(_repo.get()));
  QCOMPARE(readFile(file("a.txt")), QByteArray("resolved\n"));
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).modifiedStagedFiles,
           QStringList{QStringLiteral("a.txt")});
}

void HgRepoTest::bisectFindsFirstBadCommit() {
  QStringList commits;
  for (int i = 0; i < 6; ++i) {
    QVERIFY(writeFile(file("a.txt"), QByteArray::number(i) + "\n"));
    commits << commitAll(QStringLiteral("Commit %1").arg(i));
    QVERIFY(!commits.last().isEmpty());
  }

  GitRepo::BisectStatus status;
  QVERIFY2(HgRepo::bisect(_repo.get(), GitRepo::BisectAction::Good, commits[0],
                          status),
           qPrintable(lastErrorMessage()));
  QVERIFY(status.active);
  QVERIFY(status.waitingForBad);
  QVERIFY(!status.waitingForGood);

  QVERIFY2(HgRepo::bisect(_repo.get(), GitRepo::BisectAction::Bad, commits[5],
                          status),
           qPrintable(lastErrorMessage()));
  QVERIFY(!status.finished);
  QVERIFY(status.remaining > 0);
  QVERIFY(!status.currentCommit.isEmpty());
  QCOMPARE(tip(), status.currentCommit);

  const QHash<QString, int> marks = HgRepo::getBisectMarks(_repo.get());
  QCOMPARE(marks.value(commits[0]),
           static_cast<int>(GitRepo::BisectMark::Good));
  QCOMPARE(marks.value(commits[5]), static_cast<int>(GitRepo::BisectMark::Bad));
  QCOMPARE(marks.value(status.currentCommit),
           static_cast<int>(GitRepo::BisectMark::Current));
  QVERIFY(!HgRepo::getBranchHistory(_repo.get(), QStringLiteral(ALL_BRANCHES),
                                    GitRepo::BranchType::All, 0, _quit)
               .bisectMarks.isEmpty());

  // Commit 3 introduced the "bug".
  for (int step = 0; step < 6 && !status.finished; ++step) {
    const int index = commits.indexOf(status.currentCommit);
    QVERIFY(index >= 0);
    QVERIFY2(HgRepo::bisect(_repo.get(),
                            index >= 3 ? GitRepo::BisectAction::Bad
                                       : GitRepo::BisectAction::Good,
                            QString(), status),
             qPrintable(lastErrorMessage()));
  }
  QVERIFY(status.finished);
  QVERIFY(!status.ambiguous);
  QCOMPARE(status.firstBadCommit, commits[3]);
  QCOMPARE(status.firstBadSummary, QStringLiteral("Commit 3"));

  QVERIFY2(HgRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                          status),
           qPrintable(lastErrorMessage()));
  QVERIFY(!status.active);
  QCOMPARE(tip(), commits[5]);
  QVERIFY(HgRepo::getBisectMarks(_repo.get()).isEmpty());
  QVERIFY(!HgRepo::bisect(_repo.get(), GitRepo::BisectAction::Reset, QString(),
                          status));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::BisectNotInProgress);
}

void HgRepoTest::remotesAndTargets() {
  QVERIFY(HgRepo::listRemotes(_repo.get()).isEmpty());
  QCOMPARE(HgRepo::getDefaultRemote(_repo.get()), QStringLiteral("default"));

  QVERIFY2(HgRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.com/a"),
                             QStringLiteral("https://example.com/a-push")),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::addRemote(_repo.get(), QStringLiteral("backup"),
                            QStringLiteral("https://example.com/b"),
                            QString()));
  QVERIFY(!HgRepo::addRemote(_repo.get(), QStringLiteral("a/b"),
                             QStringLiteral("https://example.com/c"),
                             QString()));
  QVERIFY(!HgRepo::addRemote(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("https://example.com/c"),
                             QString()));

  QVector<GitRepo::RemoteInfo> remotes = HgRepo::listRemotes(_repo.get());
  QCOMPARE(remotes.size(), 2);
  QCOMPARE(remotes[0].name, QStringLiteral("backup"));
  QCOMPARE(remotes[0].pushUrl, QStringLiteral("https://example.com/b"));
  QCOMPARE(remotes[1].name, QStringLiteral("origin"));
  QCOMPARE(remotes[1].pushUrl, QStringLiteral("https://example.com/a-push"));
  QCOMPARE(remotes[1].targets.size(), 1);
  QCOMPARE(remotes[1].targets[0].name, QStringLiteral("Default"));

  // Without "default", the first remote is the default one until another is
  // chosen.
  QCOMPARE(HgRepo::getDefaultRemote(_repo.get()), QStringLiteral("backup"));
  QVERIFY(HgRepo::setDefaultRemote(_repo.get(), QStringLiteral("origin")));
  QCOMPARE(HgRepo::getDefaultRemote(_repo.get()), QStringLiteral("origin"));
  QVERIFY(HgRepo::listRemotes(_repo.get())[1].isDefault);
  QVERIFY(!HgRepo::setDefaultRemote(_repo.get(), QStringLiteral("missing")));

  QVERIFY2(HgRepo::updateRemote(_repo.get(), QStringLiteral("backup"),
                                QStringLiteral("mirror"),
                                QStringLiteral("https://example.com/m"),
                                QStringLiteral("https://example.com/m-push")),
           qPrintable(lastErrorMessage()));
  remotes = HgRepo::listRemotes(_repo.get());
  QCOMPARE(remotes[0].name, QStringLiteral("mirror"));
  QCOMPARE(remotes[0].pushUrl, QStringLiteral("https://example.com/m-push"));

  QVERIFY2(HgRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("Staging"),
                             QStringLiteral("https://staging/f"),
                             QStringLiteral("https://staging/p")),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::addTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("Default"),
                             QStringLiteral("https://x"), QString()));
  QVERIFY(HgRepo::editTarget(_repo.get(), QStringLiteral("origin"),
                             QStringLiteral("Staging"),
                             QStringLiteral("https://staging/f2"), QString()));
  remotes = HgRepo::listRemotes(_repo.get());
  QCOMPARE(remotes[1].targets.size(), 2);
  QCOMPARE(remotes[1].targets[1].name, QStringLiteral("Staging"));
  QCOMPARE(remotes[1].targets[1].fetchUrl,
           QStringLiteral("https://staging/f2"));
  QCOMPARE(remotes[1].targets[1].pushUrl, QStringLiteral("https://staging/f2"));

  QVERIFY2(HgRepo::setDefaultTarget(_repo.get(), QStringLiteral("origin"),
                                    QStringLiteral("Staging")),
           qPrintable(lastErrorMessage()));
  remotes = HgRepo::listRemotes(_repo.get());
  QCOMPARE(remotes[1].fetchUrl, QStringLiteral("https://staging/f2"));
  QCOMPARE(remotes[1].pushUrl, QStringLiteral("https://staging/f2"));

  QVERIFY(HgRepo::deleteTarget(_repo.get(), QStringLiteral("origin"),
                               QStringLiteral("Staging")));
  QCOMPARE(HgRepo::listRemotes(_repo.get())[1].targets.size(), 1);

  QVERIFY(HgRepo::removeRemote(_repo.get(), QStringLiteral("mirror")));
  QCOMPARE(HgRepo::listRemotes(_repo.get()).size(), 1);
  QVERIFY(!HgRepo::removeRemote(_repo.get(), QStringLiteral("mirror")));

  QVERIFY(HgRepo::setUpstream(_repo.get(), QStringLiteral("origin"),
                              QStringLiteral("main"),
                              QStringLiteral("default")));
  const auto [configured, remote, remoteBranch] =
      HgRepo::getUpstream(_repo.get(), QStringLiteral("default"));
  QVERIFY(configured);
  QCOMPARE(remote, QStringLiteral("origin"));
  QCOMPARE(remoteBranch, QStringLiteral("main"));
  QVERIFY(
      !std::get<0>(HgRepo::getUpstream(_repo.get(), QStringLiteral("@other"))));
}

void HgRepoTest::hgrcEditsKeepComments() {
  QFile hgrc(file(".hg/hgrc"));
  QVERIFY(hgrc.open(QIODevice::Append));
  hgrc.write("\n# keep me\n[paths]\n# path comment\nexisting = "
             "https://example.com/e\n\n[extensions]\n# extension comment\n");
  hgrc.close();

  QVERIFY2(HgRepo::addRemote(_repo.get(), QStringLiteral("added"),
                             QStringLiteral("https://example.com/n"),
                             QString()),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::removeRemote(_repo.get(), QStringLiteral("existing")));

  const QByteArray content = readFile(file(".hg/hgrc"));
  QVERIFY(content.contains("username = " + kUser.toUtf8()));
  QVERIFY(content.contains("# keep me"));
  QVERIFY(content.contains("# path comment\nadded = https://example.com/n"));
  QVERIFY(content.contains("[extensions]\n# extension comment"));
  QVERIFY(!content.contains("existing ="));
  QCOMPARE(HgRepo::listRemotes(_repo.get()).size(), 1);
}

void HgRepoTest::pullPushAndRemoteStatus() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());

  QString clonePath;
  RepoPtr clone = cloneRoot(&clonePath);
  QVERIFY2(clone, qPrintable(lastErrorMessage()));

  const auto statusOf = [&](const QString &name) {
    for (const GitRepo::BranchInfo &info :
         HgRepo::listRemoteBranchesWithStatus(clone.get(), _quit)) {
      if (info.name == name) {
        return info.statusInfo;
      }
    }
    return GitRepo::BranchStatus();
  };

  QVERIFY2(HgRepo::fetchAll(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(HgRepo::listRemoteBranches(clone.get(), _quit),
           QStringList{QStringLiteral("default/default")});
  QCOMPARE(statusOf(QStringLiteral("default/default")).status,
           GitRepo::BranchStatusType::UpToDate);

  // Behind after a fetch of new remote commits.
  QVERIFY(writeFile(file("a.txt"), "server\n"));
  const QString server = commitAll(QStringLiteral("Server"));
  QVERIFY(HgRepo::fetchRemote(clone.get(), QStringLiteral("default")));
  GitRepo::BranchStatus status = statusOf(QStringLiteral("default/default"));
  QCOMPARE(status.status, GitRepo::BranchStatusType::Behind);
  QCOMPARE(status.behindCount, size_t(1));
  QVERIFY(nodeOf(clonePath, QStringLiteral(".")) != server);

  QVERIFY2(HgRepo::pull(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(clonePath, QStringLiteral(".")), server);
  QCOMPARE(statusOf(QStringLiteral("default/default")).status,
           GitRepo::BranchStatusType::UpToDate);

  // Ahead with a local commit, up to date once pushed.
  QVERIFY(writeFile(clonePath + QStringLiteral("/b.txt"), "b\n"));
  const QString local = commitIn(clone.get(), QStringLiteral("Local"));
  QVERIFY(!local.isEmpty());
  status = statusOf(QStringLiteral("default/default"));
  QCOMPARE(status.status, GitRepo::BranchStatusType::Ahead);
  QCOMPARE(status.aheadCount, size_t(1));

  QVERIFY2(HgRepo::push(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(_root, QStringLiteral("tip")), local);
  QCOMPARE(statusOf(QStringLiteral("default/default")).status,
           GitRepo::BranchStatusType::UpToDate);

  // Nothing to push is fine.
  QVERIFY(HgRepo::push(clone.get()));

  // A new named branch is created on the remote.
  QVERIFY(HgRepo::createNamedBranchFromCommit(clone.get(), QString(),
                                              QStringLiteral("feature")));
  QVERIFY(writeFile(clonePath + QStringLiteral("/c.txt"), "c\n"));
  QVERIFY(!commitIn(clone.get(), QStringLiteral("Feature")).isEmpty());
  QVERIFY2(HgRepo::push(clone.get(), QStringLiteral("feature")),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::listRemoteBranches(clone.get(), _quit)
              .contains(QStringLiteral("default/feature")));

  // Diverged.
  QVERIFY(HgRepo::checkoutBranch(clone.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  hg({QStringLiteral("update"), QStringLiteral("--clean"),
      QStringLiteral("default")});
  QVERIFY(writeFile(file("d.txt"), "d\n"));
  QVERIFY(!commitAll(QStringLiteral("Server 2")).isEmpty());
  QVERIFY(writeFile(clonePath + QStringLiteral("/e.txt"), "e\n"));
  QVERIFY(!commitIn(clone.get(), QStringLiteral("Local 2")).isEmpty());
  QVERIFY(HgRepo::fetchAll(clone.get()));
  status = statusOf(QStringLiteral("default/default"));
  QCOMPARE(status.status, GitRepo::BranchStatusType::Diverged);
  QCOMPARE(status.aheadCount, size_t(1));
  QCOMPARE(status.behindCount, size_t(1));
}

void HgRepoTest::remoteBookmarks() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(HgRepo::createBranchFromCommit(_repo.get(), root,
                                         QStringLiteral("feat")));

  QString clonePath;
  RepoPtr clone = cloneRoot(&clonePath);
  QVERIFY2(clone, qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::fetchAll(clone.get()));
  QVERIFY(HgRepo::listRemoteBranches(clone.get(), _quit)
              .contains(QStringLiteral("default/@feat")));

  const GitRepo::GitHistory history =
      HgRepo::getBranchHistory(clone.get(), QStringLiteral("default/@feat"),
                               GitRepo::BranchType::Remote, 0, _quit);
  QVERIFY2(history.valid, qPrintable(lastErrorMessage()));
  QCOMPARE(history.commits, QVector<QString>{root});

  // Checking out the remote bookmark creates the local one with its upstream.
  QVERIFY(HgRepo::deleteBranch(clone.get(), QStringLiteral("@feat")));
  QVERIFY2(HgRepo::checkoutBranch(clone.get(), QStringLiteral("default/@feat"),
                                  GitRepo::BranchType::Remote),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(clone.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@feat"));
  const auto [configured, remote, remoteBranch] =
      HgRepo::getUpstream(clone.get(), QStringLiteral("@feat"));
  QVERIFY(configured);
  QCOMPARE(remote, QStringLiteral("default"));
  QCOMPARE(remoteBranch, QStringLiteral("@feat"));

  // The remote bookmark moves: the active local one is behind until pulled.
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("@feat"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("a.txt"), "moved\n"));
  const QString moved = commitAll(QStringLiteral("Moved"));
  QVERIFY(HgRepo::fetchAll(clone.get()));
  bool found = false;
  for (const GitRepo::BranchInfo &info :
       HgRepo::listRemoteBranchesWithStatus(clone.get(), _quit)) {
    if (info.name == QStringLiteral("default/@feat")) {
      found = true;
      QCOMPARE(info.statusInfo.status, GitRepo::BranchStatusType::Behind);
    }
  }
  QVERIFY(found);

  QVERIFY2(HgRepo::pull(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(clonePath, QStringLiteral(".")), moved);
  QCOMPARE(
      HgRepo::getActiveBranch(clone.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@feat"));

  // Pushing the active bookmark moves it on the remote.
  QVERIFY(writeFile(clonePath + QStringLiteral("/b.txt"), "b\n"));
  const QString local = commitIn(clone.get(), QStringLiteral("Local"));
  QVERIFY2(HgRepo::push(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(_root, QStringLiteral("feat")), local);

  // Deleting it on the remote keeps the local bookmark.
  QVERIFY2(
      HgRepo::deleteRemoteBranch(clone.get(), QStringLiteral("default/@feat")),
      qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::listLocalBranches(_repo.get(), _quit)
               .contains(QStringLiteral("@feat")));
  QVERIFY(HgRepo::listLocalBranches(clone.get(), _quit)
              .contains(QStringLiteral("@feat")));
}

void HgRepoTest::pullMergesDivergedHistory() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());
  QString clonePath;
  RepoPtr clone = cloneRoot(&clonePath);
  QVERIFY(clone);

  QVERIFY(writeFile(file("server.txt"), "s\n"));
  const QString server = commitAll(QStringLiteral("Server"));
  QVERIFY(writeFile(clonePath + QStringLiteral("/local.txt"), "l\n"));
  QVERIFY(!commitIn(clone.get(), QStringLiteral("Local")).isEmpty());

  // A merge is left for the user to commit.
  QVERIFY2(HgRepo::pull(clone.get()), qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::getIsMergeInProgress(clone.get()));
  QVERIFY2(HgRepo::commitStaged(clone.get(), QStringLiteral("Merge"), QString(),
                                kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(HgRepo::getCommitStatus(clone.get(),
                                   nodeOf(clonePath, QStringLiteral(".")))
               .parentShas.size(),
           2);

  // With rebase the local commit moves onto the remote one.
  QVERIFY(HgRepo::push(clone.get()));
  hg({QStringLiteral("update"), QStringLiteral("--clean"),
      QStringLiteral("tip")});
  QVERIFY(writeFile(file("server2.txt"), "s\n"));
  const QString server2 = commitAll(QStringLiteral("Server 2"));
  QVERIFY(writeFile(clonePath + QStringLiteral("/local2.txt"), "l\n"));
  QVERIFY(!commitIn(clone.get(), QStringLiteral("Local 2")).isEmpty());
  QVERIFY2(HgRepo::pull(clone.get(), QString(), true),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getIsMergeInProgress(clone.get()));
  QCOMPARE(QString::fromUtf8(
               hgIn(clonePath, {QStringLiteral("log"), QStringLiteral("-r"),
                                QStringLiteral("."), QStringLiteral("-T"),
                                QStringLiteral("{desc}|{p1node}")})),
           QStringLiteral("Local 2|") + server2);
  Q_UNUSED(server);
}

void HgRepoTest::httpAuthentication() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("First")).isEmpty());

  HgAuthServer server(_root, kGoodCredentials.username,
                      kGoodCredentials.password);
  QVERIFY(server.start());

  ScriptedCredentials provider;
  HgRepo::setCredentialsProvider(&provider);
  const auto reset =
      qScopeGuard([]() { HgRepo::setCredentialsProvider(nullptr); });

  // A rejected answer is asked again.
  provider.answers = {kBadCredentials, kGoodCredentials};
  const QString clonePath = QDir::cleanPath(_tmp->path()) + "/clone";
  QVERIFY2(
      HgRepo::cloneRepository(server.url(), clonePath, QStringLiteral("uuid")),
      qPrintable(lastErrorMessage()));
  QVERIFY(QFileInfo::exists(clonePath + "/a.txt"));

  QCOMPARE(provider.requests.size(), 2);
  QCOMPARE(provider.requests[0].url, server.url());
  QCOMPARE(provider.requests[0].repoPath, clonePath);
  QCOMPARE(provider.requests[0].attempt, 0);
  QCOMPARE(provider.requests[1].attempt, 1);
  QCOMPARE(provider.results,
           (QVector<HgRepo::HgAuthResult>{HgRepo::HgAuthResult::Rejected,
                                          HgRepo::HgAuthResult::Accepted}));

  // The clone remembers nothing, its first pull asks again.
  RepoPtr clone(HgRepo::openRepository(clonePath, kAuthor, kEmail));
  QVERIFY2(clone, qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(file("a.txt"), "a2\n"));
  const QString second = commitAll(QStringLiteral("Second"));
  QVERIFY(!second.isEmpty());

  provider.requests.clear();
  provider.results.clear();
  provider.answers = {kGoodCredentials};
  QVERIFY2(HgRepo::pull(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(clonePath, QStringLiteral(".")), second);
  QCOMPARE(provider.requests.size(), 1);
  QCOMPARE(provider.requests[0].repoPath, clonePath);
  QCOMPARE(provider.results,
           QVector<HgRepo::HgAuthResult>{HgRepo::HgAuthResult::Accepted});

  // Accepted credentials are sent with the following commands.
  QVERIFY2(HgRepo::fetchAll(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(provider.requests.size(), 1);

  // And pushes too.
  QVERIFY(writeFile(clonePath + "/b.txt", "b\n"));
  const QString third = commitIn(clone.get(), QStringLiteral("Third"));
  QVERIFY(!third.isEmpty());
  QVERIFY2(HgRepo::push(clone.get()), qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(_root, QStringLiteral("tip")), third);
  QCOMPARE(provider.requests.size(), 1);
}

void HgRepoTest::httpAuthenticationCancelled() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("First")).isEmpty());

  HgAuthServer server(_root, kGoodCredentials.username,
                      kGoodCredentials.password);
  QVERIFY(server.start());

  // Without a provider the command fails at once.
  const QString clonePath = QDir::cleanPath(_tmp->path()) + "/clone";
  QVERIFY(!HgRepo::cloneRepository(server.url(), clonePath,
                                   QStringLiteral("uuid")));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::AuthenticationFailed);

  ScriptedCredentials provider;
  HgRepo::setCredentialsProvider(&provider);
  const auto reset =
      qScopeGuard([]() { HgRepo::setCredentialsProvider(nullptr); });

  // The user cancels the prompt.
  QVERIFY(!HgRepo::cloneRepository(server.url(), clonePath,
                                   QStringLiteral("uuid")));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::AuthenticationFailed);
  QCOMPARE(provider.requests.size(), 1);
  QVERIFY(provider.results.isEmpty());
  QVERIFY(!QFileInfo::exists(clonePath + "/.hg"));

  // Rejected again and again, the command gives up.
  provider.requests.clear();
  provider.answers = QVector<HgRepo::HgCredentials>(10, kBadCredentials);
  QVERIFY(!HgRepo::cloneRepository(server.url(), clonePath,
                                   QStringLiteral("uuid")));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::AuthenticationFailed);
  QCOMPARE(provider.requests.size(), 5);
  QCOMPARE(provider.results.last(), HgRepo::HgAuthResult::GaveUp);
}

void HgRepoTest::sshKeySelection() {
#ifndef Q_OS_WIN
  QSKIP("The fake ssh is a batch file");
#endif
  const QString dir = QDir::cleanPath(_tmp->path());
  const QString argsFile = QDir::toNativeSeparators(dir + "/ssh_args.txt");
  const QString key = dir + "/keys/id_test";
  QVERIFY(writeFile(key, "not a real key\n"));

  // Stands in for ssh: records its arguments and fails like a rejected key.
  const QByteArray script = "@echo off\r\necho %*> \"" + argsFile.toUtf8() +
                            "\"\r\necho Permission denied (publickey).1>&2\r\n"
                            "exit /b 255\r\n";
  const QString fakeSsh = QDir::toNativeSeparators(dir + "/fake_ssh.cmd");
  const QString fakePlink =
      QDir::toNativeSeparators(dir + "/TortoisePlink.cmd");
  QVERIFY(writeFile(fakeSsh, script));
  QVERIFY(writeFile(fakePlink, script));

  const auto setUserSsh = [&](const QString &ssh) {
    QByteArray hgrc = readFile(file(".hg/hgrc"));
    hgrc += "\n[ui]\nssh = \"" + ssh.toUtf8() +
            "\"\n[paths]\n"
            "sshremote = ssh://hg@example.invalid/repo\n";
    return writeFile(file(".hg/hgrc"), hgrc);
  };
  const auto sshArgs = [&]() {
    QFile::remove(argsFile);
    const bool fetched =
        HgRepo::fetchRemote(_repo.get(), QStringLiteral("sshremote"));
    return fetched ? QStringLiteral("fetched")
                   : QString::fromLocal8Bit(readFile(argsFile)).trimmed();
  };

  ScriptedCredentials provider;
  provider.keys = {HgRepo::HgSshKey{QStringLiteral("example.invalid"), key},
                   HgRepo::HgSshKey{QStringLiteral("missing.invalid"),
                                    dir + "/keys/missing"}};
  HgRepo::setCredentialsProvider(&provider);
  const auto reset =
      qScopeGuard([]() { HgRepo::setCredentialsProvider(nullptr); });

  // The user's ssh gets Sourcerer's config, which names the key per host.
  QVERIFY(setUserSsh(fakeSsh));
  const QString args = sshArgs();
  QVERIFY2(args.contains(QStringLiteral("-o BatchMode=yes")), qPrintable(args));
  QVERIFY2(args.contains(QStringLiteral("hg@example.invalid")),
           qPrintable(args));
  static const QRegularExpression configArg(QStringLiteral("-F \"([^\"]+)\""));
  const QRegularExpressionMatch match = configArg.match(args);
  QVERIFY2(match.hasMatch(), qPrintable(args));
  const QString config = QString::fromUtf8(readFile(match.captured(1)));
  QVERIFY2(config.contains(QStringLiteral("Host example.invalid\n  "
                                          "IdentityFile \"%1\"\n  "
                                          "IdentitiesOnly yes")
                               .arg(key)),
           qPrintable(config));
  // Keys whose file is gone are left out.
  QVERIFY(!config.contains(QStringLiteral("missing.invalid")));

  // What to do about the rejected key is part of the error.
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("SSH keys")),
           qPrintable(lastErrorMessage()));

  // Without keys hg runs the user's ssh as it is.
  provider.keys.clear();
  QVERIFY(!sshArgs().contains(QStringLiteral("-F")));

  // PuTTY's plink reads no ssh config.
  provider.keys = {HgRepo::HgSshKey{QStringLiteral("example.invalid"), key}};
  QVERIFY(setUserSsh(fakePlink));
  const QString plinkArgs = sshArgs();
  QVERIFY2(plinkArgs.contains(QStringLiteral("hg@example.invalid")),
           qPrintable(plinkArgs));
  QVERIFY(!plinkArgs.contains(QStringLiteral("-F")));
}

void HgRepoTest::tagsOnRemote() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QString clonePath;
  RepoPtr clone = cloneRoot(&clonePath);
  QVERIFY(clone);

  const auto serverTags = [&]() {
    return QString::fromUtf8(hg({QStringLiteral("tags"), QStringLiteral("-q")}))
        .split('\n', Qt::SkipEmptyParts);
  };

  QVERIFY(HgRepo::createTagFromCommit(clone.get(), root, QStringLiteral("v1"),
                                      kAuthor, kEmail, QString()));
  QVERIFY2(HgRepo::pushTag(clone.get(), QStringLiteral("v1")),
           qPrintable(lastErrorMessage()));
  QVERIFY(serverTags().contains(QStringLiteral("v1")));

  QVERIFY(HgRepo::createTagFromCommit(clone.get(), root, QStringLiteral("l1"),
                                      kAuthor, kEmail, QString(), true));
  QVERIFY(!HgRepo::pushTag(clone.get(), QStringLiteral("l1")));

  QVERIFY(HgRepo::createTagFromCommit(clone.get(), root, QStringLiteral("v2"),
                                      kAuthor, kEmail, QString()));
  QVERIFY2(HgRepo::pushAllTags(clone.get()), qPrintable(lastErrorMessage()));
  QVERIFY(serverTags().contains(QStringLiteral("v2")));

  QVERIFY2(HgRepo::deleteRemoteTag(clone.get(), QStringLiteral("v1")),
           qPrintable(lastErrorMessage()));
  QVERIFY(!serverTags().contains(QStringLiteral("v1")));
  QVERIFY(!HgRepo::listTags(clone.get()).contains(QStringLiteral("v1")));
  QVERIFY(serverTags().contains(QStringLiteral("v2")));
}

void HgRepoTest::mercurialSubrepos() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());

  // The repository the subrepo is cloned from.
  const QString source = _tmp->path() + QStringLiteral("/core");
  hgIn(_tmp->path(), {QStringLiteral("init"), source});
  QVERIFY(writeFile(source + QStringLiteral("/core.txt"), "1\n"));
  hgIn(source, {QStringLiteral("commit"), QStringLiteral("-A"),
                QStringLiteral("-m"), QStringLiteral("Core 1")});
  const QString core1 = nodeOf(source, QStringLiteral("tip"));

  QVERIFY2(HgRepo::addSubmodule(_repo.get(), source,
                                QStringLiteral("libs/core"), QString(),
                                QString()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(HgRepo::listSubmodules(_repo.get()),
           QStringList{QStringLiteral("libs/core")});
  QVector<HgRepo::HgSubrepo> subrepos = HgRepo::listSubrepos(_repo.get());
  QCOMPARE(subrepos.size(), 1);
  QCOMPARE(subrepos[0].kind, HgRepo::HgSubrepoKind::Mercurial);
  QCOMPARE(subrepos[0].source, source);
  QVERIFY(subrepos[0].pinnedState.isEmpty());
  QCOMPARE(subrepos[0].currentState, core1);
  QCOMPARE(HgRepo::getSubmodulePath(_repo.get(), QStringLiteral("libs/core")),
           file("libs/core"));

  // .hgsub and the new subrepo are staged and committed together.
  GitRepo::GitStatus status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.newStagedFiles, (QStringList{QStringLiteral(".hgsub"),
                                               QStringLiteral("libs/core")}));
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Add core"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
  QCOMPARE(HgRepo::listSubrepos(_repo.get())[0].pinnedState, core1);
  QCOMPARE(
      HgRepo::listSubmodulesWithStatus(_repo.get(), _quit)[0].statusInfo.status,
      GitRepo::SubmoduleStatusType::UpToDate);

  // A commit in the subrepo shows it as modified until it is committed.
  QVERIFY(writeFile(file("libs/core/core.txt"), "2\n"));
  hgIn(file("libs/core"), {QStringLiteral("commit"), QStringLiteral("-m"),
                           QStringLiteral("Core 2")});
  const QString core2 = nodeOf(file("libs/core"), QStringLiteral("."));
  QCOMPARE(
      HgRepo::listSubmodulesWithStatus(_repo.get(), _quit)[0].statusInfo.status,
      GitRepo::SubmoduleStatusType::Outdated);
  status = HgRepo::getStatus(_repo.get(), _quit);
  QCOMPARE(status.modifiedFiles, QStringList{QStringLiteral("libs/core")});

  // Discarding goes back to the pinned node.
  QVERIFY2(HgRepo::discardFiles(_repo.get(), {QStringLiteral("libs/core")}),
           qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(file("libs/core"), QStringLiteral(".")), core1);
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());

  hgIn(file("libs/core"), {QStringLiteral("update"), core2});
  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("libs/core")}));
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).modifiedStagedFiles,
           QStringList{QStringLiteral("libs/core")});
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Bump core"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QCOMPARE(HgRepo::listSubrepos(_repo.get())[0].pinnedState, core2);
  QVERIFY(readFile(file(".hgsubstate")).startsWith(core2.toLatin1()));

  // A missing checkout is cloned again by an update, which needs the pinned
  // node in the subrepo's source.
  hgIn(file("libs/core"), {QStringLiteral("push")});
  QVERIFY(QDir(file("libs/core")).removeRecursively());
  QCOMPARE(
      HgRepo::listSubmodulesWithStatus(_repo.get(), _quit)[0].statusInfo.status,
      GitRepo::SubmoduleStatusType::Error);
  QVERIFY2(HgRepo::updateSubmodule(_repo.get(), QStringLiteral("libs/core")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(nodeOf(file("libs/core"), QStringLiteral(".")), core2);

  QVERIFY2(HgRepo::deleteSubmodule(_repo.get(), QStringLiteral("libs/core"),
                                   QString()),
           qPrintable(lastErrorMessage()));
  QVERIFY(!QFileInfo::exists(file("libs/core")));
  QVERIFY(!QFileInfo::exists(file(".hgsub")));
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Remove core"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::listSubmodules(_repo.get()).isEmpty());
  QVERIFY(!HgRepo::getStatus(_repo.get(), _quit).hasChanges());
}

void HgRepoTest::subrepoKinds() {
  const QString gitSha =
      QStringLiteral("0123456789abcdef0123456789abcdef01234567");
  QVERIFY(writeFile(file(".hgsub"),
                    "vendor = [git]https://example.com/vendor.git\n"
                    "legacy = [svn]https://svn.example.com/legacy\n"
                    "missing = https://example.com/missing\n"
                    "\n[subpaths]\nexample = elsewhere\n"));
  QVERIFY(writeFile(file(".hgsubstate"),
                    gitSha.toLatin1() + " vendor\n42 legacy\n" +
                        QByteArray(40, 'a') + " missing\n"));
  // A Git checkout, read without git.
  QVERIFY(writeFile(file("vendor/.git/HEAD"), "ref: refs/heads/main\n"));
  QVERIFY(writeFile(file("vendor/.git/refs/heads/main"), gitSha.toLatin1()));
  QDir().mkpath(file("legacy"));

  const QVector<HgRepo::HgSubrepo> subrepos = HgRepo::listSubrepos(_repo.get());
  QCOMPARE(subrepos.size(), 3);
  QCOMPARE(subrepos[0].kind, HgRepo::HgSubrepoKind::Git);
  QCOMPARE(subrepos[0].source,
           QStringLiteral("https://example.com/vendor.git"));
  QCOMPARE(subrepos[0].currentState, gitSha);
  QCOMPARE(subrepos[1].kind, HgRepo::HgSubrepoKind::Subversion);
  QCOMPARE(subrepos[1].pinnedState, QStringLiteral("42"));
  QCOMPARE(subrepos[2].kind, HgRepo::HgSubrepoKind::Mercurial);

  const QVector<GitRepo::SubmoduleInfo> infos =
      HgRepo::listSubmodulesWithStatus(_repo.get(), _quit);
  QCOMPARE(infos[0].statusInfo.status, GitRepo::SubmoduleStatusType::UpToDate);
  QCOMPARE(infos[1].statusInfo.status, GitRepo::SubmoduleStatusType::Unknown);
  QCOMPARE(infos[2].statusInfo.status, GitRepo::SubmoduleStatusType::Error);

  // A moved Git checkout is outdated.
  QVERIFY(writeFile(file("vendor/.git/refs/heads/main"), QByteArray(40, 'b')));
  QCOMPARE(
      HgRepo::listSubmodulesWithStatus(_repo.get(), _quit)[0].statusInfo.status,
      GitRepo::SubmoduleStatusType::Outdated);

  QVERIFY(!HgRepo::updateSubmodule(_repo.get(), QStringLiteral("vendor")));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::UnsupportedOperation);

  // Git and Subversion subrepos checked out by the caller are registered.
  QVERIFY(writeFile(file("other/.git/HEAD"), QByteArray(40, 'c') + "\n"));
  QVERIFY2(HgRepo::registerSubrepo(_repo.get(), QStringLiteral("other"),
                                   QStringLiteral("https://example.com/o.git"),
                                   HgRepo::HgSubrepoKind::Git),
           qPrintable(lastErrorMessage()));
  const QByteArray hgsub = readFile(file(".hgsub"));
  QVERIFY(hgsub.contains("missing = https://example.com/missing\n"
                         "other = [git]https://example.com/o.git\n"));
  QVERIFY(hgsub.contains("[subpaths]\nexample = elsewhere"));
  QVERIFY(!HgRepo::registerSubrepo(_repo.get(), QStringLiteral("other"),
                                   QStringLiteral("https://example.com/o.git"),
                                   HgRepo::HgSubrepoKind::Git));
}

void HgRepoTest::worktrees() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));
  QVERIFY(!root.isEmpty());

  const QString sharePath = _tmp->path() + QStringLiteral("/wt1");
  QVERIFY2(HgRepo::addWorktree(_repo.get(), QStringLiteral("wt1"), sharePath,
                               QStringLiteral("feature"), true, QString(),
                               HgRepo::HgBranchKind::Bookmark),
           qPrintable(lastErrorMessage()));
  QVERIFY(QFileInfo::exists(sharePath + QStringLiteral("/.hg/sharedpath")));
  QVERIFY(QFileInfo::exists(sharePath + QStringLiteral("/a.txt")));
  // Bookmarks are shared.
  QVERIFY(HgRepo::listLocalBranches(_repo.get(), _quit)
              .contains(QStringLiteral("@feature")));

  QVector<GitRepo::WorktreeInfo> worktrees = HgRepo::listWorktrees(_repo.get());
  QCOMPARE(worktrees.size(), 2);
  QVERIFY(worktrees[0].isMain);
  QVERIFY(worktrees[0].isCurrent);
  QCOMPARE(worktrees[0].path, _root);
  QCOMPARE(worktrees[0].branch, QStringLiteral("default"));
  QCOMPARE(worktrees[0].head, root.left(7));
  QCOMPARE(worktrees[1].name, QStringLiteral("wt1"));
  QCOMPARE(worktrees[1].path, QDir::cleanPath(sharePath));
  QCOMPARE(worktrees[1].branch, QStringLiteral("@feature"));
  QVERIFY(worktrees[1].valid);
  QVERIFY(!worktrees[1].isCurrent);

  // A named branch in a second share, which shares the store.
  const QString topicPath = _tmp->path() + QStringLiteral("/wt2");
  QVERIFY2(HgRepo::addWorktree(_repo.get(), QStringLiteral("wt2"), topicPath,
                               QStringLiteral("topic"), true, QString(),
                               HgRepo::HgBranchKind::NamedBranch),
           qPrintable(lastErrorMessage()));
  RepoPtr share(HgRepo::openRepository(topicPath, kAuthor, kEmail));
  QVERIFY(share);
  QCOMPARE(
      HgRepo::getActiveBranch(share.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("topic"));
  QVERIFY(writeFile(topicPath + QStringLiteral("/t.txt"), "t\n"));
  const QString onTopic = commitIn(share.get(), QStringLiteral("On topic"));
  QVERIFY(!onTopic.isEmpty());
  QVERIFY(HgRepo::getBranchHistory(_repo.get(), QStringLiteral(ALL_BRANCHES),
                                   GitRepo::BranchType::All, 0, _quit)
              .commits.contains(onTopic));

  // The list is the same from a share.
  worktrees = HgRepo::listWorktrees(share.get());
  QCOMPARE(worktrees.size(), 3);
  QVERIFY(!worktrees[0].isCurrent);
  QCOMPARE(worktrees[2].name, QStringLiteral("wt2"));
  QVERIFY(worktrees[2].isCurrent);
  QCOMPARE(worktrees[2].branch, QStringLiteral("topic"));
  share.reset();

  QVERIFY(!HgRepo::addWorktree(_repo.get(), QStringLiteral("wt1"),
                               _tmp->path() + QStringLiteral("/wt3"),
                               QStringLiteral("default"), false));

  QVERIFY(HgRepo::lockWorktree(_repo.get(), QStringLiteral("wt1"),
                               QStringLiteral("In use")));
  QVERIFY(HgRepo::listWorktrees(_repo.get())[1].locked);
  QVERIFY(!HgRepo::removeWorktree(_repo.get(), QStringLiteral("wt1"), true));
  QVERIFY(HgRepo::unlockWorktree(_repo.get(), QStringLiteral("wt1")));

  QVERIFY(writeFile(sharePath + QStringLiteral("/a.txt"), "dirty\n"));
  QVERIFY(!HgRepo::removeWorktree(_repo.get(), QStringLiteral("wt1"), false));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::UncommittedChanges);
  QVERIFY2(HgRepo::removeWorktree(_repo.get(), QStringLiteral("wt1"), true),
           qPrintable(lastErrorMessage()));
  QVERIFY(!QFileInfo::exists(sharePath));
  QCOMPARE(HgRepo::listWorktrees(_repo.get()).size(), 2);
  QVERIFY(
      !HgRepo::removeWorktree(_repo.get(), QFileInfo(_root).fileName(), true));
}

void HgRepoTest::lfs() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());

  QVERIFY(!HgRepo::isLfsEnabled(_repo.get()));
  QVERIFY(writeFile(file("big.bin"), QByteArray(4096, 'x')));
  QVERIFY(!HgRepo::addFilesToLfs(_repo.get(), {QStringLiteral("big.bin")}));
  QCOMPARE(HgRepo::getLastError().error, HgRepo::HgError::LfsNotEnabled);

  QVERIFY2(HgRepo::enableLfs(_repo.get()), qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::isLfsEnabled(_repo.get()));
  QVERIFY2(HgRepo::addFilesToLfs(_repo.get(), {QStringLiteral("big.bin")}),
           qPrintable(lastErrorMessage()));
  QVERIFY(readFile(file(".hglfs")).contains("path:big.bin = all()"));
  QCOMPARE(HgRepo::listLfsFiles(_repo.get()),
           QStringList{QStringLiteral("big.bin")});
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).newStagedFiles,
           QStringList{QStringLiteral(".hglfs")});

  QVERIFY(HgRepo::stageFiles(_repo.get(), {QStringLiteral("big.bin")}));
  QVERIFY2(HgRepo::commitStaged(_repo.get(), QStringLiteral("Add big file"),
                                QString(), kAuthor, kEmail, true, false, false),
           qPrintable(lastErrorMessage()));
  const GitRepo::Lfs::StorageStatus storage =
      HgRepo::getLfsStorageStatus(_repo.get());
  QCOMPARE(storage.repositoryPath, _root);
  QVERIFY(storage.objectCount >= 1);
  QVERIFY(storage.totalBytes >= 4096);
  // Share-safe repositories keep most requirements in the store.
  QVERIFY(readFile(file(".hg/requires")).contains("lfs") ||
          readFile(file(".hg/store/requires")).contains("lfs"));

  QVERIFY(HgRepo::removeFilesFromLfs(_repo.get(), {QStringLiteral("big.bin")}));
  QVERIFY(HgRepo::listLfsFiles(_repo.get()).isEmpty());
  QCOMPARE(HgRepo::getStatus(_repo.get(), _quit).modifiedStagedFiles,
           QStringList{QStringLiteral(".hglfs")});
}

void HgRepoTest::gitFlowWithNamedBranches() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());
  QCOMPARE(HgRepo::getFlowBranchKind(_repo.get()),
           HgRepo::HgBranchKind::NamedBranch);
  QCOMPARE(HgRepo::getWorkflowType(_repo.get()),
           GitRepo::WorkflowType::GitFlow);

  // Without main or develop the feature starts from "default".
  QVERIFY2(HgRepo::startGitFlowBranch(_repo.get(),
                                      GitRepo::GitFlowBranchType::Feature,
                                      QStringLiteral("login")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("feature/login"));
  QVERIFY(HgRepo::listLocalBranches(_repo.get(), _quit)
              .contains(QStringLiteral("feature/login")));

  QVERIFY(writeFile(file("login.txt"), "login\n"));
  QVERIFY(!commitAll(QStringLiteral("Login")).isEmpty());

  QVERIFY2(HgRepo::finishGitFlowBranch(
               _repo.get(), GitRepo::GitFlowBranchType::Feature,
               QStringLiteral("login"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("default"));
  QVERIFY(QFileInfo::exists(file("login.txt")));
  QCOMPARE(HgRepo::listLocalBranches(_repo.get(), _quit),
           QStringList{QStringLiteral("default")});
  QCOMPARE(HgRepo::getCommitStatus(_repo.get(), tip()).summary,
           QStringLiteral("Merge branch 'feature/login' into 'default'"));
}

void HgRepoTest::gitFlowNextWithBookmarks() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());
  QVERIFY(
      HgRepo::setFlowBranchKind(_repo.get(), HgRepo::HgBranchKind::Bookmark));

  QVERIFY(!HgRepo::startGitFlowNextBranch(
      _repo.get(), QStringLiteral("feature"), QStringLiteral("x")));
  QCOMPARE(HgRepo::getLastError().error,
           HgRepo::HgError::WorkflowNotInitialized);

  QVERIFY2(HgRepo::initGitFlowNext(_repo.get(), QStringLiteral("default"),
                                   QStringLiteral("develop")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@develop"));
  GitRepo::GitFlowNextConfig config;
  QVERIFY(HgRepo::readGitFlowNextConfig(_repo.get(), &config));
  QVERIFY(config.initialized);
  QCOMPARE(config.topicTypes().size(), 3);
  QVERIFY(readFile(file(".hg/hgrc")).contains("[gitflow]"));

  QVERIFY(
      HgRepo::setWorkflowType(_repo.get(), GitRepo::WorkflowType::GitFlowNext));
  QCOMPARE(HgRepo::getWorkflowType(_repo.get()),
           GitRepo::WorkflowType::GitFlowNext);

  QVERIFY2(HgRepo::startGitFlowNextBranch(
               _repo.get(), QStringLiteral("feature"), QStringLiteral("x")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@feature/x"));
  QVERIFY(writeFile(file("x.txt"), "x\n"));
  const QString featureCommit = commitAll(QStringLiteral("X"));

  QVERIFY2(HgRepo::finishGitFlowNextBranch(
               _repo.get(), QStringLiteral("feature/x"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("@develop"));
  QCOMPARE(nodeOf(_root, QStringLiteral("develop")), featureCommit);
  QVERIFY(!HgRepo::listLocalBranches(_repo.get(), _quit)
               .contains(QStringLiteral("@feature/x")));

  // A release is merged into main, tagged, and develop follows main.
  QVERIFY2(HgRepo::startGitFlowNextBranch(
               _repo.get(), QStringLiteral("release"), QStringLiteral("1.0")),
           qPrintable(lastErrorMessage()));
  QVERIFY(writeFile(file("version.txt"), "1.0\n"));
  QVERIFY(!commitAll(QStringLiteral("Release 1.0")).isEmpty());
  QVERIFY2(HgRepo::finishGitFlowNextBranch(
               _repo.get(), QStringLiteral("release/1.0"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::listTags(_repo.get()).contains(QStringLiteral("1.0")));
  QCOMPARE(nodeOf(_root, QStringLiteral("develop")),
           nodeOf(_root, QStringLiteral("max(branch(default))")));
  QVERIFY(!HgRepo::listLocalBranches(_repo.get(), _quit)
               .contains(QStringLiteral("@release/1.0")));

  // A tag that exists stops the finish before anything changes.
  QVERIFY(HgRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("release"),
                                         QStringLiteral("1.0")));
  QVERIFY(!HgRepo::finishGitFlowNextBranch(
      _repo.get(), QStringLiteral("release/1.0"), kAuthor, kEmail));
  QVERIFY(HgRepo::listLocalBranches(_repo.get(), _quit)
              .contains(QStringLiteral("@release/1.0")));
}

void HgRepoTest::applyCustomWorkflow() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  const QString root = commitAll(QStringLiteral("Root"));

  GitRepo::GitFlowNextWorkflow workflow;
  workflow.id = QStringLiteral("custom-1");
  workflow.name = QStringLiteral("Custom");
  workflow.revision = 3;
  GitRepo::GitFlowNextBranch main;
  main.name = QStringLiteral("default");
  GitRepo::GitFlowNextBranch staging;
  staging.name = QStringLiteral("staging");
  staging.parent = QStringLiteral("default");
  staging.autoUpdate = true;
  GitRepo::GitFlowNextBranch topic;
  topic.name = QStringLiteral("feat");
  topic.topic = true;
  topic.parent = QStringLiteral("staging");
  topic.prefix = QStringLiteral("feat/");
  workflow.branches = {main, staging, topic};

  QVERIFY2(HgRepo::applyGitFlowNextWorkflow(_repo.get(), workflow),
           qPrintable(lastErrorMessage()));
  // The missing base branch is created without updating to it.
  QCOMPARE(tip(), root);
  QVERIFY(HgRepo::listLocalBranches(_repo.get(), _quit)
              .contains(QStringLiteral("staging")));
  QCOMPARE(HgRepo::getWorkflowType(_repo.get()),
           GitRepo::WorkflowType::GitFlowNext);

  QString id;
  int revision = 0;
  QVERIFY(HgRepo::getGitFlowNextWorkflowReference(_repo.get(), &id, &revision));
  QCOMPARE(id, QStringLiteral("custom-1"));
  QCOMPARE(revision, 3);

  GitRepo::GitFlowNextConfig config;
  QVERIFY(HgRepo::readGitFlowNextConfig(_repo.get(), &config));
  QCOMPARE(config.topicTypes().size(), 1);
  QCOMPARE(config.topicTypes()[0].prefix, QStringLiteral("feat/"));

  QVERIFY2(HgRepo::startGitFlowNextBranch(_repo.get(), QStringLiteral("feat"),
                                          QStringLiteral("y")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("feat/y"));
  QVERIFY(writeFile(file("y.txt"), "y\n"));
  QVERIFY(!commitAll(QStringLiteral("Y")).isEmpty());
  QVERIFY2(HgRepo::updateGitFlowNextBranch(
               _repo.get(), QStringLiteral("feat/y"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY2(HgRepo::finishGitFlowNextBranch(
               _repo.get(), QStringLiteral("feat/y"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("staging"));
  QVERIFY(QFileInfo::exists(file("y.txt")));

  // Choosing another workflow forgets the custom one.
  QVERIFY(HgRepo::setWorkflowType(_repo.get(), GitRepo::WorkflowType::GitFlow));
  QVERIFY(
      !HgRepo::getGitFlowNextWorkflowReference(_repo.get(), &id, &revision));
}

void HgRepoTest::gitHubFlow() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());

  QVERIFY2(HgRepo::startGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                         QStringLiteral("main")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("fix"));
  QVERIFY(writeFile(file("fix.txt"), "fix\n"));
  QVERIFY(!commitAll(QStringLiteral("Fix")).isEmpty());

  QVERIFY(
      !HgRepo::updateGitHubFlowBranch(_repo.get(), QStringLiteral("default"),
                                      QStringLiteral("main"), kAuthor, kEmail));
  QVERIFY2(HgRepo::updateGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                          QStringLiteral("main"), kAuthor,
                                          kEmail),
           qPrintable(lastErrorMessage()));
  QVERIFY2(HgRepo::finishGitHubFlowBranch(_repo.get(), QStringLiteral("fix"),
                                          QStringLiteral("main"), kAuthor,
                                          kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("default"));
  QVERIFY(QFileInfo::exists(file("fix.txt")));
  QCOMPARE(HgRepo::listLocalBranches(_repo.get(), _quit),
           QStringList{QStringLiteral("default")});
}

void HgRepoTest::gitLabFlow() {
  QVERIFY(writeFile(file("a.txt"), "a\n"));
  QVERIFY(!commitAll(QStringLiteral("Root")).isEmpty());

  QVERIFY2(HgRepo::startGitLabFlowFeature(_repo.get(), QStringLiteral("f"),
                                          QStringLiteral("main")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("feature/f"));
  QVERIFY(writeFile(file("f.txt"), "f\n"));
  QVERIFY(!commitAll(QStringLiteral("F")).isEmpty());
  QVERIFY2(HgRepo::finishGitLabFlowFeature(_repo.get(), QStringLiteral("f"),
                                           QStringLiteral("main"), kAuthor,
                                           kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("default"));

  const QStringList chain = GitRepo::gitLabFlowEnvironmentChain(
      QStringLiteral("default"), QStringLiteral("staging,production"));
  QVERIFY2(HgRepo::promoteGitLabFlowEnvironment(
               _repo.get(), chain, QStringLiteral("default"), kAuthor, kEmail),
           qPrintable(lastErrorMessage()));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("staging"));
  QVERIFY(HgRepo::promoteGitLabFlowEnvironment(
      _repo.get(), chain, QStringLiteral("staging"), kAuthor, kEmail));
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("production"));
  QVERIFY(!HgRepo::promoteGitLabFlowEnvironment(
      _repo.get(), chain, QStringLiteral("production"), kAuthor, kEmail));

  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY2(HgRepo::startGitLabFlowRelease(_repo.get(),
                                          QStringLiteral("1-0-stable"),
                                          QStringLiteral("main")),
           qPrintable(lastErrorMessage()));
  QVERIFY(HgRepo::checkoutBranch(_repo.get(), QStringLiteral("default"),
                                 GitRepo::BranchType::Local));
  QVERIFY(writeFile(file("hotfix.txt"), "h\n"));
  const QString hotfix = commitAll(QStringLiteral("Hotfix"));

  bool conflicted = true;
  QVERIFY2(HgRepo::cherryPickToGitLabFlowRelease(_repo.get(), hotfix,
                                                 QStringLiteral("1-0-stable"),
                                                 kAuthor, kEmail, &conflicted),
           qPrintable(lastErrorMessage()));
  QVERIFY(!conflicted);
  QCOMPARE(
      HgRepo::getActiveBranch(_repo.get(), GitRepo::BranchType::Local, _quit),
      QStringLiteral("default"));
  QCOMPARE(
      QString::fromUtf8(hg({QStringLiteral("log"), QStringLiteral("-r"),
                            QStringLiteral("max(branch('1-0-stable'))"),
                            QStringLiteral("-T"), QStringLiteral("{desc}")})),
      QStringLiteral("Hotfix"));
}

QTEST_GUILESS_MAIN(HgRepoTest)

#include "tst_hgrepo.moc"
