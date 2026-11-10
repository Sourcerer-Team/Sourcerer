// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

// Helpers shared by the HgRepo translation units. Not part of the public API.

#include "HgCommandServer.h"
#include "HgRepo.h"

#include <QByteArray>
#include <QHash>
#include <QJsonArray>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>
#include <memory>

namespace HgRepo {

// Commit order of the history being paged, computed on page 0 and reused for
// the following pages.
struct HistoryCursor {
  QString key;
  QStringList nodes;
  QString headCommit;
  QHash<QString, QStringList> refTips;
  QHash<QString, QStringList> refLabels;
};

// Credentials passed to hg's [auth] section for the remotes starting with
// prefix.
struct HgAuthEntry {
  QString prefix;
  QString username;
  QString password;
};

class Repository {
public:
  QString root;
  QString author;
  QString email;
  std::unique_ptr<Private::HgCommandServer> server;
  HistoryCursor historyCursor;
  // Modification time and size of the files the command server reads once at
  // start (the repository config and the remote names), when it started.
  QHash<QString, QPair<qint64, qint64>> serverStamps;
  // Credentials the remotes accepted, sent with every later command.
  QVector<HgAuthEntry> auth;
  // The user's ui.ssh, read once per command server.
  QString userSsh;
  bool userSshKnown = false;
};

namespace Private {

// Repository config sections of Sourcerer, ignored by Mercurial.
inline constexpr const char *kSourcererSection = "sourcerer";
inline constexpr const char *kUpstreamSection = "sourcerer-upstream";
inline constexpr const char *kTargetsSection = "sourcerer-targets";

// Node id of the working directory in "hg annotate -r wdir()".
inline constexpr const char *kWorkingDirectoryNode =
    "ffffffffffffffffffffffffffffffffffffffff";
inline constexpr const char *kNullNode =
    "0000000000000000000000000000000000000000";

void setLastError(HgError error, int errorCode, const QString &errorMessage);

// Records an unsupported operation and returns false.
bool setUnsupported(const char *operation);

// Makes the commands of this thread stop when *quitFlag becomes true while
// the scope is alive.
class QuitScope {
public:
  explicit QuitScope(const bool *quitFlag);
  ~QuitScope();

