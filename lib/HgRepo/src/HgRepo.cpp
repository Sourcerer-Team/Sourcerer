// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimeZone>

#include <algorithm>
#include <cstring>

#define THREAD_ID "HgRepo"

namespace HgRepo {

namespace {

constexpr int kOneShotTimeoutMs = 10 * 60 * 1000;
constexpr int kOneShotSliceMs = 100;

thread_local HgErrorContext gLastError;
thread_local const bool *gQuitFlag = nullptr;

QMutex gExecutableMutex;
QString gExecutablePath;

// Last lines of hg's error output, for error messages.
QString lastLines(const QByteArray &output, int count = 8) {
  QStringList lines =
      QString::fromUtf8(output).remove('\r').split('\n', Qt::SkipEmptyParts);
  if (lines.size() > count) {
    lines = lines.mid(lines.size() - count);
  }
  return lines.join('\n').trimmed();
}

QString commandErrorMessage(const QStringList &args,
                            const Private::HgCommandResult &result,
                            const QString &serverError) {
  QString message = lastLines(result.err);
  if (message.isEmpty()) {
    message = lastLines(result.out);
  }
  if (message.isEmpty()) {
    message = serverError;
  }
  if (message.isEmpty()) {
    message = QStringLiteral("hg %1 failed with exit code %2")
                  .arg(args.value(0))
                  .arg(result.exitCode);
  }
  return Private::withSshHint(message);
}

// Global options of every command server.
QStringList serverConfigArgs(const QString &root) {
  QStringList configs = {
      QStringLiteral("ui.interactive=False"),
      // Conflict markers in the working file, like Git.
      QStringLiteral("ui.merge=internal:merge"),
      // A commit never recurses into subrepos silently.
      QStringLiteral("ui.commitsubrepos=False"),
      // Updates keep uncommitted changes unless they conflict, like a safe
      // Git checkout.
      QStringLiteral("commands.update.check=noconflict"),
      // Git and Subversion subrepos are off by default since Mercurial 4.4;
      // Sourcerer supports them. The transports Mercurial's git may use are
      // limited in hgEnvironment().
      QStringLiteral("subrepos.git:allowed=True"),
      QStringLiteral("subrepos.svn:allowed=True"),
      QStringLiteral("extensions.rebase="),
      QStringLiteral("extensions.strip="),
      QStringLiteral("extensions.share="),
      QStringLiteral("extensions.remotenames="),
      QStringLiteral("remotenames.bookmarks=True"),
      QStringLiteral("remotenames.branches=True"),
  };

  if (Private::hasRequirement(root, "lfs")) {
    configs << QStringLiteral("extensions.lfs=");
  }

  QStringList args;
  for (const QString &config : configs) {
    args << QStringLiteral("--config") << config;
  }
  return args;
}

// Files a running command server has read once and doesn't read again: the
// repository config and the remote names recorded by pulls and pushes.
QHash<QString, QPair<qint64, qint64>> serverStamps(const Repository *repo) {
  static const QStringList files = {
      QStringLiteral("/.hg/hgrc"),
      QStringLiteral("/.hg/logexchange/bookmarks"),
      QStringLiteral("/.hg/logexchange/branches"),
  };

  QHash<QString, QPair<qint64, qint64>> stamps;
  for (const QString &file : files) {
    const QFileInfo info(repo->root + file);
    stamps.insert(file, info.exists()
                            ? qMakePair(info.lastModified().toMSecsSinceEpoch(),
                                        info.size())
                            : qMakePair(qint64(-1), qint64(-1)));
  }
  return stamps;
}

bool startServer(Repository *repo) {
  if (repo->server && repo->server->isRunning()) {
    // A server that read outdated files is replaced.
    if (serverStamps(repo) == repo->serverStamps) {
      return true;
    }
    repo->server->stop();
    repo->server.reset();
  }

  repo->serverStamps = serverStamps(repo);
  // A new server may read another ui.ssh.
  repo->userSshKnown = false;

  const QString executable = findExecutable();
  if (executable.isEmpty()) {
    Private::setLastError(HgError::MercurialNotFound, -1,
                          QStringLiteral("Mercurial (hg) was not found. "
                                         "Install Mercurial or set its path "
                                         "in Settings."));
    return false;
  }

  repo->server = std::make_unique<Private::HgCommandServer>(
      executable, repo->root, serverConfigArgs(repo->root));
  if (!repo->server->start()) {
    Private::setLastError(HgError::ServerStartFailed, -1,
                          repo->server->errorMessage());
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to start the Mercurial command server for '{}': {}",
              repo->root.toStdString(),
              repo->server->errorMessage().toStdString());
    return false;
  }

