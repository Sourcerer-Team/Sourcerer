// Tests for the server certificate policy of lib/GitRepo
// (GitRepoCertificates.cpp): certificates libgit2 trusts are accepted, a
// self-signed TLS certificate or an unknown SSH host key is trusted on first
// use and pinned per host, anything else is rejected. The first part calls
// the check directly with certificates made here, the second clones over
// HTTPS from GitHttpServer. See docs/GIT_TEST_PLAN.md, section 6 item 5.

#include "GitHttpServer.h"
#include "GitTestHelpers.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QNetworkProxyFactory>
#include <QRandomGenerator>
#include <QSslCertificate>
#include <QSslKey>
#include <QSslSocket>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUuid>
#include <QtTest>

#include <git2/sys/errors.h>

#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>

#include <memory>

using namespace GitTest;

namespace {

using KeyPtr = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>;
using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;

QString newUuid() { return QUuid::createUuid().toString(QUuid::WithoutBraces); }

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
  // subjectAltName of a server certificate.
  const char *subjectAltName = "IP:127.0.0.1,DNS:localhost";
  // keyUsage of a server certificate, none if empty.
  const char *keyUsage = nullptr;
  // Validity in days relative to now.
  long notBefore = -1;
  long notAfter = 30;
};

// A certificate for key, valid for 127.0.0.1 and localhost.
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
    if (options.keyUsage) {
      addExtension(cert.get(), issuer, NID_key_usage, options.keyUsage);
    }
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

QByteArray bioData(BIO *bio) {
  char *data = nullptr;
  const long length = BIO_get_mem_data(bio, &data);
  return QByteArray(data, length);
}

QSslCertificate toQt(X509 *cert) {
  return QSslCertificate(toDer(cert), QSsl::Der);
}

QSslKey toQt(EVP_PKEY *key) {
  BIO *bio = BIO_new(BIO_s_mem());
  PEM_write_bio_PrivateKey(bio, key, nullptr, nullptr, 0, nullptr, nullptr);
  const QByteArray pem = bioData(bio);
  BIO_free(bio);
  return QSslKey(pem, QSsl::Rsa, QSsl::Pem);
}

// Calls the certificate check with this DER certificate.
int checkX509(const QByteArray &der, const char *host, int valid = 0) {
  git_cert_x509 cert{};
  cert.parent.cert_type = GIT_CERT_X509;
  cert.data = const_cast<char *>(der.constData());
  cert.len = static_cast<size_t>(der.size());
  return GitRepo::Testing::checkCertificate(&cert.parent, valid, host);
}

// Calls the certificate check with an SSH host key whose SHA-256 is that of
// key, or (sha256 false) with only the raw key.
int checkHostKey(const QByteArray &key, const char *host, bool sha256 = true,
                 int valid = 0) {
  git_cert_hostkey cert{};
  cert.parent.cert_type = GIT_CERT_HOSTKEY_LIBSSH2;
  if (sha256) {
    cert.type = GIT_CERT_SSH_SHA256;
    const QByteArray hash =
        QCryptographicHash::hash(key, QCryptographicHash::Sha256);
    memcpy(cert.hash_sha256, hash.constData(), sizeof(cert.hash_sha256));
  } else {
    cert.type = GIT_CERT_SSH_RAW;
    cert.raw_type = GIT_CERT_SSH_RAW_TYPE_KEY_ED25519;
    cert.hostkey = key.constData();
    cert.hostkey_len = static_cast<size_t>(key.size());
  }
  return GitRepo::Testing::checkCertificate(&cert.parent, valid, host);
}

QString libgit2Error() {
  const git_error *error = git_error_last();
  return error && error->message ? QString::fromUtf8(error->message)
                                 : QString();
}

QString pinsFile() {
  return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) +
         QStringLiteral("/git_pinned_certificates.txt");
}

} // namespace

class GitCertificatesTest : public QObject {
  Q_OBJECT

private slots:
  void initTestCase();
  void init();
  void cleanup();

