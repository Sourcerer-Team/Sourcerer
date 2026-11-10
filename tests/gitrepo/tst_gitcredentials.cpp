// MIT License
// Copyright (c) 2026 Sourcerer

// Tests for the credential stores of lib/GitRepo: GitCredentialStorage (the
// encrypted HTTP credentials), GitKeyStorage (SSH keys per host), the Basic
// authentication header and the SSH key lookup with its passphrase prompt.
// See docs/GIT_TEST_PLAN.md, section 5. Everything is written below the
// QStandardPaths test locations, the master key is a fixed test key instead
// of the OS keychain.

#include "FakeCredentialsOverlay.h"
#include "GitTestHelpers.h"

#include <GitCredentialStorage.h>
#include <GitKeyStorage.h>

#include <QDataStream>
#include <QDateTime>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QThread>
#include <QtTest>

#include <memory>

using namespace GitTest;
using GitRepo::GitCredentialStorage;
using GitRepo::GitKeyStorage;

namespace {

// The file GitCredentialStorage persists to.
QString credentialsFile() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
         QStringLiteral("/git_credentials.dat");
}

// Entries of the credentials file: repository path to encrypted data.
QVector<QPair<QString, QByteArray>> readEntries() {
  QVector<QPair<QString, QByteArray>> entries;
  QFile file(credentialsFile());
  if (!file.open(QIODevice::ReadOnly)) {
    return entries;
  }
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);
  int version = 0;
  qsizetype count = 0;
  stream >> version >> count;
  for (qsizetype i = 0; i < count && stream.status() == QDataStream::Ok; ++i) {
    QString path;
    QByteArray data;
    stream >> path >> data;
    entries.append({path, data});
  }
  return entries;
}

void writeEntries(const QVector<QPair<QString, QByteArray>> &entries) {
  QFile file(credentialsFile());
  QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
  QDataStream stream(&file);
  stream.setVersion(QDataStream::Qt_6_0);
  stream << 1 << static_cast<qsizetype>(entries.size());
  for (const auto &[path, data] : entries) {
    stream << path << data;
  }
}

} // namespace

class GitCredentialsTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  // GitCredentialStorage.
  void storeGetAndRemove();
  void persistedAndReloaded();
  void passwordsAreEncrypted();
  void unusualCredentials_data();
  void unusualCredentials();
  void expiry();
  void clearMemoryKeepsPersisted();
  void corruptFileIsIgnored();
  void truncatedFileKeepsCompleteEntries();
  void tamperedEntryIsRejected();
  void pathsAreExactKeys();

  // Basic authentication.
  void basicAuthHeader();

  // GitKeyStorage and the SSH key lookup.
  void keyPairsPerHost();
  void sshAuthInfo();
  void sshPassphrasePrompt_data();
  void sshPassphrasePrompt();
  void sshPassphraseIsCheckedAndRemembered();
  void defaultKeyFallback_data();
  void defaultKeyFallback();
  void generateKeyPair();
  void generateKeyPairWithPassphrase();
  void generateKeyPairRefusesBadNames_data();
  void generateKeyPairRefusesBadNames();

private:
  // Runs ssh-keygen, returns its output or a null string on failure.
  QString sshKeygen(const QStringList &args) const;
  // The .ssh directory of the test home.
  QString sshDir() const { return _base + QStringLiteral("/home/.ssh"); }

  // A key made with ssh-keygen, empty if ssh-keygen is missing.
  QString makeKey(const QString &name, const QString &passphrase,
                  const QStringList &extraArgs = {});

  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  QString _sshKeygen;
};

QString GitCredentialsTest::makeKey(const QString &name,
                                    const QString &passphrase,
                                    const QStringList &extraArgs) {
  if (_sshKeygen.isEmpty()) {
    return {};
  }
  const QString key = _base + QStringLiteral("/keys/") + name;
  QDir().mkpath(QFileInfo(key).absolutePath());
  QProcess process;
  process.start(_sshKeygen, QStringList{QStringLiteral("-q"),
                                        QStringLiteral("-N"), passphrase,
                                        QStringLiteral("-C"), kEmail,
                                        QStringLiteral("-f"), key} +
                                extraArgs);
  if (!process.waitForFinished(60000) || process.exitCode() != 0) {
    qWarning() << "ssh-keygen failed:" << process.readAllStandardError();
    return {};
  }
  return key;
}