  return true;
}

// Parses "7.1.2", "6.9+20-abc" or "5.0rc0" into major and minor.
bool parseVersion(const QString &version, int *major, int *minor) {
  static const QRegularExpression pattern(QStringLiteral("^(\\d+)\\.(\\d+)"));
  const QRegularExpressionMatch match = pattern.match(version);
  if (!match.hasMatch()) {
    return false;
  }
  *major = match.captured(1).toInt();
  *minor = match.captured(2).toInt();
  return true;
}

QString stagedPathsFile(const Repository *repo) {
  return repo->root + QStringLiteral("/.hg/sourcerer/staged");
}

} // namespace

namespace Private {

void setLastError(HgError error, int errorCode, const QString &errorMessage) {
  gLastError.error = error;
  gLastError.errorCode = errorCode;
  gLastError.errorMessage = errorMessage;
}

bool hasRequirement(const QString &root, const QByteArray &requirement) {
  const auto listed = [&](const QString &fileName) {
    QFile file(fileName);
    return file.open(QIODevice::ReadOnly) &&
           file.readAll().replace('\r', "").split('\n').contains(requirement);
  };

  QString store = root + QStringLiteral("/.hg/store");
  QFile sharedPath(root + QStringLiteral("/.hg/sharedpath"));
  if (sharedPath.open(QIODevice::ReadOnly)) {
    store = normalizedPath(QString::fromUtf8(sharedPath.readAll()).trimmed()) +
            QStringLiteral("/store");
  }

  return listed(root + QStringLiteral("/.hg/requires")) ||
         listed(store + QStringLiteral("/requires"));
}

bool setUnsupported(const char *operation) {
  setLastError(HgError::UnsupportedOperation, -1,
               QStringLiteral("%1 is not supported for Mercurial "
                              "repositories.")
                   .arg(QString::fromUtf8(operation)));
  return false;
}

QuitScope::QuitScope(const bool *quitFlag) : _previous(gQuitFlag) {
  gQuitFlag = quitFlag;
}

QuitScope::~QuitScope() {
  gQuitFlag = _previous;
}

namespace {

// Runs the command once, configArgs are global options put before it.
bool runCommandOnce(Repository *repo, HgError kind, const QStringList &args,
                    const QStringList &configArgs, HgCommandResult *result,
                    const RunOptions &options, bool allowExitCodeOne) {
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!startServer(repo)) {
      return false;
    }

    *result = repo->server->run(configArgs + args, options.input, gQuitFlag);

    if (result->cancelled) {
      setLastError(HgError::Cancelled, -1,
                   QStringLiteral("The operation was cancelled."));
      return false;
    }

    if (result->finished) {
      break;
    }

    // The server died or the pipe broke. A read-only command is safe to run
    // again on a new server.
    const QString serverError = repo->server->errorMessage();
    repo->server.reset();
    if (!options.readOnly || attempt > 0) {
      setLastError(HgError::ServerDied, -1, serverError);
      LOG_ERROR(Log::ErrorId::Git,
                "Mercurial command server failed running '{}': {}",
                args.value(0).toStdString(), serverError.toStdString());
      return false;
    }
  }

  if (result->exitCode == 0 || (allowExitCodeOne && result->exitCode == 1)) {
    return true;
  }

  setLastError(kind, result->exitCode,
               commandErrorMessage(args, *result, QString()));
  return false;
}

// URL of the remote a command talks to, for hg messages that don't name it:
// a URL argument, a path name argument or the default path.
QString remoteUrlOf(Repository *repo, const QStringList &args) {
  for (const QString &arg : args) {
    if (arg.contains(QLatin1String("://"))) {
      return arg;
    }
  }

  // Keeps the error of the command for the caller.
  const HgErrorContext error = getLastError();
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  QHash<QString, QString> urls;
  if (runCommandOnce(repo, HgError::RemoteFailed,
                     {QStringLiteral("paths"), QStringLiteral("--template"),
                      QStringLiteral("json")},
                     {}, &result, options, true)) {
    for (const QJsonValue &value :
         QJsonDocument::fromJson(result.out).array()) {
      const QJsonObject path = value.toObject();
      urls.insert(path.value("name").toString(), path.value("url").toString());
    }
  }
  setLastError(error.error, error.errorCode, error.errorMessage);

  for (const QString &arg : args) {
    if (urls.contains(arg)) {
      return urls.value(arg);
    }
  }
  return urls.value(QStringLiteral("default"));
}

} // namespace