  // The check itself.
  void systemRootsAreLoaded();
  void validCertificateIsAccepted();
  void selfSignedIsTrustedOnFirstUse();
  void selfSignedServerKeyUsage();
  void pinIsPerHostIgnoringCase();
  void changedSelfSignedIsRejected();
  void pinsArePersisted();
  void oldPinsFileIsRead();
  void pinnedCertificatesAreListed();
  void rejectedCertificateCanBeTrusted();
  void forgetOnePinnedCertificate();
  void forgetTrustsTheNextCertificate();
  void expiredSelfSignedIsTrustedOnFirstUse();
  void untrustedIssuerIsRejected();
  void otherHostIsRejected();
  void unreadableCertificateIsRejected();
  void sshHostKeyIsTrustedOnFirstUse();
  void sshHostKeyFromRawKey();
  void tlsAndSshPinsAreSeparate();
  void otherCertificatesAreRejected();

  // Clones over HTTPS.
  void httpsCloneSelfSigned();
  void httpsCloneChangedCertificateFails();
  void httpsCloneUntrustedIssuerFails();
  void httpsCloneTrustedChain();
  void httpsCloneTrustedChainOtherAddressFails();

private:
  // A bare repository with one commit, served as remote.git.
  void makeRemote();
  // Serves the remote over HTTPS with this certificate.
  std::unique_ptr<GitHttpServer> serve(X509 *cert, EVP_PKEY *key);

  std::unique_ptr<QTemporaryDir> _tmp;
  QString _base;
  IsolatedConfig _config;
  bool _haveGit = false;
};

void GitCertificatesTest::initTestCase() {
  QStandardPaths::setTestModeEnabled(true);
  QNetworkProxyFactory::setUseSystemConfiguration(false);
  _haveGit = !QStandardPaths::findExecutable(QStringLiteral("git")).isEmpty();
  GitRepo::initForCurrentThread();
}

void GitCertificatesTest::init() {
  _tmp = std::make_unique<QTemporaryDir>();
  QVERIFY(_tmp->isValid());
  _base = QDir::cleanPath(QDir::fromNativeSeparators(_tmp->path()));
  _config.setUp(_base + QStringLiteral("/home"));
  GitRepo::Testing::resetPinnedCertificates();
  git_error_clear();
}

void GitCertificatesTest::cleanup() {
  GitRepo::Testing::resetPinnedCertificates();
  makeWritable(_base);
  _tmp.reset();
}

void GitCertificatesTest::makeRemote() {
  const QString seed = _base + QStringLiteral("/seed");
  QVERIFY(QDir().mkpath(seed));
  gitIn(_config, seed, {QStringLiteral("init"), QStringLiteral("-q")});
  QVERIFY(writeFile(seed + QStringLiteral("/a.txt"), "a\n"));
  gitIn(_config, seed, {QStringLiteral("add"), QStringLiteral("-A")});
  gitIn(_config, seed,
        {QStringLiteral("commit"), QStringLiteral("-qm"), QStringLiteral("A")});
  bool ok = false;
  gitIn(_config, _base,
        {QStringLiteral("clone"), QStringLiteral("-q"), QStringLiteral("--bare"),
         seed, _base + QStringLiteral("/served/remote.git")},
        &ok);
  QVERIFY(ok);
}

std::unique_ptr<GitHttpServer> GitCertificatesTest::serve(X509 *cert,
                                                         EVP_PKEY *key) {
  auto server = std::make_unique<GitHttpServer>(
      _base + QStringLiteral("/served"), QString(), QString(),
      _config.environment());
  server->useTls(toQt(cert), toQt(key));
  if (!server->start()) {
    return nullptr;
  }
  return server;
}

void GitCertificatesTest::systemRootsAreLoaded() {
  // libgit2's OpenSSL knows no roots on Windows without them.
  QVERIFY(GitRepo::Testing::systemRootCertificateCount() > 0);
}

void GitCertificatesTest::validCertificateIsAccepted() {
  KeyPtr key = newKey();
  X509Ptr cert = makeCert(key.get());
  QCOMPARE(checkX509(toDer(cert.get()), "example.invalid", 1), 0);
  // Nothing is pinned for certificates libgit2 trusts.
  QVERIFY(!QFile::exists(pinsFile()));
  // Also for something that isn't a certificate: libgit2 has checked it.
  QCOMPARE(checkX509("garbage", "example.invalid", 1), 0);
}