void GitCredentialsTest::initTestCase() {
  QStandardPaths::setTestModeEnabled(true);
  useTestCredentialsKey();
  _sshKeygen = QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
  GitRepo::initForCurrentThread();
}

void GitCredentialsTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  GitCredentialStorage::getInstance().clearAll();
  // Default keys and generated keys go to <base>/home/.ssh.
  GitRepo::Testing::setHomeDirectory(_base + QStringLiteral("/home"));
}

void GitCredentialsTest::cleanup() {
  GitCredentialStorage::getInstance().clearAll();
  GitRepo::clearCachedCredentials(QString());
  auto &keys = GitKeyStorage::getInstance();
  for (const GitRepo::GitKeyPair &pair : keys.getAllKeyPairs()) {
    keys.removeKeyPair(pair.host);
  }
  GitRepo::Testing::setHomeDirectory(QString());
  _tmp.reset();
}

// GitCredentialStorage.

void GitCredentialsTest::storeGetAndRemove() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  const qint64 before = QDateTime::currentMSecsSinceEpoch();

  QVERIFY(!storage.hasCredentials(repo));
  QString username;
  QString password;
  QVERIFY(!storage.getCredentials(repo, username, password));

  storage.storeCredentials(repo, QStringLiteral("alice"),
                           QStringLiteral("s3cret"), false);
  QVERIFY(storage.hasCredentials(repo));
  QVERIFY(storage.getCredentials(repo, username, password));
  QCOMPARE(username, QStringLiteral("alice"));
  QCOMPARE(password, QStringLiteral("s3cret"));
  QCOMPARE(storage.getAllRepositoryPaths(), QList<QString>{repo});
  qint64 created = 0;
  QVERIFY(storage.getCredentialsInfo(repo, username, created));
  QVERIFY(created >= before && created <= QDateTime::currentMSecsSinceEpoch());

  // Storing again replaces them.
  storage.storeCredentials(repo, QStringLiteral("bob"), QStringLiteral("pw"),
                           false);
  QVERIFY(storage.getCredentials(repo, username, password));
  QCOMPARE(username, QStringLiteral("bob"));
  QCOMPARE(storage.getAllRepositoryPaths().size(), 1);

  storage.removeCredentials(repo);
  QVERIFY(!storage.hasCredentials(repo));
  QVERIFY(!storage.getCredentials(repo, username, password));
  QVERIFY(!storage.getCredentialsInfo(repo, username, created));
  // Removing what isn't there is harmless.
  storage.removeCredentials(repo);
}

void GitCredentialsTest::persistedAndReloaded() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString kept = _base + QStringLiteral("/kept");
  const QString memoryOnly = _base + QStringLiteral("/memory");
  storage.storeCredentials(kept, QStringLiteral("alice"),
                           QStringLiteral("s3cret"), true);
  storage.storeCredentials(memoryOnly, QStringLiteral("bob"),
                           QStringLiteral("pw"), false);
  QVERIFY(QFileInfo::exists(credentialsFile()));
  qint64 created = 0;
  QString username;
  QVERIFY(storage.getCredentialsInfo(kept, username, created));

  // Reloading reads them back from the file, the others are gone.
  storage.clearMemoryCredentials();
  QString password;
  QVERIFY(storage.getCredentials(kept, username, password));
  QCOMPARE(username, QStringLiteral("alice"));
  QCOMPARE(password, QStringLiteral("s3cret"));
  qint64 reloaded = 0;
  QVERIFY(storage.getCredentialsInfo(kept, username, reloaded));
  QCOMPARE(reloaded, created);
  QVERIFY(!storage.hasCredentials(memoryOnly));

  // Removing one keeps the others in the file.
  const QString second = _base + QStringLiteral("/second");
  storage.storeCredentials(second, QStringLiteral("carol"),
                           QStringLiteral("pw2"), true);
  storage.removeCredentials(kept);
  storage.clearMemoryCredentials();
  QVERIFY(!storage.hasCredentials(kept));
  QVERIFY(storage.getCredentials(second, username, password));
  QCOMPARE(password, QStringLiteral("pw2"));

  // Removing the last one removes the file, clearAll as well.
  storage.removeCredentials(second);
  QVERIFY(!QFileInfo::exists(credentialsFile()));
  storage.storeCredentials(second, QStringLiteral("carol"),
                           QStringLiteral("pw2"), true);
  storage.clearAll();
  QVERIFY(!QFileInfo::exists(credentialsFile()));
  QVERIFY(storage.getAllRepositoryPaths().isEmpty());
}

