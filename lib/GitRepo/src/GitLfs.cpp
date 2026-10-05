#include "GitLfs.h"

#include <GitCredentialStorage.h>
#include <GitKeyStorage.h>
#include <GitRepo.h>
#include <Log.h>

#include <git2/attr.h>
#include <git2/blob.h>
#include <git2/branch.h>
#include <git2/commit.h>
#include <git2/config.h>
#include <git2/errors.h>
#include <git2/filter.h>
#include <git2/index.h>
#include <git2/remote.h>
#include <git2/revparse.h>
#include <git2/revwalk.h>
#include <git2/sys/errors.h>
#include <git2/sys/filter.h>
#include <git2/tag.h>
#include <git2/tree.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPair>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>

#include <libssh2.h>

#include <array>
#include <memory>

#define THREAD_ID "GitLfs"

namespace GitRepo::Lfs {
namespace {

constexpr char POINTER_VERSION[] = "version https://git-lfs.github.com/spec/v1";
constexpr char POINTER_OID_PREFIX[] = "oid sha256:";
constexpr char POINTER_SIZE_PREFIX[] = "size ";
constexpr char FILTER_NAME[] = "lfs";

QMutex gMutex;
int gRefCount = 0;
git_filter *gFilter = nullptr;

QString repoWorkdirPath(git_repository *repo) {
  if (!repo) {
    return {};
  }

  const char *workdir = git_repository_workdir(repo);
  if (workdir && *workdir) {
    return QDir::fromNativeSeparators(QString::fromUtf8(workdir));
  }

  const char *path = git_repository_path(repo);
  return path ? QDir::fromNativeSeparators(QString::fromUtf8(path)) : QString();
}

QString normalizeRepoRoot(QString path) {
  if (path.endsWith("/.git/") || path.endsWith("\\.git\\")) {
    path.chop(6);
  } else if (path.endsWith("/.git") || path.endsWith("\\.git")) {
    path.chop(5);
  }

  return QDir::cleanPath(path);
}

bool writeAll(git_writestream *stream, const QByteArray &data) {
  if (!stream) {
    return false;
  }

  int error = 0;
  if (!data.isEmpty()) {
    error = stream->write(stream, data.constData(), data.size());
  }
  if (!error) {
    error = stream->close(stream);
  }
  return error == 0;
}

bool isHttpUrl(const QString &url) {
  return url.startsWith("http://", Qt::CaseInsensitive) ||
         url.startsWith("https://", Qt::CaseInsensitive);
}

bool isSshUrl(const QString &remoteUrl) {
  return remoteUrl.startsWith("ssh://", Qt::CaseInsensitive) ||
         (!remoteUrl.contains("://") && remoteUrl.contains("@") &&
          remoteUrl.contains(":"));
}

QString normalizeRemoteBaseUrl(const QString &remoteUrl) {
  if (remoteUrl.isEmpty()) {
    return {};
  }

  if (isHttpUrl(remoteUrl)) {
    QString normalized = remoteUrl;
    while (normalized.endsWith('/')) {
      normalized.chop(1);
    }
    return normalized;
  }

  const QUrl parsed(remoteUrl);
  if (parsed.isValid() &&
      parsed.scheme().compare("ssh", Qt::CaseInsensitive) == 0 &&
      !parsed.host().isEmpty()) {
    QUrl httpsUrl;
    httpsUrl.setScheme(QStringLiteral("https"));
    httpsUrl.setHost(parsed.host());
    if (parsed.port() > 0 && parsed.port() != 22) {
      httpsUrl.setPort(parsed.port());
    }
    httpsUrl.setPath(parsed.path());
    QString normalized = httpsUrl.toString(QUrl::FullyEncoded);
    while (normalized.endsWith('/')) {
      normalized.chop(1);
    }
    return normalized;
  }

  const QRegularExpression scpLike(
      QStringLiteral(R"(^([^@]+@)?([^:]+):/?(.+)$)"));
  const QRegularExpressionMatch match = scpLike.match(remoteUrl);
  if (match.hasMatch()) {
    const QString host = match.captured(2);
    QString path = match.captured(3);
    while (path.startsWith('/')) {
      path.remove(0, 1);
    }
    QString normalized = QStringLiteral("https://") + host + '/' + path;
    while (normalized.endsWith('/')) {
      normalized.chop(1);
    }
    return normalized;
  }

  return {};
}

QString deriveLfsUrlFromRemote(const QString &remoteUrl) {
  const QString base = normalizeRemoteBaseUrl(remoteUrl);
  if (base.isEmpty()) {
    return {};
  }

  return base + "/info/lfs";
}

struct RemoteInfo {
  QString user;
  QString host;
  int port = -1;
  QString path;
};

QMutex gLibssh2Mutex;
int gLibssh2RefCount = 0;

bool ensureLibssh2Initialized() {
  QMutexLocker locker(&gLibssh2Mutex);
  if (gLibssh2RefCount == 0 && libssh2_init(0) != 0) {
    return false;
  }
  ++gLibssh2RefCount;
  return true;
}

void releaseLibssh2() {
  QMutexLocker locker(&gLibssh2Mutex);
  if (gLibssh2RefCount > 0) {
    --gLibssh2RefCount;
    if (gLibssh2RefCount == 0) {
      libssh2_exit();
    }
  }
}

QString quoteRemoteShellArg(const QString &value) {
  QString result("'");
  result += value;
  result.replace("'", "'\\''");
  result += "'";
  return result;
}

bool authenticateWithLibssh2KeyFile(LIBSSH2_SESSION *session,
                                    const GitSshAuthInfo &sshAuth,
                                    QString &errorMessage) {
  if (!session || sshAuth.username.isEmpty() ||
      sshAuth.privateKeyPath.isEmpty() || sshAuth.publicKeyPath.isEmpty() ||
      !QFile::exists(sshAuth.privateKeyPath) ||
      !QFile::exists(sshAuth.publicKeyPath)) {
    return false;
  }

  const QByteArray userUtf8 = sshAuth.username.toUtf8();
  const QByteArray publicKeyUtf8 = sshAuth.publicKeyPath.toUtf8();
  const QByteArray privateKeyUtf8 = sshAuth.privateKeyPath.toUtf8();
  const QByteArray passphraseUtf8 = sshAuth.passphrase.toUtf8();
  const int authResult = libssh2_userauth_publickey_fromfile_ex(
      session, userUtf8.constData(), static_cast<unsigned int>(userUtf8.size()),
      publicKeyUtf8.constData(), privateKeyUtf8.constData(),
      sshAuth.passphrasePresent ? passphraseUtf8.constData() : nullptr);
  if (authResult == 0) {
    return true;
  }

  char *libssh2Error = nullptr;
  int errorCode = 0;
  libssh2_session_last_error(session, &libssh2Error, &errorCode, 0);
  errorMessage = libssh2Error ? QString::fromUtf8(libssh2Error)
                              : QStringLiteral("ssh authentication failed");
  return false;
}

bool authenticateWithLibssh2Agent(LIBSSH2_SESSION *session,
                                  const QString &username,
                                  QString &errorMessage) {
  if (!session || username.isEmpty()) {
    return false;
  }

  LIBSSH2_AGENT *agent = libssh2_agent_init(session);
  if (!agent) {
    errorMessage = QStringLiteral("failed to initialize ssh agent");
    return false;
  }

  bool authenticated = false;
  const QByteArray userUtf8 = username.toUtf8();

  do {
    if (libssh2_agent_connect(agent) != 0) {
      errorMessage = QStringLiteral("failed to connect to ssh agent");
      break;
    }

    if (libssh2_agent_list_identities(agent) != 0) {
      errorMessage = QStringLiteral("failed to list ssh agent identities");
      break;
    }

    struct libssh2_agent_publickey *identity = nullptr;
    while (true) {
      const int rc = libssh2_agent_get_identity(agent, &identity, identity);
      if (rc == 1) {
        break;
      }
      if (rc < 0) {
        errorMessage = QStringLiteral("failed to read ssh agent identity");
        break;
      }

      if (libssh2_agent_userauth(agent, userUtf8.constData(), identity) == 0) {
        authenticated = true;
        break;
      }
    }
  } while (false);

  libssh2_agent_disconnect(agent);
  libssh2_agent_free(agent);
  if (authenticated) {
    return true;
  }

  if (errorMessage.isEmpty()) {
    errorMessage = QStringLiteral("ssh agent authentication failed");
  }
  return false;
}

QList<GitSshAuthInfo> buildLibssh2KeyCandidates(const QString &host,
                                                const QString &username) {
  QList<GitSshAuthInfo> candidates;
  const auto addCandidate = [&candidates](const GitSshAuthInfo &candidate) {
    if (candidate.privateKeyPath.isEmpty() ||
        candidate.publicKeyPath.isEmpty()) {
      return;
    }
    for (const GitSshAuthInfo &existing : candidates) {
      if (existing.privateKeyPath == candidate.privateKeyPath &&
          existing.publicKeyPath == candidate.publicKeyPath) {
        return;
      }
    }
    candidates.append(candidate);
  };

  GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
  GitKeyPair keyPair;
  if (keyStorage.getKeyPairForHost(host, keyPair)) {
    GitSshAuthInfo candidate;
    candidate.username = username.isEmpty() ? QStringLiteral("git") : username;
    candidate.privateKeyPath = keyPair.privateKeyPath;
    candidate.publicKeyPath = keyPair.publicKeyPath;
    addCandidate(candidate);
  }

  const QString homePath =
      QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
  const QString sshDir = QDir(homePath).filePath(QStringLiteral(".ssh"));
  const QStringList defaultKeyNames{
      QStringLiteral("id_ed25519"), QStringLiteral("id_ecdsa"),
      QStringLiteral("id_rsa"), QStringLiteral("id_dsa")};

  for (const QString &keyName : defaultKeyNames) {
    GitSshAuthInfo candidate;
    candidate.username = username.isEmpty() ? QStringLiteral("git") : username;
    candidate.privateKeyPath = QDir(sshDir).filePath(keyName);
    candidate.publicKeyPath = candidate.privateKeyPath + QStringLiteral(".pub");
    if (QFile::exists(candidate.privateKeyPath) &&
        QFile::exists(candidate.publicKeyPath)) {
      addCandidate(candidate);
    }
  }

  return candidates;
}

struct LfsUploadCandidate {
  QString oid;
  QString path;
  QString source;
  qint64 size = 0;
};

Pointer parsePointerBytes(const QByteArray &data);

struct CollectLfsObjectsPayload {
  git_repository *repo = nullptr;
  QString repoPath;
  QHash<QString, LfsUploadCandidate> *candidates = nullptr;
};

QString lfsObjectStorePath(const QString &repoPath, const QString &oid) {
  const QString root = objectsRootPath(repoPath);
  if (root.isEmpty() || oid.size() < 4) {
    return {};
  }

  return QDir(QDir(root).filePath(oid.left(2)))
      .filePath(oid.mid(2, 2) + "/" + oid);
}

QString resolveLfsUploadPath(const QString &repoPath, const QString &oid,
                             const QString &fallbackPath = QString()) {
  const QString storePath = lfsObjectStorePath(repoPath, oid);
  if (!storePath.isEmpty() && QFile::exists(storePath)) {
    LOG_DEBUG("Resolved LFS upload path for oid '{}' to store path '{}'",
              oid.toStdString(), storePath.toStdString());
    return storePath;
  }

  LOG_DEBUG("Resolved LFS upload path for oid '{}' to fallback path '{}'",
            oid.toStdString(), fallbackPath.toStdString());
  return fallbackPath;
}

bool isLfsTrackedPath(git_repository *repo, const QString &relativePath) {
  if (!repo || relativePath.isEmpty()) {
    return false;
  }

  const QByteArray pathUtf8 = QDir::fromNativeSeparators(relativePath).toUtf8();
  const char *value = nullptr;
  if (git_attr_get(&value, repo,
                   GIT_ATTR_CHECK_NO_SYSTEM | GIT_ATTR_CHECK_FILE_THEN_INDEX,
                   pathUtf8.constData(), "filter") != 0 ||
      !value) {
    return false;
  }

  return QString::fromUtf8(value).compare(QStringLiteral("lfs"),
                                          Qt::CaseInsensitive) == 0;
}

bool addLfsUploadCandidate(QHash<QString, LfsUploadCandidate> *candidates,
                           const QString &oid, const QString &path,
                           const QString &source, qint64 size) {
  if (!candidates) {
    LOG_DEBUG("Skipping LFS upload candidate from '{}' because the candidate "
              "map is null",
              source.toStdString());
    return false;
  }

  if (oid.isEmpty()) {
    LOG_DEBUG(
        "Skipping LFS upload candidate from '{}' because the oid is empty",
        source.toStdString());
    return false;
  }

  if (path.isEmpty()) {
    LOG_DEBUG("Skipping LFS upload candidate '{}' from '{}' because the path "
              "is empty",
              oid.toStdString(), source.toStdString());
    return false;
  }

  if (!QFile::exists(path)) {
    LOG_DEBUG("Skipping LFS upload candidate '{}' from '{}' because the file "
              "does not exist at '{}'",
              oid.toStdString(), source.toStdString(), path.toStdString());
    return false;
  }

  if (candidates->contains(oid)) {
    return true;
  }

  LfsUploadCandidate candidate;
  candidate.oid = oid;
  candidate.path = path;
  candidate.source = source;
  candidate.size = size >= 0 ? size : QFileInfo(path).size();
  candidates->insert(oid, candidate);
  return true;
}

static int collectLfsPointerCandidatesCallback(const char *root,
                                               const git_tree_entry *entry,
                                               void *payload) {
  auto *data = static_cast<CollectLfsObjectsPayload *>(payload);
  if (!data || !data->repo || !data->candidates || !entry) {
    return -1;
  }

  if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
    return 0;
  }