void GitCertificatesTest::selfSignedIsTrustedOnFirstUse() {
  KeyPtr key = newKey();
  const QByteArray der = toDer(makeCert(key.get()).get());
  QCOMPARE(checkX509(der, "self.invalid"), 0);
  QCOMPARE(checkX509(der, "self.invalid"), 0);
}

void GitCertificatesTest::selfSignedServerKeyUsage() {
  // Without keyCertSign, as many server certificates are.
  KeyPtr key = newKey();
  CertOptions options;
  options.keyUsage = "critical,digitalSignature,keyEncipherment";
  QCOMPARE(checkX509(toDer(makeCert(key.get(), options).get()), "usage.invalid"),
           0);
}

void GitCertificatesTest::pinIsPerHostIgnoringCase() {
  KeyPtr key = newKey();
  KeyPtr otherKey = newKey();
  const QByteArray first = toDer(makeCert(key.get()).get());
  const QByteArray second = toDer(makeCert(otherKey.get()).get());

  QCOMPARE(checkX509(first, "one.invalid"), 0);
  // Another host pins its own certificate.
  QCOMPARE(checkX509(second, "two.invalid"), 0);
  // Host names are case insensitive.
  QCOMPARE(checkX509(first, "ONE.invalid"), 0);
  QCOMPARE(checkX509(second, "One.Invalid"), GIT_ECERTIFICATE);
}

void GitCertificatesTest::changedSelfSignedIsRejected() {
  KeyPtr key = newKey();
  QCOMPARE(checkX509(toDer(makeCert(key.get()).get()), "self.invalid"), 0);

  // Same key, new certificate: still a different certificate.
  const QByteArray renewed = toDer(makeCert(key.get()).get());
  git_error_clear();
  QCOMPARE(checkX509(renewed, "self.invalid"), GIT_ECERTIFICATE);
  QVERIFY2(libgit2Error().contains(QStringLiteral("not the one trusted")),
           qPrintable(libgit2Error()));
  QVERIFY(libgit2Error().contains(QStringLiteral("self.invalid")));
}

void GitCertificatesTest::pinsArePersisted() {
  KeyPtr key = newKey();
  const QByteArray der = toDer(makeCert(key.get()).get());
  QCOMPARE(checkX509(der, "Saved.Invalid"), 0);

  QFile file(pinsFile());
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QList<QByteArray> fields = file.readAll().trimmed().split(' ');
  // Closed: an open file can't be replaced on Windows.
  file.close();
  QCOMPARE(fields.size(), 4);
  QCOMPARE(fields.at(0), QByteArray("tls"));
  QCOMPARE(fields.at(1), QByteArray("saved.invalid"));
  QCOMPARE(fields.at(2),
           QCryptographicHash::hash(der, QCryptographicHash::Sha256).toHex());
  const qint64 trusted = fields.at(3).toLongLong();
  QVERIFY(qAbs(QDateTime::currentMSecsSinceEpoch() - trusted) < 60000);

  // A refused replacement is saved too.
  KeyPtr otherKey = newKey();
  const QByteArray other = toDer(makeCert(otherKey.get()).get());
  QCOMPARE(checkX509(other, "saved.invalid"), GIT_ECERTIFICATE);
  QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
  const QList<QByteArray> withRejected = file.readAll().trimmed().split(' ');
  QCOMPARE(withRejected.size(), 6);
  QCOMPARE(withRejected.at(4),
           QCryptographicHash::hash(other, QCryptographicHash::Sha256).toHex());
}