void GitCredentialsTest::passwordsAreEncrypted() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  const QString password = QStringLiteral("VeryRecognizablePassword123");
  storage.storeCredentials(repo, QStringLiteral("RecognizableUser"), password,
                           true);

  const QByteArray file = readFile(credentialsFile());
  QVERIFY(!file.isEmpty());
  for (const QByteArray &needle :
       {password.toUtf8(), QByteArray(reinterpret_cast<const char *>(
                                          password.utf16()),
                                      password.size() * 2),
        QByteArray("RecognizableUser")}) {
    QVERIFY2(!file.contains(needle), needle.constData());
  }
  // The same credentials encrypt differently every time (random IV).
  const QByteArray first = readEntries().value(0).second;
  storage.storeCredentials(repo, QStringLiteral("RecognizableUser"), password,
                           true);
  QVERIFY(readEntries().value(0).second != first);
}

void GitCredentialsTest::unusualCredentials_data() {
  QTest::addColumn<QString>("username");
  QTest::addColumn<QString>("password");
  QTest::newRow("symbols") << QStringLiteral("alice") << QStringLiteral("s3cret=!");
  QTest::newRow("colons") << QStringLiteral("al:ice") << QStringLiteral(":pa:ss:");
  QTest::newRow("unicode") << QString::fromUtf16(u"älice")
                           << QString::fromUtf16(u"päss 日本 \U0001F600");
  QTest::newRow("empty password") << QStringLiteral("alice") << QString();
  QTest::newRow("empty username") << QString() << QStringLiteral("token");
  QTest::newRow("long") << QStringLiteral("alice")
                        << QStringLiteral("x").repeated(4096);
  QTest::newRow("whitespace") << QStringLiteral(" alice ")
                              << QStringLiteral(" pass word \t");
}

void GitCredentialsTest::unusualCredentials() {
  QFETCH(QString, username);
  QFETCH(QString, password);
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  storage.storeCredentials(repo, username, password, true);
  storage.clearMemoryCredentials();

  QString readUsername;
  QString readPassword;
  QVERIFY(storage.getCredentials(repo, readUsername, readPassword));
  QCOMPARE(readUsername, username);
  QCOMPARE(readPassword, password);

  // The Basic authentication header encodes them as UTF-8.
  QCOMPARE(GitRepo::getBasicAuthHeader(repo, QStringLiteral("https://example.invalid")),
           QStringLiteral("Basic ") +
               QString::fromLatin1((username + ':' + password).toUtf8().toBase64()));
}

void GitCredentialsTest::expiry() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  QVERIFY(storage.areCredentialsExpired(repo, 60000));

  storage.storeCredentials(repo, QStringLiteral("alice"), QStringLiteral("pw"),
                           false);
  QVERIFY(!storage.areCredentialsExpired(repo, 60000));
  QThread::msleep(20);
  QVERIFY(storage.areCredentialsExpired(repo, 5));
  QVERIFY(!storage.areCredentialsExpired(repo, 60 * 60 * 1000));
}

void GitCredentialsTest::clearMemoryKeepsPersisted() {
  auto &storage = GitCredentialStorage::getInstance();
  storage.storeCredentials(_base + QStringLiteral("/a"), QStringLiteral("a"),
                           QStringLiteral("1"), true);
  storage.storeCredentials(_base + QStringLiteral("/b"), QStringLiteral("b"),
                           QStringLiteral("2"), false);
  storage.clearMemoryCredentials();
  QCOMPARE(storage.getAllRepositoryPaths(),
           QList<QString>{_base + QStringLiteral("/a")});
}

void GitCredentialsTest::corruptFileIsIgnored() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  storage.storeCredentials(repo, QStringLiteral("alice"), QStringLiteral("pw"),
                           true);

  for (const QByteArray &garbage :
       {QByteArray("this is not a credentials file"), QByteArray(),
        QByteArray(3, '\x01'), QByteArray("\x00\x00\x00\x02", 4)}) {
    QVERIFY(writeFile(credentialsFile(), garbage));
    storage.clearMemoryCredentials();
    QVERIFY(storage.getAllRepositoryPaths().isEmpty());
  }

  // Storing again writes a valid file.
  storage.storeCredentials(repo, QStringLiteral("alice"), QStringLiteral("pw"),
                           true);
  storage.clearMemoryCredentials();
  QVERIFY(storage.hasCredentials(repo));
}