bool runCommand(Repository *repo, HgError kind, const QStringList &args,
                HgCommandResult *result, const RunOptions &options,
                bool allowExitCodeOne) {
  if (!repo) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("The repository is not open."));
    return false;
  }

  const QStringList sshArgs = sshConfigArgs([repo]() {
    // A changed hgrc restarts the server first, which forgets ui.ssh.
    if (!startServer(repo) || !repo->userSshKnown) {
      // Keeps the error of an earlier command.
      const HgErrorContext error = getLastError();
      HgCommandResult userSsh;
      RunOptions readOnly;
      readOnly.readOnly = true;
      repo->userSsh =
          runCommandOnce(repo, HgError::CommandFailed,
                         {QStringLiteral("config"), QStringLiteral("ui.ssh")},
                         {}, &userSsh, readOnly, true)
              ? QString::fromUtf8(userSsh.out).trimmed()
              : QString();
      repo->userSshKnown = true;
      setLastError(error.error, error.errorCode, error.errorMessage);
    }
    return repo->userSsh;
  });

  return runAuthenticated(
      repo->root, &repo->auth, sshArgs,
      [&]() { return remoteUrlOf(repo, args); },
      [&](const QStringList &configArgs, QString *errorText) {
        const bool success = runCommandOnce(repo, kind, args, configArgs,
                                            result, options, allowExitCodeOne);
        if (!success) {
          *errorText = QString::fromUtf8(result->err + result->out);
        }
        return success;
      });
}

bool runJson(Repository *repo, HgError kind, const QStringList &args,
             QJsonArray *array, const RunOptions &options) {
  // Right after the command name, before any "--" ending the options.
  QStringList jsonArgs = args;
  jsonArgs.insert(1, QStringLiteral("json"));
  jsonArgs.insert(1, QStringLiteral("--template"));

  HgCommandResult result;
  if (!runCommand(repo, kind, jsonArgs, &result, options, true)) {
    return false;
  }

  // Exit code 1 means "no matches" for status-like commands; the output is
  // still a valid, empty array, or nothing at all.
  if (result.out.trimmed().isEmpty()) {
    *array = QJsonArray();
    return true;
  }

  QJsonParseError parseError;
  const QJsonDocument document =
      QJsonDocument::fromJson(result.out, &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isArray()) {
    setLastError(kind, result.exitCode,
                 QStringLiteral("Failed to parse the output of hg %1: %2")
                     .arg(args.value(0), parseError.errorString()));
    return false;
  }

  *array = document.array();
  return true;
}