void GitCertificatesTest::oldPinsFileIsRead() {
  // Files written before the dates were added have three fields.
  const QByteArray fingerprint(32, '\x11');
  QVERIFY(QDir().mkpath(QFileInfo(pinsFile()).absolutePath()));
  QVERIFY(writeFile(pinsFile(), "tls old.invalid " + fingerprint.toHex() +
                                    "\nnot a pin\n\nssh old.invalid " +
                                    fingerprint.toHex() + " 1000\n"));
  const QVector<GitRepo::PinnedCertificate> pins =
      GitRepo::getPinnedCertificates();
  QCOMPARE(pins.size(), 2);
  QCOMPARE(pins.at(0).type, GitRepo::PinnedCertificateType::Tls);
  QCOMPARE(pins.at(0).trustedSince, qint64(0));
  QCOMPARE(pins.at(1).type, GitRepo::PinnedCertificateType::Ssh);
  QCOMPARE(pins.at(1).trustedSince, qint64(1000));
  QVERIFY(pins.at(1).rejectedFingerprint.isEmpty());
}

void GitCertificatesTest::pinnedCertificatesAreListed() {
  QVERIFY(GitRepo::getPinnedCertificates().isEmpty());
  KeyPtr key = newKey();
  const QByteArray der = toDer(makeCert(key.get()).get());
  QCOMPARE(checkX509(der, "B.invalid"), 0);
  const QByteArray hostKey = "host key";
  QCOMPARE(checkHostKey(hostKey, "a.invalid"), 0);
  QCOMPARE(checkHostKey(hostKey, "b.invalid"), 0);

  const QVector<GitRepo::PinnedCertificate> pins =
      GitRepo::getPinnedCertificates();
  QCOMPARE(pins.size(), 3);
  // By host, TLS before SSH; host names in lower case.
  QCOMPARE(pins.at(0).host, QStringLiteral("a.invalid"));
  QCOMPARE(pins.at(1).host, QStringLiteral("b.invalid"));
  QCOMPARE(pins.at(1).type, GitRepo::PinnedCertificateType::Tls);
  QCOMPARE(pins.at(2).type, GitRepo::PinnedCertificateType::Ssh);

  // Fingerprints as OpenSSL and OpenSSH show them.
  const QByteArray tlsHash =
      QCryptographicHash::hash(der, QCryptographicHash::Sha256);
  QCOMPARE(pins.at(1).fingerprint,
           QString::fromLatin1(tlsHash.toHex(':')).toUpper());
  const QByteArray sshHash =
      QCryptographicHash::hash(hostKey, QCryptographicHash::Sha256);
  QCOMPARE(pins.at(0).fingerprint,
           QStringLiteral("SHA256:") +
               QString::fromLatin1(
                   sshHash.toBase64(QByteArray::OmitTrailingEquals)));
  for (const GitRepo::PinnedCertificate &pin : pins) {
    QVERIFY(qAbs(QDateTime::currentMSecsSinceEpoch() - pin.trustedSince) <
            60000);
    QVERIFY(pin.rejectedFingerprint.isEmpty());
    QCOMPARE(pin.rejectedAt, qint64(0));
  }
}

void GitCertificatesTest::rejectedCertificateCanBeTrusted() {
  KeyPtr key = newKey();
  KeyPtr newerKey = newKey();
  const QByteArray old = toDer(makeCert(key.get()).get());
  const QByteArray replaced = toDer(makeCert(newerKey.get()).get());
  QCOMPARE(checkX509(old, "self.invalid"), 0);
  // Nothing refused yet.
  QVERIFY(!GitRepo::trustRejectedCertificate(
      GitRepo::PinnedCertificateType::Tls, QStringLiteral("self.invalid")));

  QCOMPARE(checkX509(replaced, "self.invalid"), GIT_ECERTIFICATE);
  QVector<GitRepo::PinnedCertificate> pins = GitRepo::getPinnedCertificates();
  QCOMPARE(pins.size(), 1);
  const QString replacedFingerprint = QString::fromLatin1(
      QCryptographicHash::hash(replaced, QCryptographicHash::Sha256)
          .toHex(':')).toUpper();
  QCOMPARE(pins.at(0).rejectedFingerprint, replacedFingerprint);
  QVERIFY(pins.at(0).rejectedAt >= pins.at(0).trustedSince);

  // Not for the other kind or another host.
  QVERIFY(!GitRepo::trustRejectedCertificate(
      GitRepo::PinnedCertificateType::Ssh, QStringLiteral("self.invalid")));
  QVERIFY(!GitRepo::trustRejectedCertificate(
      GitRepo::PinnedCertificateType::Tls, QStringLiteral("other.invalid")));

  QVERIFY(GitRepo::trustRejectedCertificate(
      GitRepo::PinnedCertificateType::Tls, QStringLiteral("SELF.invalid")));
  pins = GitRepo::getPinnedCertificates();
  QCOMPARE(pins.at(0).fingerprint, replacedFingerprint);
  QVERIFY(pins.at(0).rejectedFingerprint.isEmpty());
  QCOMPARE(checkX509(replaced, "self.invalid"), 0);
  QCOMPARE(checkX509(old, "self.invalid"), GIT_ECERTIFICATE);

  // The same for SSH host keys.
  QCOMPARE(checkHostKey("old key", "ssh.invalid"), 0);
  QCOMPARE(checkHostKey("new key", "ssh.invalid"), GIT_ECERTIFICATE);
  QVERIFY(GitRepo::trustRejectedCertificate(
      GitRepo::PinnedCertificateType::Ssh, QStringLiteral("ssh.invalid")));
  QCOMPARE(checkHostKey("new key", "ssh.invalid"), 0);
}

