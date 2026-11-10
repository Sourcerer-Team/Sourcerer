// MIT License
// Copyright (c) 2026 Sourcerer

// Credentials and server certificates for SvnRepo: credential prompts over
// svn:// (svnserve with a password database), and the server certificate
// policy, called directly with certificates made here and reached by a
// checkout from a TLS server. See docs/SVN_TEST_PLAN.md section 6;
// QEXPECT_FAIL markers name the suspected defect (section 7) that makes the
// check fail today.

#include "../gitrepo/FakeCredentialsOverlay.h"
#include "SvnServe.h"
#include "SvnTestHelpers.h"
#include "TlsHandshakeServer.h"

#include <GitCredentialStorage.h>

#include <QDir>
#include <QNetworkProxyFactory>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslKey>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <limits>
#include <memory>

using namespace SvnTest;

namespace {

const QString kUser = QStringLiteral("alice");
const QString kPassword = QStringLiteral("s3cret=!");

// Certificates, as in tst_gitcertificates.cpp.
using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;

KeyPtr newKey() { return KeyPtr(EVP_RSA_gen(2048), &EVP_PKEY_free); }

void addExtension(X509 *cert, X509 *issuer, int nid, const char *value) {
  X509V3_CTX ctx;
  X509V3_set_ctx_nodb(&ctx);
  X509V3_set_ctx(&ctx, issuer, cert, nullptr, nullptr, 0);
  X509_EXTENSION *extension = X509V3_EXT_conf_nid(nullptr, &ctx, nid, value);
  if (extension) {
    X509_add_ext(cert, extension, -1);
    X509_EXTENSION_free(extension);
  }
}

struct CertOptions {
  QString commonName = QStringLiteral("127.0.0.1");
  // Issued by this certificate and key, self-signed without them.
  X509 *issuer = nullptr;
  EVP_PKEY *issuerKey = nullptr;
  bool ca = false;
  const char *subjectAltName = "IP:127.0.0.1,DNS:localhost";
  // Validity in days relative to now.
  long notBefore = -1;
  long notAfter = 30;
};

X509Ptr makeCert(EVP_PKEY *key, const CertOptions &options = {}) {
  X509Ptr cert(X509_new(), &X509_free);
  X509_set_version(cert.get(), 2);
  ASN1_INTEGER_set(X509_get_serialNumber(cert.get()),
                   static_cast<long>(QRandomGenerator::global()->bounded(
                       1, std::numeric_limits<int>::max())));
  X509_gmtime_adj(X509_getm_notBefore(cert.get()), options.notBefore * 86400);
  X509_gmtime_adj(X509_getm_notAfter(cert.get()), options.notAfter * 86400);
  X509_set_pubkey(cert.get(), key);
  X509_NAME *name = X509_get_subject_name(cert.get());
  const QByteArray commonName = options.commonName.toUtf8();
  X509_NAME_add_entry_by_txt(
      name, "CN", MBSTRING_UTF8,
      reinterpret_cast<const unsigned char *>(commonName.constData()), -1, -1,
      0);
  X509 *issuer = options.issuer ? options.issuer : cert.get();
  X509_set_issuer_name(cert.get(), X509_get_subject_name(issuer));
  if (options.ca) {
    addExtension(cert.get(), issuer, NID_basic_constraints, "critical,CA:TRUE");
    addExtension(cert.get(), issuer, NID_key_usage,
                 "critical,keyCertSign,cRLSign");
  } else {
    addExtension(cert.get(), issuer, NID_subject_alt_name,
                 options.subjectAltName);
  }
  X509_sign(cert.get(), options.issuerKey ? options.issuerKey : key,
            EVP_sha256());
  return cert;
}

QByteArray toDer(X509 *cert) {
  unsigned char *data = nullptr;
  const int length = i2d_X509(cert, &data);
  QByteArray der(reinterpret_cast<const char *>(data), length);
  OPENSSL_free(data);
  return der;
}

QSslCertificate toQt(X509 *cert) {
  return QSslCertificate(toDer(cert), QSsl::Der);
}

QSslKey toQt(EVP_PKEY *key) {
  BIO *bio = BIO_new(BIO_s_mem());
  PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
  char *data = nullptr;
  const long length = BIO_get_mem_data(bio, &data);
  const QByteArray pem(data, length);
  BIO_free(bio);
  return QSslKey(pem, QSsl::Rsa, QSsl::Pem);
}

// What the trust prompt is given for cert.
SvnRepo::Testing::ServerCertificateInfo infoOf(X509 *cert) {
  SvnRepo::Testing::ServerCertificateInfo info;
  info.hostname = QStringLiteral("127.0.0.1");
  info.asciiCertificate = QString::fromLatin1(toDer(cert).toBase64());
  return info;
}

bool pinnedFor(const QString &host) {
  for (const GitRepo::PinnedCertificate &pin :
       GitRepo::getPinnedCertificates()) {
    if (pin.host == host) {
      return true;
    }
  }
  return false;
}

// SVN_AUTH_SSL_* (svn_auth.h).
constexpr quint32 kNotYetValid = 0x00000001;
constexpr quint32 kExpired = 0x00000002;
constexpr quint32 kCnMismatch = 0x00000004;
constexpr quint32 kUnknownCa = 0x00000008;
constexpr quint32 kOther = 0x40000000;

} // namespace

class SvnAuthTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();
  void cleanupTestCase();

  // Credentials over svn://.
  void rightCredentials();
  void serverGoneIsAnError();
  void wrongThenRight();
  void wrongStoredCredentialsPromptAgain();
  void cancel();
  void storedCredentialsSkipPrompt();
  void expiredStoredCredentials();
  void credentialsKeyedByPathSpelling_data();
  void credentialsKeyedByPathSpelling();
  void userConfigStorePasswordsIsIgnored();

  // Server certificates.
  void trustPolicy_data();
  void trustPolicy();
  void changedPinnedCertificate();
  void httpsCheckoutHandshake_data();
  void httpsCheckoutHandshake();

private:
  // Clones trunk into _base/<name> with the API under test.
  bool clone(const QString &name);

  std::unique_ptr<QTemporaryDir> _tmp;
  std::unique_ptr<SvnServe> _server;
  QString _base;
};

void SvnAuthTest::initTestCase() {
  if (!toolsAvailable()) {
    QSKIP("svn, svnadmin or svnlook was not found");
  }
  QStandardPaths::setTestModeEnabled(true);
  useTestCredentialsKey();
  QNetworkProxyFactory::setUseSystemConfiguration(false);
  QVERIFY(SvnRepo::init());
  SvnRepo::initForCurrentThread();
  SvnRepo::Testing::installMalfunctionHandler();
}

void SvnAuthTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  setConfigDir(_base + QStringLiteral("/svnconfig"));
  GitRepo::GitCredentialStorage::getInstance().clearAll();
  GitRepo::Testing::resetPinnedCertificates();

  // The certificate tests don't need a server.
  const QByteArray test = QTest::currentTestFunction();
  if (test == "trustPolicy" || test == "changedPinnedCertificate" ||
      test == "httpsCheckoutHandshake") {
    return;
  }
  if (!SvnServe::available()) {
    QSKIP("svnserve was not found");
  }
  QVERIFY(QDir().mkpath(_base + QStringLiteral("/served")));
  _server = std::make_unique<SvnServe>(_base + QStringLiteral("/served"));
  QVERIFY(_server->start(kUser, kPassword));
}

void SvnAuthTest::cleanup() {
  _server.reset();
  GitRepo::GitCredentialStorage::getInstance().clearAll();
  GitRepo::Testing::resetPinnedCertificates();
  if (_tmp) {
    makeWritable(_tmp->path());
  }
  _tmp.reset();
  const QStringList malfunctions = SvnRepo::Testing::takeMalfunctions();
  QVERIFY2(malfunctions.isEmpty(), qPrintable(malfunctions.join('\n')));
}

void SvnAuthTest::cleanupTestCase() {
  SvnRepo::Testing::setConfigDirectory(QString());
}