namespace {

bool runOneShotOnce(HgError kind, const QStringList &args,
                    const QStringList &configArgs,
                    const QString &workingDirectory, HgCommandResult *result) {
  *result = HgCommandResult();

  const QString executable = findExecutable();
  if (executable.isEmpty()) {
    setLastError(HgError::MercurialNotFound, -1,
                 QStringLiteral("Mercurial (hg) was not found. Install "
                                "Mercurial or set its path in Settings."));
    return false;
  }

  QProcess process;
  process.setProgram(executable);
  process.setArguments(QStringList{QStringLiteral("--config"),
                                   QStringLiteral("ui.interactive=False"),
                                   QStringLiteral("--config"),
                                   QStringLiteral("extensions.share=")} +
                       configArgs + args);
  if (!workingDirectory.isEmpty()) {
    process.setWorkingDirectory(workingDirectory);
  }
  process.setProcessEnvironment(hgEnvironment());
  hideConsoleWindow(process);
  process.start(QIODevice::ReadOnly);

  if (!process.waitForStarted()) {
    setLastError(kind, -1,
                 QStringLiteral("Failed to start %1: %2")
                     .arg(executable, process.errorString()));
    return false;
  }

  int waited = 0;
  while (!process.waitForFinished(kOneShotSliceMs)) {
    if (process.state() == QProcess::NotRunning) {
      break;
    }
    waited += kOneShotSliceMs;
    if ((gQuitFlag && *gQuitFlag) || waited >= kOneShotTimeoutMs) {
      process.kill();
      process.waitForFinished();
      result->cancelled = gQuitFlag && *gQuitFlag;
      setLastError(result->cancelled ? HgError::Cancelled : kind, -1,
                   result->cancelled
                       ? QStringLiteral("The operation was cancelled.")
                       : QStringLiteral("hg %1 timed out.").arg(args.value(0)));
      return false;
    }
  }

  result->out = process.readAllStandardOutput();
  result->err = process.readAllStandardError();
  result->finished = process.exitStatus() == QProcess::NormalExit;
  result->exitCode = result->finished ? process.exitCode() : -1;

  if (result->finished && result->exitCode == 0) {
    return true;
  }

  setLastError(kind, result->exitCode,
               commandErrorMessage(args, *result, process.errorString()));
  return false;
}

// sshConfigArgs for an hg run outside a command server in workingDirectory.
QStringList oneShotSshArgs(const QString &workingDirectory) {
  return sshConfigArgs([&]() {
    const HgErrorContext error = getLastError();
    HgCommandResult result;
    const QString userSsh =
        runOneShotOnce(HgError::CommandFailed,
                       {QStringLiteral("config"), QStringLiteral("ui.ssh")}, {},
                       workingDirectory, &result)
            ? QString::fromUtf8(result.out).trimmed()
            : QString();
    setLastError(error.error, error.errorCode, error.errorMessage);
    return userSsh;
  });
}

} // namespace

bool runOneShot(HgError kind, const QStringList &args,
                const QString &workingDirectory, HgCommandResult *result) {
  // Credentials accepted here are not kept, one-shot commands run once.
  QVector<HgAuthEntry> entries;
  return runAuthenticated(
      workingDirectory, &entries, oneShotSshArgs(workingDirectory),
      [&]() {
        for (const QString &arg : args) {
          if (arg.contains(QLatin1String("://"))) {
            return arg;
          }
        }
        return QString();
      },
      [&](const QStringList &configArgs, QString *errorText) {
        const bool success =
            runOneShotOnce(kind, args, configArgs, workingDirectory, result);
        if (!success) {
          *errorText = QString::fromUtf8(result->err + result->out);
        }
        return success;
      });
}

QString pathPattern(const QString &relativePath) {
  return QStringLiteral("path:") + relativePath;
}

QStringList pathPatterns(const QStringList &relativePaths) {
  QStringList patterns;
  patterns.reserve(relativePaths.size());
  for (const QString &path : relativePaths) {
    patterns << pathPattern(path);
  }
  return patterns;
}

QString normalizedPath(const QString &path) {
  if (path.isEmpty()) {
    return QString();
  }

  return QDir::cleanPath(
      QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath()));
}

QString relativeFilePath(const QString &root, const QString &file) {
  const QString unixFile = QDir::fromNativeSeparators(file);
  if (!QFileInfo(unixFile).isAbsolute()) {
    QString path = QDir::cleanPath(unixFile);
    while (path.endsWith('/')) {
      path.chop(1);
    }
    return path;
  }

  return QDir::fromNativeSeparators(QDir(root).relativeFilePath(unixFile));
}

QStringList relativeFilePaths(const QString &root, const QStringList &files) {
  QStringList paths;
  paths.reserve(files.size());
  for (const QString &file : files) {
    const QString path = relativeFilePath(root, file);
    if (!path.isEmpty() && !paths.contains(path)) {
      paths << path;
    }
  }
  return paths;
}

QString revsetString(const QString &value) {
  QString escaped = value;
  escaped.replace('\\', QStringLiteral("\\\\"));
  escaped.replace('\'', QStringLiteral("\\'"));
  return QStringLiteral("'") + escaped + QStringLiteral("'");
}

QString userString(const QString &author, const QString &email) {
  if (email.isEmpty()) {
    return author;
  }
  if (author.isEmpty()) {
    return QStringLiteral("<%1>").arg(email);
  }
  return QStringLiteral("%1 <%2>").arg(author, email);
}