void GitCertificatesTest::forgetOnePinnedCertificate() {
  KeyPtr key = newKey();
  QCOMPARE(checkX509(toDer(makeCert(key.get()).get()), "both.invalid"), 0);
  QCOMPARE(checkHostKey("host key", "both.invalid"), 0);

  QVERIFY(GitRepo::forgetPinnedCertificate(GitRepo::PinnedCertificateType::Tls,
                                           QStringLiteral("Both.invalid")));
  QVERIFY(!GitRepo::forgetPinnedCertificate(
      GitRepo::PinnedCertificateType::Tls, QStringLiteral("both.invalid")));
  const QVector<GitRepo::PinnedCertificate> pins =
      GitRepo::getPinnedCertificates();
  QCOMPARE(pins.size(), 1);
  QCOMPARE(pins.at(0).type, GitRepo::PinnedCertificateType::Ssh);
  // The SSH pin is still checked.
  QCOMPARE(checkHostKey("other key", "both.invalid"), GIT_ECERTIFICATE);
}

void GitCertificatesTest::forgetTrustsTheNextCertificate() {
  KeyPtr key = newKey();
  KeyPtr newerKey = newKey();
  const QByteArray old = toDer(makeCert(key.get()).get());
  const QByteArray replaced = toDer(makeCert(newerKey.get()).get());
  QCOMPARE(checkX509(old, "self.invalid"), 0);
  QCOMPARE(checkX509(replaced, "self.invalid"), GIT_ECERTIFICATE);

  // Forgetting another host changes nothing.
  GitRepo::forgetPinnedCertificates(QStringLiteral("other.invalid"));
  QCOMPARE(checkX509(replaced, "self.invalid"), GIT_ECERTIFICATE);

  GitRepo::forgetPinnedCertificates(QStringLiteral("SELF.invalid"));
  QCOMPARE(checkX509(replaced, "self.invalid"), 0);
  QCOMPARE(checkX509(old, "self.invalid"), GIT_ECERTIFICATE);
}

void GitCertificatesTest::expiredSelfSignedIsTrustedOnFirstUse() {
  // A self-signed certificate is trusted by its pin, not its dates.
  KeyPtr key = newKey();
  CertOptions expired;
  expired.notBefore = -60;
  expired.notAfter = -30;
  QCOMPARE(checkX509(toDer(makeCert(key.get(), expired).get()), "old.invalid"),
           0);
}

void GitCertificatesTest::untrustedIssuerIsRejected() {
  KeyPtr caKey = newKey();
  CertOptions caOptions;
  caOptions.commonName = QStringLiteral("Untrusted Test CA");
  caOptions.ca = true;
  X509Ptr ca = makeCert(caKey.get(), caOptions);

  KeyPtr key = newKey();
  CertOptions leaf;
  leaf.issuer = ca.get();
  leaf.issuerKey = caKey.get();
  const QByteArray der = toDer(makeCert(key.get(), leaf).get());

  QCOMPARE(checkX509(der, "127.0.0.1"), GIT_ECERTIFICATE);
  QVERIFY2(libgit2Error().contains(QStringLiteral("is not trusted")),
           qPrintable(libgit2Error()));
  // Rejected every time, nothing is pinned.
  QCOMPARE(checkX509(der, "127.0.0.1"), GIT_ECERTIFICATE);
  QVERIFY(!QFile::exists(pinsFile()));

  // The CA certificate itself is self-signed, so it is trusted on first use.
  QCOMPARE(checkX509(toDer(ca.get()), "ca.invalid"), 0);
}

