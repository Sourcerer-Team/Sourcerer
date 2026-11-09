#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <LockGuard.h>
#include <Log.h>

#include <git2/sys/errors.h>

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHostAddress>
#include <QMap>
#include <QSaveFile>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QStandardPaths>
#include <QTextStream>

#include <openssl/evp.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>
#include <openssl/x509_vfy.h>

#include <algorithm>
#include <memory>

#define THREAD_ID "GitRepo"

// Server certificates: the trusted roots given to libgit2 and the policy of
// certificateCheckCallback.
//
// A certificate that libgit2 accepts (a trusted chain for the host) is
// accepted. Otherwise a self-signed TLS certificate or an SSH host key that
// is not in known_hosts is trusted on first use: its fingerprint is pinned
// per host, and a different one later is rejected. Everything else (an
// expired certificate, an untrusted issuer, a name that doesn't match the
// host) is rejected.
namespace GitRepo {

using namespace Private;

namespace {

using X509Ptr = std::unique_ptr<X509, decltype(&X509_free)>;
using X509StorePtr = std::unique_ptr<X509_STORE, decltype(&X509_STORE_free)>;
using X509StoreCtxPtr =
    std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)>;

const QString kTlsPin = QStringLiteral("tls");
const QString kSshPin = QStringLiteral("ssh");

X509Ptr parseDer(const QByteArray &der) {
  const auto *data = reinterpret_cast<const unsigned char *>(der.constData());
  return X509Ptr(d2i_X509(nullptr, &data, static_cast<long>(der.size())),
                 &X509_free);
}

// The system's trusted roots (the Windows certificate store, the macOS
// keychain or the system CA directory), read once. libgit2's OpenSSL backend
// only knows OpenSSL's default paths, which are empty on Windows.
const QList<QByteArray> &systemRootCertificates() {
  static const QList<QByteArray> roots = [] {
    QList<QByteArray> der;
    const QList<QSslCertificate> certificates =
        QSslConfiguration::systemCaCertificates();
    for (const QSslCertificate &certificate : certificates) {
      der.append(certificate.toDer());
    }
    if (der.isEmpty()) {
      LOG_WARN("No system root certificates found, HTTPS servers can't be "
               "verified");
    } else {
      LOG_INFO("Read {} system root certificates", der.size());
    }
    return der;
  }();
  return roots;
}

// Issued by itself and signed with its own key. Not X509_check_issued: it
// also wants keyCertSign, which server certificates often don't have.
bool isSelfSigned(X509 *cert) {
  if (X509_NAME_cmp(X509_get_subject_name(cert), X509_get_issuer_name(cert)) !=
      0) {
    return false;
  }
  EVP_PKEY *key = X509_get0_pubkey(cert);
  return key && X509_verify(cert, key) == 1;
}

// Why cert isn't trusted for host, for the error message.
QString verificationFailure(X509 *cert, const QString &host) {
  const QByteArray name = host.toUtf8();
  const bool isAddress = !QHostAddress(host).isNull();
  const int matches =
      isAddress ? X509_check_ip_asc(cert, name.constData(), 0)
                : X509_check_host(cert, name.constData(), name.size(),
                                  X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS, nullptr);
  if (matches != 1) {
    return QStringLiteral("it was issued for another host");
  }

  X509StorePtr store(X509_STORE_new(), &X509_STORE_free);
  X509StoreCtxPtr ctx(X509_STORE_CTX_new(), &X509_STORE_CTX_free);
  if (!store || !ctx) {
    return QStringLiteral("it could not be verified");
  }
  for (const QByteArray &der : systemRootCertificates()) {
    if (X509Ptr root = parseDer(der)) {
      X509_STORE_add_cert(store.get(), root.get());
    }
  }
  if (X509_STORE_CTX_init(ctx.get(), store.get(), cert, nullptr) != 1) {
    return QStringLiteral("it could not be verified");
  }
  if (X509_verify_cert(ctx.get()) == 1) {
    // libgit2 has only the leaf certificate here, not the chain it was sent.
    return QStringLiteral("its issuer is not trusted");
  }
  return QString::fromUtf8(
      X509_verify_cert_error_string(X509_STORE_CTX_get_error(ctx.get())));
}

// A pinned fingerprint, and a different one refused since.
struct Pin {
  QByteArray fingerprint;
  qint64 trustedSince = 0;
  QByteArray rejected;
  qint64 rejectedAt = 0;
};

// Pinned fingerprints by "<kind> <host>", kept in git_pinned_certificates.txt
// in the application's config directory, one per line:
// <kind> <host> <fingerprint> [<trusted since> [<rejected> <rejected at>]]
struct PinStore {
  Util::Mutex mutex;
  bool loaded = false;
  QString path;
  QMap<QString, Pin> pins;
};

thread_local QString gLastRejection;

PinStore &pinStore() {
  static PinStore store;
  return store;
}

QString pinKey(const QString &kind, const QString &host) {
  return kind + QLatin1Char(' ') + host.trimmed().toLower();
}

// Mutex locked by the caller.
void loadPins(PinStore &store) {
  if (store.loaded) {
    return;
  }
  store.loaded = true;
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
  store.path = dir + QStringLiteral("/git_pinned_certificates.txt");

  QFile file(store.path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }
  QTextStream in(&file);
  while (!in.atEnd()) {
    const QStringList fields =
        in.readLine().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (fields.size() < 3) {
      continue;
    }
    Pin pin;
    pin.fingerprint = QByteArray::fromHex(fields.at(2).toLatin1());
    if (pin.fingerprint.isEmpty()) {
      continue;
    }
    pin.trustedSince = fields.value(3).toLongLong();
    if (fields.size() >= 6) {
      pin.rejected = QByteArray::fromHex(fields.at(4).toLatin1());
      pin.rejectedAt = fields.at(5).toLongLong();
    }
    store.pins.insert(pinKey(fields.at(0), fields.at(1)), pin);
  }
}

// Mutex locked by the caller.
void savePins(const PinStore &store) {
  QDir().mkpath(QFileInfo(store.path).absolutePath());
  QSaveFile file(store.path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to open the pinned certificates file: {}", store.path);
    return;
  }
  QTextStream out(&file);
  for (auto it = store.pins.constBegin(); it != store.pins.constEnd(); ++it) {
    const Pin &pin = it.value();
    out << it.key() << ' ' << pin.fingerprint.toHex() << ' '
        << pin.trustedSince;
    if (!pin.rejected.isEmpty()) {
      out << ' ' << pin.rejected.toHex() << ' ' << pin.rejectedAt;
    }
    out << '\n';
  }
  out.flush();
  if (!file.commit()) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to write the pinned certificates file: {}", store.path);
  }
}