void splitUser(const QString &user, QString *name, QString *email) {
  static const QRegularExpression pattern(
      QStringLiteral("^\\s*(.*?)\\s*<([^>]*)>\\s*$"));
  const QRegularExpressionMatch match = pattern.match(user);
  if (match.hasMatch()) {
    *name = match.captured(1);
    *email = match.captured(2);
    if (name->isEmpty()) {
      *name = *email;
    }
    return;
  }

  *name = user.trimmed();
  *email =
      user.contains('@') && !user.contains(' ') ? user.trimmed() : QString();
}

QString makeLogMessage(const QString &summary, const QString &message,
                       bool cleanup, bool signoff, const QString &author,
                       const QString &email) {
  QString result = summary;
  if (!message.isEmpty()) {
    result += QStringLiteral("\n\n") + message;
  }

  if (cleanup) {
    static const QRegularExpression trailingWhitespace(QStringLiteral("\\s+$"));

    QStringList lines;
    for (QString line : result.split('\n')) {
      if (!line.startsWith('#')) {
        lines.append(line.remove(trailingWhitespace));
      }
    }
    result = lines.join('\n').trimmed();
  }

  if (signoff && !author.isEmpty()) {
    const QString signoffLine =
        email.isEmpty()
            ? QStringLiteral("Signed-off-by: %1").arg(author)
            : QStringLiteral("Signed-off-by: %1 <%2>").arg(author, email);
    if (!result.contains(signoffLine)) {
      result += QStringLiteral("\n\n") + signoffLine;
    }
  }

  return result;
}

QString summaryOf(const QString &message) {
  return message.section('\n', 0, 0).trimmed();
}

qint64 timeStampFromHgDate(const QJsonArray &date) {
  const qint64 seconds = static_cast<qint64>(date.at(0).toDouble());
  const qint64 offsetWest = static_cast<qint64>(date.at(1).toDouble());
  return (seconds - offsetWest) * 1000;
}

QString dateStringFromHgDate(const QJsonArray &date) {
  const qint64 seconds = static_cast<qint64>(date.at(0).toDouble());
  const int offsetWest = date.at(1).toInt();
  const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(
      seconds, QTimeZone::fromSecondsAheadOfUtc(-offsetWest));
  return QLocale::system().toString(dateTime, QLocale::ShortFormat);
}

QSet<QString> readStagedPaths(const Repository *repo) {
  QSet<QString> paths;
  QFile file(stagedPathsFile(repo));
  if (!file.open(QIODevice::ReadOnly)) {
    return paths;
  }

  for (const QByteArray &line : file.readAll().split('\n')) {
    const QString path = QString::fromUtf8(line).trimmed();
    if (!path.isEmpty()) {
      paths.insert(path);
    }
  }
  return paths;
}

bool writeStagedPaths(const Repository *repo, const QSet<QString> &paths) {
  const QString fileName = stagedPathsFile(repo);

  if (paths.isEmpty()) {
    if (QFile::exists(fileName) && !QFile::remove(fileName)) {
      setLastError(HgError::StagingFailed, -1,
                   QStringLiteral("Failed to remove %1").arg(fileName));
      return false;
    }
    return true;
  }

  QDir().mkpath(QFileInfo(fileName).absolutePath());

  QStringList sorted(paths.cbegin(), paths.cend());
  sorted.sort();

  QSaveFile file(fileName);
  if (!file.open(QIODevice::WriteOnly) ||
      file.write((sorted.join('\n') + '\n').toUtf8()) < 0 || !file.commit()) {
    setLastError(HgError::StagingFailed, -1,
                 QStringLiteral("Failed to write %1: %2")
                     .arg(fileName, file.errorString()));
    return false;
  }
  return true;
}

bool readStatusEntries(Repository *repo, const QStringList &statusFlags,
                       const QStringList &patterns,
                       QVector<StatusEntry> *entries, HgError kind) {
  QStringList args{QStringLiteral("status"), QStringLiteral("-C")};
  args << statusFlags;
  if (!patterns.isEmpty()) {
    args << QStringLiteral("--") << patterns;
  }

  QJsonArray array;
  RunOptions options;
  options.readOnly = true;
  if (!runJson(repo, kind, args, &array, options)) {
    return false;
  }

  entries->clear();
  entries->reserve(array.size());
  for (const QJsonValue &value : array) {
    const QJsonObject object = value.toObject();
    StatusEntry entry;
    entry.path = QDir::fromNativeSeparators(object.value("path").toString());
    const QString status = object.value("status").toString();
    entry.status = status.isEmpty() ? QChar() : status.at(0);
    entry.source =
        QDir::fromNativeSeparators(object.value("source").toString());
    entries->append(entry);
  }
  return true;
}