void GitCredentialsTest::truncatedFileKeepsCompleteEntries() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString first = _base + QStringLiteral("/a");
  const QString second = _base + QStringLiteral("/b");
  storage.storeCredentials(first, QStringLiteral("a"), QStringLiteral("1"),
                           true);
  storage.storeCredentials(second, QStringLiteral("b"), QStringLiteral("2"),
                           true);
  QByteArray file = readFile(credentialsFile());
  // The entries are written sorted by path: cut into the second one.
  file.chop(10);
  QVERIFY(writeFile(credentialsFile(), file));

  storage.clearMemoryCredentials();
  QString username;
  QString password;
  QVERIFY(storage.getCredentials(first, username, password));
  QCOMPARE(password, QStringLiteral("1"));
  // The cut entry doesn't decrypt and is left out.
  QVERIFY(!storage.hasCredentials(second));
}

void GitCredentialsTest::tamperedEntryIsRejected() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  storage.storeCredentials(repo, QStringLiteral("alice"), QStringLiteral("pw"),
                           true);

  // Flip a bit of the IV: with AES-CBC that flips the same bit of the first
  // plaintext block, which holds the username ("a" -> "`").
  QVector<QPair<QString, QByteArray>> entries = readEntries();
  QCOMPARE(entries.size(), 1);
  QByteArray &data = entries[0].second;
  data[5] = static_cast<char>(data[5] ^ 0x01);
  writeEntries(entries);

  storage.clearMemoryCredentials();
  QString username;
  QString password;
  const bool loaded = storage.getCredentials(repo, username, password);
  QVERIFY(!loaded || username == QStringLiteral("alice"));
}

void GitCredentialsTest::pathsAreExactKeys() {
  auto &storage = GitCredentialStorage::getInstance();
  const QString repo = _base + QStringLiteral("/repo");
  storage.storeCredentials(repo, QStringLiteral("alice"), QStringLiteral("pw"),
                           false);
  QString username;
  QString password;
  // Spellings of the same directory.
  const bool trailingSlash =
      storage.getCredentials(repo + '/', username, password);
  const bool nativeSeparators =
      storage.getCredentials(QDir::toNativeSeparators(repo), username, password);
  QVERIFY(trailingSlash);
  QVERIFY(nativeSeparators);
}

// Basic authentication.

void GitCredentialsTest::basicAuthHeader() {
  const QString repo = _base + QStringLiteral("/repo");
  const QString url = QStringLiteral("https://example.invalid/team/repo.git");
  QVERIFY(GitRepo::getBasicAuthHeader(QString(), url).isEmpty());
  QVERIFY(GitRepo::getBasicAuthHeader(repo, QString()).isEmpty());

  // Without stored credentials the user is asked.
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{QStringLiteral("alice"), QStringLiteral("s3cret")}});
  QCOMPARE(GitRepo::getBasicAuthHeader(repo, url),
           QStringLiteral("Basic YWxpY2U6czNjcmV0"));
  QCOMPARE(overlay.events(), QStringList{QStringLiteral("prompt")});
  QCOMPARE(overlay.requests().value(0).url, url);
  QCOMPARE(overlay.requests().value(0).repoPath, repo);
  QVERIFY(!overlay.requests().value(0).needsPassphrase);

  // Cancelled: no header.
  overlay.setAnswers({});
  QVERIFY(GitRepo::getBasicAuthHeader(repo, url).isEmpty());
  QCOMPARE(overlay.events(), QStringList{QStringLiteral("prompt")});

  // Stored ones are used without asking.
  GitCredentialStorage::getInstance().storeCredentials(
      repo, QStringLiteral("alice"), QStringLiteral("s3cret"), false);
  overlay.setAnswers({});
  QCOMPARE(GitRepo::getBasicAuthHeader(repo, url),
           QStringLiteral("Basic YWxpY2U6czNjcmV0"));
  QVERIFY(overlay.events().isEmpty());
}