enum class PinResult { Pinned, Matched, Changed };

PinResult checkPin(const QString &kind, const QString &host,
                   const QByteArray &fingerprint) {
  PinStore &store = pinStore();
  LOCKGUARD(store.mutex);
  loadPins(store);

  const QString key = pinKey(kind, host);
  const auto it = store.pins.find(key);
  if (it == store.pins.end()) {
    Pin pin;
    pin.fingerprint = fingerprint;
    pin.trustedSince = QDateTime::currentMSecsSinceEpoch();
    store.pins.insert(key, pin);
    savePins(store);
    return PinResult::Pinned;
  }
  if (it->fingerprint == fingerprint) {
    return PinResult::Matched;
  }
  // Remembered, so the user can trust it in the credentials or SSH keys
  // manager.
  it->rejected = fingerprint;
  it->rejectedAt = QDateTime::currentMSecsSinceEpoch();
  savePins(store);
  return PinResult::Changed;
}

QString hexFingerprint(const QByteArray &fingerprint) {
  return QString::fromLatin1(fingerprint.toHex(':')).toUpper();
}

QString sshFingerprint(const QByteArray &fingerprint) {
  return QStringLiteral("SHA256:") +
         QString::fromLatin1(
             fingerprint.toBase64(QByteArray::OmitTrailingEquals));
}

QString shownFingerprint(const QString &kind, const QByteArray &fingerprint) {
  return kind == kSshPin ? sshFingerprint(fingerprint)
                         : hexFingerprint(fingerprint);
}

QString kindOf(PinnedCertificateType type) {
  return type == PinnedCertificateType::Ssh ? kSshPin : kTlsPin;
}

int reject(const QString &message) {
  LOG_WARN("{}", message);
  gLastRejection = message;
  git_error_set_str(GIT_ERROR_SSL, message.toUtf8().constData());
  return GIT_ECERTIFICATE;
}

