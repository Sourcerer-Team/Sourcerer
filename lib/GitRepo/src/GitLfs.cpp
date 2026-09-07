#include "GitLfs.h"

#include <GitCredentialStorage.h>
#include <Log.h>

#include <git2/config.h>
#include <git2/errors.h>
#include <git2/filter.h>
#include <git2/remote.h>
#include <git2/sys/errors.h>
#include <git2/sys/filter.h>

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
#include <QTemporaryFile>
#include <QUrl>
#include <QUrlQuery>

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

QString deriveLfsUrlFromRemote(const QString &remoteUrl) {
  if (!isHttpUrl(remoteUrl)) {
    return {};
  }

  QString normalized = remoteUrl;
  while (normalized.endsWith('/')) {
    normalized.chop(1);
  }

  return normalized + "/info/lfs";
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

QString getCredentialHeader(git_repository *repo) {
  const QString root = repositoryRootPath(repo);
  if (root.isEmpty()) {
    return {};
  }

  QString username;
  QString password;
  if (!GitCredentialStorage::getInstance().getCredentials(root, username,
                                                          password)) {
    return {};
  }

  const QByteArray auth =
      QString("%1:%2").arg(username, password).toUtf8().toBase64();
  return QString::fromLatin1("Basic ") + QString::fromLatin1(auth);
}

QString findRemoteUrl(git_repository *repo, const QString &remoteName) {
  if (!repo) {
    return {};
  }

  const QString explicitUrl = getConfigString(repo, "lfs.url");
  if (!explicitUrl.isEmpty()) {
    return explicitUrl;
  }

  const QString preferredRemoteName =
      remoteName.isEmpty() ? QStringLiteral("origin") : remoteName;

  if (!preferredRemoteName.isEmpty()) {
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
  }

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

QNetworkRequest makeRequest(const QString &url, const QString &authorization) {
  QNetworkRequest request{QUrl(url)};
  request.setRawHeader("Accept", "application/vnd.git-lfs+json");
  request.setHeader(QNetworkRequest::ContentTypeHeader,
                    QStringLiteral("application/vnd.git-lfs+json"));
  request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                       QNetworkRequest::NoLessSafeRedirectPolicy);
  if (!authorization.isEmpty()) {
    request.setRawHeader("Authorization", authorization.toUtf8());
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
      oid = QString::fromLatin1(line.mid(sizeof(POINTER_OID_PREFIX) - 1));
    } else if (line.startsWith(POINTER_SIZE_PREFIX)) {
      size = QString::fromLatin1(line.mid(sizeof(POINTER_SIZE_PREFIX) - 1));
    }
  }

  if (version != QLatin1String(POINTER_VERSION)) {
    pointer.errorMessage = QStringLiteral("Invalid LFS pointer version");
    return pointer;
  }

  if (!oid.startsWith("sha256:") || oid.size() != 71) {
    pointer.errorMessage = QStringLiteral("Invalid LFS pointer oid");
    return pointer;
  }

  bool ok = false;
  const qint64 parsedSize = size.toLongLong(&ok);
  if (!ok || parsedSize < 0) {
    pointer.errorMessage = QStringLiteral("Invalid LFS pointer size");
    return pointer;
  }

  pointer.oid = oid.mid(QStringLiteral("sha256:").size());
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

    hasher.addData(buffer, static_cast<int>(len));
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
      if (!QFile::rename(tempFile->fileName(), finalPath)) {
        git_error_set_str(GIT_ERROR_OS, "unable to store lfs object");
        QFile::remove(tempFile->fileName());
        return -1;
      }
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

QString objectsRootPath(git_repository *repo) {
  const QString root = repositoryRootPath(repo);
  if (root.isEmpty()) {
    return {};
  }

  const QString gitDir =
      git_repository_workdir(repo) ? QDir(root).filePath(".git") : root;
  return QDir(gitDir).filePath("lfs/objects");
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

  if (!QFile::rename(tempPath, finalPath)) {
    QFile::remove(tempPath);
    git_error_set_str(GIT_ERROR_OS, "unable to store lfs object");
    return false;
  }

  return true;
}

static bool requestBasicTransfer(git_repository *repo,
                                 const QString &remoteName,
                                 const QString &operation,
                                 const QJsonArray &objects,
                                 QJsonObject &responseRoot) {
  const QString baseUrl = findRemoteUrl(repo, remoteName);
  if (baseUrl.isEmpty()) {
    git_error_set_str(GIT_ERROR_FILTER,
                      "unable to determine lfs endpoint for repository");
    return false;
  }

  const QString auth = getCredentialHeader(repo);
  QNetworkAccessManager manager;
  const QUrl batchUrl(baseUrl + "/objects/batch");
  QNetworkRequest request = makeRequest(batchUrl.toString(), auth);

  QJsonObject body;
  QJsonArray transfers;
  transfers.append(QStringLiteral("basic"));
  body.insert("operation", operation);
  body.insert("transfers", transfers);
  body.insert("objects", objects);

  const NetworkResult result =
      doRequest(manager, request, QJsonDocument(body).toJson(), "POST");
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
  QJsonObject response;
  if (!requestBasicTransfer(repo, QString(), QStringLiteral("download"),
                            objects, response)) {
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

  QNetworkAccessManager manager;
  QNetworkRequest request{QUrl(href)};
  const QJsonObject headers = download.value("header").toObject();
  for (auto it = headers.begin(); it != headers.end(); ++it) {
    request.setRawHeader(it.key().toUtf8(), it.value().toString().toUtf8());
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

bool uploadLocalObjects(git_repository *repo, const QString &remoteName) {
  const QString root = objectsRootPath(repo);
  if (root.isEmpty()) {
    return true;
  }

  QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
  QJsonArray objects;
  QList<QPair<QString, QString>> localObjects;

  while (it.hasNext()) {
    const QString path = it.next();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      continue;
    }

    const qint64 size = file.size();
    file.close();

    const QString oid = QFileInfo(path).fileName();
    if (oid.size() != 64 ||
        !QRegularExpression(QStringLiteral("^[0-9a-fA-F]{64}$"))
             .match(oid)
             .hasMatch()) {
      continue;
    }

    objects.append(QJsonObject{{"oid", oid}, {"size", size}});
    localObjects.append(qMakePair(oid, path));
  }

  if (objects.isEmpty()) {
    return true;
  }

  QJsonObject response;
  if (!requestBasicTransfer(repo, remoteName, QStringLiteral("upload"), objects,
                            response)) {
    return false;
  }

  const QJsonArray responseObjects = response.value("objects").toArray();
  QHash<QString, QJsonObject> responseByOid;
  for (const auto &value : responseObjects) {
    const QJsonObject object = value.toObject();
    responseByOid.insert(object.value("oid").toString(), object);
  }

  QNetworkAccessManager manager;
  for (const auto &localObject : localObjects) {
    const QString oid = localObject.first;
    const QString path = localObject.second;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
      continue;
    }

    const QByteArray bytes = file.readAll();
    file.close();

    const QJsonObject object = responseByOid.value(oid);
    const QJsonObject actions = object.value("actions").toObject();
    const QJsonObject upload = actions.value("upload").toObject();
    const QString href = upload.value("href").toString();
    if (href.isEmpty()) {
      continue;
    }

    QNetworkRequest request{QUrl(href)};
    const QJsonObject headers = upload.value("header").toObject();
    for (auto itHeader = headers.begin(); itHeader != headers.end();
         ++itHeader) {
      request.setRawHeader(itHeader.key().toUtf8(),
                           itHeader.value().toString().toUtf8());
    }

    const NetworkResult result = doRequest(manager, request, bytes, "PUT");
    if (result.statusCode < 200 || result.statusCode >= 300) {
      git_error_set_str(GIT_ERROR_NET, "lfs object upload failed");
      return false;
    }
  }

  return true;
}

} // namespace GitRepo::Lfs