  const git_oid *entryOid = git_tree_entry_id(entry);
  if (!entryOid) {
    return 0;
  }

  git_blob *blob = nullptr;
  if (git_blob_lookup(&blob, data->repo, entryOid) != 0 || !blob) {
    return 0;
  }

  const QByteArray blobBytes(
      static_cast<const char *>(git_blob_rawcontent(blob)),
      static_cast<int>(git_blob_rawsize(blob)));
  const Pointer pointer = parsePointerBytes(blobBytes);
  git_blob_free(blob);
  if (!pointer.valid) {
    return 0;
  }

  const QString relativePath = QString::fromUtf8(root ? root : "") +
                               QString::fromUtf8(git_tree_entry_name(entry));
  const QString worktreePath =
      QDir(data->repoPath).filePath(QDir::fromNativeSeparators(relativePath));
  const QString uploadPath =
      resolveLfsUploadPath(data->repoPath, pointer.oid, worktreePath);
  addLfsUploadCandidate(data->candidates, pointer.oid, uploadPath,
                        QStringLiteral("ref"), pointer.size);
  return 0;
}

static bool
collectLfsObjectsFromRef(git_repository *repo, const QString &repoPath,
                         const QString &refName,
                         QHash<QString, LfsUploadCandidate> *candidates) {
  if (!repo || !candidates || refName.isEmpty()) {
    return true;
  }

  git_object *target = nullptr;
  if (git_revparse_single(&target, repo, refName.toStdString().c_str()) != 0 ||
      !target) {
    const git_error *error = git_error_last();
    LOG_ERROR(Log::ErrorId::Git, "Failed to resolve LFS ref '{}': {}",
              refName.toStdString(),
              error && error->message ? error->message : "unknown error");
    return false;
  }

  git_object *commitObject = nullptr;
  if (git_object_peel(&commitObject, target, GIT_OBJECT_COMMIT) != 0 ||
      !commitObject) {
    const git_error *error = git_error_last();
    LOG_ERROR(Log::ErrorId::Git, "Failed to peel LFS ref '{}' to a commit: {}",
              refName.toStdString(),
              error && error->message ? error->message : "unknown error");
    git_object_free(target);
    return false;
  }

  git_commit *commit = reinterpret_cast<git_commit *>(commitObject);
  git_tree *tree = nullptr;
  if (git_commit_tree(&tree, commit) != 0 || !tree) {
    const git_error *error = git_error_last();
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to read commit tree for LFS ref '{}': {}",
              refName.toStdString(),
              error && error->message ? error->message : "unknown error");
    git_object_free(commitObject);
    git_object_free(target);
    return false;
  }