bool SvnAuthTest::clone(const QString &name) {
  const QString dest = _base + QLatin1Char('/') + name;
  const bool cloned = SvnRepo::cloneRepository(
      _server->url(QStringLiteral("/trunk")), dest, QStringLiteral("clone"));
  SvnRepo::closeRepository(dest);
  return cloned;
}

// --- Credentials over svn:// ------------------------------------------------

void SvnAuthTest::rightCredentials() {
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, kPassword}, {kUser, kPassword}});

  QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));
  QVERIFY(QFileInfo(_base + QStringLiteral("/clone/.svn")).isDir());
  QTRY_COMPARE(overlay.events(),
               (QStringList{QStringLiteral("prompt"),
                            QStringLiteral("finished")}));
  const GitRepo::GitCredentialsRequest request = overlay.requests().value(0);
  QCOMPARE(request.repoPath, _base + QStringLiteral("/clone"));
  // Every prompt is finished exactly once.
  QCOMPARE(overlay.finishedIds(), QVector<size_t>{request.id});

  // A commit in the clone is made as the authenticated user.
  const QString wc = _base + QStringLiteral("/clone");
  QVERIFY(writeFile(wc + QStringLiteral("/a.txt"), "a\n"));
  QVERIFY2(commitAll(wc, QStringLiteral("Add a")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(svnlookAuthor(_server->repoDir(), head(_server->repoDir())), kUser);
}

void SvnAuthTest::serverGoneIsAnError() {
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, kPassword}, {kUser, kPassword}});
  QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));
  const QString wc = _base + QStringLiteral("/clone");
  const QString revision = head(_server->repoDir());
  _server->stop();

  // B25h (fixed): a server that can't be reached is an error, not a
  // repository without branches.
  const bool quit = false;
  resetLastError();
  QVERIFY(SvnRepo::getCommitHeads(wc, revision, quit).isEmpty());
  QVERIFY2(lastError() != SvnRepo::SvnError::Unknown,
           "an unreachable server isn't reported");
  resetLastError();
  QVERIFY(SvnRepo::listRemoteBranches(wc, quit).isEmpty());
  QVERIFY2(lastError() != SvnRepo::SvnError::Unknown,
           "an unreachable server isn't reported");
}

void SvnAuthTest::wrongThenRight() {
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, QStringLiteral("wrong")}, {kUser, kPassword}});

  QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));
  // Asked again after the wrong answer, like the Git backend does.
  QTRY_COMPARE(overlay.events().count(QStringLiteral("prompt")), 2);
  QCOMPARE(overlay.events(),
           (QStringList{QStringLiteral("prompt"), QStringLiteral("invalid"),
                        QStringLiteral("prompt"), QStringLiteral("finished")}));
}

void SvnAuthTest::wrongStoredCredentialsPromptAgain() {
  // Stored credentials that the server refuses lead to a prompt.
  const QString dest = _base + QStringLiteral("/clone");
  GitRepo::GitCredentialStorage::getInstance().storeCredentials(
      dest, kUser, QStringLiteral("wrong"), false);
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, kPassword}});

  resetLastError();
  const bool cloned = clone(QStringLiteral("clone"));
  QVERIFY2(cloned, qPrintable(lastErrorMessage()));
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 1);
}

void SvnAuthTest::cancel() {
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({});

  resetLastError();
  QVERIFY(!clone(QStringLiteral("cancelled")));
  QVERIFY(!lastErrorMessage().isEmpty());
  QCOMPARE(lastError(), SvnRepo::SvnError::Cancelled);
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 1);
  // Nothing is left behind.
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/cancelled/.svn")));
}

void SvnAuthTest::storedCredentialsSkipPrompt() {
  GitRepo::GitCredentialStorage::getInstance().storeCredentials(
      _base + QStringLiteral("/clone"), kUser, kPassword, false);
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({});

  QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));
  QVERIFY(overlay.requests().isEmpty());
}

void SvnAuthTest::expiredStoredCredentials() {
  GitRepo::GitCredentialStorage::getInstance().storeCredentials(
      _base + QStringLiteral("/clone"), kUser, kPassword, false);
  const ConfigOverride expiry(
      ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime, 1);
  QThread::msleep(20);
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, kPassword}});

  // Expired credentials are asked for again, as in the Git backend.
  QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));
  QCOMPARE(overlay.events().count(QStringLiteral("prompt")), 1);
}