void GitCertificatesTest::otherHostIsRejected() {
  KeyPtr caKey = newKey();
  CertOptions caOptions;
  caOptions.commonName = QStringLiteral("Untrusted Test CA");
  caOptions.ca = true;
  X509Ptr ca = makeCert(caKey.get(), caOptions);
  KeyPtr key = newKey();
  CertOptions leaf;
  leaf.issuer = ca.get();
  leaf.issuerKey = caKey.get();
  leaf.subjectAltName = "IP:10.0.0.1,DNS:other.invalid";
  const QByteArray der = toDer(makeCert(key.get(), leaf).get());

  for (const char *host : {"127.0.0.1", "example.invalid"}) {
    git_error_clear();
    QCOMPARE(checkX509(der, host), GIT_ECERTIFICATE);
    QVERIFY2(libgit2Error().contains(QStringLiteral("another host")),
             qPrintable(libgit2Error()));
  }
}

void GitCertificatesTest::unreadableCertificateIsRejected() {
  QCOMPARE(checkX509("garbage", "bad.invalid"), GIT_ECERTIFICATE);
  QCOMPARE(checkX509(QByteArray(), "bad.invalid"), GIT_ECERTIFICATE);
  QVERIFY(!QFile::exists(pinsFile()));
}

void GitCertificatesTest::sshHostKeyIsTrustedOnFirstUse() {
  const QByteArray key = "ssh-ed25519 host key one";
  const QByteArray other = "ssh-ed25519 host key two";
  QCOMPARE(checkHostKey(key, "ssh.invalid"), 0);
  QCOMPARE(checkHostKey(key, "SSH.invalid"), 0);

  git_error_clear();
  QCOMPARE(checkHostKey(other, "ssh.invalid"), GIT_ECERTIFICATE);
  QVERIFY2(libgit2Error().contains(QStringLiteral("SSH host key")),
           qPrintable(libgit2Error()));
  // A key in known_hosts (valid) is accepted whatever the pin.
  QCOMPARE(checkHostKey(other, "ssh.invalid", true, 1), 0);

  GitRepo::forgetPinnedCertificates(QStringLiteral("ssh.invalid"));
  QCOMPARE(checkHostKey(other, "ssh.invalid"), 0);
}

void GitCertificatesTest::sshHostKeyFromRawKey() {
  // Without a SHA-256 hash the raw key's SHA-256 is pinned, so both kinds of
  // callback data agree.
  const QByteArray key = "raw host key";
  QCOMPARE(checkHostKey(key, "raw.invalid", false), 0);
  QCOMPARE(checkHostKey(key, "raw.invalid", true), 0);
  QCOMPARE(checkHostKey("another raw key", "raw.invalid", false),
           GIT_ECERTIFICATE);

  // Neither a hash nor the key.
  git_cert_hostkey empty{};
  empty.parent.cert_type = GIT_CERT_HOSTKEY_LIBSSH2;
  empty.type = GIT_CERT_SSH_MD5;
  QCOMPARE(GitRepo::Testing::checkCertificate(&empty.parent, 0, "md5.invalid"),
           GIT_ECERTIFICATE);
}

void GitCertificatesTest::tlsAndSshPinsAreSeparate() {
  KeyPtr key = newKey();
  QCOMPARE(checkX509(toDer(makeCert(key.get()).get()), "both.invalid"), 0);
  QCOMPARE(checkHostKey("host key", "both.invalid"), 0);
  QCOMPARE(checkHostKey("host key", "both.invalid"), 0);
  QCOMPARE(checkHostKey("other key", "both.invalid"), GIT_ECERTIFICATE);
}