  CollectLfsObjectsPayload payload{repo, repoPath, candidates};
  const int walkError = git_tree_walk(
      tree, GIT_TREEWALK_PRE, collectLfsPointerCandidatesCallback, &payload);
  git_tree_free(tree);
  git_object_free(commitObject);
  git_object_free(target);
  return walkError == 0;
}

static bool
collectLfsObjectsFromIndex(git_repository *repo, const QString &repoPath,
                           QHash<QString, LfsUploadCandidate> *candidates) {
  if (!repo || !candidates) {
    return false;
  }

  git_index *index = nullptr;
  if (git_repository_index(&index, repo) != 0 || !index) {
    return false;
  }

  if (git_index_read(index, 1) != 0) {
    git_index_free(index);
    return false;
  }

  const size_t entryCount = git_index_entrycount(index);
  for (size_t i = 0; i < entryCount; ++i) {
    const git_index_entry *entry = git_index_get_byindex(index, i);
    if (!entry || !entry->path ||
        GIT_INDEX_ENTRY_STAGE(entry) != GIT_INDEX_STAGE_NORMAL) {
      continue;
    }

    const QString relativePath = QString::fromUtf8(entry->path);
    if (!isLfsTrackedPath(repo, relativePath)) {
      continue;
    }

    git_blob *blob = nullptr;
    if (git_blob_lookup(&blob, repo, &entry->id) != 0 || !blob) {
      continue;
    }

    const QByteArray blobBytes(
        static_cast<const char *>(git_blob_rawcontent(blob)),
        static_cast<int>(git_blob_rawsize(blob)));
    const Pointer pointer = parsePointerBytes(blobBytes);
    git_blob_free(blob);
    if (!pointer.valid) {
      continue;
    }

    const QString worktreePath = QDir(repoPath).filePath(relativePath);
    const QString uploadPath =
        resolveLfsUploadPath(repoPath, pointer.oid, worktreePath);
    addLfsUploadCandidate(candidates, pointer.oid, uploadPath,
                          QStringLiteral("index"), pointer.size);
  }

  git_index_free(index);
  return true;
}

static bool
collectLfsObjectsFromAllRefs(git_repository *repo, const QString &repoPath,
                             QHash<QString, LfsUploadCandidate> *candidates) {
  if (!repo || !candidates) {
    return false;
  }

  bool success = true;

  git_branch_iterator *iterator = nullptr;
  git_reference *reference = nullptr;
  git_branch_t branchType = GIT_BRANCH_LOCAL;
  if (git_branch_iterator_new(&iterator, repo, branchType) == 0 && iterator) {
    while (git_branch_next(&reference, &branchType, iterator) == 0) {
      const char *branchName = nullptr;
      if (git_branch_name(&branchName, reference) == 0 && branchName) {
        const QString refName = QStringLiteral("refs/heads/%1").arg(branchName);
        success =
            collectLfsObjectsFromRef(repo, repoPath, refName, candidates) &&
            success;
      }
      git_reference_free(reference);
      reference = nullptr;
    }
    git_branch_iterator_free(iterator);
  }

  git_strarray tags = {0};
  if (git_tag_list(&tags, repo) == 0) {
    for (size_t i = 0; i < tags.count; ++i) {
      const QString refName =
          QStringLiteral("refs/tags/%1").arg(tags.strings[i]);
      success = collectLfsObjectsFromRef(repo, repoPath, refName, candidates) &&
                success;
    }
    git_strarray_dispose(&tags);
  }

  return success;
}

bool parseSshRemoteUrl(const QString &remoteUrl, RemoteInfo &info) {
  QUrl url(remoteUrl);
  if (url.isValid() && url.scheme().compare("ssh", Qt::CaseInsensitive) == 0) {
    info.user = url.userName();
    info.host = url.host();
    info.port = url.port();
    info.path = url.path();
    while (info.path.startsWith('/')) {
      info.path.remove(0, 1);
    }
    return !info.host.isEmpty() && !info.path.isEmpty();
  }

  const QRegularExpression scpLike(
      QStringLiteral(R"(^(([^@]+)@)?([^:]+):/?(.+)$)"));
  const QRegularExpressionMatch match = scpLike.match(remoteUrl);
  if (!match.hasMatch()) {
    return false;
  }

  info.user = match.captured(2);
  info.host = match.captured(3);
  info.path = match.captured(4);
  while (info.path.startsWith('/')) {
    info.path.remove(0, 1);
  }
  return !info.host.isEmpty() && !info.path.isEmpty();
}