int checkX509(const git_cert_x509 *cert, const QString &host) {
  if (!cert->data || cert->len == 0) {
    return reject(
        QStringLiteral("The server %1 sent no TLS certificate.").arg(host));
  }
  const QByteArray der(static_cast<const char *>(cert->data),
                       static_cast<qsizetype>(cert->len));
  X509Ptr x509 = parseDer(der);
  if (!x509) {
    return reject(
        QStringLiteral("The TLS certificate of %1 could not be read.")
            .arg(host));
  }

  if (!isSelfSigned(x509.get())) {
    return reject(QStringLiteral("The TLS certificate of %1 is not trusted: %2.")
                      .arg(host, verificationFailure(x509.get(), host)));
  }

  const QByteArray fingerprint =
      QCryptographicHash::hash(der, QCryptographicHash::Sha256);
  switch (checkPin(kTlsPin, host, fingerprint)) {
  case PinResult::Pinned:
    LOG_INFO("Trusting the self-signed certificate of {} (SHA-256 {})", host,
             hexFingerprint(fingerprint));
    return 0;
  case PinResult::Matched:
    return 0;
  case PinResult::Changed:
    break;
  }
  return reject(
      QStringLiteral(
          "The self-signed TLS certificate of %1 is not the one trusted "
          "before (now SHA-256 %2). If the server's certificate was "
          "replaced on purpose, trust the new one in the Credentials "
          "manager.")
          .arg(host, hexFingerprint(fingerprint)));
}

int checkHostKey(const git_cert_hostkey *cert, const QString &host) {
  QByteArray fingerprint;
  if (cert->type & GIT_CERT_SSH_SHA256) {
    fingerprint = QByteArray(reinterpret_cast<const char *>(cert->hash_sha256),
                             sizeof(cert->hash_sha256));
  } else if ((cert->type & GIT_CERT_SSH_RAW) && cert->hostkey) {
    fingerprint = QCryptographicHash::hash(
        QByteArray(cert->hostkey, static_cast<qsizetype>(cert->hostkey_len)),
        QCryptographicHash::Sha256);
  } else {
    return reject(
        QStringLiteral("The SSH host key of %1 has no SHA-256 fingerprint.")
            .arg(host));
  }

  const QString shown = sshFingerprint(fingerprint);
  switch (checkPin(kSshPin, host, fingerprint)) {
  case PinResult::Pinned:
    LOG_INFO("Trusting the SSH host key of {} ({})", host, shown);
    return 0;
  case PinResult::Matched:
    return 0;
  case PinResult::Changed:
    break;
  }
  return reject(QStringLiteral("The SSH host key of %1 is not the one trusted "
                               "before (now %2). If the server's key was "
                               "replaced on purpose, trust the new one in the "
                               "SSH keys manager.")
                    .arg(host, shown));
}

} // namespace

namespace Private {

void addSystemRootCertificates() {
  int added = 0;
  for (const QByteArray &der : systemRootCertificates()) {
    X509Ptr cert = parseDer(der);
    // OpenSSL keeps its own reference and ignores duplicates.
    if (cert && git_libgit2_opts(GIT_OPT_ADD_SSL_X509_CERT, cert.get()) == 0) {
      ++added;
    }
  }
  LOG_DEBUG("Added {} system root certificates to libgit2", added);
}

int certificateCheckCallback(git_cert *cert, int valid, const char *host,
                             void *payload) {
  Q_UNUSED(payload);
  gLastRejection.clear();

  if (!cert || !host) {
    return reject(QStringLiteral("The server sent no certificate."));
  }
  const QString hostName = QString::fromUtf8(host);

  if (valid) {
    LOG_DEBUG("Certificate for host '{}' is valid", hostName);
    return 0;
  }

  switch (cert->cert_type) {
  case GIT_CERT_X509:
    return checkX509(reinterpret_cast<const git_cert_x509 *>(cert), hostName);
  case GIT_CERT_HOSTKEY_LIBSSH2:
    return checkHostKey(reinterpret_cast<const git_cert_hostkey *>(cert),
                        hostName);
  default:
    return reject(
        QStringLiteral("The certificate of %1 could not be verified.")
            .arg(hostName));
  }
}

QString takeCertificateRejection() {
  QString rejection;
  rejection.swap(gLastRejection);
  return rejection;
}

} // namespace Private