void GitCertificatesTest::otherCertificatesAreRejected() {
  git_cert strings{};
  strings.cert_type = GIT_CERT_STRARRAY;
  QCOMPARE(GitRepo::Testing::checkCertificate(&strings, 0, "list.invalid"),
           GIT_ECERTIFICATE);
  QCOMPARE(GitRepo::Testing::checkCertificate(&strings, 1, "list.invalid"), 0);

  KeyPtr key = newKey();
  const QByteArray der = toDer(makeCert(key.get()).get());
  git_cert_x509 cert{};
  cert.parent.cert_type = GIT_CERT_X509;
  cert.data = const_cast<char *>(der.constData());
  cert.len = static_cast<size_t>(der.size());
  QCOMPARE(GitRepo::Testing::checkCertificate(&cert.parent, 1, nullptr),
           GIT_ECERTIFICATE);
  QCOMPARE(GitRepo::Testing::checkCertificate(nullptr, 1, "none.invalid"),
           GIT_ECERTIFICATE);
}

void GitCertificatesTest::httpsCloneSelfSigned() {
  if (!_haveGit) {
    QSKIP("git was not found");
  }
  makeRemote();
  KeyPtr key = newKey();
  X509Ptr cert = makeCert(key.get());
  std::unique_ptr<GitHttpServer> server = serve(cert.get(), key.get());
  QVERIFY(server);

  const QString clone = _base + QStringLiteral("/clone");
  const bool cloned = GitRepo::cloneRepository(
      server->url(QStringLiteral("remote.git")), clone, newUuid());
  if (!cloned && server->passed() == 0 && server->failedHandshakes() > 0 &&
      !lastErrorMessage().contains(QStringLiteral("certificate"))) {
    QSKIP(qPrintable(QStringLiteral("The TLS server failed: ") +
                     lastErrorMessage()));
  }
  QVERIFY2(cloned, qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(clone + QStringLiteral("/a.txt")), QByteArray("a\n"));

  // Pinned for the host, and a second clone uses the pin.
  QFile pins(pinsFile());
  QVERIFY(pins.open(QIODevice::ReadOnly | QIODevice::Text));
  QVERIFY(pins.readAll().startsWith("tls 127.0.0.1 "));
  QVERIFY2(GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                    _base + QStringLiteral("/again"),
                                    newUuid()),
           qPrintable(lastErrorMessage()));
}

void GitCertificatesTest::httpsCloneChangedCertificateFails() {
  if (!_haveGit) {
    QSKIP("git was not found");
  }
  makeRemote();
  KeyPtr key = newKey();
  X509Ptr cert = makeCert(key.get());
  {
    std::unique_ptr<GitHttpServer> server = serve(cert.get(), key.get());
    QVERIFY(server);
    if (!GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                  _base + QStringLiteral("/first"),
                                  newUuid())) {
      QSKIP(qPrintable(QStringLiteral("HTTPS clone failed: ") +
                       lastErrorMessage()));
    }
  }

  // The server now presents another self-signed certificate.
  KeyPtr otherKey = newKey();
  X509Ptr other = makeCert(otherKey.get());
  std::unique_ptr<GitHttpServer> server = serve(other.get(), otherKey.get());
  QVERIFY(server);
  const QString clone = _base + QStringLiteral("/second");
  QVERIFY(!GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                    clone, newUuid()));
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("not the one trusted")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(server->passed(), 0);
  QVERIFY(!QFileInfo::exists(clone));

  QVERIFY2(lastErrorMessage().contains(QStringLiteral("Credentials manager")),
           qPrintable(lastErrorMessage()));

  // Trusted in the credentials manager.
  const QVector<GitRepo::PinnedCertificate> pins =
      GitRepo::getPinnedCertificates();
  QCOMPARE(pins.size(), 1);
  QVERIFY(!pins.at(0).rejectedFingerprint.isEmpty());
  QVERIFY(GitRepo::trustRejectedCertificate(
      GitRepo::PinnedCertificateType::Tls, QStringLiteral("127.0.0.1")));
  QVERIFY2(GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                    clone, newUuid()),
           qPrintable(lastErrorMessage()));
}