bool runSshGitLfsAuthenticate(const RemoteInfo &info, const QString &operation,
                              QString &endpointUrl, QJsonObject &headers) {
  if (info.host.isEmpty() || info.path.isEmpty()) {
    return false;
  }

  if (!ensureLibssh2Initialized()) {
    git_error_set_str(GIT_ERROR_NET, "failed to initialize libssh2");
    return false;
  }

  const QList<GitSshAuthInfo> keyCandidates =
      buildLibssh2KeyCandidates(info.host, info.user);

  QTcpSocket socket;
  const quint16 port = info.port > 0 ? static_cast<quint16>(info.port) : 22;
  socket.connectToHost(info.host, port);
  if (!socket.waitForConnected(30000)) {
    releaseLibssh2();
    const QByteArray message = socket.errorString().toUtf8();
    git_error_set_str(GIT_ERROR_NET, message.constData());
    return false;
  }

  LIBSSH2_SESSION *session = libssh2_session_init();
  if (!session) {
    socket.disconnectFromHost();
    releaseLibssh2();
    git_error_set_str(GIT_ERROR_NET, "failed to create libssh2 session");
    return false;
  }

  libssh2_session_set_blocking(session, 1);
  if (libssh2_session_handshake(session, static_cast<libssh2_socket_t>(
                                             socket.socketDescriptor())) != 0) {
    libssh2_session_free(session);
    socket.disconnectFromHost();
    releaseLibssh2();
    git_error_set_str(GIT_ERROR_NET, "failed to complete ssh handshake");
    return false;
  }

  QString authErrorMessage;
  bool authenticated = false;
  for (const GitSshAuthInfo &candidate : keyCandidates) {
    authErrorMessage.clear();
    if (authenticateWithLibssh2KeyFile(session, candidate, authErrorMessage)) {
      authenticated = true;
      break;
    }
  }

  if (!authenticated) {
    const QString agentUsername =
        info.user.isEmpty() ? QStringLiteral("git") : info.user;
    authErrorMessage.clear();
    authenticated =
        authenticateWithLibssh2Agent(session, agentUsername, authErrorMessage);
  }

  if (!authenticated) {
    const QByteArray message = authErrorMessage.toUtf8();
    libssh2_session_disconnect(session, "bye");
    libssh2_session_free(session);
    socket.disconnectFromHost();
    releaseLibssh2();
    git_error_set_str(GIT_ERROR_NET, message.isEmpty()
                                         ? "ssh authentication failed"
                                         : message.constData());
    return false;
  }

  LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(session);
  if (!channel) {
    libssh2_session_disconnect(session, "bye");
    libssh2_session_free(session);
    socket.disconnectFromHost();
    releaseLibssh2();
    git_error_set_str(GIT_ERROR_NET, "failed to open ssh channel");
    return false;
  }

  const QString command =
      QStringLiteral("git-lfs-authenticate %1 %2").arg(info.path, operation);
  if (libssh2_channel_exec(channel, command.toUtf8().constData()) != 0) {
    libssh2_channel_free(channel);
    libssh2_session_disconnect(session, "bye");
    libssh2_session_free(session);
    socket.disconnectFromHost();
    releaseLibssh2();
    git_error_set_str(GIT_ERROR_NET,
                      "failed to execute git-lfs-authenticate over ssh");
    return false;
  }

  QByteArray output;
  QByteArray stderrOutput;
  char buffer[4096];
  while (true) {
    const ssize_t bytesRead =
        libssh2_channel_read(channel, buffer, sizeof(buffer));
    if (bytesRead > 0) {
      output.append(buffer, static_cast<int>(bytesRead));
      continue;
    }

    if (bytesRead == LIBSSH2_ERROR_EAGAIN) {
      continue;
    }

    break;
  }

  while (true) {
    const ssize_t bytesRead =
        libssh2_channel_read_stderr(channel, buffer, sizeof(buffer));
    if (bytesRead > 0) {
      stderrOutput.append(buffer, static_cast<int>(bytesRead));
      continue;
    }

    if (bytesRead == LIBSSH2_ERROR_EAGAIN) {
      continue;
    }

    break;
  }

  libssh2_channel_wait_closed(channel);
  const int exitStatus = libssh2_channel_get_exit_status(channel);
  libssh2_channel_free(channel);
  libssh2_session_disconnect(session, "bye");
  libssh2_session_free(session);
  socket.disconnectFromHost();
  releaseLibssh2();

  if (exitStatus != 0) {
    const QByteArray message =
        stderrOutput.isEmpty() ? output.trimmed() : stderrOutput.trimmed();
    const QByteArray finalMessage =
        message.isEmpty() ? QByteArray("ssh lfs endpoint discovery failed")
                          : message;
    git_error_set_str(GIT_ERROR_NET, finalMessage.constData());
    return false;
  }

  const QJsonDocument response = QJsonDocument::fromJson(output.trimmed());
  if (!response.isObject()) {
    git_error_set_str(GIT_ERROR_NET, "invalid ssh lfs-authenticate response");
    return false;
  }

  const QJsonObject responseObject = response.object();
  endpointUrl = responseObject.value("href").toString();
  headers = responseObject.value("header").toObject();
  return !endpointUrl.isEmpty();
}

QString getConfigString(git_repository *repo, const char *key) {
  git_config *config = nullptr;
  QString value;

  if (!repo || !key) {
    return value;
  }

  if (git_repository_config(&config, repo) != 0) {
    return value;
  }

  const char *raw = nullptr;
  if (git_config_get_string(&raw, config, key) == 0 && raw) {
    value = QString::fromUtf8(raw);
  }

  git_config_free(config);
  return value;
}

QString getCredentialHeader(const QString &repoPath, const QString &url) {
  return GitRepo::getBasicAuthHeader(repoPath, url);
}

QString findGitRemoteUrl(git_repository *repo, const QString &remoteName) {
  if (!repo) {
    return {};
  }

  const QString preferredRemoteName =
      remoteName.isEmpty() ? QStringLiteral("origin") : remoteName;

  if (!preferredRemoteName.isEmpty()) {
    git_remote *remote = nullptr;
    if (git_remote_lookup(&remote, repo,
                          preferredRemoteName.toUtf8().constData()) == 0 &&
        remote) {
      const char *remoteUrl = git_remote_url(remote);
      const QString result =
          remoteUrl ? QString::fromUtf8(remoteUrl) : QString();
      git_remote_free(remote);
      return result;
    }
  }

  git_strarray remotes = {0};
  if (git_remote_list(&remotes, repo) == 0) {
    for (size_t i = 0; i < remotes.count; ++i) {
      git_remote *remote = nullptr;
      if (git_remote_lookup(&remote, repo, remotes.strings[i]) != 0 ||
          !remote) {
        continue;
      }

      const char *remoteUrl = git_remote_url(remote);
      const QString result =
          remoteUrl ? QString::fromUtf8(remoteUrl) : QString();
      git_remote_free(remote);
      git_strarray_dispose(&remotes);
      return result;
    }
    git_strarray_dispose(&remotes);
  }

  return {};
}

QString findConfiguredLfsUrl(git_repository *repo, const QString &remoteName) {
  if (!repo) {
    return {};
  }

  const QString explicitUrl = getConfigString(repo, "lfs.url");
  if (!explicitUrl.isEmpty()) {
    return explicitUrl;
  }

  const QString preferredRemoteName =
      remoteName.isEmpty() ? QStringLiteral("origin") : remoteName;
  if (preferredRemoteName.isEmpty()) {
    return {};
  }

  const QString remoteUrlKey =
      QStringLiteral("remote.%1.lfsurl").arg(preferredRemoteName);
  const QString remotePushUrlKey =
      QStringLiteral("remote.%1.lfspushurl").arg(preferredRemoteName);

  const QString remoteSpecific =
      getConfigString(repo, remotePushUrlKey.toUtf8().constData());
  if (!remoteSpecific.isEmpty()) {
    return remoteSpecific;
  }

  const QString remoteDownloadUrl =
      getConfigString(repo, remoteUrlKey.toUtf8().constData());
  if (!remoteDownloadUrl.isEmpty()) {
    return remoteDownloadUrl;
  }

  return {};
}