void SvnAuthTest::credentialsKeyedByPathSpelling_data() {
  QTest::addColumn<QString>("spelling");
  QTest::newRow("same") << QStringLiteral("same");
  QTest::newRow("native") << QStringLiteral("native");
  QTest::newRow("trailing slash") << QStringLiteral("trailing");
#ifdef Q_OS_WIN
  QTest::newRow("lowercase drive") << QStringLiteral("lower");
#endif
}

void SvnAuthTest::credentialsKeyedByPathSpelling() {
  QFETCH(QString, spelling);

  // Credentials stored for the working copy are found whatever the spelling
  // of its path in the request.
  const QString wc = _base + QStringLiteral("/clone");
  GitRepo::GitCredentialStorage::getInstance().storeCredentials(
      wc, kUser, kPassword, false);
  {
    FakeCredentialsOverlay overlay;
    overlay.setAnswers({});
    QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));
  }
  QString path = wc;
  if (spelling == QLatin1String("native")) {
    path = QDir::toNativeSeparators(wc);
  } else if (spelling == QLatin1String("trailing")) {
    path = wc + QLatin1Char('/');
  } else if (spelling == QLatin1String("lower")) {
    path[0] = path[0].toLower();
  }

  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, kPassword}});
  QVERIFY2(SvnRepo::pull(path, QStringLiteral("trunk"), false),
           qPrintable(lastErrorMessage()));
  QVERIFY2(overlay.requests().isEmpty(),
           qPrintable(overlay.requests().value(0).repoPath));
}

void SvnAuthTest::userConfigStorePasswordsIsIgnored() {
  FakeCredentialsOverlay overlay;
  overlay.setAnswers({{kUser, kPassword}});
  QVERIFY2(clone(QStringLiteral("clone")), qPrintable(lastErrorMessage()));

  // Credentials live in GitCredentialStorage only, never in Subversion's own
  // auth area (plain text on most systems).
  const QDir simple(configDir() + QStringLiteral("/auth/svn.simple"));
  QVERIFY2(!simple.exists() || simple.isEmpty(),
           qPrintable(simple.entryList(QDir::Files).join(", ")));
}

// --- Server certificates ----------------------------------------------------

void SvnAuthTest::trustPolicy_data() {
  QTest::addColumn<QString>("kind");
  QTest::addColumn<quint32>("failures");
  QTest::addColumn<bool>("trusted");
  QTest::newRow("chain the system trusts") << QStringLiteral("self-signed")
                                           << quint32(0) << true;
  QTest::newRow("self-signed") << QStringLiteral("self-signed") << kUnknownCa
                               << true;
  QTest::newRow("unknown CA") << QStringLiteral("issued") << kUnknownCa
                              << false;
  QTest::newRow("another host")
      << QStringLiteral("self-signed") << (kUnknownCa | kCnMismatch) << false;
  QTest::newRow("expired") << QStringLiteral("self-signed")
                           << (kUnknownCa | kExpired) << false;
  QTest::newRow("not yet valid")
      << QStringLiteral("self-signed") << (kUnknownCa | kNotYetValid) << false;
  QTest::newRow("other failure")
      << QStringLiteral("self-signed") << (kUnknownCa | kOther) << false;
}