bool checkUnverifiedServerCertificate(const QByteArray &der,
                                      const QString &host,
                                      QString *rejection) {
  gLastRejection.clear();
  git_cert_x509 cert{};
  cert.parent.cert_type = GIT_CERT_X509;
  cert.data = const_cast<char *>(der.constData());
  cert.len = static_cast<size_t>(der.size());
  const bool trusted = checkX509(&cert, host) == 0;
  // libgit2 isn't the caller here: its error is not left behind.
  git_error_clear();
  if (rejection) {
    *rejection = takeCertificateRejection();
  } else {
    gLastRejection.clear();
  }
  return trusted;
}

bool certificateMatchesHost(const QByteArray &der, const QString &host) {
  X509Ptr x509 = parseDer(der);
  if (!x509) {
    return false;
  }
  const QByteArray name = host.toUtf8();
  return !QHostAddress(host).isNull()
             ? X509_check_ip_asc(x509.get(), name.constData(), 0) == 1
             : X509_check_host(x509.get(), name.constData(), name.size(),
                               X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS,
                               nullptr) == 1;
}

void forgetPinnedCertificates(const QString &host) {
  PinStore &store = pinStore();
  LOCKGUARD(store.mutex);
  loadPins(store);
  const bool removedTls = store.pins.remove(pinKey(kTlsPin, host)) > 0;
  const bool removedSsh = store.pins.remove(pinKey(kSshPin, host)) > 0;
  if (removedTls || removedSsh) {
    savePins(store);
  }
}

QVector<PinnedCertificate> getPinnedCertificates() {
  PinStore &store = pinStore();
  LOCKGUARD(store.mutex);
  loadPins(store);

  QVector<PinnedCertificate> certificates;
  for (auto it = store.pins.constBegin(); it != store.pins.constEnd(); ++it) {
    const QString kind = it.key().section(QLatin1Char(' '), 0, 0);
    PinnedCertificate certificate;
    certificate.type = kind == kSshPin ? PinnedCertificateType::Ssh
                                       : PinnedCertificateType::Tls;
    certificate.host = it.key().section(QLatin1Char(' '), 1);
    certificate.fingerprint = shownFingerprint(kind, it->fingerprint);
    certificate.trustedSince = it->trustedSince;
    if (!it->rejected.isEmpty()) {
      certificate.rejectedFingerprint = shownFingerprint(kind, it->rejected);
      certificate.rejectedAt = it->rejectedAt;
    }
    certificates.append(certificate);
  }
  std::sort(certificates.begin(), certificates.end(),
            [](const PinnedCertificate &a, const PinnedCertificate &b) {
              return a.host == b.host ? a.type < b.type : a.host < b.host;
            });
  return certificates;
}

bool forgetPinnedCertificate(PinnedCertificateType type, const QString &host) {
  PinStore &store = pinStore();
  LOCKGUARD(store.mutex);
  loadPins(store);
  if (store.pins.remove(pinKey(kindOf(type), host)) == 0) {
    return false;
  }
  savePins(store);
  return true;
}

bool trustRejectedCertificate(PinnedCertificateType type, const QString &host) {
  PinStore &store = pinStore();
  LOCKGUARD(store.mutex);
  loadPins(store);
  const auto it = store.pins.find(pinKey(kindOf(type), host));
  if (it == store.pins.end() || it->rejected.isEmpty()) {
    return false;
  }
  LOG_INFO("Trusting the replaced certificate of {} ({})", host,
           shownFingerprint(kindOf(type), it->rejected));
  it->fingerprint = it->rejected;
  it->trustedSince = QDateTime::currentMSecsSinceEpoch();
  it->rejected.clear();
  it->rejectedAt = 0;
  savePins(store);
  return true;
}

#ifdef SOURCERER_GITREPO_TESTING
namespace Testing {

int checkCertificate(git_cert *cert, int valid, const char *host) {
  return certificateCheckCallback(cert, valid, host, nullptr);
}

void resetPinnedCertificates() {
  PinStore &store = pinStore();
  LOCKGUARD(store.mutex);
  loadPins(store);
  store.pins.clear();
  QFile::remove(store.path);
  // Read the file again on the next use, for tests that write one.
  store.loaded = false;
}

int systemRootCertificateCount() {
  return static_cast<int>(systemRootCertificates().size());
}

} // namespace Testing
#endif

} // namespace GitRepo