QString findRemoteUrl(git_repository *repo, const QString &remoteName) {
  if (!repo) {
    return {};
  }

  const QString configuredUrl = findConfiguredLfsUrl(repo, remoteName);
  if (!configuredUrl.isEmpty()) {
    return configuredUrl;
  }

  const QString preferredRemoteName =
      remoteName.isEmpty() ? QStringLiteral("origin") : remoteName;

  git_remote *remote = nullptr;
  if (!preferredRemoteName.isEmpty() &&
      git_remote_lookup(&remote, repo,
                        preferredRemoteName.toUtf8().constData()) == 0 &&
      remote) {
    const char *remoteUrl = git_remote_url(remote);
    const QString result = remoteUrl ? QString::fromUtf8(remoteUrl) : QString();
    git_remote_free(remote);
    return deriveLfsUrlFromRemote(result);
  }

  git_strarray remotes = {0};
  if (git_remote_list(&remotes, repo) == 0) {
    for (size_t i = 0; i < remotes.count; ++i) {
      if (git_remote_lookup(&remote, repo, remotes.strings[i]) != 0 ||
          !remote) {
        continue;
      }

      const char *remoteUrl = git_remote_url(remote);
      const QString result =
          remoteUrl ? QString::fromUtf8(remoteUrl) : QString();
      git_remote_free(remote);

      const QString derived = deriveLfsUrlFromRemote(result);
      if (!derived.isEmpty()) {
        git_strarray_dispose(&remotes);
        return derived;
      }
    }
    git_strarray_dispose(&remotes);
  }

  return {};
}

struct NetworkResult {
  int statusCode = -1;
  QByteArray body;
  QByteArray errorString;
};

struct TransferEndpoint {
  QString url;
  QJsonObject headers;
  bool allowCredentialFallback = true;
};

NetworkResult doRequest(QNetworkAccessManager &manager,
                        const QNetworkRequest &request, const QByteArray &body,
                        const QString &method) {
  QEventLoop loop;
  QNetworkReply *reply = nullptr;

  if (method == QLatin1String("POST")) {
    reply = manager.post(request, body);
  } else if (method == QLatin1String("PUT")) {
    reply = manager.put(request, body);
  } else {
    reply = manager.get(request);
  }

  QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
  QObject::connect(reply, &QNetworkReply::errorOccurred, &loop,
                   &QEventLoop::quit);
  loop.exec();

  NetworkResult result;
  result.statusCode =
      reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
  result.body = reply->readAll();
  result.errorString = reply->errorString().toUtf8();
  reply->deleteLater();
  return result;
}

QNetworkRequest makeRequest(const QString &url, const QJsonObject &headers) {
  QNetworkRequest request{QUrl(url)};
  request.setRawHeader("Accept", "application/vnd.git-lfs+json");
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/json"));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);

  for (auto it = headers.begin(); it != headers.end(); ++it) {
    const QByteArray key = it.key().toUtf8();
    const QString value = it.value().toString();
    if (!value.isEmpty()) {
      request.setRawHeader(key, value.toUtf8());
    }
  }

  return request;
}

Pointer parsePointerBytes(const QByteArray &data) {
  Pointer pointer;
  const QList<QByteArray> lines = data.split('\n');
  QString version;
  QString oid;
  QString size;

  for (QByteArray line : lines) {
    line = line.trimmed();
    if (line.isEmpty()) {
      continue;
    }

    if (line.startsWith(POINTER_VERSION)) {
      version = QString::fromLatin1(line);
    } else if (line.startsWith(POINTER_OID_PREFIX)) {
      oid = QString::fromLatin1(line);
    } else if (line.startsWith(POINTER_SIZE_PREFIX)) {
      size = QString::fromLatin1(line.mid(sizeof(POINTER_SIZE_PREFIX) - 1));
    }
  }

  if (version != QLatin1String(POINTER_VERSION)) {
    pointer.errorMessage = QStringLiteral("Invalid LFS pointer version");
    return pointer;
  }

  if (!oid.startsWith("oid sha256:") || oid.size() != 75) {
    pointer.errorMessage = QStringLiteral("Invalid LFS pointer oid");
    return pointer;
  }

  bool ok = false;
  const qint64 parsedSize = size.toLongLong(&ok);
  if (!ok || parsedSize < 0) {
    pointer.errorMessage = QStringLiteral("Invalid LFS pointer size");
    return pointer;
  }

  pointer.oid = oid.mid(QStringLiteral("oid sha256:").size());
  pointer.size = parsedSize;
  pointer.valid = true;
  return pointer;
}

class CleanStream final {
public:
  CleanStream(git_repository *repo, git_writestream *next)
      : repo(repo), next(next) {
    const QString root = objectsRootPath(repo);
    QDir().mkpath(root);
    tempFile.reset(new QTemporaryFile(QDir(root).filePath("lfs-clean-XXXXXX")));
    tempFile->setAutoRemove(false);
    if (!tempFile->open()) {
      tempFile.reset();
    }
  }

  int write(const char *buffer, size_t len) {
    if (!tempFile) {
      git_error_set_str(GIT_ERROR_OS, "unable to open temporary lfs file");
      return -1;
    }

    const qint64 written = tempFile->write(buffer, static_cast<qint64>(len));
    if (written != static_cast<qint64>(len)) {
      git_error_set_str(GIT_ERROR_OS, "unable to write lfs content");
      return -1;
    }

    hasher.addData(QByteArrayView(buffer, static_cast<qsizetype>(len)));
    size += static_cast<qint64>(len);
    return 0;
  }

  int close() {
    if (closed) {
      return 0;
    }
    closed = true;

    if (!tempFile) {
      git_error_set_str(GIT_ERROR_OS, "unable to prepare lfs object");
      return -1;
    }

    tempFile->flush();
    tempFile->close();

    const QString oid = QString::fromLatin1(hasher.result().toHex());
    const QString finalPath = objectPath(repo, oid);
    QDir().mkpath(QFileInfo(finalPath).absolutePath());

    if (!QFile::exists(finalPath)) {
      QFile::remove(finalPath);
      if (!QFile::copy(tempFile->fileName(), finalPath)) {
        git_error_set_str(GIT_ERROR_OS, "unable to store lfs object");
        QFile::remove(tempFile->fileName());
        return -1;
      }
      QFile::remove(tempFile->fileName());
    } else {
      QFile::remove(tempFile->fileName());
    }

    const QByteArray pointer = makePointer(oid, size);
    if (!writeAll(next, pointer)) {
      git_error_set_str(GIT_ERROR_OS, "unable to write lfs pointer");
      return -1;
    }

    return 0;
  }

  void destroy() {
    if (tempFile) {
      const QString name = tempFile->fileName();
      tempFile->close();
      QFile::remove(name);
    }
  }

private:
  git_repository *repo = nullptr;
  git_writestream *next = nullptr;
  std::unique_ptr<QTemporaryFile> tempFile;
  QCryptographicHash hasher{QCryptographicHash::Sha256};
  qint64 size = 0;
  bool closed = false;
};

class SmudgeStream final {
public:
  SmudgeStream(git_repository *repo, git_writestream *next)
      : repo(repo), next(next) {}

  int write(const char *buffer, size_t len) {
    raw.append(buffer, static_cast<int>(len));
    return 0;
  }