void SvnAuthTest::trustPolicy() {
  QFETCH(QString, kind);
  QFETCH(quint32, failures);
  QFETCH(bool, trusted);

  KeyPtr key = newKey();
  KeyPtr caKey = newKey();
  CertOptions caOptions;
  caOptions.commonName = QStringLiteral("Test CA");
  caOptions.ca = true;
  X509Ptr ca = makeCert(caKey.get(), caOptions);
  CertOptions options;
  if (kind == QLatin1String("issued")) {
    options.issuer = ca.get();
    options.issuerKey = caKey.get();
  }
  // For a server addressed by IP, serf reports every certificate as one for
  // another host (B67), so the certificate must really name another host.
  if (QByteArray(QTest::currentDataTag()) == "another host") {
    options.commonName = QStringLiteral("other.example");
    options.subjectAltName = "DNS:other.example";
  }
  X509Ptr cert = makeCert(key.get(), options);

  quint32 accepted = 0xffffffff;
  resetLastError();
  const bool result = SvnRepo::Testing::sslServerTrustCheck(
      failures, infoOf(cert.get()), QStringLiteral("https://127.0.0.1:8443"),
      &accepted);
  QCOMPARE(result, trusted);
  if (trusted) {
    QCOMPARE(accepted, failures);
    // A self-signed certificate is pinned when it is trusted on first use.
    QCOMPARE(pinnedFor(QStringLiteral("127.0.0.1")), failures != 0);
  } else {
    QVERIFY2(lastErrorMessage().contains(QStringLiteral("127.0.0.1")),
             qPrintable(lastErrorMessage()));
    QVERIFY(!pinnedFor(QStringLiteral("127.0.0.1")));
  }
}

void SvnAuthTest::changedPinnedCertificate() {
  KeyPtr key = newKey();
  X509Ptr first = makeCert(key.get());
  X509Ptr second = makeCert(key.get());
  const QString realm = QStringLiteral("https://127.0.0.1:8443");

  // Trusted on first use, then again; a different certificate for the same
  // host is refused, the same as for Git.
  QVERIFY(SvnRepo::Testing::sslServerTrustCheck(kUnknownCa,
                                                infoOf(first.get()), realm));
  QVERIFY(SvnRepo::Testing::sslServerTrustCheck(kUnknownCa,
                                                infoOf(first.get()), realm));
  resetLastError();
  QVERIFY(!SvnRepo::Testing::sslServerTrustCheck(kUnknownCa,
                                                 infoOf(second.get()), realm));
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("not the one trusted")),
           qPrintable(lastErrorMessage()));

  // Trusted after the user accepts the new one.
  QVERIFY(GitRepo::trustRejectedCertificate(GitRepo::PinnedCertificateType::Tls,
                                            QStringLiteral("127.0.0.1")));
  QVERIFY(SvnRepo::Testing::sslServerTrustCheck(kUnknownCa,
                                                infoOf(second.get()), realm));
}

void SvnAuthTest::httpsCheckoutHandshake_data() {
  QTest::addColumn<QString>("host");
  QTest::addColumn<bool>("expired");
  QTest::newRow("self-signed, localhost") << QStringLiteral("localhost")
                                          << false;
  QTest::newRow("self-signed, IP address") << QStringLiteral("127.0.0.1")
                                           << false;
  QTest::newRow("expired") << QStringLiteral("localhost") << true;
}

void SvnAuthTest::httpsCheckoutHandshake() {
  QFETCH(QString, host);
  QFETCH(bool, expired);

  KeyPtr key = newKey();
  CertOptions options;
  if (expired) {
    options.notBefore = -30;
    options.notAfter = -1;
  }
  X509Ptr cert = makeCert(key.get(), options);
  TlsHandshakeServer server(toQt(cert.get()), toQt(key.get()));
  QVERIFY(server.startServer());

  // The checkout reaches the certificate check. The server is no Subversion
  // server, so it fails either way; the assert is on the trust decision.
  resetLastError();
  const QString url =
      QStringLiteral("https://%1:%2/svn/repo").arg(host).arg(server.port());
  QVERIFY(!SvnRepo::cloneRepository(url, _base + QStringLiteral("/clone")));
  QVERIFY(!lastErrorMessage().isEmpty());
  if (expired) {
    QVERIFY2(lastErrorMessage().contains(QStringLiteral("has expired")),
             qPrintable(lastErrorMessage()));
    QVERIFY(!pinnedFor(host));
  } else {
    // The certificate names both localhost and 127.0.0.1.
    QVERIFY2(pinnedFor(host), qPrintable(lastErrorMessage()));
    QVERIFY2(server.handshakes() > 0, qPrintable(lastErrorMessage()));
  }
  QVERIFY(!QFileInfo::exists(_base + QStringLiteral("/clone/.svn")));
}

QTEST_GUILESS_MAIN(SvnAuthTest)

#include "tst_svnauth.moc"