  QuitScope(const QuitScope &) = delete;
  QuitScope &operator=(const QuitScope &) = delete;

private:
  const bool *_previous;
};

// HTTP authentication, see setCredentialsProvider.

// --config arguments passing entries to hg's [auth] section.
QStringList authConfigArgs(const QVector<HgAuthEntry> &entries);

// --config argument making ssh use the keys of the credentials provider, see
// HgCredentialsProvider::sshKeys. Empty when there are none or userSsh, the
// user's ui.ssh (asked only when there are keys), is PuTTY's plink.
QStringList sshConfigArgs(const std::function<QString()> &userSsh);

// message, with what to do about SSH failures hg reports in it.
QString withSshHint(const QString &message);

struct AuthFailure {
  bool isAuthFailure = false;
  // [auth] prefix of the remote, empty when hg's message doesn't name it.
  QString url;
};

// Whether errorText is hg's report of a remote asking for, or rejecting,
// credentials. sentCredentials tells whether the command passed any.
AuthFailure authFailure(const QString &errorText, bool sentCredentials);

// Runs run with the --config arguments of entries. While it fails because a
// remote asks for, or rejects, credentials, asks the credentials provider of
// the thread and runs it again. entries keeps the accepted credentials.
// fallbackUrl gives the remote when hg's message doesn't name one.
// baseConfigArgs are put before the credentials.
bool runAuthenticated(const QString &repoPath, QVector<HgAuthEntry> *entries,
                      const QStringList &baseConfigArgs,
                      const std::function<QString()> &fallbackUrl,
                      const std::function<bool(const QStringList &configArgs,
                                               QString *errorText)> &run);

struct RunOptions {
  // Sent when the command reads its standard input.
  QByteArray input;
  // The command doesn't change the repository, so it is retried once when the
  // command server died.
  bool readOnly = false;
};

// Runs "hg <args>" through the repository's command server, starting it when
// needed. Returns false and records kind with hg's error output when the
// command can't run or exits with a code other than 0 (or 1 when
// allowExitCodeOne, which hg uses for "nothing changed").
bool runCommand(Repository *repo, HgError kind, const QStringList &args,
                HgCommandResult *result, const RunOptions &options = {},
                bool allowExitCodeOne = false);

// Runs a command with "-T json" appended and parses its output as an array.
bool runJson(Repository *repo, HgError kind, const QStringList &args,
             QJsonArray *array, const RunOptions &options = {});

// Runs hg outside a command server, for commands that run before a
// repository exists.
bool runOneShot(HgError kind, const QStringList &args,
                const QString &workingDirectory, HgCommandResult *result);

// "path:<file>" pattern matching a path relative to the repository root, or
// all of it for a directory.
QString pathPattern(const QString &relativePath);
QStringList pathPatterns(const QStringList &relativePaths);

// Absolute path with forward slashes and without a trailing slash.
QString normalizedPath(const QString &path);

// Path of file relative to root, with forward slashes. file is relative to
// root or absolute.
QString relativeFilePath(const QString &root, const QString &file);
QStringList relativeFilePaths(const QString &root, const QStringList &files);

// Revset string literal.
QString revsetString(const QString &value);

// "Name <email>" as used by ui.username and commit -u.
QString userString(const QString &author, const QString &email);
// Splits "Name <email>".
void splitUser(const QString &user, QString *name, QString *email);

// Commit message built like the Git commit message.
QString makeLogMessage(const QString &summary, const QString &message,
                       bool cleanup, bool signoff, const QString &author,
                       const QString &email);

// First line of a commit message.
QString summaryOf(const QString &message);

// Milliseconds in the format of GitCommitInfo::timeStamp, from the [unixtime,
// offset] pair of hg's JSON output (offset in seconds west of UTC).
qint64 timeStampFromHgDate(const QJsonArray &date);
// Date in the format of GitCommitStatus::committerDate.
QString dateStringFromHgDate(const QJsonArray &date);

// Paths of the staging area, relative to the repository root.
QSet<QString> readStagedPaths(const Repository *repo);
bool writeStagedPaths(const Repository *repo, const QSet<QString> &paths);

// One entry of "hg status -T json -C".
struct StatusEntry {
  QString path;
  // Single letter hg status: M A R ! ? I C.
  QChar status;
  // Copy or rename source of an added file.
  QString source;
};

bool readStatusEntries(Repository *repo, const QStringList &statusFlags,
                       const QStringList &patterns,
                       QVector<StatusEntry> *entries,
                       HgError kind = HgError::StatusFailed);

// Entries for the subrepos whose checkout differs from .hgsubstate, which hg
// status doesn't report: 'M', or 'A' before the subrepo's first commit.
QVector<StatusEntry> subrepoStatusEntries(Repository *repo);

// Paths of the subrepos in .hgsub.
QStringList subrepoPaths(Repository *repo);

// Unresolved files of a merge in progress.
bool readUnresolvedPaths(Repository *repo, QSet<QString> *paths);

// Content of file at revision, empty when it doesn't exist there.
QByteArray readFileAtRevision(Repository *repo, const QString &relativePath,
                              const QString &revision, bool *exists = nullptr);

// Reads a file of the working directory.
QByteArray readWorkingFile(const Repository *repo, const QString &relativePath);

// Content within GitRepoMaxDiffFileSize, else empty.
QString contentWithinLimit(const QByteArray &content);

// Parses "hg diff --git" output.
QVector<GitRepo::FilesDiffDelta> parseGitDiff(const QString &diff);

// Diff text in the format GitRepo uses for FilesDiff::diff.
QString diffText(const QVector<GitRepo::FilesDiffDelta> &deltas);

// Delta adding every line of content, for unknown files.
GitRepo::FilesDiffDelta addedFileDelta(const QString &file,
                                       const QString &content);

// Fills a git_oid from a 40 character hex node id. Done by hand so HgRepo
// doesn't link libgit2 for it.
void nodeToOid(const QString &node, git_oid *oid);

// Runs a command whose output isn't needed.
bool runSimple(Repository *repo, HgError kind, const QStringList &args,
               bool allowExitCodeOne = false);

// Revset of the tip of a branch list entry: "name" (named branch), "@name"
// (bookmark), "path/name" and "path/@name" (remote names) or a tag.
QString branchRevset(const QString &name, GitRepo::BranchType type);

// Node of the last revision of revset. Records kind when it is empty.
bool resolveNode(Repository *repo, HgError kind, const QString &revset,
                 QString *node);

// Number of revisions of revset, -1 on failure.
int countRevisions(Repository *repo, const QString &revset);

// Node of the working directory parent, empty for an empty repository.
QString workingDirectoryNode(Repository *repo);
// Active bookmark, empty when none is active.
QString activeBookmark(Repository *repo);
// Records UncommittedChanges and returns false when the working directory
// has changes to tracked files.
bool requireCleanWorkingDirectory(Repository *repo);

// The repository has the requirement, in .hg/requires or, for share-safe
// repositories, .hg/store/requires (of the main repository for a share).
bool hasRequirement(const QString &root, const QByteArray &requirement);

// Absolute path of a file in the .hg directory.
QString hgFile(const Repository *repo, const QString &name);

// "--user" argument pair for author, or the repository's author.
QStringList userArgs(const Repository *repo, const QString &author,
                     const QString &email);

// Splits "path/name" or "path/@name". Returns false when name has no path.
bool splitRemoteName(const QString &remoteName, QString *path, QString *name);

} // namespace Private
} // namespace HgRepo