  int close() {
    if (closed) {
      return 0;
    }
    closed = true;

    const Pointer pointer = parsePointerBytes(raw);
    if (!pointer.valid) {
      const QByteArray errorBytes = pointer.errorMessage.toUtf8();
      git_error_set_str(GIT_ERROR_FILTER, errorBytes.constData());
      return -1;
    }

    QByteArray content;
    if (objectExists(repo, pointer.oid)) {
      if (!readObject(repo, pointer.oid, content)) {
        return -1;
      }
    } else if (!downloadObject(repo, pointer.oid, pointer.size, content)) {
      return -1;
    }

    if (content.size() != pointer.size) {
      git_error_set_str(GIT_ERROR_FILTER,
                        "downloaded lfs object has wrong size");
      return -1;
    }

    if (!writeAll(next, content)) {
      git_error_set_str(GIT_ERROR_OS, "unable to write lfs content");
      return -1;
    }

    return 0;
  }

  void destroy() {}

private:
  git_repository *repo = nullptr;
  git_writestream *next = nullptr;
  QByteArray raw;
  bool closed = false;
};

struct FilterStream final : git_writestream {
  explicit FilterStream(std::unique_ptr<CleanStream> cleanStream)
      : clean(std::move(cleanStream)) {
    write = [](git_writestream *stream, const char *buffer, size_t len) -> int {
      return static_cast<FilterStream *>(stream)->clean->write(buffer, len);
    };
    close = [](git_writestream *stream) -> int {
      return static_cast<FilterStream *>(stream)->clean->close();
    };
    free = [](git_writestream *stream) {
      auto *self = static_cast<FilterStream *>(stream);
      if (self->clean) {
        self->clean->destroy();
        self->clean.reset();
      }
      delete self;
    };
  }

  explicit FilterStream(std::unique_ptr<SmudgeStream> smudgeStream)
      : smudge(std::move(smudgeStream)) {
    write = [](git_writestream *stream, const char *buffer, size_t len) -> int {
      return static_cast<FilterStream *>(stream)->smudge->write(buffer, len);
    };
    close = [](git_writestream *stream) -> int {
      return static_cast<FilterStream *>(stream)->smudge->close();
    };
    free = [](git_writestream *stream) {
      auto *self = static_cast<FilterStream *>(stream);
      if (self->smudge) {
        self->smudge->destroy();
        self->smudge.reset();
      }
      delete self;
    };
  }

  std::unique_ptr<CleanStream> clean;
  std::unique_ptr<SmudgeStream> smudge;
};

int lfsFilterStream(git_writestream **out, git_filter *, void **,
                    const git_filter_source *src, git_writestream *next) {
  if (!out || !src || !next) {
    git_error_set_str(GIT_ERROR_INVALID, "invalid lfs filter arguments");
    return -1;
  }

  git_repository *repo = git_filter_source_repo(src);
  const auto mode = git_filter_source_mode(src);

  if (mode == GIT_FILTER_TO_ODB) {
    *out = new FilterStream(std::make_unique<CleanStream>(repo, next));
  } else {
    *out = new FilterStream(std::make_unique<SmudgeStream>(repo, next));
  }

  return 0;
}

} // namespace

bool initialize() {
  QMutexLocker locker(&gMutex);
  ++gRefCount;
  if (gFilter) {
    return true;
  }

  gFilter = new git_filter{};
  if (git_filter_init(gFilter, GIT_FILTER_VERSION) != 0) {
    delete gFilter;
    gFilter = nullptr;
    --gRefCount;
    return false;
  }

  gFilter->attributes = "filter=lfs";
  gFilter->stream = &lfsFilterStream;

  if (git_filter_register(FILTER_NAME, gFilter, 250) != 0) {
    delete gFilter;
    gFilter = nullptr;
    --gRefCount;
    return false;
  }

  return true;
}

void shutdown() {
  QMutexLocker locker(&gMutex);
  if (gRefCount > 0) {
    --gRefCount;
  }

  if (gRefCount > 0 || !gFilter) {
    return;
  }

  git_filter_unregister(FILTER_NAME);
  delete gFilter;
  gFilter = nullptr;
}

Pointer parsePointer(const QByteArray &data) {
  return parsePointerBytes(data);
}

QByteArray makePointer(const QString &oid, qint64 size) {
  QByteArray result;
  result += POINTER_VERSION;
  result += "\n";
  result += POINTER_OID_PREFIX;
  result += oid.toLatin1();
  result += "\n";
  result += POINTER_SIZE_PREFIX;
  result += QByteArray::number(size);
  result += "\n";
  return result;
}

QString repositoryRootPath(git_repository *repo) {
  if (!repo) {
    return {};
  }

  if (git_repository_is_bare(repo)) {
    const char *path = git_repository_path(repo);
    return path ? QDir::cleanPath(QString::fromUtf8(path)) : QString();
  }

  return normalizeRepoRoot(repoWorkdirPath(repo));
}

QString repositoryRootPath(const QString &repoPath) {
  return normalizeRepoRoot(repoPath);
}

QString objectsRootPath(git_repository *repo) {
  const QString root = repositoryRootPath(repo);
  if (root.isEmpty()) {
    return {};
  }

  const QString gitDir =
      git_repository_workdir(repo) ? QDir(root).filePath(".git") : root;
  return QDir(gitDir).filePath("lfs/objects");
}

QString objectsRootPath(const QString &repoPath) {
  const QString root = repositoryRootPath(repoPath);
  if (root.isEmpty()) {
    return {};
  }

  return QDir(root).filePath(".git/lfs/objects");
}

QString objectPath(git_repository *repo, const QString &oid) {
  const QString root = objectsRootPath(repo);
  if (root.isEmpty() || oid.size() < 4) {
    return {};
  }

  return QDir(QDir(root).filePath(oid.left(2)))
      .filePath(oid.mid(2, 2) + "/" + oid);
}

GitRepo::Lfs::StorageStatus getStorageStatus(git_repository *repo) {
  GitRepo::Lfs::StorageStatus status;
  status.repositoryPath = repositoryRootPath(repo);

  const QString root = objectsRootPath(repo);
  if (root.isEmpty()) {
    return status;
  }

  QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
  while (it.hasNext()) {
    const QString path = it.next();
    QFileInfo info(path);
    status.totalBytes += info.size();
    ++status.objectCount;
  }

  return status;
}

bool objectExists(git_repository *repo, const QString &oid) {
  const QString path = objectPath(repo, oid);
  return !path.isEmpty() && QFile::exists(path);
}

bool readObject(git_repository *repo, const QString &oid, QByteArray &data) {
  const QString path = objectPath(repo, oid);
  if (path.isEmpty()) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    git_error_set_str(GIT_ERROR_OS, "unable to read lfs object");
    return false;
  }

  data = file.readAll();
  return true;
}

bool storeObject(git_repository *repo, const QByteArray &data, QString &oid) {
  QCryptographicHash hash(QCryptographicHash::Sha256);
  hash.addData(data);
  oid = QString::fromLatin1(hash.result().toHex());

  const QString finalPath = objectPath(repo, oid);
  if (finalPath.isEmpty()) {
    git_error_set_str(GIT_ERROR_FILTER, "unable to determine lfs object path");
    return false;
  }

  QDir().mkpath(QFileInfo(finalPath).absolutePath());
  if (QFile::exists(finalPath)) {
    return true;
  }

  const QString tempPath = QFileInfo(finalPath).absolutePath() + "/.tmp-" + oid;
  QFile temp(tempPath);
  if (!temp.open(QIODevice::WriteOnly)) {
    git_error_set_str(GIT_ERROR_OS, "unable to open temp lfs object file");
    return false;
  }

  if (temp.write(data) != data.size() || !temp.flush()) {
    temp.close();
    QFile::remove(tempPath);
    git_error_set_str(GIT_ERROR_OS, "unable to write lfs object");
    return false;
  }
  temp.close();

  if (!QFile::copy(tempPath, finalPath)) {
    QFile::remove(tempPath);
    git_error_set_str(GIT_ERROR_OS, "unable to store lfs object");
    return false;
  }
  QFile::remove(tempPath);

  return true;
}