// GitKeyStorage and the SSH key lookup.

void GitCredentialsTest::keyPairsPerHost() {
  auto &keys = GitKeyStorage::getInstance();
  const QString privateKey = _base + QStringLiteral("/keys/id_test");
  QVERIFY(writeFile(privateKey, "private"));
  QVERIFY(writeFile(privateKey + QStringLiteral(".pub"), "public"));

  GitRepo::GitKeyPair pair;
  QVERIFY(!keys.getKeyPairForHost(QStringLiteral("example.invalid"), pair));
  // Key files must exist.
  QVERIFY(!keys.setKeyPair(QStringLiteral("example.invalid"),
                           QStringLiteral("missing"),
                           _base + QStringLiteral("/keys/missing"),
                           _base + QStringLiteral("/keys/missing.pub")));

  QVERIFY(keys.setKeyPair(QStringLiteral("example.invalid"),
                          QStringLiteral("id_test"), privateKey,
                          privateKey + QStringLiteral(".pub")));
  QVERIFY(keys.getKeyPairForHost(QStringLiteral("example.invalid"), pair));
  QCOMPARE(pair.host, QStringLiteral("example.invalid"));
  QCOMPARE(pair.keyName, QStringLiteral("id_test"));
  QCOMPARE(pair.privateKeyPath, privateKey);
  QCOMPARE(pair.publicKeyPath, privateKey + QStringLiteral(".pub"));
  QCOMPARE(keys.getAllKeyPairs().size(), 1);
  QVERIFY(!keys.getKeyPairForHost(QStringLiteral("other.invalid"), pair));

  // Host names are case insensitive.
  const bool upperCase =
      keys.getKeyPairForHost(QStringLiteral("EXAMPLE.invalid"), pair);
  QVERIFY(upperCase);

  // Replacing and removing.
  QVERIFY(keys.setKeyPair(QStringLiteral("example.invalid"),
                          QStringLiteral("again"), privateKey,
                          privateKey + QStringLiteral(".pub")));
  QVERIFY(keys.getKeyPairForHost(QStringLiteral("example.invalid"), pair));
  QCOMPARE(pair.keyName, QStringLiteral("again"));
  QCOMPARE(keys.getAllKeyPairs().size(), 1);
  QVERIFY(keys.removeKeyPair(QStringLiteral("example.invalid")));
  QVERIFY(!keys.removeKeyPair(QStringLiteral("example.invalid")));
  QVERIFY(!keys.getKeyPairForHost(QStringLiteral("example.invalid"), pair));
}

void GitCredentialsTest::sshAuthInfo() {
  auto &keys = GitKeyStorage::getInstance();
  const QString privateKey = _base + QStringLiteral("/keys/id_test");
  QVERIFY(writeFile(privateKey, "-----BEGIN OPENSSH PRIVATE KEY-----\n"));
  QVERIFY(writeFile(privateKey + QStringLiteral(".pub"), "ssh-ed25519 AAAA"));
  QVERIFY(keys.setKeyPair(QStringLiteral("example.invalid"),
                          QStringLiteral("id_test"), privateKey,
                          privateKey + QStringLiteral(".pub")));

  FakeCredentialsOverlay overlay;
  const QString repo = _base + QStringLiteral("/repo");

  const GitRepo::GitSshAuthInfo info = GitRepo::getSshAuthInfo(
      repo, QStringLiteral("git@example.invalid:team/repo.git"));
  QVERIFY(info.valid);
  QCOMPARE(info.privateKeyPath, privateKey);
  QCOMPARE(info.publicKeyPath, privateKey + QStringLiteral(".pub"));
  QCOMPARE(info.username, QStringLiteral("git"));
  QVERIFY(!info.passphrasePresent);

  // ssh:// URLs name the same host.
  for (const QString &url :
       {QStringLiteral("ssh://git@example.invalid/team/repo.git"),
        QStringLiteral("ssh://example.invalid:2222/team/repo.git")}) {
    const GitRepo::GitSshAuthInfo sshInfo = GitRepo::getSshAuthInfo(repo, url);
    QVERIFY2(sshInfo.valid, qPrintable(url));
    QCOMPARE(sshInfo.privateKeyPath, privateKey);
  }
  QCOMPARE(GitRepo::getSshAuthInfo(repo,
                                   QStringLiteral("git@example.invalid:x.git"),
                                   QStringLiteral("deploy"))
               .username,
           QStringLiteral("deploy"));

  // Not an SSH URL, or a missing key file.
  QVERIFY(!GitRepo::getSshAuthInfo(repo, QStringLiteral("https://example.invalid/x"))
               .valid);
  QVERIFY(QFile::remove(privateKey + QStringLiteral(".pub")));
  QVERIFY(!GitRepo::getSshAuthInfo(repo,
                                   QStringLiteral("git@example.invalid:x.git"))
               .valid);
  QVERIFY(overlay.events().isEmpty());
}