bool readUnresolvedPaths(Repository *repo, QSet<QString> *paths) {
  QJsonArray array;
  RunOptions options;
  options.readOnly = true;
  if (!runJson(repo, HgError::StatusFailed,
               {QStringLiteral("resolve"), QStringLiteral("--list")}, &array,
               options)) {
    return false;
  }

  paths->clear();
  for (const QJsonValue &value : array) {
    const QJsonObject object = value.toObject();
    if (object.value("mergestatus").toString() == QStringLiteral("U")) {
      paths->insert(
          QDir::fromNativeSeparators(object.value("path").toString()));
    }
  }
  return true;
}

QByteArray readFileAtRevision(Repository *repo, const QString &relativePath,
                              const QString &revision, bool *exists) {
  if (exists) {
    *exists = false;
  }

  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  // A file missing at the revision is not an error for the caller.
  const HgErrorContext previousError = gLastError;
  if (!runCommand(repo, HgError::DiffFailed,
                  {QStringLiteral("cat"), QStringLiteral("-r"), revision,
                   QStringLiteral("--"), pathPattern(relativePath)},
                  &result, options)) {
    gLastError = previousError;
    return QByteArray();
  }

  if (exists) {
    *exists = true;
  }
  return result.out;
}

QByteArray readWorkingFile(const Repository *repo,
                           const QString &relativePath) {
  QFile file(repo->root + '/' + relativePath);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QString contentWithinLimit(const QByteArray &content) {
  const qint64 limit = ConfigManager::ConfigManager::getInstance().getLong(
      ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
  if (limit > 0 && content.size() > limit) {
    return QString();
  }
  return QString::fromUtf8(content);
}

void nodeToOid(const QString &node, git_oid *oid) {
  *oid = git_oid{};
  const QByteArray bytes = QByteArray::fromHex(node.toLatin1());
  const int size =
      std::min<int>(static_cast<int>(sizeof(oid->id)), bytes.size());
  std::memcpy(oid->id, bytes.constData(), static_cast<size_t>(size));
}

} // namespace Private

using namespace Private;

HgErrorContext getLastError() {
  return gLastError;
}

void initForCurrentThread() {
  gLastError = HgErrorContext();
}

void shutdownForCurrentThread() {
  gQuitFlag = nullptr;
}

void setCancelFlag(const bool *flag) {
  gQuitFlag = flag;
}

bool cloneRepository(
    const QString &url, const QString &destinationDir, const QString &uuid,
    const std::function<void(const GitRepo::CloneProgress &)> &progress) {
  if (url.isEmpty() || destinationDir.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A URL and a destination are required."));
    return false;
  }

  const QString executable = findExecutable();
  if (executable.isEmpty()) {
    setLastError(HgError::MercurialNotFound, -1,
                 QStringLiteral("Mercurial (hg) was not found. Install "
                                "Mercurial or set its path in Settings."));
    return false;
  }

  GitRepo::CloneProgress state;
  state.uuid = uuid;
  state.cloning = true;
  const auto report = [&]() {
    if (progress) {
      progress(state);
    }
  };

  // Progress is written to stderr as "\r<topic> <done>/<total>" when hg
  // believes it writes to a terminal, and HGPLAIN keeps it unless excepted.
  QProcessEnvironment env = hgEnvironment();
  env.insert(QStringLiteral("HGPLAINEXCEPT"), QStringLiteral("progress"));

  static const QRegularExpression progressLine(
      QStringLiteral("^(.*\\S)\\s+(\\d+)/(\\d+)\\s*$"));

  // Runs hg clone once, configArgs are the credentials of a retry.
  bool cancelled = false;
  int exitCode = -1;
  const auto cloneOnce = [&](const QStringList &configArgs,
                             QString *errorText) {
    QProcess process;
    process.setProgram(executable);
    process.setArguments(
        QStringList{
            QStringLiteral("--config"), QStringLiteral("ui.interactive=False"),
            QStringLiteral("--config"),
            QStringLiteral("subrepos.git:allowed=True"),
            QStringLiteral("--config"),
            QStringLiteral("subrepos.svn:allowed=True"),
            QStringLiteral("--config"),
            QStringLiteral("progress.assume-tty=True"),
            QStringLiteral("--config"), QStringLiteral("progress.delay=0"),
            QStringLiteral("--config"), QStringLiteral("progress.refresh=0.2"),
            QStringLiteral("--config"),
            QStringLiteral("progress.format=topic number")} +
        configArgs +
        QStringList{QStringLiteral("clone"), url,
                    QDir::toNativeSeparators(destinationDir)});
    process.setProcessEnvironment(env);
    hideConsoleWindow(process);
    process.start(QIODevice::ReadOnly);

    if (!process.waitForStarted()) {
      setLastError(HgError::CloneFailed, -1,
                   QStringLiteral("Failed to start %1: %2")
                       .arg(executable, process.errorString()));
      return false;
    }

    if (!state.started) {
      state.started = true;
      report();
    }

    QByteArray pending;
    QByteArray errorOutput;
    while (process.state() != QProcess::NotRunning) {
      process.waitForReadyRead(kOneShotSliceMs);

      if (gQuitFlag && *gQuitFlag) {
        process.kill();
        process.waitForFinished();
        cancelled = true;
        setLastError(HgError::Cancelled, -1,
                     QStringLiteral("The operation was cancelled."));
        return false;
      }

      const QByteArray chunk = process.readAllStandardError();
      errorOutput += chunk;
      pending += chunk;

      int separator = -1;
      while ((separator = pending.indexOf('\r')) >= 0 ||
             (separator = pending.indexOf('\n')) >= 0) {
        const QString line =
            QString::fromUtf8(pending.left(separator)).trimmed();
        pending.remove(0, separator + 1);

        const QRegularExpressionMatch match = progressLine.match(line);
        if (!match.hasMatch()) {
          continue;
        }
        state.stage = match.captured(1) == QStringLiteral("updating")
                          ? GitRepo::CloneProgressStageType::Checkout
                          : GitRepo::CloneProgressStageType::Fetch;
        state.completedSteps = match.captured(2).toULongLong();
        state.totalSteps = match.captured(3).toULongLong();
        state.lastStepPercentage =
            state.totalSteps > 0 ? static_cast<int>(100 * state.completedSteps /
                                                    state.totalSteps)
                                 : -1;
        report();
      }
    }

    errorOutput += process.readAllStandardError();
    exitCode = process.exitCode();
    if (process.exitStatus() == QProcess::NormalExit && exitCode == 0) {
      return true;
    }

    // Without the progress lines, which end with "\r".
    QStringList lines;
    for (const QByteArray &line : errorOutput.split('\n')) {
      const QByteArray text = line.mid(line.lastIndexOf('\r') + 1).trimmed();
      if (!text.isEmpty()) {
        lines << QString::fromUtf8(text);
      }
    }
    *errorText = lines.join('\n');
    setLastError(HgError::CloneFailed, exitCode,
                 lines.isEmpty() ? QStringLiteral("hg clone failed.")
                                 : withSshHint(*errorText));
    return false;
  };

  // The credentials are asked for the destination, the repository they are
  // remembered for.
  QVector<HgAuthEntry> entries;
  const bool success = runAuthenticated(
      QDir::fromNativeSeparators(QDir::cleanPath(destinationDir)), &entries,
      oneShotSshArgs(QString()), [&]() { return url; }, cloneOnce);
  if (cancelled) {
    return false;
  }

  state.finished = true;
  state.error = !success;
  state.lastStepPercentage = success ? 100 : state.lastStepPercentage;
  report();
  return success;
}

void setExecutablePath(const QString &path) {
  QMutexLocker locker(&gExecutableMutex);
  gExecutablePath = path;
}

QString findExecutable() {
  {
    QMutexLocker locker(&gExecutableMutex);
    if (!gExecutablePath.isEmpty()) {
      return QFileInfo(gExecutablePath).isExecutable() ? gExecutablePath
                                                       : QString();
    }
  }

  const QString fromPath = QStandardPaths::findExecutable(QStringLiteral("hg"));
  if (!fromPath.isEmpty()) {
    return fromPath;
  }

#ifdef Q_OS_WIN
  const QStringList roots = {qEnvironmentVariable("ProgramFiles"),
                             qEnvironmentVariable("ProgramW6432")};
  for (const QString &root : roots) {
    if (root.isEmpty()) {
      continue;
    }
    for (const QString &dir :
         {QStringLiteral("/Mercurial"), QStringLiteral("/TortoiseHg")}) {
      const QString candidate =
          QDir::fromNativeSeparators(root) + dir + QStringLiteral("/hg.exe");
      if (QFileInfo(candidate).isExecutable()) {
        return candidate;
      }
    }
  }
#endif

  return QString();
}

QString version() {
  HgCommandResult result;
  if (!runOneShot(HgError::MercurialNotFound,
                  {QStringLiteral("version"), QStringLiteral("-T"),
                   QStringLiteral("{ver}")},
                  QString(), &result)) {
    return QString();
  }
  return QString::fromUtf8(result.out).trimmed();
}

bool checkExecutable() {
  const QString ver = version();
  if (ver.isEmpty()) {
    return false;
  }

  int major = 0;
  int minor = 0;
  if (!parseVersion(ver, &major, &minor) || major < kMinimumMajorVersion ||
      (major == kMinimumMajorVersion && minor < kMinimumMinorVersion)) {
    setLastError(HgError::MercurialTooOld, -1,
                 QStringLiteral("Mercurial %1 is too old, version %2.%3 or "
                                "newer is required.")
                     .arg(ver)
                     .arg(kMinimumMajorVersion)
                     .arg(kMinimumMinorVersion));
    return false;
  }
  return true;
}

Repository *openRepository(const QString &path, const QString &author,
                           const QString &email) {
  LOG_DEBUG("Opening Mercurial repository at path: '{}'", path.toStdString());

  const QString root = normalizedPath(path);
  if (root.isEmpty() || !QFileInfo(root + QStringLiteral("/.hg")).isDir()) {
    setLastError(HgError::NotARepository, -1,
                 QStringLiteral("%1 is not a Mercurial repository.").arg(path));
    return nullptr;
  }

  auto repo = std::make_unique<Repository>();
  repo->root = root;
  repo->author = author;
  repo->email = email;

  if (!startServer(repo.get())) {
    return nullptr;
  }

  // The repository root, in case path is a directory below it.
  HgCommandResult result;
  RunOptions options;
  options.readOnly = true;
  if (!runCommand(repo.get(), HgError::NotARepository, {QStringLiteral("root")},
                  &result, options)) {
    return nullptr;
  }

  const QString hgRoot =
      normalizedPath(QString::fromUtf8(result.out).trimmed());
  if (!hgRoot.isEmpty() && hgRoot != repo->root) {
    repo->root = hgRoot;
    repo->server.reset();
  }

  return repo.release();
}

void closeRepository(Repository *repo) {
  if (!repo) {
    return;
  }

  if (repo->server) {
    repo->server->stop();
  }
  delete repo;
}

QString repositoryPath(const Repository *repo) {
  return repo ? repo->root : QString();
}

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email) {
  Q_UNUSED(defaultBranch);

  if (repoName.isEmpty() || workingDir.isEmpty()) {
    setLastError(HgError::InvalidArgument, -1,
                 QStringLiteral("A repository name and directory are "
                                "required."));
    return false;
  }

  const QString repoPath =
      QDir::cleanPath(QDir::fromNativeSeparators(workingDir) + '/' + repoName);

  HgCommandResult result;
  if (!runOneShot(HgError::InitFailed,
                  {QStringLiteral("init"), QDir::toNativeSeparators(repoPath)},
                  QString(), &result)) {
    return false;
  }

  if (author.isEmpty()) {
    return true;
  }

  QFile hgrc(repoPath + QStringLiteral("/.hg/hgrc"));
  if (!hgrc.open(QIODevice::WriteOnly | QIODevice::Append) ||
      hgrc.write(QStringLiteral("[ui]\nusername = %1\n")
                     .arg(userString(author, email))
                     .toUtf8()) < 0) {
    setLastError(HgError::InitFailed, -1,
                 QStringLiteral("Failed to write %1: %2")
                     .arg(hgrc.fileName(), hgrc.errorString()));
    return false;
  }

  return true;
}

} // namespace HgRepo