static bool resolveTransferEndpoint(git_repository *repo,
                                    const QString &repoPath,
                                    const QString &remoteName,
                                    const QString &operation,
                                    TransferEndpoint &endpoint,
                                    const QString &remoteUrlHint = QString()) {
  if (!remoteUrlHint.isEmpty()) {
    if (isHttpUrl(remoteUrlHint)) {
      const QString auth = getCredentialHeader(repoPath, remoteUrlHint);
      if (!auth.isEmpty()) {
        endpoint.headers.insert(QStringLiteral("Authorization"), auth);
      }
      endpoint.url = deriveLfsUrlFromRemote(remoteUrlHint);
      if (endpoint.url.isEmpty()) {
        git_error_set_str(GIT_ERROR_FILTER,
                          "unable to determine lfs endpoint for repository");
        return false;
      }
      return true;
    }

    if (isSshUrl(remoteUrlHint)) {
      RemoteInfo info;
      if (!parseSshRemoteUrl(remoteUrlHint, info)) {
        git_error_set_str(GIT_ERROR_FILTER,
                          "unable to parse ssh remote for lfs endpoint");
        return false;
      }

      endpoint.allowCredentialFallback = false;
      return runSshGitLfsAuthenticate(info, operation, endpoint.url,
                                      endpoint.headers);
    }
  }

  const QString configuredUrl = findConfiguredLfsUrl(repo, remoteName);
  if (!configuredUrl.isEmpty()) {
    endpoint.url = configuredUrl;
    if (isHttpUrl(configuredUrl)) {
      const QString auth = getCredentialHeader(repoPath, configuredUrl);
      if (!auth.isEmpty()) {
        endpoint.headers.insert(QStringLiteral("Authorization"), auth);
      }
    }
    return true;
  }

  const QString remoteUrl = findGitRemoteUrl(repo, remoteName);
  if (remoteUrl.isEmpty()) {
    git_error_set_str(GIT_ERROR_FILTER,
                      "unable to determine lfs endpoint for repository");
    return false;
  }

  if (isHttpUrl(remoteUrl)) {
    endpoint.url = deriveLfsUrlFromRemote(remoteUrl);
    if (endpoint.url.isEmpty()) {
      git_error_set_str(GIT_ERROR_FILTER,
                        "unable to determine lfs endpoint for repository");
      return false;
    }

    const QString auth = getCredentialHeader(repoPath, endpoint.url);
    if (!auth.isEmpty()) {
      endpoint.headers.insert(QStringLiteral("Authorization"), auth);
    }
    return true;
  }

  if (isSshUrl(remoteUrl)) {
    RemoteInfo info;
    if (!parseSshRemoteUrl(remoteUrl, info)) {
      git_error_set_str(GIT_ERROR_FILTER,
                        "unable to parse ssh remote for lfs endpoint");
      return false;
    }

    endpoint.allowCredentialFallback = false;
    return runSshGitLfsAuthenticate(info, operation, endpoint.url,
                                    endpoint.headers);
  }

  git_error_set_str(GIT_ERROR_FILTER,
                    "unable to determine lfs endpoint for repository");
  return false;
}

static bool requestBasicTransfer(git_repository *repo, const QString &repoPath,
                                 const QString &remoteName,
                                 const QString &operation,
                                 const QJsonArray &objects,
                                 QJsonObject &responseRoot,
                                 const QString &remoteUrlHint = QString(),
                                 TransferEndpoint *resolvedEndpoint = nullptr) {
  TransferEndpoint endpoint;
  if (!resolveTransferEndpoint(repo, repoPath, remoteName, operation, endpoint,
                               remoteUrlHint)) {
    return false;
  }
  if (resolvedEndpoint) {
    *resolvedEndpoint = endpoint;
  }

  QNetworkAccessManager manager;
  const QUrl batchUrl(endpoint.url + "/objects/batch");
  QNetworkRequest request = makeRequest(batchUrl.toString(), endpoint.headers);

  QJsonObject body;
  QJsonArray transfers;
  transfers.append(QStringLiteral("basic"));
  body.insert("operation", operation);
  body.insert("transfers", transfers);
  body.insert("objects", objects);

  const QByteArray requestBody = QJsonDocument(body).toJson();
  LOG_DEBUG("LFS batch request: url='{}', operation='{}', objectCount={}",
            batchUrl.toString().toStdString(), operation.toStdString(),
            objects.size());
  const NetworkResult result = doRequest(manager, request, requestBody, "POST");
  LOG_DEBUG("LFS batch response: url='{}', status={}, error='{}', bodySize={}",
            batchUrl.toString().toStdString(), result.statusCode,
            result.errorString.toStdString(), result.body.size());
  if (result.statusCode < 200 || result.statusCode >= 300) {
    const QByteArray errorBytes = result.errorString.isEmpty()
                                      ? QByteArray("lfs batch request failed")
                                      : result.errorString;
    git_error_set_str(GIT_ERROR_NET, errorBytes.constData());
    return false;
  }

  const QJsonDocument response = QJsonDocument::fromJson(result.body);
  if (!response.isObject()) {
    git_error_set_str(GIT_ERROR_NET, "invalid lfs batch response");
    return false;
  }

  responseRoot = response.object();
  return true;
}

bool downloadObject(git_repository *repo, const QString &oid, qint64 size,
                    QByteArray &data) {
  QJsonArray objects;
  objects.append(QJsonObject{{"oid", oid}, {"size", size}});
  TransferEndpoint endpoint;
  QJsonObject response;
  const QString repoPath = repositoryRootPath(repo);
  if (!requestBasicTransfer(repo, repoPath, QString(),
                            QStringLiteral("download"), objects, response,
                            QString(), &endpoint)) {
    return false;
  }

  const QJsonArray transferObjects = response.value("objects").toArray();
  if (transferObjects.isEmpty()) {
    git_error_set_str(GIT_ERROR_NET, "lfs server returned no download action");
    return false;
  }

  const QJsonObject object = transferObjects.first().toObject();
  const QJsonObject actions = object.value("actions").toObject();
  const QJsonObject download = actions.value("download").toObject();
  const QString href = download.value("href").toString();
  if (href.isEmpty()) {
    git_error_set_str(GIT_ERROR_NET,
                      "lfs server did not provide a download url");
    return false;
  }

  QString auth;
  if (endpoint.allowCredentialFallback) {
    auth = getCredentialHeader(repoPath, href);
  }
  QNetworkAccessManager manager;
  QNetworkRequest request{QUrl(href)};
  const QJsonObject headers = download.value("header").toObject();
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
  }
  if (!headers.contains(QStringLiteral("Authorization")) && !auth.isEmpty()) {
    request.setRawHeader("Authorization", auth.toUtf8());
  }

  const NetworkResult result = doRequest(manager, request, QByteArray(), "GET");
  if (result.statusCode < 200 || result.statusCode >= 300) {
    const QByteArray errorBytes = result.errorString.isEmpty()
                                      ? QByteArray("lfs object download failed")
                                      : result.errorString;
    git_error_set_str(GIT_ERROR_NET, errorBytes.constData());
    return false;
  }

  data = result.body;
  QString storedOid;
  if (!storeObject(repo, data, storedOid) || storedOid != oid) {
    git_error_set_str(GIT_ERROR_FILTER,
                      "downloaded lfs object failed checksum");
    return false;
  }

  return true;
}