void GitCredentialsTest::sshPassphrasePrompt_data() {
  QTest::addColumn<QStringList>("keygenArgs");
  QTest::newRow("PEM RSA") << QStringList{QStringLiteral("-t"),
                                          QStringLiteral("rsa"),
                                          QStringLiteral("-b"),
                                          QStringLiteral("2048"),
                                          QStringLiteral("-m"),
                                          QStringLiteral("PEM")};
  QTest::newRow("OpenSSH ed25519")
      << QStringList{QStringLiteral("-t"), QStringLiteral("ed25519")};
}

void GitCredentialsTest::sshPassphrasePrompt() {
  QFETCH(QStringList, keygenArgs);
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }
  const QString passphrase = QStringLiteral("c0rrect horse!");
  const QString key = makeKey(QStringLiteral("id_protected"), passphrase,
                              keygenArgs);
  QVERIFY(!key.isEmpty());
  QVERIFY(GitKeyStorage::getInstance().setKeyPair(
      QStringLiteral("example.invalid"), QStringLiteral("id_protected"), key,
      key + QStringLiteral(".pub")));

  FakeCredentialsOverlay overlay;
  const QString repo = _base + QStringLiteral("/repo");
  const QString url = QStringLiteral("git@example.invalid:team/repo.git");

  overlay.setAnswers({{QString(), passphrase}});
  const GitRepo::GitSshAuthInfo info = GitRepo::getSshAuthInfo(repo, url);
  QCOMPARE(overlay.events().value(0), QStringLiteral("prompt"));
  QVERIFY(info.valid);
  QVERIFY(info.passphrasePresent);
  QCOMPARE(info.passphrase, passphrase);
  QVERIFY(overlay.requests().value(0).needsPassphrase);
  QCOMPARE(overlay.requests().value(0).repoPath, repo);

  // Cancelled: no usable key (once the remembered passphrase is gone).
  GitRepo::clearCachedCredentials(QString());
  overlay.setAnswers({});
  QVERIFY(!GitRepo::getSshAuthInfo(repo, url).valid);
}

void GitCredentialsTest::sshPassphraseIsCheckedAndRemembered() {
  if (_sshKeygen.isEmpty()) {
    QSKIP("ssh-keygen was not found");
  }
  const QString passphrase = QStringLiteral("c0rrect horse!");
  const QString key = makeKey(
      QStringLiteral("id_protected"), passphrase,
      {QStringLiteral("-t"), QStringLiteral("rsa"), QStringLiteral("-b"),
       QStringLiteral("2048"), QStringLiteral("-m"), QStringLiteral("PEM")});
  QVERIFY(!key.isEmpty());
  QVERIFY(GitKeyStorage::getInstance().setKeyPair(
      QStringLiteral("example.invalid"), QStringLiteral("id_protected"), key,
      key + QStringLiteral(".pub")));
  FakeCredentialsOverlay overlay;
  const QString repo = _base + QStringLiteral("/repo");
  const QString url = QStringLiteral("git@example.invalid:team/repo.git");

  // A wrong passphrase is caught before libssh2 gets it, and asked again.
  overlay.setAnswers({{QString(), QStringLiteral("wrong")}, {QString(), passphrase}});
  const GitRepo::GitSshAuthInfo info = GitRepo::getSshAuthInfo(repo, url);
  QCOMPARE(info.passphrase, passphrase);

  // Once known, it is not asked again for the same key.
  overlay.setAnswers({{QString(), passphrase}});
  GitRepo::getSshAuthInfo(repo, url);
  overlay.setAnswers({});
  const GitRepo::GitSshAuthInfo again = GitRepo::getSshAuthInfo(repo, url);
  QVERIFY(again.valid && overlay.events().isEmpty());
}