void GitCertificatesTest::httpsCloneUntrustedIssuerFails() {
  if (!_haveGit) {
    QSKIP("git was not found");
  }
  makeRemote();
  KeyPtr caKey = newKey();
  CertOptions caOptions;
  caOptions.commonName = QStringLiteral("Untrusted Test CA");
  caOptions.ca = true;
  X509Ptr ca = makeCert(caKey.get(), caOptions);
  KeyPtr key = newKey();
  CertOptions leaf;
  leaf.issuer = ca.get();
  leaf.issuerKey = caKey.get();
  X509Ptr cert = makeCert(key.get(), leaf);

  std::unique_ptr<GitHttpServer> server = serve(cert.get(), key.get());
  QVERIFY(server);
  const QString clone = _base + QStringLiteral("/clone");
  QVERIFY(!GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                    clone, newUuid()));
  if (server->passed() == 0 && server->failedHandshakes() > 0 &&
      !lastErrorMessage().contains(QStringLiteral("certificate"))) {
    QSKIP(qPrintable(QStringLiteral("The TLS server failed: ") +
                     lastErrorMessage()));
  }
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("is not trusted")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(server->passed(), 0);
  QVERIFY(!QFileInfo::exists(clone));
}

void GitCertificatesTest::httpsCloneTrustedChain() {
  if (!_haveGit) {
    QSKIP("git was not found");
  }
  makeRemote();
  KeyPtr caKey = newKey();
  CertOptions caOptions;
  caOptions.commonName = QStringLiteral("Trusted Test CA");
  caOptions.ca = true;
  X509Ptr ca = makeCert(caKey.get(), caOptions);
  KeyPtr key = newKey();
  CertOptions leaf;
  leaf.issuer = ca.get();
  leaf.issuerKey = caKey.get();
  X509Ptr cert = makeCert(key.get(), leaf);

  // Trusted by libgit2 for the rest of the process, like a system root.
  QCOMPARE(git_libgit2_opts(GIT_OPT_ADD_SSL_X509_CERT, ca.get()), 0);
  std::unique_ptr<GitHttpServer> server = serve(cert.get(), key.get());
  QVERIFY(server);
  const QString clone = _base + QStringLiteral("/clone");
  QVERIFY2(GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                    clone, newUuid()),
           qPrintable(lastErrorMessage()));
  QCOMPARE(readFile(clone + QStringLiteral("/a.txt")), QByteArray("a\n"));
  // libgit2 verified it, nothing is pinned.
  QVERIFY(!QFile::exists(pinsFile()));
}

void GitCertificatesTest::httpsCloneTrustedChainOtherAddressFails() {
  // libgit2 1.9 compared IP addresses in subjectAltName inverted: only a
  // certificate for another address matched (fixed in lib/libgit2).
  if (!_haveGit) {
    QSKIP("git was not found");
  }
  makeRemote();
  KeyPtr caKey = newKey();
  CertOptions caOptions;
  caOptions.commonName = QStringLiteral("Trusted Test CA 2");
  caOptions.ca = true;
  X509Ptr ca = makeCert(caKey.get(), caOptions);
  KeyPtr key = newKey();
  CertOptions leaf;
  leaf.issuer = ca.get();
  leaf.issuerKey = caKey.get();
  leaf.subjectAltName = "IP:10.0.0.1";
  X509Ptr cert = makeCert(key.get(), leaf);
  QCOMPARE(git_libgit2_opts(GIT_OPT_ADD_SSL_X509_CERT, ca.get()), 0);

  std::unique_ptr<GitHttpServer> server = serve(cert.get(), key.get());
  QVERIFY(server);
  const QString clone = _base + QStringLiteral("/clone");
  QVERIFY(!GitRepo::cloneRepository(server->url(QStringLiteral("remote.git")),
                                    clone, newUuid()));
  QVERIFY2(lastErrorMessage().contains(QStringLiteral("another host")),
           qPrintable(lastErrorMessage()));
  QCOMPARE(server->passed(), 0);
  QVERIFY(!QFileInfo::exists(clone));
}

QTEST_GUILESS_MAIN(GitCertificatesTest)
#include "tst_gitcertificates.moc"