bool uploadLocalObjects(git_repository *repo, const QString &repoPath,
                        const QString &remoteName, const QString &remoteUrlHint,
                        const QString &refName) {
  const QString root = objectsRootPath(repoPath);
  if (root.isEmpty()) {
    return true;
  }

  LOG_DEBUG("Scanning LFS objects for repoPath '{}' using ref '{}'",
            repoPath.toStdString(), refName.toStdString());

  QHash<QString, LfsUploadCandidate> candidates;
  const int refCandidateCountBefore = candidates.size();
  const bool refScanSuccess =
      refName.isEmpty()
          ? collectLfsObjectsFromAllRefs(repo, repoPath, &candidates)
          : collectLfsObjectsFromRef(repo, repoPath, refName, &candidates);
  const int refCandidateCountAfter = candidates.size();
  const QString refLabel =
      refName.isEmpty() ? QStringLiteral("<all refs>") : refName;
  LOG_DEBUG("LFS ref scan for '{}' produced {} candidate(s)",
            refLabel.toStdString(),
            refCandidateCountAfter - refCandidateCountBefore);
  if (!refScanSuccess) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to collect LFS upload candidates from '{}'",
              refLabel.toStdString());
    return false;
  }

  const int indexCandidateCountBefore = candidates.size();
  const bool indexScanSuccess =
      collectLfsObjectsFromIndex(repo, repoPath, &candidates);
  const int indexCandidateCountAfter = candidates.size();
  LOG_DEBUG("LFS index scan produced {} candidate(s)",
            indexCandidateCountAfter - indexCandidateCountBefore);
  if (!indexScanSuccess) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to collect LFS upload candidates from the index");
    return false;
  }

  if (candidates.isEmpty()) {
    LOG_DEBUG("No LFS upload candidates found for repoPath '{}' and ref '{}'",
              repoPath.toStdString(), refName.toStdString());
    return true;
  }

  LOG_DEBUG(
      "Prepared {} LFS upload candidate(s) for repoPath '{}' and ref '{}'",
      candidates.size(), repoPath.toStdString(), refName.toStdString());
  for (const auto &candidate : candidates) {
    LOG_DEBUG("LFS upload candidate: oid='{}', path='{}', source='{}', size={}",
              candidate.oid.toStdString(), candidate.path.toStdString(),
              candidate.source.toStdString(), candidate.size);
  }

  QJsonArray objects;
  for (const LfsUploadCandidate &candidate : candidates) {
    objects.append(
        QJsonObject{{"oid", candidate.oid}, {"size", candidate.size}});
  }

  TransferEndpoint endpoint;
  QJsonObject response;
  if (!requestBasicTransfer(repo, repoPath, remoteName,
                            QStringLiteral("upload"), objects, response,
                            remoteUrlHint, &endpoint)) {
    return false;
  }

  const QJsonArray responseObjects = response.value("objects").toArray();
  QHash<QString, QJsonObject> responseByOid;
  for (const auto &value : responseObjects) {
    const QJsonObject object = value.toObject();
    responseByOid.insert(object.value("oid").toString(), object);
  }

  QNetworkAccessManager manager;
  for (const LfsUploadCandidate &candidate : candidates) {
    QFile file(candidate.path);
    if (!file.open(QIODevice::ReadOnly)) {
      continue;
    }

    const QByteArray bytes = file.readAll();
    file.close();

    const int bytesLen = bytes.length();

    const QJsonObject object = responseByOid.value(candidate.oid);
    const QJsonObject actions = object.value("actions").toObject();
    const QJsonObject upload = actions.value("upload").toObject();
    const QString href = upload.value("href").toString();
    if (href.isEmpty()) {
      continue;
    }

    const QJsonObject verify = actions.value("verify").toObject();

    QNetworkRequest request{QUrl(href)};
    const QJsonObject headers = upload.value("header").toObject();
    for (auto itHeader = headers.begin(); itHeader != headers.end();
         ++itHeader) {
      request.setRawHeader(itHeader.key().toUtf8(),
                           itHeader.value().toString().toUtf8());
    }
    if (endpoint.allowCredentialFallback &&
        !headers.contains(QStringLiteral("Authorization"))) {
      const QString auth = getCredentialHeader(repoPath, href);
      if (!auth.isEmpty()) {
        request.setRawHeader("Authorization", auth.toUtf8());
      }
    }

    LOG_DEBUG("LFS upload PUT: oid='{}', url='{}', bytes={}",
              candidate.oid.toStdString(), href.toStdString(), bytesLen);
    const NetworkResult result = doRequest(manager, request, bytes, "PUT");
    LOG_DEBUG("LFS upload PUT response: oid='{}', url='{}', status={}, "
              "error='{}', bodySize={}",
              candidate.oid.toStdString(), href.toStdString(),
              result.statusCode, result.errorString.toStdString(),
              result.body.size());
    if (result.statusCode < 200 || result.statusCode >= 300) {
      git_error_set_str(GIT_ERROR_NET, "lfs object upload failed");
      return false;
    }

    const QString verifyHref = verify.value("href").toString();
    if (!verifyHref.isEmpty()) {
      const QJsonObject verifyHeaders = verify.value("header").toObject();
      QNetworkRequest verifyRequest = makeRequest(verifyHref, verifyHeaders);
      verifyRequest.setRawHeader("Accept", "application/vnd.git-lfs+json");
      verifyRequest.setRawHeader("Content-Type",
                                 "application/vnd.git-lfs+json");
      for (auto itHeader = verifyHeaders.begin();
           itHeader != verifyHeaders.end(); ++itHeader) {
        verifyRequest.setRawHeader(itHeader.key().toUtf8(),
                                   itHeader.value().toString().toUtf8());
      }
      if (endpoint.allowCredentialFallback &&
          !verifyHeaders.contains(QStringLiteral("Authorization"))) {
        const QString auth = getCredentialHeader(repoPath, verifyHref);
        if (!auth.isEmpty()) {
          verifyRequest.setRawHeader("Authorization", auth.toUtf8());
        }
      }

      const QJsonObject verifyBody{{"oid", candidate.oid},
                                   {"size", candidate.size}};
      LOG_DEBUG("LFS verify POST: oid='{}', url='{}'",
                candidate.oid.toStdString(), verifyHref.toStdString());
      const NetworkResult verifyResult = doRequest(
          manager, verifyRequest,
          QJsonDocument(verifyBody).toJson(QJsonDocument::Compact), "POST");
      LOG_DEBUG("LFS verify POST response: oid='{}', url='{}', status={}, "
                "error='{}', bodySize={}",
                candidate.oid.toStdString(), verifyHref.toStdString(),
                verifyResult.statusCode, verifyResult.errorString.toStdString(),
                verifyResult.body.size());
      if (verifyResult.statusCode < 200 || verifyResult.statusCode >= 300) {
        git_error_set_str(GIT_ERROR_NET, "lfs object verify failed");
        return false;
      }
    }
  }

  return true;
}

} // namespace GitRepo::Lfs