QString GitCredentialsTest::sshKeygen(const QStringList &args) const {
  QProcess process;
  process.start(_sshKeygen, args);
  if (!process.waitForFinished(60000) || process.exitCode() != 0) {
    qWarning() << "ssh-keygen" << args << "failed:"
               << process.readAllStandardError();
    return QString();
  }
  const QString output = QString::fromUtf8(process.readAllStandardOutput());
  return output.isNull() ? QStringLiteral("") : output.trimmed();
}

void GitCredentialsTest::defaultKeyFallback_data() {
  // Key files in ~/.ssh, and the one used for a host without a stored key.
  QTest::addColumn<QStringList>("files");
  QTest::addColumn<QString>("expected");
  QTest::addColumn<bool>("valid");
  QTest::newRow("none") << QStringList() << "id_ed25519" << false;
  QTest::newRow("rsa only") << QStringList{"id_rsa", "id_rsa.pub"} << "id_rsa"
                            << true;
  QTest::newRow("ecdsa before rsa")
      << QStringList{"id_rsa", "id_rsa.pub", "id_ecdsa", "id_ecdsa.pub"}
      << "id_ecdsa" << true;
  QTest::newRow("ed25519 first")
      << QStringList{"id_rsa", "id_rsa.pub", "id_ed25519", "id_ed25519.pub"}
      << "id_ed25519" << true;
  QTest::newRow("public key missing")
      << QStringList{"id_ed25519", "id_rsa", "id_rsa.pub"} << "id_rsa" << true;
  QTest::newRow("private key missing")
      << QStringList{"id_ed25519.pub"} << "id_ed25519" << false;
}

void GitCredentialsTest::defaultKeyFallback() {
  QFETCH(QStringList, files);
  QFETCH(QString, expected);
  QFETCH(bool, valid);
  for (const QString &file : files) {
    QVERIFY(writeFile(sshDir() + '/' + file,
                      "-----BEGIN OPENSSH PRIVATE KEY-----\n"));
  }

  const GitRepo::GitSshAuthInfo info = GitRepo::getSshAuthInfo(
      _base + QStringLiteral("/repo"),
      QStringLiteral("git@example.invalid:team/repo.git"));
  QCOMPARE(info.valid, valid);
  QCOMPARE(info.privateKeyPath, sshDir() + '/' + expected);
  QCOMPARE(info.publicKeyPath,
           sshDir() + '/' + expected + QStringLiteral(".pub"));

  // A key stored for the host wins over the defaults.
  const QString stored = _base + QStringLiteral("/keys/id_host");
  QVERIFY(writeFile(stored, "-----BEGIN OPENSSH PRIVATE KEY-----\n"));
  QVERIFY(writeFile(stored + QStringLiteral(".pub"), "ssh-ed25519 AAAA"));
  QVERIFY(GitKeyStorage::getInstance().setKeyPair(
      QStringLiteral("example.invalid"), QStringLiteral("id_host"), stored,
      stored + QStringLiteral(".pub")));
  QCOMPARE(GitRepo::getSshAuthInfo(_base + QStringLiteral("/repo"),
                                   QStringLiteral("git@example.invalid:x.git"))
               .privateKeyPath,
           stored);
}

