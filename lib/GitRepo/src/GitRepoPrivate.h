#pragma once

// Helpers shared by the GitRepo translation units. Not part of the public API.

#include "GitRepo.h"

#include <git2.h>

#include <LockGuard.h>

#include <QMap>
#include <QString>
#include <QStringList>

#define HEAD "HEAD"

namespace GitRepo::Private {

inline constexpr int MAX_RETRIES = 3;

// Errors (GitRepo.cpp).

QString gitErrorName(const GitError errorType);

QString gitErrorMessage(const GitError errorType);

void setErrorContext(const GitError errorType, const QString &gitErrorMessage,
                     const int errorCode);

void setError(const GitError errorType, const int error);

// Common helpers (GitRepo.cpp).

bool isCommercialPlan();

QString buildCommitMessage(const QString &summary, const QString &message,
                           const QString &author, const QString &email,
                           bool signoff);

QString commitSignatureDateToString(const git_signature *signature);

bool markConflictedFiles(git_repository *repo, const QStringList &filePaths);

bool file_has_conflict_marker_triplet(const QString &path,
                                      const QString &startMarker,
                                      const QString &middleMarker,
                                      const QString &endMarker);

void git_strarray_destroy(git_strarray &arr);

git_strarray qstringlist_to_git_strarray(const QStringList &list);

// Transfer callbacks and progress (GitRepo.cpp).

struct PushReferenceStatus {
  bool rejected = false;
  QString refname;
  QString status;
};

extern thread_local PushReferenceStatus gPushReferenceStatus;

int pushUpdateReferenceCallback(const char *refname, const char *status,
                                void *data);

int pushSidebandProgressCallback(const char *str, int len, void *data);

int fetchProgressCallback(const git_indexer_progress *stats, void *payload);

void checkoutProgressCallback(const char *path, size_t completed_steps,
                              const size_t total_steps, void *payload);

int certificateCheckCallback(git_cert *cert, int valid, const char *host,
                             void *payload);

void finishFetchProgress(const QString &uuid, const bool error,
                         const bool cloning);

// Credentials (GitRepoCredentials.cpp).

struct GitPlaintextCredentials {
  QString username;
  QString password;
  qint64 createdTime;
};

struct GitCredentialPayload : public CloneProgress {
  QString repoPath;
};

extern Util::Mutex gGitCredentialsMutex;

extern QMap<QString, GitPlaintextCredentials> gGitCredentials;

void invalidateCredentials(const QString &url);

void sendInvalidCredentialsSignal(const QString &url);

void finishCredentialsAcquiring(const QString &url);

int credentialsAcquireCallback(git_credential **out, const char *url,
                               const char *username_from_url,
                               const unsigned int allowed_types, void *payload);

// Commit signing (GitRepoSigning.cpp).

enum class CommitSigningFormat {
  Unknown = 0,
  OpenPgp,
  Ssh,
};

struct CommitSigningConfig {
  bool enabled = false;
  CommitSigningFormat format = CommitSigningFormat::OpenPgp;
  QString program;
  QString signingKey;
};

struct CommitSigningPayload {
  CommitSigningConfig config;
  QString *errorMessage = nullptr;
  git_repository *repo = nullptr;
};

bool loadCommitSignatureInfo(git_repository *repo, const git_oid *oid,
                             QString *signature, QString *signatureType);

bool loadCommitSigningConfig(CommitSigningConfig *config,
                             QString *errorMessage);

int commitSigningCallback(git_commitbuilder *builder, git_repository *repo,
                          const char *commit_content, void *payload);

int commitRebaseSigningCallback(git_oid *out, const git_signature *author,
                                const git_signature *committer,
                                const char *message_encoding,
                                const char *message, const git_tree *tree,
                                size_t parent_count,
                                const git_commit *parents[], void *payload);

// History (GitRepoHistory.cpp).

// Drops the cached history order of repo, see getBranchHistory.
void forgetHistoryCursor(git_repository *repo);

} // namespace GitRepo::Private