void GitCredentialsTest::generateKeyPair() {
  auto &keys = GitKeyStorage::getInstance();
  QCOMPARE(GitKeyStorage::getSshDirectoryPath(), sshDir());
  QVERIFY(!keys.keyPairExists(QStringLiteral("id_sourcerer")));

  QString privateKey;
  QString publicKey;
  QString error;
  QVERIFY2(keys.generateKeyPair(QStringLiteral("id_sourcerer"), QString(),
                                privateKey, publicKey, error),
           qPrintable(error));
  QCOMPARE(privateKey, sshDir() + QStringLiteral("/id_sourcerer"));
  QCOMPARE(publicKey, privateKey + QStringLiteral(".pub"));
  QVERIFY(keys.keyPairExists(QStringLiteral("id_sourcerer")));

  const QByteArray privatePem = readFile(privateKey);
  QVERIFY2(privatePem.startsWith("-----BEGIN PRIVATE KEY-----"),
           privatePem.left(40).constData());
  const QList<QByteArray> publicFields = readFile(publicKey).split(' ');
  QCOMPARE(publicFields.size(), 3);
  QCOMPARE(publicFields.at(0), QByteArray("ssh-rsa"));

  if (!_sshKeygen.isEmpty()) {
    // OpenSSH reads both halves, they belong together, and it is RSA-4096.
    const QString fingerprint =
        sshKeygen({QStringLiteral("-l"), QStringLiteral("-f"), publicKey});
    QVERIFY2(fingerprint.startsWith(QStringLiteral("4096 SHA256:")),
             qPrintable(fingerprint));
    const QString derived =
        sshKeygen({QStringLiteral("-y"), QStringLiteral("-P"), QString(),
                   QStringLiteral("-f"), privateKey});
    QVERIFY(!derived.isNull());
    QCOMPARE(derived.split(' ').value(1),
             QString::fromLatin1(publicFields.at(1)));
  }

  // An existing key is never overwritten.
  QString again;
  QString againPublic;
  QVERIFY(!keys.generateKeyPair(QStringLiteral("id_sourcerer"), QString(),
                                again, againPublic, error));
  QVERIFY2(error.contains(QStringLiteral("already exists")), qPrintable(error));
  QCOMPARE(readFile(privateKey), privatePem);
}

void GitCredentialsTest::generateKeyPairWithPassphrase() {
  auto &keys = GitKeyStorage::getInstance();
  const QString passphrase = QString::fromUtf8("gener4ted p\xc3\xa4ss");
  QString privateKey;
  QString publicKey;
  QString error;
  QVERIFY2(keys.generateKeyPair(QStringLiteral("id_protected"), passphrase,
                                privateKey, publicKey, error),
           qPrintable(error));
  QVERIFY(readFile(privateKey)
              .startsWith("-----BEGIN ENCRYPTED PRIVATE KEY-----"));

  if (!_sshKeygen.isEmpty()) {
    QVERIFY(!sshKeygen({QStringLiteral("-y"), QStringLiteral("-P"), passphrase,
                        QStringLiteral("-f"), privateKey})
                 .isNull());
    QVERIFY(sshKeygen({QStringLiteral("-y"), QStringLiteral("-P"),
                       QStringLiteral("wrong"), QStringLiteral("-f"),
                       privateKey})
                .isNull());
  }

  // Used for a host, the passphrase is asked for and checked.
  QVERIFY(keys.setKeyPair(QStringLiteral("example.invalid"),
                          QStringLiteral("id_protected"), privateKey,
                          publicKey));
  FakeCredentialsOverlay overlay;
  overlay.setAnswers(
      {{QString(), QStringLiteral("wrong")}, {QString(), passphrase}});
  const GitRepo::GitSshAuthInfo info = GitRepo::getSshAuthInfo(
      _base + QStringLiteral("/repo"),
      QStringLiteral("git@example.invalid:team/repo.git"));
  QVERIFY(info.valid);
  QVERIFY(info.passphrasePresent);
  QCOMPARE(info.passphrase, passphrase);
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 2);
}

void GitCredentialsTest::generateKeyPairRefusesBadNames_data() {
  QTest::addColumn<QString>("name");
  QTest::newRow("empty") << QString();
  QTest::newRow("blank") << QStringLiteral("  ");
  QTest::newRow("dot") << QStringLiteral(".");
  QTest::newRow("dot dot") << QStringLiteral("..");
  QTest::newRow("parent") << QStringLiteral("../escaped");
  QTest::newRow("subdirectory") << QStringLiteral("keys/id");
  QTest::newRow("backslash") << QStringLiteral("..\\escaped");
  QTest::newRow("drive") << QStringLiteral("c:escaped");
  QTest::newRow("newline") << QStringLiteral("id\nkey");
}

void GitCredentialsTest::generateKeyPairRefusesBadNames() {
  QFETCH(QString, name);
  QString privateKey;
  QString publicKey;
  QString error;
  QVERIFY(!GitKeyStorage::getInstance().generateKeyPair(
      name, QString(), privateKey, publicKey, error));
  QVERIFY2(error.contains(QStringLiteral("Invalid key name")),
           qPrintable(error));
  // Nothing written next to or outside .ssh.
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/home/escaped")));
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/home/escaped.pub")));
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/escaped")));
}

QTEST_GUILESS_MAIN(GitCredentialsTest)

#include "tst_gitcredentials.moc"
