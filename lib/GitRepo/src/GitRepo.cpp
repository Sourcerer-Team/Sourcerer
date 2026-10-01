#include "GitRepo.h"
#include "GitCredentialStorage.h"
#include "GitKeyStorage.h"
#include "GitLfs.h"

#include "../../../component/GitRepoManager/include/GitRepoManagerStructs.h"

#include "PatchFileUtils.h"
#include <CloudCredentialStorage.h>
#include <CloudRepo.h>
#include <ConfigManager.h>
#include <EventManager.h>
#include <JWTDecoder.h>
#include <LockGuard.h>
#include <Log.h>
#ifdef _WIN32
#include <applink.c>
#endif
#include <Uuid.h>

#include <git2.h>
#include <git2/attr.h>
#include <git2/blame.h>
#include <git2/blob.h>
#include <git2/branch.h>
#include <git2/cert.h>
#include <git2/checkout.h>
#include <git2/clone.h>
#include <git2/commit.h>
#include <git2/errors.h>
#include <git2/graph.h>
#include <git2/index.h>
#include <git2/patch.h>
#include <git2/rebase.h>
#include <git2/refs.h>
#include <git2/revparse.h>
#include <git2/revwalk.h>
#include <git2/signature.h>
#include <git2/submodule.h>
#include <git2/sys/errors.h>
#include <git2/tag.h>
#include <git2/types.h>

#include <QByteArray>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>

#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QLocale>
#include <QMap>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

#include <cstring>
#include <set>
#include <string>
#include <thread>

#define THREAD_ID "GitRepo"

#define ALL_BRANCHES "*"
#define HEAD "HEAD"

namespace GitRepo {
constexpr qint64 DEFAULT_CREDENTIALS_EXPIRATION_TIME =
    30 * 60 * 1000; // 30 minutes

constexpr int MAX_RETRIES = 3;

struct GitPlaintextCredentials {
  QString username;
  QString password;
  qint64 createdTime;
};

Util::Mutex gGitCredentialsMutex;
QMap<QString, GitPlaintextCredentials> gGitCredentials;

Util::Mutex gErrorMutex;
QMap<std::thread::id, GitErrorContext> gErrorMap;

static QString capitalizeFirst(const QString &str) {
  QString capitalized = str;
  if (!capitalized.isEmpty()) {
    capitalized[0] = capitalized[0].toUpper();
  }

  return capitalized;
}

static QString buildCommitMessage(const QString &summary,
                                  const QString &message, const QString &author,
                                  const QString &email, bool signoff) {
  QString commitMessage = summary;

  if (!message.isEmpty()) {
    commitMessage += "\n\n" + message;
  }

  if (!signoff) {
    return commitMessage;
  }

  const QString signoffLine =
      QString("Signed-off-by: %1 <%2>").arg(author, email);

  while (!commitMessage.isEmpty() &&
         (commitMessage.endsWith('\n') || commitMessage.endsWith('\r'))) {
    commitMessage.chop(1);
  }

  if (!commitMessage.isEmpty()) {
    commitMessage += "\n\n";
  }

  commitMessage += signoffLine;
  return commitMessage;
}

static QString blameSignatureToString(const git_signature *signature) {
  if (!signature) {
    return QStringLiteral("Unknown");
  }

  const QString name =
      signature->name ? QString::fromUtf8(signature->name) : QString();
  const QString trimmedName = name.trimmed();

  return trimmedName.isEmpty() ? QStringLiteral("Unknown") : trimmedName;
}

static QString commitSignatureDateToString(const git_signature *signature) {
  if (!signature) {
    return {};
  }

  const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(
      signature->when.time, Qt::OffsetFromUTC, signature->when.offset * 60);
  return QLocale::system().toString(dateTime, QLocale::ShortFormat);
}

static QString blameCommitToString(const git_oid *oid) {
  if (!oid) {
    return QStringLiteral("0000000");
  }

  char buffer[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(buffer, sizeof(buffer), oid);
  return QString::fromLatin1(buffer).left(7);
}

static BlameReport createBlameReport(git_repository *repo,
                                     const QStringList &files) {
  BlameReport report;

  git_blame_options blameOptions = GIT_BLAME_OPTIONS_INIT;
  blameOptions.flags = GIT_BLAME_USE_MAILMAP;

  const QString repoWorkdir =
      repo && git_repository_workdir(repo)
          ? QString::fromUtf8(git_repository_workdir(repo))
          : QString();

  for (const QString &file : files) {
    const QString normalizedFile = QDir::fromNativeSeparators(file);
    BlameFileInfo fileInfo;
    fileInfo.filePath = normalizedFile;

    const QByteArray utf8Path = normalizedFile.toUtf8();
    git_blame *blame = nullptr;
    const int error =
        git_blame_file(&blame, repo, utf8Path.constData(), &blameOptions);
    if (error) {
      const git_error *gitError = git_error_last();
      fileInfo.errorMessage = gitError && gitError->message
                                  ? QString::fromUtf8(gitError->message)
                                  : QStringLiteral("Unknown error");
      report.fileInfos += fileInfo;
      continue;
    }

    const QString filePath = QDir(repoWorkdir).filePath(normalizedFile);
    QFile fileObj(filePath);
    QString fileContent;
    if (fileObj.open(QIODevice::ReadOnly | QIODevice::Text)) {
      fileContent = QString::fromUtf8(fileObj.readAll());
      fileObj.close();
    }

    QStringList fileLines;
    if (!fileContent.isEmpty()) {
      fileLines = fileContent.split('\n', Qt::KeepEmptyParts);
      if (fileContent.endsWith('\n') && !fileLines.isEmpty() &&
          fileLines.last().isEmpty()) {
        fileLines.removeLast();
      }
    }

    const size_t lineCount = git_blame_linecount(blame);
    for (size_t lineNumber = 1; lineNumber <= lineCount; ++lineNumber) {
      const git_blame_hunk *hunk = git_blame_hunk_byline(blame, lineNumber);
      BlameLineInfo lineInfo;
      lineInfo.filePath = normalizedFile;
      lineInfo.lineNumber = static_cast<qint64>(lineNumber);
      lineInfo.content = lineNumber - 1 < static_cast<size_t>(fileLines.size())
                             ? fileLines.at(static_cast<int>(lineNumber - 1))
                             : QString();
      lineInfo.commit =
          blameCommitToString(hunk ? &hunk->final_commit_id : nullptr);
      lineInfo.author =
          blameSignatureToString(hunk ? hunk->final_signature : nullptr);
      lineInfo.summary =
          hunk && hunk->summary ? QString::fromUtf8(hunk->summary) : QString();

      fileInfo.lineInfos += lineInfo;
    }

    git_blame_free(blame);
    report.fileInfos += fileInfo;
  }

  report.valid = true;
  return report;
}

enum class CommitSignatureType {
  Unknown = 0,
  OpenPgp,
  Ssh,
};

static QString commitSignatureTypeToString(CommitSignatureType type) {
  switch (type) {
  case CommitSignatureType::OpenPgp:
    return QStringLiteral("OpenPGP");
  case CommitSignatureType::Ssh:
    return QStringLiteral("SSH");
  case CommitSignatureType::Unknown:
  default:
    return QStringLiteral("Unknown");
  }
}

static CommitSignatureType detectCommitSignatureType(const QString &signature) {
  if (signature.contains(QStringLiteral("BEGIN SSH SIGNATURE"),
                         Qt::CaseInsensitive)) {
    return CommitSignatureType::Ssh;
  }

  if (signature.contains(QStringLiteral("BEGIN PGP SIGNATURE"),
                         Qt::CaseInsensitive)) {
    return CommitSignatureType::OpenPgp;
  }

  return CommitSignatureType::Unknown;
}

static bool loadCommitSignatureInfo(git_repository *repo, const git_oid *oid,
                                    QString *signature,
                                    QString *signatureType) {
  if (!repo || !oid || !signature || !signatureType) {
    return false;
  }

  git_buf signatureBuf = {nullptr};
  git_buf signedDataBuf = {nullptr};

  const int error = git_commit_extract_signature(
      &signatureBuf, &signedDataBuf, repo, const_cast<git_oid *>(oid), nullptr);
  if (error == GIT_ENOTFOUND) {
    *signature = QString();
    *signatureType = QStringLiteral("Unsigned");
    git_buf_dispose(&signatureBuf);
    git_buf_dispose(&signedDataBuf);
    return true;
  }

  if (error) {
    LOG_WARN_ID(Log::ErrorId::Git,
                "Failed to extract commit signature for display (error {})",
                error);
    *signature = QString();
    *signatureType = QStringLiteral("Unavailable");
    git_buf_dispose(&signatureBuf);
    git_buf_dispose(&signedDataBuf);
    return false;
  }

  if (signatureBuf.ptr && signatureBuf.size > 0) {
    *signature = QString::fromUtf8(signatureBuf.ptr, signatureBuf.size);
  } else {
    signature->clear();
  }

  *signatureType =
      commitSignatureTypeToString(detectCommitSignatureType(*signature));

  git_buf_dispose(&signatureBuf);
  git_buf_dispose(&signedDataBuf);
  return true;
}

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

static QString expandUserPath(const QString &path) {
  if (path == "~") {
    return QDir::homePath();
  }

  if (path.startsWith("~/")) {
    return QDir::homePath() + path.mid(1);
  }

  return path;
}

static bool isPassphraseProtectedSshPrivateKey(const QString &program,
                                               const QString &privateKeyPath) {
  if (program.isEmpty() || privateKeyPath.isEmpty() ||
      !QFile::exists(privateKeyPath)) {
    return false;
  }

  QProcess process;
  process.start(program, {QStringLiteral("-y"), QStringLiteral("-f"),
                          privateKeyPath, QStringLiteral("-P"), QString()});

  if (!process.waitForStarted()) {
    return false;
  }

  if (!process.waitForFinished(-1)) {
    process.kill();
    process.waitForFinished();
    return false;
  }

  if (process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0) {
    return false;
  }

  const QString output = QString::fromUtf8(process.readAllStandardError()) +
                         QString::fromUtf8(process.readAllStandardOutput());
  return output.contains(QStringLiteral("incorrect passphrase"),
                         Qt::CaseInsensitive) ||
         output.contains(QStringLiteral("enter passphrase"),
                         Qt::CaseInsensitive) ||
         (output.contains(QStringLiteral("passphrase"), Qt::CaseInsensitive) &&
          (output.contains(QStringLiteral("decrypt"), Qt::CaseInsensitive) ||
           output.contains(QStringLiteral("encrypted"), Qt::CaseInsensitive)));
}

static bool loadCommitSigningConfig(CommitSigningConfig *config,
                                    QString *errorMessage) {
  if (!config) {
    return false;
  }

  auto &configManager = ConfigManager::ConfigManager::getInstance();
  config->enabled =
      configManager.getBoolean(ConfigManager::ConfigKey::GitRepoSigningEnabled);
  if (!config->enabled) {
    return true;
  }

  const QString formatValue =
      configManager.getString(ConfigManager::ConfigKey::GitRepoSigningFormat)
          .trimmed();
  if (formatValue.compare(QStringLiteral("ssh"), Qt::CaseInsensitive) == 0) {
    config->format = CommitSigningFormat::Ssh;
  } else {
    config->format = CommitSigningFormat::OpenPgp;
  }

  config->signingKey = expandUserPath(
      configManager.getString(ConfigManager::ConfigKey::GitRepoSigningKey)
          .trimmed());
  config->program = expandUserPath(
      configManager.getString(ConfigManager::ConfigKey::GitRepoSigningProgram)
          .trimmed());

  if (config->program.isEmpty()) {
    config->program = config->format == CommitSigningFormat::Ssh
                          ? QStringLiteral("ssh-keygen")
                          : QStringLiteral("gpg");
  }

  if (config->format == CommitSigningFormat::Ssh) {
    if (config->signingKey.endsWith(QStringLiteral(".pub"))) {
      const QString privateKeyPath =
          config->signingKey.left(config->signingKey.length() - 4);
      if (QFile::exists(privateKeyPath)) {
        config->signingKey = privateKeyPath;
      }
    }

    if (config->signingKey.isEmpty()) {
      if (errorMessage) {
        *errorMessage =
            QStringLiteral("SSH commit signing requires a private key path.");
      }
      return false;
    }

    if (isPassphraseProtectedSshPrivateKey(config->program,
                                           config->signingKey)) {
      if (errorMessage) {
        *errorMessage = QStringLiteral("SSH commit signing does not support "
                                       "passphrase-protected private keys.");
      }
      return false;
    }
  }

  return true;
}

static bool writeTextFile(const QString &path, const QString &content,
                          QString *errorMessage) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    if (errorMessage) {
      *errorMessage = QString("Failed to open file for writing: %1").arg(path);
    }
    return false;
  }

  const QByteArray bytes = content.toUtf8();
  if (file.write(bytes) != bytes.size()) {
    if (errorMessage) {
      *errorMessage = QString("Failed to write file: %1").arg(path);
    }
    return false;
  }

  return true;
}

static bool readTextFile(const QString &path, QString *content,
                         QString *errorMessage) {
  if (!content) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    if (errorMessage) {
      *errorMessage = QString("Failed to open signature file: %1").arg(path);
    }
    return false;
  }

  *content = QString::fromUtf8(file.readAll());
  while (!content->isEmpty() &&
         (content->endsWith('\n') || content->endsWith('\r'))) {
    content->chop(1);
  }

  return true;
}

static bool runProcess(const QString &program, const QStringList &arguments,
                       const QString &workingDirectory, QString *errorMessage) {
  QProcess process;
  process.setWorkingDirectory(workingDirectory);
  process.start(program, arguments);

  if (!process.waitForStarted()) {
    if (errorMessage) {
      *errorMessage = QString("Failed to start '%1': %2")
                          .arg(program, process.errorString());
    }
    return false;
  }

  if (!process.waitForFinished(-1)) {
    if (errorMessage) {
      *errorMessage = QString("Signing command '%1' did not finish: %2")
                          .arg(program, process.errorString());
    }
    return false;
  }

  if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
    const QString stderrOutput =
        QString::fromUtf8(process.readAllStandardError()).trimmed();
    if (errorMessage) {
      *errorMessage = QString("Signing command '%1' failed (exit code %2)%3")
                          .arg(program)
                          .arg(process.exitCode())
                          .arg(stderrOutput.isEmpty()
                                   ? QString()
                                   : QStringLiteral(": %1").arg(stderrOutput));
    }
    return false;
  }

  return true;
}

static bool createCommitSignature(const QString &commitContent,
                                  const CommitSigningConfig &config,
                                  QString *signature, QString *errorMessage) {
  if (!signature) {
    return false;
  }

  QTemporaryDir tempDir;
  if (!tempDir.isValid()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "Failed to create a temporary directory for commit signing.");
    }
    return false;
  }

  const QString contentPath = tempDir.filePath(QStringLiteral("commit.txt"));
  if (!writeTextFile(contentPath, commitContent, errorMessage)) {
    return false;
  }

  const QString workingDirectory = tempDir.path();
  QString signErrorMessage;

  if (config.format == CommitSigningFormat::Ssh) {
    const QString signaturePath = contentPath + QStringLiteral(".sig");
    QStringList arguments;
    arguments << QStringLiteral("-Y") << QStringLiteral("sign")
              << QStringLiteral("-n") << QStringLiteral("git")
              << QStringLiteral("-f") << config.signingKey << contentPath;

    if (!runProcess(config.program, arguments, workingDirectory,
                    &signErrorMessage)) {
      if (errorMessage) {
        *errorMessage = signErrorMessage;
      }
      return false;
    }

    return readTextFile(signaturePath, signature, errorMessage);
  }

  const QString signaturePath = tempDir.filePath(QStringLiteral("commit.asc"));
  QStringList arguments;
  arguments << QStringLiteral("--batch") << QStringLiteral("--yes")
            << QStringLiteral("--armor") << QStringLiteral("--detach-sign")
            << QStringLiteral("--output") << signaturePath;

  if (!config.signingKey.isEmpty()) {
    arguments << QStringLiteral("--local-user") << config.signingKey;
  }

  arguments << contentPath;

  if (!runProcess(config.program, arguments, workingDirectory,
                  &signErrorMessage)) {
    if (errorMessage) {
      *errorMessage = signErrorMessage;
    }
    return false;
  }

  return readTextFile(signaturePath, signature, errorMessage);
}

static int commitSigningCallback(git_commitbuilder *builder,
                                 git_repository *repo,
                                 const char *commit_content, void *payload) {
  Q_UNUSED(repo);

  if (!builder || !commit_content || !payload) {
    return -1;
  }

  auto *signingPayload = static_cast<CommitSigningPayload *>(payload);
  QString errorMessage;
  QString signature;

  if (!createCommitSignature(QString::fromUtf8(commit_content),
                             signingPayload->config, &signature,
                             &errorMessage)) {
    if (signingPayload->errorMessage) {
      *signingPayload->errorMessage = errorMessage;
    }
    LOG_ERROR(Log::ErrorId::Git, "Failed to create commit signature: {}",
              errorMessage.toStdString());
    return -1;
  }

  const QByteArray signatureUtf8 = signature.toUtf8();
  if (git_commitbuilder_add_header(builder, "gpgsig",
                                   signatureUtf8.constData()) < 0) {
    errorMessage = QStringLiteral("Failed to add commit signature header.");
    if (signingPayload->errorMessage) {
      *signingPayload->errorMessage = errorMessage;
    }
    LOG_ERROR(Log::ErrorId::Git, "Failed to add commit signature header");
    return -1;
  }

  return 0;
}

static int commitRebaseSigningCallback(
    git_oid *out, const git_signature *author, const git_signature *committer,
    const char *message_encoding, const char *message, const git_tree *tree,
    size_t parent_count, const git_commit *parents[], void *payload) {
  Q_ASSERT(payload != nullptr);

  if (!payload) {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Payload not specified.");
    return -1;
  };

  auto *signingPayload = static_cast<CommitSigningPayload *>(payload);

  if (!signingPayload->config.enabled) {
    return GIT_PASSTHROUGH;
  }

  git_repository *repo = signingPayload->repo;

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Repository not specified.");
    return -1;
  }

  QString errorMessage;
  QString signature;

  if (!createCommitSignature(QString::fromUtf8(message), signingPayload->config,
                             &signature, &errorMessage)) {
    if (signingPayload->errorMessage) {
      *signingPayload->errorMessage = errorMessage;
    }
    LOG_ERROR(Log::ErrorId::Git, "Failed to create commit signature: {}",
              errorMessage.toStdString());
    return -1;
  }

  const QByteArray signatureUtf8 = signature.toUtf8();

  git_buf commit_content = {0};
  int error = 0;

  error = git_commit_create_buffer(&commit_content, repo, author, committer,
                                   message_encoding, message, tree,
                                   parent_count, parents);
  if (error) {
    errorMessage = QStringLiteral("Failed to create commit buffer.");
    if (signingPayload->errorMessage) {
      *signingPayload->errorMessage = errorMessage;
    }
    LOG_ERROR(Log::ErrorId::Git, "Failed to create commit buffer");
    return -1;
  }

  error = git_commit_create_with_signature(
      out, repo, commit_content.ptr, signature.toStdString().c_str(), "gpgsig");
  if (error) {
    errorMessage = QStringLiteral("Failed to create commit with signature.");
    if (signingPayload->errorMessage) {
      *signingPayload->errorMessage = errorMessage;
    }
    git_buf_dispose(&commit_content);
    LOG_ERROR(Log::ErrorId::Git, "Failed to create commit with signature");
    return -1;
  }

  git_buf_dispose(&commit_content);

  return error;
};

static int encodeLibgitError(const int error, const int klass) {
  return (klass & 0x00ff) << 16 | (error & 0xffff);
}

static QString gitErrorName(const GitError errorType) {
  switch (errorType) {
  case GitError::Unknown:
    return QCoreApplication::translate("GitRepoError",
                                       "An unknown Git error occurred.");
  case GitError::MarkConflictedFilesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToCreateAncestorBlobFromWorkdir:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create ancestor blob from workdir while "
                        "marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToCreateOursBlobFromWorkdir:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create ours blob from workdir while "
                        "marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToCreateTheirsBlobFromWorkdir:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create theirs blob from workdir while "
                        "marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToAddConflictToIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add conflict to index while marking conflicted files.");
  case GitError::MarkConflictedFilesFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index while marking conflicted files.");
  case GitError::
      CollectConflictedFilePathsFromResultIndexFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while collecting "
                        "conflicted file paths from the result index.");
  case GitError::CollectConflictedFilePathsFailedToPeelHeadToCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel head to commit while collecting "
                        "conflicted file paths.");
  case GitError::CollectConflictedFilePathsFromResultIndexFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get commit tree while collecting "
                        "conflicted file paths from the result index.");
  case GitError::
      CollectConflictedFilePathsFromResultIndexFailedToDiffTreeAgainstIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to diff tree against index while collecting "
                        "conflicted file paths from the result index.");
  case GitError::
      CollectConflictedFilePathsFromResultIndexFailedToCreateConflictIterator:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create conflict iterator while collecting "
                        "conflicted file paths from the result index.");
  case GitError::CreateInitialCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create signature from profile while "
                        "creating the initial commit.");
  case GitError::CreateInitialCommitFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToReadNextBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read next branch while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while creating the initial commit.");
  case GitError::CreateInitialCommitFailedToRenameBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to rename branch while creating the initial commit.");
  case GitError::CreateInitialCommitWithoutStagingFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature from profile while creating the initial "
        "commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToCreateTreeBuilder:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create tree builder while creating the "
                        "initial commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToWriteTreeBuilder:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write tree builder while creating the "
                        "initial commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while creating the initial "
                        "commit without staging.");
  case GitError::CreateInitialCommitWithoutStagingFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit while creating the initial "
                        "commit without staging.");
  case GitError::GetLastCommitFailedToResolveReferenceNameToId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve reference name to id while "
                        "getting the last commit.");
  case GitError::GetLastCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while getting the last commit.");
  case GitError::ApplyStashFailedToApplyStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to apply stash while applying the stash.");
  case GitError::ApplyStashFailedToDiffIndexAgainstWorkdir:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to diff index against workdir while applying the stash.");
  case GitError::HasConflictsFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while checking for conflicts.");
  case GitError::CheckoutBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup branch while checking out the branch.");
  case GitError::CheckoutBranchFailedToPeelLocalBranchToCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel local branch to commit while checking out the branch.");
  case GitError::CheckoutBranchFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToCheckoutLocalBranchTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout local branch tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToGetReferenceName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference name while checking out the branch.");
  case GitError::CheckoutBranchFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup reference while checking out the branch.");
  case GitError::CheckoutBranchFailedToPeelRemoteBranchToCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel remote branch to commit while "
                        "checking out the branch.");
  case GitError::CheckoutBranchFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while checking out the branch.");
  case GitError::CheckoutBranchFailedToCreateLocalBranchFromRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create local branch from remote while "
                        "checking out the branch.");
  case GitError::CheckoutBranchFailedToSetBranchUpstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set branch upstream while checking out the branch.");
  case GitError::CheckoutBranchFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToCheckoutTagTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout tag tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToSetHeadToTagReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set head to tag reference while checking out the branch.");
  case GitError::CheckoutBranchFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while checking out the branch.");
  case GitError::CheckoutBranchFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout tree while checking out the branch.");
  case GitError::CheckoutBranchFailedToSetHeadDetachedAtTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set head detached at tag while checking out the branch.");
  case GitError::OpenRepositoryFailedToOpenRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to open repository while opening the repository.");
  case GitError::ListCheckoutBranchesFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToReadNextBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read next branch while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToCheckIfBranchIsCheckedOut:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to check if branch is checked out while "
                        "listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToListTags:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list tags while listing checkout branches.");
  case GitError::ListCheckoutBranchesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while listing checkout branches.");
  case GitError::ListBranchesFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while listing branches.");
  case GitError::ListBranchesFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while listing branches.");
  case GitError::ListBranchesFailedToReadNextBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read next branch while listing branches.");
  case GitError::ListBranchesFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while listing branches.");
  case GitError::ListRemoteBranchesWithStatusFailedToCalculateAheadBehind:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to calculate ahead behind while listing "
                        "remote branches with status.");
  case GitError::ListStashesFailedToListStashes:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list stashes while listing stashes.");
  case GitError::GetStashStatusFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while getting stash status.");
  case GitError::GetStashStatusFailedToPeelObjectToCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel object to commit while getting stash status.");
  case GitError::GetStashStatusFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToGetBaseParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get base parent commit while getting stash status.");
  case GitError::GetStashStatusFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToGetThirdCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get third commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToGetFourthCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get fourth commit tree while getting stash status.");
  case GitError::GetStashStatusFailedToCreateStashDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create stash diff while getting stash status.");
  case GitError::GetStashStatusFailedToWalkUntrackedStashTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to walk untracked stash tree while getting stash status.");
  case GitError::ListTagsFailedToListTags:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list tags while listing tags.");
  case GitError::ListSubmodulesFailedToIterateSubmodules:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to iterate submodules while listing submodules.");
  case GitError::CreateBranchFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while creating a branch from a commit.");
  case GitError::CreateBranchFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while creating a branch from a commit.");
  case GitError::CreateBranchFromCommitFailedToCreateBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch while creating a branch from a commit.");
  case GitError::DeleteStashFailedToDropStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to drop stash while delete stash.");
  case GitError::DeleteAllStashesFailedToReadObjectId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read object id while deleting all stashes.");
  case GitError::DeleteAllStashesFailedToDropStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to drop stash while deleting all stashes.");
  case GitError::InitRepositoryFailedToInitializeRepositoryOptions:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to initialize repository options while "
                        "initializing the repository.");
  case GitError::InitRepositoryFailedToInitializeRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to initialize repository while initializing the repository.");
  case GitError::CredentialsAcquireCallbackFailedToCreatePlaintextCredentials:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create plaintext credentials while acquiring credentials.");
  case GitError::CheckoutCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while checking out the commit.");
  case GitError::CheckoutCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while checking out the commit.");
  case GitError::CheckoutCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while checking out the commit.");
  case GitError::CheckoutCommitFailedToCheckoutCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout commit tree while checking out the commit.");
  case GitError::CheckoutCommitFailedToDetachHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to detach head while checking out the commit.");
  case GitError::CloneRepositoryFailedToCloneRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to clone repository while cloning the repository.");
  case GitError::CloneRepositoryGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Cloning git@ repositories requires a commercial plan.");
  case GitError::GetBranchHistoryFailedToCreateRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToConfigureRevwalkSorting:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to configure revwalk sorting while getting branch history.");
  case GitError::GetBranchHistoryFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while getting branch history.");
  case GitError::GetBranchHistoryFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddReferenceToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add reference to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddLocalBranchToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add local branch to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddRemoteBranchToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add remote branch to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToAddTagToRevwalk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add tag to revwalk while getting branch history.");
  case GitError::GetBranchHistoryFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while getting branch history.");
  case GitError::GetCommitBranchesFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while getting commit branches.");
  case GitError::GetCommitBranchesFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while getting commit branches.");
  case GitError::GetCommitBranchesFailedToResolveReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve reference while getting commit branches.");
  case GitError::GetCommitBranchesFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel reference while getting commit branches.");
  case GitError::GetCommitBranchesFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while getting commit branches.");
  case GitError::GetCommitTagsFailedToListMatchingTags:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list matching tags while get commit tags.");
  case GitError::GetCommitHeadsFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create branch iterator while getting commit heads.");
  case GitError::GetCommitHeadsFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while getting commit heads.");
  case GitError::GetCommitHeadsFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read branch name while getting commit heads.");
  case GitError::GetCommitHeadsFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while getting commit heads.");
  case GitError::GetCommitIsHeadFailedToCreateBranchIterator:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create branch iterator while checking "
                        "whether the commit is a branch head.");
  case GitError::GetCommitIsHeadFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while checking whether the "
                        "commit is a branch head.");
  case GitError::GetCommitIsHeadFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while checking whether "
                        "the commit is a branch head.");
  case GitError::GetCommitIsHeadFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to parse object id while checking whether "
                        "the commit is a branch head.");
  case GitError::GetStatusFailedToCreateStatusList:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create status list while get status.");
  case GitError::GetCommitStatusFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup commit while getting commit status.");
  case GitError::GetCommitStatusFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while getting commit status.");
  case GitError::GetCommitStatusFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while getting commit status.");
  case GitError::GetCommitStatusFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while getting commit status.");
  case GitError::GetCommitStatusFailedToCreateCommitDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit diff while getting commit status.");
  case GitError::GetCommitStatusFailedToDetectRenames:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to detect renames while getting commit status.");
  case GitError::StageFilesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository index while stage files.");
  case GitError::StageFilesFailedToRemovePathFromIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to remove path from index while stage files.");
  case GitError::StageFilesFailedToAddSubmoduleToIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add submodule to index while stage files.");
  case GitError::StageFilesFailedToAddPathToIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add path to index while stage files.");
  case GitError::StageFilesFailedToAddMissingPathToIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add missing path to index while stage files.");
  case GitError::StageFilesFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index while stage files.");
  case GitError::UnstageFilesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while unstage files.");
  case GitError::UnstageFilesFailedToResetPathsToHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to reset paths to head while unstage files.");
  case GitError::CommitStagedFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while committing staged changes.");
  case GitError::CommitStagedFailedToAbortRebaseSession:
    return QCoreApplication::translate("GitRepoError",
                                       "Failed to abort rebase session.");
  case GitError::CommitStagedFailedToOpenRebaseSession:
    return QCoreApplication::translate("GitRepoError",
                                       "Failed to open rebase session.");
  case GitError::CommitStagedFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree while committing staged changes.");
  case GitError::CommitStagedFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree while committing staged changes.");
  case GitError::CommitStagedFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while committing staged changes.");
  case GitError::CommitStagedFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while committing staged changes.");
  case GitError::CommitStagedFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while committing staged changes.");
  case GitError::CommitStagedFailedToResolveMergeHeadReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve merge head reference while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToGetSecondReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get second reference target while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToLoadSigningConfiguration:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to load signing configuration while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToSignCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to sign commit while committing staged changes.");
  case GitError::CommitStagedFailedToCreateCommitFromStagedChanges:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit from staged changes while "
                        "committing staged changes.");
  case GitError::CommitStagedFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit while committing staged changes.");
  case GitError::CommitStagedFailedToCleanupRepositoryState:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to cleanup repository state while committing staged changes.");
  case GitError::DiffFilesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while diffing files.");
  case GitError::DiffFilesFailedToGetHeadObjectId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get head object id while diffing files.");
  case GitError::DiffFilesFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get commit tree while diffing files.");
  case GitError::DiffFilesFailedToDiffTreeAgainstWorkdir:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to diff tree against workdir while diffing files.");
  case GitError::DiffFilesFailedToCreatePatchFromDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create patch from diff while diffing files.");
  case GitError::DiffFilesFailedToReadPatchHunk:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read patch hunk while diffing files.");
  case GitError::DiffFilesFailedToReadPatchLine:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read patch line while diffing files.");
  case GitError::DiffFilesFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while diffing files.");
  case GitError::DiffFilesFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while diffing files.");
  case GitError::DiffFilesFailedToReadHeadCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read head commit while diffing files.");
  case GitError::DiffFilesFailedToLookupTreeEntryByPath:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree entry by path while diffing files.");
  case GitError::DiffFilesFailedToReadTreeEntryId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read tree entry id while diffing files.");
  case GitError::DiffFilesFailedToLookupBlobByPrefix:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup blob by prefix while diffing files.");
  case GitError::DiscardFilesFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while discarding files.");
  case GitError::DiscardFilesFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout tree while discarding files.");
  case GitError::DiscardFilesFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while discarding files.");
  case GitError::DiscardFilesFailedToResolveHeadRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve head revision while discarding files.");
  case GitError::DiscardFilesFailedToGetHeadTreeObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get head tree object id while discarding files.");
  case GitError::DiscardFilesFailedToLookupTreeEntryByPath:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree entry by path while discarding files.");
  case GitError::DiscardFilesFailedToReadTreeEntryId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read tree entry id while discarding files.");
  case GitError::DiscardFilesFailedToRemoveIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to remove index entry while discarding files.");
  case GitError::DiscardFilesFailedToAddIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to add index entry while discarding files.");
  case GitError::DiscardFilesFailedToRemoveDeletedIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to remove deleted index entry while discarding files.");
  case GitError::DiscardFilesFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index while discarding files.");
  case GitError::PullFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while pulling changes.");
  case GitError::PullFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while pulling changes.");
  case GitError::PullNoUpstreamConfigured:
    return QCoreApplication::translate(
        "GitRepoError", "No upstream is configured while pulling changes.");
  case GitError::PullFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pulling changes.");
  case GitError::PullFailedToFetchRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to fetch remote while pulling changes.");
  case GitError::PullFailedToFetchRemoteWithCredentials:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to fetch remote with credentials while pulling changes.");
  case GitError::PullFailedToGetRepositoryHeadAfterFetch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head after fetch while pulling changes.");
  case GitError::PullFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup reference while pulling changes.");
  case GitError::PullFailedToCreateAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create annotated commit from reference "
                        "while pulling changes.");
  case GitError::PullFailedToGetDefaultSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get default signature while pulling changes.");
  case GitError::PullFailedToStartRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to start rebase while pulling changes.");
  case GitError::PullFailedToCommitRebaseStep:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to commit rebase step while pulling changes.");
  case GitError::PullFailedToFinishRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to finish rebase while pulling changes.");
  case GitError::PullFailedToAbortRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to abort rebase while pulling changes.");
  case GitError::PullFailedToCreateAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create annotated commit while pulling changes.");
  case GitError::PullFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup commit while pulling changes.");
  case GitError::PullFailedToPeelFetchedTargetCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel fetched target commit while pulling changes.");
  case GitError::PullFailedToSetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set reference target while pulling changes.");
  case GitError::PullFailedToCreateMergeAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create merge annotated commit while pulling changes.");
  case GitError::PullGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pulling git@ repositories requires a commercial plan.");
  case GitError::FetchRemoteInternalFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while fetching from the remote.");
  case GitError::FetchRemoteGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Fetching git@ repositories requires a commercial plan.");
  case GitError::FetchRemoteInternalFailedToFetchRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to fetch remote while fetching from the remote.");
  case GitError::FetchRemoteInternalFailedToFetchRemoteWithCredentials:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to fetch remote with credentials while "
                        "fetching from the remote.");
  case GitError::PushFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get repository head while pushing changes.");
  case GitError::PushGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pushing git@ repositories requires a commercial plan.");
  case GitError::PushFailedToReadBranchName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read branch name while pushing changes.");
  case GitError::PushFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pushing changes.");
  case GitError::PushFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while pushing changes.");
  case GitError::PushTagFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup reference while pushing the tag.");
  case GitError::PushTagGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pushing tags to git@ repositories requires a commercial plan.");
  case GitError::PushTagFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pushing the tag.");
  case GitError::PushTagFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while pushing the tag.");
  case GitError::PushAllTagsFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup remote while pushing all tags.");
  case GitError::PushAllTagsGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Pushing all tags to git@ repositories requires a commercial plan.");
  case GitError::PushAllTagsFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while pushing all tags.");
  case GitError::StashFailedToGetDefaultSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get default signature while creating a stash.");
  case GitError::StashFailedToCreateStash:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create stash while creating a stash.");
  case GitError::SetUpstreamFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while setting upstream.");
  case GitError::SetUpstreamFailedToSetBranchUpstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set branch upstream while setting upstream.");
  case GitError::GetUpstreamFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while getting upstream.");
  case GitError::GetUpstreamFailedToGetBranchUpstream:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get branch upstream while getting upstream.");
  case GitError::GetUpstreamFailedToOpenGitConfig:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to open git config while getting upstream.");
  case GitError::ResetToCommitFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve revision while resetting to the commit.");
  case GitError::ResetToCommitFailedToResetRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to reset repository while resetting to the commit.");
  case GitError::RevertCommitFailedToLookupTargetCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup target commit while reverting the commit.");
  case GitError::RevertCommitFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while reverting the commit.");
  case GitError::RevertCommitFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while reverting the commit.");
  case GitError::RevertCommitFailedToPrepareRevertCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to prepare revert commit while reverting the commit.");
  case GitError::RevertCommitFailedToCheckoutIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout index while reverting the commit.");
  case GitError::RevertCommitFailedToWriteIndexTreeToRepository:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree to repository while reverting the commit.");
  case GitError::RevertCommitFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while reverting the commit.");
  case GitError::RevertCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while reverting the commit.");
  case GitError::RevertCommitFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit while reverting the commit.");
  case GitError::ApplyPatchFailedToParsePatchBuffer:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse patch buffer while applying the patch.");
  case GitError::ApplyPatchFailedToApplyPatch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to apply patch while applying the patch.");
  case GitError::CherryPickCommitFailedToLookupTargetCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup target commit while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToPrepareCherryPickCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to prepare cherry pick commit while "
                        "cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToCheckoutIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout index while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToWriteIndexTreeToRepository:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index tree to repository while "
                        "cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup tree while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while cherry-picking the commit.");
  case GitError::CherryPickCommitFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit while cherry-picking the commit.");
  case GitError::RenameBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while renaming the branch.");
  case GitError::RenameBranchFailedToRenameBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to rename branch while renaming the branch.");
  case GitError::DeleteBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while deleting the branch.");
  case GitError::DeleteBranchFailedToDeleteBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete branch while deleting the branch.");
  case GitError::DeleteRemoteTagFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while deleting the remote tag.");
  case GitError::DeleteRemoteTagGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError", "Deleting remote tags from git@ repositories requires "
                        "a commercial plan.");
  case GitError::DeleteRemoteTagFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to push remote while deleting the remote tag.");
  case GitError::CreateTagFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while creating a tag from a commit.");
  case GitError::CreateTagFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while creating a tag from a commit.");
  case GitError::CreateTagFromCommitFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while creating a tag from a commit.");
  case GitError::CreateTagFromCommitFailedToPeelCommitForTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to peel commit for tag while creating a tag from a commit.");
  case GitError::CreatePatchFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get second commit tree while creating a "
                        "patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToCreateCommitDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit diff while creating a patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToCreatePatchFromDiff:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create patch from diff while creating a "
                        "patch from a commit.");
  case GitError::CreatePatchFromCommitFailedToRenderPatchToBuffer:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to render patch to buffer while creating a "
                        "patch from a commit.");
  case GitError::RenameTagFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup reference while renaming the tag.");
  case GitError::RenameTagFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while renaming the tag.");
  case GitError::RenameTagFailedToReadTagTarget:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read tag target while renaming the tag.");
  case GitError::RenameTagFailedToCreateAnnotatedTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create annotated tag while renaming the tag.");
  case GitError::RenameTagFailedToCreateLightweightTag:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create lightweight tag while renaming the tag.");
  case GitError::RenameTagFailedToDeleteTag:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete tag while renaming the tag.");
  case GitError::DeleteTagFailedToDeleteTag:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete tag while deleting the tag.");
  case GitError::DeleteRemoteBranchFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while deleting the remote branch.");
  case GitError::DeleteRemoteBranchGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError", "Deleting remote branches from git@ repositories "
                        "requires a commercial plan.");
  case GitError::DeleteRemoteBranchFailedToPushRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to push remote while deleting the remote branch.");
  case GitError::GetSubmodulePathFailedToLookupSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup submodule while getting the submodule path.");
  case GitError::DeleteSubmoduleFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while deleting the submodule.");
  case GitError::DeleteSubmoduleFailedToRemoveIndexEntry:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to remove index entry while deleting the submodule.");
  case GitError::DeleteSubmoduleFailedToWriteIndex:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index while deleting the submodule.");
  case GitError::CheckoutSubmoduleBranchFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToFetchRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to fetch remote while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToLookupReference:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup reference while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToPeelReferenceToCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference to commit while checking "
                        "out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToCreateLocalBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create local branch while checking out "
                        "the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToReadCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read commit id while checking out the submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToSetHeadToBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set head to branch while checking out the "
                        "submodule branch.");
  case GitError::CheckoutSubmoduleBranchFailedToCheckoutHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout head while checking out the submodule branch.");
  case GitError::CheckoutLocalSubmoduleBranchFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while checking out the "
                        "local submodule branch.");
  case GitError::CheckoutLocalSubmoduleBranchFailedToSetHeadToBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set head to branch while checking out the "
                        "local submodule branch.");
  case GitError::CheckoutLocalSubmoduleBranchFailedToCheckoutHead:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout head while checking out the "
                        "local submodule branch.");
  case GitError::UpdateSubmoduleFailedToUpdateSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to update submodule while updating the submodule.");
  case GitError::UpdateSubmoduleFailedToAddSubmoduleToIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to add submodule to index while updating the submodule.");
  case GitError::AddSubmoduleFailedToSetupSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to setup submodule while adding the submodule.");
  case GitError::AddSubmoduleGitSshUrlRequiresCommercialPlan:
    return QCoreApplication::translate(
        "GitRepoError",
        "Adding submodules to git@ repositories requires a commercial plan.");
  case GitError::AddSubmoduleFailedToCloneSubmodule:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to clone submodule while adding the submodule.");
  case GitError::AddSubmoduleFailedToFinalizeSubmoduleAdd:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to finalize submodule add while adding the submodule.");
  case GitError::DiffFilesFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetSecondCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second commit tree while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetSecondParentCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get second parent commit while diffing "
                        "files from a commit.");
  case GitError::DiffFilesFromCommitFailedToGetThirdCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get third commit tree while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToCreateCommitDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create commit diff while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToCreatePatchFromDiff:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create patch from diff while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToReadPatchHunk:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read patch hunk while diffing files from a commit.");
  case GitError::DiffFilesFromCommitFailedToReadPatchLine:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read patch line while diffing files from a commit.");
  case GitError::GetFileSizeFromCommitFailedToGetRepositoryHead:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository head while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToGetReferenceName:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference name while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToParseObjectId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to parse object id while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup commit while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToGetCommitTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get commit tree while getting file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToLookupTreeEntryByPath:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree entry by path while getting "
                        "file size from a commit.");
  case GitError::GetFileSizeFromCommitFailedToLookupBlob:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup blob while getting file size from a commit.");
  case GitError::MergeFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while merging branches.");
  case GitError::MergeFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while merging branches.");
  case GitError::MergeFailedToPeelReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to peel reference while merging branches.");
  case GitError::MergeFailedToCheckoutTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to checkout tree while merging branches.");
  case GitError::MergeFailedToGetReferenceName:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to get reference name while merging branches.");
  case GitError::MergeFailedToCreateAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create annotated commit from reference "
                        "while merging branches.");
  case GitError::MergeFailedToCreateSecondAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create second annotated commit from "
                        "reference while merging branches.");
  case GitError::MergeFailedToAnalyzeMerge:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to analyze merge while merging branches.");
  case GitError::MergeFailedToReadAnnotatedCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read annotated commit id while merging branches.");
  case GitError::MergeFailedToReadCommitId:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to read commit id while merging branches.");
  case GitError::MergeFailedToReadSecondCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read second commit id while merging branches.");
  case GitError::MergeFailedToCheckoutDestinationTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to checkout destination tree while merging branches.");
  case GitError::MergeFailedToMerge:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to merge while merging branches.");
  case GitError::MergeFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while merging branches.");
  case GitError::MergeFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write index tree while merging branches.");
  case GitError::MergeFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while merging branches.");
  case GitError::MergeFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create signature while merging branches.");
  case GitError::MergeFailedToGetReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get reference target while merging branches.");
  case GitError::MergeFailedToGetSecondReferenceTarget:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get second reference target while merging branches.");
  case GitError::MergeFailedToCreateCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create commit while merging branches.");
  case GitError::RebaseBranchesFailedToLookupBranch:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup branch while rebasing branches.");
  case GitError::RebaseBranchesFailedToLookupDestinationBranch:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup destination branch while rebasing branches.");
  case GitError::RebaseBranchesFailedToCreateAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create annotated commit from reference "
                        "while rebasing branches.");
  case GitError::RebaseBranchesFailedToCreateSecondAnnotatedCommitFromReference:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create second annotated commit from "
                        "reference while rebasing branches.");
  case GitError::RebaseBranchesFailedToReadAnnotatedCommitId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to read annotated commit id while rebasing branches.");
  case GitError::RebaseBranchesFailedToLookupAnnotatedCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup annotated commit while rebasing branches.");
  case GitError::RebaseBranchesFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create signature while rebasing branches.");
  case GitError::RebaseBranchesFailedToStartRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to start rebase while rebasing branches.");
  case GitError::RebaseBranchesFailedToCommitRebaseStep:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to commit rebase step while rebasing branches.");
  case GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to commit rebase step while rebasing branches "
                        "due to conflicts in files.");
  case GitError::RebaseBranchesFailedToFinishRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to finish rebase while rebasing branches.");
  case GitError::RebaseBranchesFailedToAbortRebase:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to abort rebase while rebasing branches.");
  case GitError::ListConflictsForAFileFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while listing conflicts for a file.");
  case GitError::ListConflictsForAFileFailedToCreateConflictIterator:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create conflict iterator while listing "
                        "conflicts for a file.");
  case GitError::AbortMergeFailedToCleanupRepositoryState:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to cleanup repository state while aborting the merge.");
  case GitError::AbortMergeFailedToResolveRevision:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to resolve revision while aborting the merge.");
  case GitError::AbortMergeFailedToResetRepository:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to reset repository while aborting the merge.");
  case GitError::ListRemotesFailedToListRemotes:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to list remotes while listing remotes.");
  case GitError::SetDefaultRemoteFailedToLookupRemote:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to lookup remote while setting the default remote.");
  case GitError::SetDefaultRemoteFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while setting the default remote.");
  case GitError::SetDefaultRemoteFailedToWriteDefaultRemoteConfig:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to write default remote config while setting "
                        "the default remote.");
  case GitError::AddRemoteFailedToCreateRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to create remote while adding the remote.");
  case GitError::AddRemoteFailedToSetPushUrl:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set push url while adding the remote.");
  case GitError::RemoveRemoteFailedToDeleteRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to delete remote while removing the remote.");
  case GitError::UpdateRemoteFailedToRenameRemote:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to rename remote while updating the remote.");
  case GitError::UpdateRemoteFailedToSetFetchUrl:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set fetch url while updating the remote.");
  case GitError::UpdateRemoteFailedToSetPushUrl:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to set push url while updating the remote.");
  case GitError::AddTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while adding the target.");
  case GitError::AddTargetFailedToSetFetchUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set fetch url config while adding the target.");
  case GitError::AddTargetFailedToSetPushUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set push url config while adding the target.");
  case GitError::EditTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while editing the target.");
  case GitError::EditTargetFailedToSetFetchUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set fetch url config while editing the target.");
  case GitError::EditTargetFailedToSetPushUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set push url config while editing the target.");
  case GitError::DeleteTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while deleting the target.");
  case GitError::DeleteTargetFailedToDeleteFetchUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to delete fetch url config while deleting the target.");
  case GitError::DeleteTargetFailedToDeletePushUrlConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to delete push url config while deleting the target.");
  case GitError::SetDefaultTargetFailedToGetRepositoryConfig:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository config while setting the default target.");
  case GitError::SetDefaultTargetFailedToCreateConfigIterator:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create config iterator while setting the default target.");
  case GitError::SetDefaultTargetFailedToSetDefaultFetchUrl:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to set default fetch url while setting the default target.");
  case GitError::AmendFailedToResolveReferenceNameToId:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to resolve reference name to id while amending the commit.");
  case GitError::AmendFailedToLookupCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup commit while amending the commit.");
  case GitError::AmendFailedToGetRepositoryIndex:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get repository index while amending the commit.");
  case GitError::AmendFailedToWriteIndexTree:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to write index tree while amending the commit.");
  case GitError::AmendFailedToLookupTree:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to lookup tree while amending the commit.");
  case GitError::AmendFailedToCreateSignature:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signature while amending the commit.");
  case GitError::AmendFailedToGetParentCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to get parent commit while amending the commit.");
  case GitError::AmendFailedToLoadSigningConfiguration:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to load signing configuration while amending the commit.");
  case GitError::AmendFailedToSignCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to sign commit while amending the commit.");
  case GitError::AmendFailedToCreateSignedAmendCommit:
    return QCoreApplication::translate(
        "GitRepoError",
        "Failed to create signed amend commit while amending the commit.");
  case GitError::AmendFailedToAmendCommit:
    return QCoreApplication::translate(
        "GitRepoError", "Failed to amend commit while amending the commit.");
  case GitError::Last:
    return QCoreApplication::translate("GitRepoError",
                                       "Invalid Git error marker.");
  }

  return QCoreApplication::translate("GitRepoError",
                                     "An unknown Git error occurred.");
}

static QString gitErrorMessage(const GitError errorType) {
  return gitErrorName(errorType);
}

static void setErrorContext(const GitError errorType, const int errorCode) {
  LOCKGUARD(gErrorMutex);
  gErrorMap[std::this_thread::get_id()] = {errorType, errorCode,
                                           gitErrorMessage(errorType)};
}

static void setError(const GitError errorType, const int error) {
  const git_error *gitError = git_error_last();
  const int klass = gitError ? gitError->klass : 0;
  const QString libgitMessage =
      gitError && gitError->message
          ? capitalizeFirst(QString::fromUtf8(gitError->message))
          : QString();

  setErrorContext(errorType, encodeLibgitError(error, klass));

  LOG_ERROR(
      Log::ErrorId::Git,
      "Error type is {}, error is {}, error klass is {}, error message is "
      "'{}', libgit message is '{}'",
      static_cast<int>(errorType), error, klass,
      gitErrorMessage(errorType).toStdString(), libgitMessage.toStdString());
}

static bool markConflictedFiles(git_repository *repo,
                                const QStringList &filePaths) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (filePaths.isEmpty()) {
    LOG_DEBUG("No conflicted files to mark in the index");
    LOG_PERF(0, "Exit");
    return true;
  }

  git_index *index = nullptr;
  int error = git_repository_index(&index, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get repository index when marking conflicts: "
              "error={}",
              error);
    setError(GitError::MarkConflictedFilesFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  for (const auto &filePath : filePaths) {
    if (filePath.isEmpty()) {
      continue;
    }

    const QByteArray data = filePath.toUtf8();

    LOG_INFO("Marking conflict in: {}", filePath);

    // Add conflict entry manually
    git_index_entry ancestor = {0}, ours = {0}, theirs = {0};
    ancestor.path = data.constData();
    ancestor.mode = GIT_FILEMODE_BLOB;

    error = git_blob_create_fromworkdir(&ancestor.id, repo, ancestor.path);
    if (error) {
      setError(
          GitError::MarkConflictedFilesFailedToCreateAncestorBlobFromWorkdir,
          error);
    }

    ours.path = data.constData();
    ours.mode = GIT_FILEMODE_BLOB;

    error = git_blob_create_fromworkdir(&ours.id, repo, ours.path);
    if (error) {
      setError(GitError::MarkConflictedFilesFailedToCreateOursBlobFromWorkdir,
               error);
    }

    theirs.path = data.constData();
    theirs.mode = GIT_FILEMODE_BLOB;

    error = git_blob_create_fromworkdir(&theirs.id, repo, theirs.path);
    if (error) {
      setError(GitError::MarkConflictedFilesFailedToCreateTheirsBlobFromWorkdir,
               error);
    }

    error = git_index_conflict_add(index, &ancestor, &ours, &theirs);
    if (error) {
      setError(GitError::MarkConflictedFilesFailedToAddConflictToIndex, error);
    }
  }

  error = git_index_write(index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to write conflicted files to the index: error={}", error);
    setError(GitError::MarkConflictedFilesFailedToWriteIndex, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  LOG_DEBUG("Marked {} conflicted files in the index", filePaths.size());
  LOG_PERF(0, "Exit");
  return true;
}

static bool file_has_conflict_marker_triplet(const QString &path,
                                             const QString &startMarker,
                                             const QString &middleMarker,
                                             const QString &endMarker) {
  if (path.isEmpty()) {
    return false;
  }

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  bool foundStart = false;
  bool foundMiddle = false;
  bool foundEnd = false;

  while (!stream.atEnd()) {
    const QString line = stream.readLine();
    foundStart |= line.startsWith(startMarker, Qt::CaseSensitive);
    foundMiddle |= line.startsWith(middleMarker, Qt::CaseSensitive);
    foundEnd |= line.startsWith(endMarker, Qt::CaseSensitive);

    if (foundStart && foundMiddle && foundEnd) {
      break;
    }
  }

  return foundStart && foundMiddle && foundEnd;
}

static bool file_has_conflict_markers(const QString &path) {
  return file_has_conflict_marker_triplet(path, "<<<<<<<",
                                          "=======", ">>>>>>>");
}

static bool collectConflictedFilePathsFromResultIndex(git_repository *repo,
                                                      git_index *index,
                                                      QStringList *filePaths) {
  LOG_PERF(0, "Enter");

  if (!repo || !index || !filePaths) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository, index pointer or file paths container is null when "
              "collecting conflicted files");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *headRef = nullptr;
  git_commit *headCommit = nullptr;
  git_tree *headTree = nullptr;
  git_diff *candidateDiff = nullptr;
  git_index_conflict_iterator *conflictIterator = nullptr;
  QStringList candidatePaths;
  QSet<QString> uniqueCandidatePaths;

  int error = git_repository_head(&headRef, repo);
  if (error) {
    setError(
        GitError::
            CollectConflictedFilePathsFromResultIndexFailedToGetRepositoryHead,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reference_peel(reinterpret_cast<git_object **>(&headCommit),
                             headRef, GIT_OBJECT_COMMIT);
  if (error) {
    setError(GitError::CollectConflictedFilePathsFailedToPeelHeadToCommit,
             error);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_tree(&headTree, headCommit);
  if (error) {
    setError(GitError::
                 CollectConflictedFilePathsFromResultIndexFailedToGetCommitTree,
             error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_diff_tree_to_index(&candidateDiff, repo, headTree, index, NULL);
  git_tree_free(headTree);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  if (error) {
    setError(
        GitError::
            CollectConflictedFilePathsFromResultIndexFailedToDiffTreeAgainstIndex,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const auto appendCandidatePath = [&](const char *relativePathCstr) {
    if (!relativePathCstr) {
      return;
    }

    const QString relativePath = QString::fromUtf8(relativePathCstr);
    if (relativePath.isEmpty() || uniqueCandidatePaths.contains(relativePath)) {
      return;
    }

    uniqueCandidatePaths.insert(relativePath);
    candidatePaths.push_back(relativePath);
  };

  const size_t numDeltas = git_diff_num_deltas(candidateDiff);
  for (size_t i = 0; i < numDeltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(candidateDiff, i);
    if (!delta) {
      continue;
    }

    appendCandidatePath(delta->new_file.path ? delta->new_file.path
                                             : delta->old_file.path);
  }

  if (candidatePaths.isEmpty()) {
    error = git_index_conflict_iterator_new(&conflictIterator, index);
    if (error) {
      setError(
          GitError::
              CollectConflictedFilePathsFromResultIndexFailedToCreateConflictIterator,
          error);
      git_diff_free(candidateDiff);
      LOG_PERF(0, "Exit");
      return false;
    }

    const git_index_entry *ancestor = nullptr;
    const git_index_entry *ours = nullptr;
    const git_index_entry *theirs = nullptr;

    while (
        !git_index_conflict_next(&ancestor, &ours, &theirs, conflictIterator)) {
      const git_index_entry *entry = ours ? ours : (theirs ? theirs : ancestor);
      if (!entry) {
        continue;
      }

      appendCandidatePath(entry->path);
    }
  }

  const char *repoWorkdir = git_repository_workdir(repo);
  QString workDir;
  if (repoWorkdir) {
    workDir = repoWorkdir;
  }

  QSet<QString> uniquePaths;
  for (const QString &relativePath : candidatePaths) {
    QString filePath = QDir(workDir).absoluteFilePath(relativePath);
    filePath = QDir::toNativeSeparators(filePath);

    if (!file_has_conflict_markers(filePath) ||
        uniquePaths.contains(relativePath)) {
      continue;
    }

    uniquePaths.insert(relativePath);
    filePaths->push_back(relativePath);
  }

  git_index_conflict_iterator_free(conflictIterator);
  git_diff_free(candidateDiff);

  LOG_DEBUG("Collected {} conflicted files", filePaths->size());
  LOG_PERF(0, "Exit");
  return true;
}

struct GitCredentialPayload : public CloneProgress {
  QString repoPath;
};

static void git_strarray_destroy(git_strarray &arr) {
  for (int i = 0; i < arr.count; ++i) {
    free(arr.strings[i]);
  }

  free(arr.strings);

  arr.strings = nullptr;
  arr.count = 0;
}

static git_strarray qstringlist_to_git_strarray(const QStringList &list) {
  git_strarray arr;
  arr.count = static_cast<size_t>(list.length());
  arr.strings = static_cast<char **>(malloc(arr.count * sizeof(char *)));

  for (size_t i = 0; i < arr.count; ++i) {
    arr.strings[i] = strdup(list[i].toStdString().c_str()); // allocates a copy
  }

  return arr;
}

static void collectCommitStatusFromDiff(git_diff *diff,
                                        GitCommitStatus *status) {
  if (!diff || !status) {
    return;
  }

  git_diff_foreach(
      diff,
      [](const git_diff_delta *delta, float, void *payload) {
        auto *status = static_cast<GitCommitStatus *>(payload);

        switch (delta->status) {
        case GIT_DELTA_ADDED:
        case GIT_DELTA_UNTRACKED:
          status->newFiles += delta->new_file.path;
          break;
        case GIT_DELTA_MODIFIED:
          status->modifiedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_DELETED:
          status->deletedFiles += delta->old_file.path;
          break;
        case GIT_DELTA_RENAMED:
          status->renamedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_TYPECHANGE:
          status->typeChangedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_UNMODIFIED:
        case GIT_DELTA_IGNORED:
        case GIT_DELTA_CONFLICTED:
          break;
        }

        return 0;
      },
      nullptr, // binary_cb
      nullptr, // hunk_cb
      nullptr, // line_cb
      status);
}

struct CollectTreePathsPayload {
  GitCommitStatus *status = nullptr;
  QStringList *paths = nullptr;
};

static int collectTreePathsCallback(const char *root,
                                    const git_tree_entry *entry,
                                    void *payload) {
  auto *data = static_cast<CollectTreePathsPayload *>(payload);
  if (!data || !data->status || !data->paths || !entry) {
    return -1;
  }

  if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
    return 0;
  }

  const QString path = QString::fromUtf8(root ? root : "") +
                       QString::fromUtf8(git_tree_entry_name(entry));
  data->paths->append(path);

  return 0;
}

static bool isLfsTrackedPath(git_repository *repo,
                             const QString &relativePath) {
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

static bool stageLfsTrackedFile(git_repository *repo, git_index *index,
                                const QString &relativePath,
                                const QString &fullPath, QString *stagedOid) {
  QFile file(fullPath);
  if (!file.open(QIODevice::ReadOnly)) {
    git_error_set_str(GIT_ERROR_OS, "unable to read lfs source file");
    return false;
  }

  const QByteArray content = file.readAll();
  file.close();

  LOG_DEBUG("Staging LFS file '{}' from '{}' ({} bytes)",
            relativePath.toStdString(), fullPath.toStdString(), content.size());

  QString oid;
  if (!GitRepo::Lfs::storeObject(repo, content, oid)) {
    return false;
  }

  if (stagedOid) {
    *stagedOid = oid;
  }

  const QByteArray pointer = GitRepo::Lfs::makePointer(oid, content.size());
  git_index_entry entry = {};
  const QByteArray relativeUtf8 = relativePath.toUtf8();
  entry.path = relativeUtf8.constData();
  entry.mode = GIT_FILEMODE_BLOB;
  const int error = git_index_add_from_buffer(
      index, &entry, pointer.constData(), pointer.size());
  if (error) {
    setError(GitError::StageFilesFailedToAddPathToIndex, error);
    return false;
  }

  LOG_DEBUG("Staged LFS pointer for '{}' with oid '{}'",
            relativePath.toStdString(), oid.toStdString());
  return true;
}

static bool readTreeFileContent(git_repository *repo, git_tree *tree,
                                const QString &filePath,
                                const qint64 maxBlobSize, QString *content) {
  if (!repo || !tree || !content) {
    return false;
  }

  git_tree_entry *entry = nullptr;
  const int error =
      git_tree_entry_bypath(&entry, tree, filePath.toStdString().c_str());
  if (error) {
    return false;
  }

  const git_oid *fullOid = git_tree_entry_id(entry);
  if (!fullOid) {
    git_tree_entry_free(entry);
    return false;
  }

  git_blob *blob = nullptr;
  int blobError = git_blob_lookup(&blob, repo, fullOid);
  if (blobError) {
    git_tree_entry_free(entry);
    return false;
  }

  const size_t blobSize = git_blob_rawsize(blob);
  if (blobSize > static_cast<size_t>(maxBlobSize)) {
    *content =
        QString("[[FILE SIZE EXCEEDS LIMIT OF %1 BYTES]]").arg(maxBlobSize);
    git_blob_free(blob);
    git_tree_entry_free(entry);
    return true;
  }

  const char *blobContent =
      static_cast<const char *>(git_blob_rawcontent(blob));
  *content = QString::fromUtf8(blobContent, blobSize);

  git_blob_free(blob);
  git_tree_entry_free(entry);
  return true;
}

static void invalidateCredentials(const QString &url) {
  LOCKGUARD(gGitCredentialsMutex);

  const auto it = gGitCredentials.find(url);
  if (it != gGitCredentials.end())
    gGitCredentials.erase(it);
}

static void sendInvalidCredentialsSignal(const QString &url) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  EventManager::Event event(
      EventManager::EventType::GitRepoManagerGetCredentialsInvalid);
  event.addData(qHash(url));

  eventManager.send(event);
}

static bool createProfileSignature(git_signature **sig, GitError errorId,
                                   const QString &author,
                                   const QString &email) {
  if (author.isEmpty() || email.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot create signature for initial commit: author or email "
              "is empty.");
    setError(errorId, GIT_EINVALID);
    return false;
  }

  const std::string authorStd = author.toStdString();
  const std::string emailStd = email.toStdString();

  const int error = git_signature_now(sig, authorStd.c_str(), emailStd.c_str());
  if (error) {
    setError(errorId, error);
    return false;
  }

  return true;
}

static bool createInitialCommit(git_repository *repo,
                                const QString &defaultBranchName,
                                const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  git_signature *sig = nullptr;

  if (!createProfileSignature(
          &sig, GitError::CreateInitialCommitFailedToCreateSignature, author,
          email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = 0;

  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid tree_id;

  error = git_index_write_tree(&tree_id, index);
  if (error) {
    git_index_free(index);
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToWriteIndexTree, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  git_tree *tree = nullptr;

  error = git_tree_lookup(&tree, repo, &tree_id);
  if (error) {
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToLookupTree, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid commit_id;

  error = git_commit_create_v(&commit_id, repo, HEAD, sig, sig, NULL,
                              "Initial commit", tree, 0);
  if (error) {
    git_tree_free(tree);
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitFailedToCreateCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree_free(tree);
  git_signature_free(sig);

  git_branch_iterator *it = nullptr;

  error = git_branch_iterator_new(&it, repo, GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::CreateInitialCommitFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!it) {
    LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
    LOG_PERF(0, "Exit");
    return false;
  }

  bool foundMaster = false;
  git_reference *branchRef = nullptr;
  git_branch_t branchType = GIT_BRANCH_LOCAL;

  for (;;) {
    error = git_branch_next(&branchRef, &branchType, it);
    if (error == GIT_ITEROVER)
      break;

    if (error) {
      setError(GitError::CreateInitialCommitFailedToReadNextBranch, error);
      break;
    }

    if (!branchRef) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
      continue;
    }

    const char *branchName = nullptr;

    error = git_branch_name(&branchName, branchRef);
    if (error == GIT_EINVALID) {
      git_reference_free(branchRef);
      LOG_ERROR(Log::ErrorId::Git,
                "Reference is not a local or a remote branch.");
      continue;
    }

    if (error) {
      git_reference_free(branchRef);
      setError(GitError::CreateInitialCommitFailedToReadBranchName, error);
      break;
    }

    if (strcmp(branchName, "master") == 0) {
      foundMaster = true;
      break;
    }
  }

  if (!foundMaster) {
    git_reference_free(branchRef);
    LOG_ERROR(Log::ErrorId::Git, "Coldn't find master branch.");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *newRef = nullptr;

  error = git_branch_move(&newRef, branchRef,
                          defaultBranchName.toStdString().c_str(), false);
  if (error) {
    setError(GitError::CreateInitialCommitFailedToRenameBranch, error);
    git_reference_free(branchRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(newRef);
  git_reference_free(branchRef);

  LOG_PERF(0, "Exit");
  return true;
}

static bool createInitialCommitWithoutStaging(git_repository *repo,
                                              const QString &author,
                                              const QString &email) {
  LOG_PERF(0, "Enter");

  git_signature *sig = nullptr;
  if (!createProfileSignature(
          &sig,
          GitError::CreateInitialCommitWithoutStagingFailedToCreateSignature,
          author, email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = 0;

  // Create an empty tree builder
  git_treebuilder *builder = nullptr;
  error = git_treebuilder_new(&builder, repo, nullptr);

  if (error) {
    git_signature_free(sig);
    setError(
        GitError::CreateInitialCommitWithoutStagingFailedToCreateTreeBuilder,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write the empty tree
  git_oid tree_id;
  error = git_treebuilder_write(&tree_id, builder);
  git_treebuilder_free(builder);

  if (error) {
    git_signature_free(sig);
    setError(
        GitError::CreateInitialCommitWithoutStagingFailedToWriteTreeBuilder,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Lookup the empty tree
  git_tree *tree = nullptr;
  error = git_tree_lookup(&tree, repo, &tree_id);

  if (error) {
    git_signature_free(sig);
    setError(GitError::CreateInitialCommitWithoutStagingFailedToLookupTree,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create the initial commit with empty tree
  git_oid commit_id;
  error = git_commit_create_v(&commit_id, repo, HEAD, sig, sig, nullptr,
                              "Initial commit", tree, 0);

  git_tree_free(tree);
  git_signature_free(sig);

  if (error) {
    setError(GitError::CreateInitialCommitWithoutStagingFailedToCreateCommit,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

static git_commit *getLastCommit(git_repository *repo, const QString &name) {
  LOG_PERF(0, "Enter");

  git_commit *commit = nullptr;
  git_oid oid_parent_commit;

  int error = git_reference_name_to_id(&oid_parent_commit, repo,
                                       name.toStdString().c_str());
  if (error) {
    setError(GitError::GetLastCommitFailedToResolveReferenceNameToId, error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  error = git_commit_lookup(&commit, repo, &oid_parent_commit);
  if (error) {
    setError(GitError::GetLastCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return nullptr;
  }

  LOG_PERF(0, "Exit");
  return commit;
}

static bool isCommercialPlan() {
  LOG_PERF(0, "Enter");

  Util::CloudCredentialtorage &credentialStorage =
      Util::CloudCredentialtorage::getInstance();

  QString username;
  qint64 creationTime = -1;
  qint64 expirationTime = -1;
  if (!credentialStorage.getCredentialsInfo(username, creationTime,
                                            expirationTime)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get creation time for checking commercial plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  QString jwt;
  if (!credentialStorage.getJWT(jwt)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get JWT for checking commercial plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify JWT signature using public key before processing claims
  const QString publicKeyPem = Util::JWTDecoder::getJWTPublicKey();
  if (publicKeyPem.isEmpty()) {
    LOG_WARN("JWT public key is not configured, skipping signature "
             "verification. This should be configured for security.");
  } else {
    if (!Util::JWTDecoder::verifySignature(jwt, publicKeyPem)) {
      LOG_ERROR(Log::ErrorId::Security,
                "JWT signature verification failed for commercial plan check.");
      LOG_PERF(0, "Exit");
      return false;
    }
    LOG_DEBUG("JWT signature verification successful");
  }

  const Util::JWTDecoder::Claims claims = Util::JWTDecoder::decodeClaims(jwt);
  const qint64 now = QDateTime::currentMSecsSinceEpoch();
  const bool creationTimeInTheFuture = creationTime > now;
  const bool storageExpired = expirationTime < now;
  const bool jwtExpired = claims.exp < now;

  LOG_PERF(0, "Exit");

  return !creationTimeInTheFuture && !storageExpired && !jwtExpired &&
         (claims.plan == "Pro" || claims.plan == "Business");
}

GitErrorContext getLastError() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Retrieving last error for current thread");

  LOCKGUARD(gErrorMutex);

  const auto threadId = std::this_thread::get_id();
  if (gErrorMap.contains(threadId)) {
    const auto &error = gErrorMap[threadId];
    LOG_DEBUG("Thread error type: {}, code: {}, message: '{}'",
              static_cast<int>(error.error), error.errorCode,
              error.errorMessage.toStdString());
  } else {
    LOG_DEBUG("No error found for current thread");
  }

  LOG_PERF(0, "Exit");
  return gErrorMap[std::this_thread::get_id()];
}

void initForCurrentThread() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Initializing libgit2 for current thread");

  const int numberOfCalls = git_libgit2_init();

  if (!GitRepo::Lfs::initialize()) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to initialize Git LFS support");
  }

  LOG_DEBUG("Number of initializations is {}", numberOfCalls);
  LOG_DEBUG("libgit2 initialized successfully, total initialization count: {}",
            numberOfCalls);

  // Get timeout values from ConfigManager
  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong connectTimeout = configManager.getLong(
      ConfigManager::ConfigKey::GitRepoServerConnectTimeout);
  const qlonglong serverTimeout =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoServerTimeout);

  LOG_DEBUG("Setting server connect timeout to {} ms", connectTimeout);
  LOG_DEBUG("Setting server timeout to {} ms", serverTimeout);

  // Set SSH connection timeout (applies to initial connection establishment)
  git_libgit2_opts(GIT_OPT_SET_SERVER_CONNECT_TIMEOUT,
                   static_cast<int>(connectTimeout));

  // Set SSH timeout (for libssh2 operations and data transfer)
  git_libgit2_opts(GIT_OPT_SET_SERVER_TIMEOUT, static_cast<int>(serverTimeout));

  LOG_PERF(0, "Exit");
}

void shutdownForCurrentThread() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Shutting down libgit2 for current thread");

  GitRepo::Lfs::shutdown();

  const int numberofRemaining = git_libgit2_shutdown();

  LOG_DEBUG("Number of remaining initializations is {}", numberofRemaining);
  LOG_DEBUG("libgit2 shutdown complete, remaining init count: {}",
            numberofRemaining);

  LOG_PERF(0, "Exit");
}

Lfs::StorageStatus getLfsStorageStatus(git_repository *repo) {
  return Lfs::getStorageStatus(repo);
}

static QString repositoryRootPath(git_repository *repo);
static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &repoPath, const QString &url,
                   const QString &username, const int allowedTypes);
static QString extractSshHost(const QString &url);
static bool promptForSshPassphrase(const QString &repoPath, const QString &url,
                                   const QString &username,
                                   QString &passphrase);

QString getBasicAuthHeader(const QString &repoPath, const QString &url) {
  LOG_PERF(0, "Enter");

  if (repoPath.isEmpty() || url.isEmpty()) {
    LOG_PERF(0, "Exit");
    return {};
  }

  GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();
  QString username;
  QString password;
  if (!credStorage.getCredentials(repoPath, username, password)) {
    const auto [success, credentials] = acquireCredentials(
        repoPath, url, QString(), GIT_CREDENTIAL_USERPASS_PLAINTEXT);
    if (!success) {
      LOG_PERF(0, "Exit");
      return {};
    }

    username = credentials.username;
    password = credentials.password;
  }

  const QByteArray auth =
      QString("%1:%2").arg(username, password).toUtf8().toBase64();
  LOG_PERF(0, "Exit");
  return QString::fromLatin1("Basic ") + QString::fromLatin1(auth);
}

QString getBasicAuthHeader(git_repository *repo, const QString &url) {
  if (!repo) {
    return {};
  }

  return getBasicAuthHeader(repositoryRootPath(repo), url);
}

static QString repoWorkdirPath(git_repository *repo) {
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

static QString normalizeRepoRoot(QString path) {
  if (path.endsWith("/.git/") || path.endsWith("\\.git\\")) {
    path.chop(6);
  } else if (path.endsWith("/.git") || path.endsWith("\\.git")) {
    path.chop(5);
  }

  return QDir::cleanPath(path);
}

static QString repositoryRootPath(git_repository *repo) {
  if (!repo) {
    return {};
  }

  if (git_repository_is_bare(repo)) {
    const char *path = git_repository_path(repo);
    return path ? QDir::cleanPath(QString::fromUtf8(path)) : QString();
  }

  return normalizeRepoRoot(repoWorkdirPath(repo));
}

static QString extractSshHost(const QString &url) {
  const QRegularExpression scpLike(
      QStringLiteral(R"(^(([^@]+)@)?([^:]+):/?(.+)$)"));
  const QRegularExpressionMatch scpMatch = scpLike.match(url);
  if (scpMatch.hasMatch()) {
    return scpMatch.captured(3);
  }

  const QUrl parsed(url);
  if (parsed.isValid() &&
      parsed.scheme().compare("ssh", Qt::CaseInsensitive) == 0) {
    return parsed.host();
  }

  return {};
}

static bool promptForSshPassphrase(const QString &repoPath, const QString &url,
                                   const QString &username,
                                   QString &passphrase) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();
  const int token = eventManager.startWait();

  GitCredentialsRequest request;
  request.id = qHash(url);
  request.url = url;
  request.username = username;
  request.allowedTypes = GIT_CREDENTIAL_SSH_KEY;
  request.repoPath = repoPath;
  request.needsPassphrase = true;

  EventManager::Event credentialsEvent(
      EventManager::EventType::GitRepoManagerGetCredentials);
  credentialsEvent.addData(request);
  eventManager.send(credentialsEvent);

  const auto predicate = [&](EventManager::Event event) -> bool {
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
      const auto [id] = event.data<GitCredentialsErrorResponse>();
      return id == request.id;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();
      return response.id == request.id;
    }
    default:
      return false;
    }
  };

  const std::set<EventManager::EventType> types{
      EventManager::EventType::GitRepoManagerGetCredentialsCancel,
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit};

  const auto &[success, event] =
      eventManager.waitOneOf(token, types, predicate);
  if (!success ||
      event.type() ==
          EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
    return false;
  }

  const GitCredentialsResponse response = event.data<GitCredentialsResponse>();
  passphrase =
      !response.passphrase.isEmpty() ? response.passphrase : response.password;
  return true;
}

GitSshAuthInfo getSshAuthInfo(const QString &repoPath, const QString &url,
                              const QString &usernameFromUrl) {
  LOG_PERF(0, "Enter");

  GitSshAuthInfo authInfo;
  authInfo.username =
      usernameFromUrl.isEmpty() ? QStringLiteral("git") : usernameFromUrl;

  const QString host = extractSshHost(url);
  if (host.isEmpty()) {
    LOG_PERF(0, "Exit");
    return authInfo;
  }

  GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
  GitKeyPair keyPair;
  if (!keyStorage.getKeyPairForHost(host, keyPair)) {
    const QString homePath =
        QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    keyPair.privateKeyPath = QString("%1/.ssh/id_ed25519").arg(homePath);
    keyPair.publicKeyPath = QString("%1/.ssh/id_ed25519.pub").arg(homePath);
    keyPair.keyName = QStringLiteral("id_ed25519");
  }

  authInfo.privateKeyPath = keyPair.privateKeyPath;
  authInfo.publicKeyPath = keyPair.publicKeyPath;
  authInfo.passphrasePresent = false;
  authInfo.valid = QFile::exists(authInfo.privateKeyPath) &&
                   QFile::exists(authInfo.publicKeyPath);

  if (!authInfo.valid) {
    LOG_PERF(0, "Exit");
    return authInfo;
  }

  QFile keyFile(authInfo.privateKeyPath);
  if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QString keyContent = QString::fromUtf8(keyFile.readAll());
    keyFile.close();
    if (keyContent.contains("ENCRYPTED", Qt::CaseInsensitive)) {
      authInfo.passphrasePresent = true;
      if (!promptForSshPassphrase(repoPath, url, authInfo.username,
                                  authInfo.passphrase)) {
        authInfo.valid = false;
        LOG_PERF(0, "Exit");
        return authInfo;
      }
    }
  }

  authInfo.valid = true;
  LOG_PERF(0, "Exit");
  return authInfo;
}

void clearCachedCredentials(const QString &repoPath) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Clearing cached credentials for repository: {}", repoPath);

  LOCKGUARD(gGitCredentialsMutex);

  // Clear all cached credentials to ensure fresh ones are loaded from storage
  // This is necessary when credentials are updated by the user
  gGitCredentials.clear();

  LOG_INFO("Cleared all cached credentials");
  LOG_PERF(0, "Exit");
}

static bool file_has_conflict_markers_for_apply_stash(const QString &path) {
  return file_has_conflict_marker_triplet(path, "<<<<<<< Updated upstream",
                                          "=======", ">>>>>>> Stashed changes");
}

bool applyStash(git_repository *repo, int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Applying stash at index: {}", index);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_stash_apply_options options;
  git_stash_apply_options_init(&options, GIT_STASH_APPLY_OPTIONS_VERSION);

  // Optional: customize behavior
  options.flags = GIT_STASH_APPLY_REINSTATE_INDEX; // restores index state
  options.checkout_options.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;

  int error = 0;

  error = git_stash_apply(repo, index, &options);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to apply stash at index {}: error={}",
              index, error);
    setError(GitError::ApplyStashFailedToApplyStash, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Detect conflicts by diffing index vs workdir
  git_diff *workdir_diff = NULL;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

  error = git_diff_index_to_workdir(&workdir_diff, repo, NULL, &diff_opts);
  if (error) {
    setError(GitError::ApplyStashFailedToDiffIndexAgainstWorkdir, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const size_t num_deltas = git_diff_num_deltas(workdir_diff);

  const char *repo_workdir = git_repository_workdir(repo);

  QString workDir;
  if (repo_workdir) {
    workDir = repo_workdir;
  }

  QStringList conflictedFiles;

  for (size_t i = 0; i < num_deltas; i++) {
    const git_diff_delta *delta = git_diff_get_delta(workdir_diff, i);

    if (!delta->new_file.path) {
      LOG_WARN("New file path is not defined for delta {}", i);
      continue;
    }

    QString filePath = QDir(workDir).absoluteFilePath(delta->new_file.path);
    filePath = QDir::toNativeSeparators(filePath);

    if (file_has_conflict_markers_for_apply_stash(filePath)) {
      conflictedFiles += filePath;
    }
  }

  markConflictedFiles(repo, conflictedFiles);

  git_diff_free(workdir_diff);

  LOG_DEBUG("Stash at index {} applied successfully", index);
  LOG_PERF(0, "Exit");
  return true;
}

bool hasConflicts(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *index_ptr = nullptr;
  int error = git_repository_index(&index_ptr, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get repository index when checking for conflicts: "
              "error={}",
              error);
    setError(GitError::HasConflictsFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool hasConflicts_result = git_index_has_conflicts(index_ptr) != 0;
  git_index_free(index_ptr);

  LOG_DEBUG("Repository has conflicts: {}", hasConflicts_result);
  LOG_PERF(0, "Exit");
  return hasConflicts_result;
}

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    const BranchType type) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to checkout branch/tag: '{}', type: {}",
            branchOrTagName.toStdString(), static_cast<int>(type));

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when checking out branch '{}'",
              branchOrTagName.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  if (type == BranchType::Local) {
    LOG_DEBUG("Checking out local branch: '{}'", branchOrTagName.toStdString());
    git_tree *tree = nullptr;
    git_reference *localRef = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    int error = git_branch_lookup(&localRef, repo,
                                  branchOrTagName.toStdString().c_str(),
                                  GIT_BRANCH_LOCAL);
    if (error) {
      setError(GitError::CheckoutBranchFailedToLookupBranch, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Resolve the reference to a commit
    git_commit *targetCommit = nullptr;
    error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                               localRef, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::CheckoutBranchFailedToPeelLocalBranchToCommit, error);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 3. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      setError(GitError::CheckoutBranchFailedToGetCommitTree, error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 4. Set checkout strategy
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 5. Checkout the tree
    error =
        git_checkout_tree(repo, reinterpret_cast<git_object *>(tree), &opts);
    if (error) {
      setError(GitError::CheckoutBranchFailedToCheckoutLocalBranchTree, error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      setError(GitError::CheckoutBranchFailedToGetReferenceName, error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_commit_free(targetCommit);
    git_reference_free(localRef);
    git_tree_free(tree);
  } else if (type == BranchType::Remote) {
    git_tree *tree = nullptr;
    git_reference *localRef = nullptr;
    git_reference *remoteRef = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    QString localName;
    if (const auto strList = branchOrTagName.split('/'); strList.length() > 0) {
      localName = strList.last();
    } else {
      localName = branchOrTagName;
    }

    const QString fullBranchName =
        QString("refs/remotes/%1").arg(branchOrTagName);

    // 1. Lookup the branch reference
    int error = git_reference_lookup(&remoteRef, repo,
                                     fullBranchName.toStdString().c_str());
    if (error) {
      setError(GitError::CheckoutBranchFailedToLookupReference, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Resolve the reference to a commit
    git_commit *targetCommit = nullptr;
    error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                               remoteRef, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::CheckoutBranchFailedToPeelRemoteBranchToCommit, error);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 3. Create local branch that tracks the remote branch
    error = git_branch_create(&localRef, repo, localName.toStdString().c_str(),
                              targetCommit, 0);
    if (error) {
      if (error == GIT_EEXISTS) {
        // 3.1 Branch already exists, try to lookup it
        error = git_branch_lookup(
            &localRef, repo, localName.toStdString().c_str(), GIT_BRANCH_LOCAL);
        if (error) {
          setError(GitError::CheckoutBranchFailedToLookupDestinationBranch,
                   error);
          git_commit_free(targetCommit);
          git_reference_free(remoteRef);
          LOG_PERF(0, "Exit");
          return false;
        }
      } else {
        setError(GitError::CheckoutBranchFailedToCreateLocalBranchFromRemote,
                 error);
        git_commit_free(targetCommit);
        git_reference_free(remoteRef);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    // 4. Set up tracking
    error = git_branch_set_upstream(localRef,
                                    branchOrTagName.toStdString().c_str());
    if (error) {
      setError(GitError::CheckoutBranchFailedToSetBranchUpstream, error);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 5. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      setError(GitError::CheckoutBranchFailedToGetSecondCommitTree, error);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Set checkout strategy
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 7. Checkout the tree
    error =
        git_checkout_tree(repo, reinterpret_cast<git_object *>(tree), &opts);
    if (error) {
      setError(GitError::CheckoutBranchFailedToCheckoutTagTree, error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      setError(GitError::CheckoutBranchFailedToSetHeadToTagReference, error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_tree_free(tree);
    git_reference_free(localRef);
    git_commit_free(targetCommit);
    git_reference_free(remoteRef);
  } else if (type == BranchType::Tag) {
    git_object *tagTarget = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    // 1. Resolve the tag to its target commit/tree
    int error = git_revparse_single(&tagTarget, repo,
                                    branchOrTagName.toStdString().c_str());
    if (error) {
      setError(GitError::CheckoutBranchFailedToResolveRevision, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Set checkout options
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 3. Checkout the tree
    error = git_checkout_tree(repo, tagTarget, &opts);
    if (error) {
      setError(GitError::CheckoutBranchFailedToCheckoutTree, error);
      git_object_free(tagTarget);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 4. Detach HEAD to the tag's commit
    error = git_repository_set_head_detached(repo, git_object_id(tagTarget));
    if (error) {
      setError(GitError::CheckoutBranchFailedToSetHeadDetachedAtTag, error);
      git_object_free(tagTarget);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(tagTarget);
  } else {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Unsupported branch type {}",
              static_cast<int>(type));
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

void closeRepository(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return;
  }

  git_repository_free(repo);

  LOG_PERF(0, "Exit");
}

git_repository *openRepository(const QString &path, const QString &author,
                               const QString &email) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Opening repository at path: '{}'", path.toStdString());

  git_repository *repo = nullptr;

  if (const int error =
          git_repository_open(&repo, path.toStdString().c_str())) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open repository at '{}': error={}",
              path.toStdString(), error);
    setError(GitError::OpenRepositoryFailedToOpenRepository, error);
    return nullptr;
  }

  LOG_DEBUG("Repository opened successfully at '{}'", path.toStdString());

  // Check if repository has no commits (HEAD is unborn)
  if (repo && git_repository_head_unborn(repo) == 1) {
    LOG_DEBUG("Repository has unborn HEAD, creating initial commit");
    // Create empty initial commit without staging existing files
    createInitialCommitWithoutStaging(repo, author, email);
    LOG_DEBUG("Initial commit created for new repository");
  }

  LOG_PERF(0, "Exit");
  return repo;
}

static QStringList listCheckoutBranches(git_repository *repo,
                                        const BranchType type,
                                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  bool tagRequested = false;

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
  case BranchType::Local: {
    branchType = GIT_BRANCH_LOCAL;
    break;
  }
  case BranchType::Remote: {
    branchType = GIT_BRANCH_REMOTE;
    break;
  }
  case BranchType::Tag: {
    tagRequested = true;
    break;
  }
  case BranchType::All:
  default: {
    break;
  }
  }

  if (!tagRequested && branchType == GIT_BRANCH_ALL) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList activeBranches;

  if (!tagRequested) {
    git_branch_iterator *it = nullptr;

    int error = git_branch_iterator_new(&it, repo, branchType);
    if (error) {
      setError(GitError::ListCheckoutBranchesFailedToCreateBranchIterator,
               error);
      return {};
    }

    if (!it) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
      return {};
    }

    git_reference *ref = nullptr;

    while (!quitFlag) {
      error = git_branch_next(&ref, &branchType, it);
      if (error == GIT_ITEROVER)
        break;

      if (error) {
        setError(GitError::ListCheckoutBranchesFailedToReadNextBranch, error);
        break;
      }

      if (!ref) {
        LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
        continue;
      }

      const char *branchName = nullptr;

      error = git_branch_name(&branchName, ref);
      if (error == GIT_EINVALID) {
        git_reference_free(ref);
        LOG_ERROR(Log::ErrorId::Git,
                  "Reference is not a local or a remote branch.");
        continue;
      }

      if (error) {
        git_reference_free(ref);
        setError(GitError::ListCheckoutBranchesFailedToReadBranchName, error);
        break;
      }

      if (!branchName) {
        git_reference_free(ref);
        LOG_ERROR(Log::ErrorId::Git, "Invalid branch name.");
        continue;
      }

      error = git_branch_is_checked_out(ref);
      if (error == 1) {
        activeBranches += branchName;
      } else if (error != 0) {
        git_reference_free(ref);
        setError(
            GitError::ListCheckoutBranchesFailedToCheckIfBranchIsCheckedOut,
            error);
        break;
      }

      git_reference_free(ref);
    };

    git_branch_iterator_free(it);
  }

  // Include HEAD if detached for local branches or if tag requested
  if ((type == BranchType::Local && activeBranches.isEmpty()) || tagRequested) {
    git_reference *headRef = nullptr;

    int error = git_repository_head(&headRef, repo);
    if (error) {
      setError(GitError::ListCheckoutBranchesFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (git_reference_type(headRef) == GIT_REFERENCE_DIRECT) {
      // Detached HEAD — could be tag or raw commit
      if (tagRequested) {
        git_strarray tags = {nullptr};

        error = git_tag_list(&tags, repo);
        if (error) {
          setError(GitError::ListCheckoutBranchesFailedToListTags, error);
          git_reference_free(headRef);
          LOG_PERF(0, "Exit");
          return {};
        }

        for (size_t i = 0; i < tags.count; ++i) {
          git_object *tagObj = nullptr;

          error = git_revparse_single(&tagObj, repo, tags.strings[i]);
          if (error) {
            setError(GitError::ListCheckoutBranchesFailedToResolveRevision,
                     error);
            continue;
          }

          if (git_oid_equal(git_reference_target(headRef),
                            git_object_id(tagObj))) {
            // HEAD is detached at tag
            activeBranches += tags.strings[i];
            git_object_free(tagObj);
            break;
          }

          git_object_free(tagObj);
        }

        git_strarray_dispose(&tags);
      } else {
        // Raw commit
        if (const QString referenceName = git_reference_name(headRef);
            referenceName == HEAD) {
          activeBranches += HEAD;
        }
      }
    }

    git_reference_free(headRef);
  }

  LOG_PERF(0, "Exit");

  return activeBranches;
}

static QStringList listBranches(git_repository *repo, const BranchType type,
                                const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
  case BranchType::Local: {
    branchType = GIT_BRANCH_LOCAL;
    break;
  }
  case BranchType::Remote: {
    branchType = GIT_BRANCH_REMOTE;
    break;
  }
  case BranchType::All:
  default: {
    break;
  }
  }

  if (branchType == GIT_BRANCH_ALL) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList activeBranches;

  if (branchType == GIT_BRANCH_LOCAL) {
    git_reference *headRef = nullptr;

    if (const int error = git_repository_head(&headRef, repo)) {
      setError(GitError::ListBranchesFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (git_reference_type(headRef) == GIT_REFERENCE_DIRECT) {
      // Detached HEAD — could be tag or raw commit
      if (const QString referenceName = git_reference_name(headRef);
          referenceName == HEAD) {
        activeBranches += HEAD;
      }
    }

    git_reference_free(headRef);
  }

  git_branch_iterator *it = nullptr;

  int error = git_branch_iterator_new(&it, repo, branchType);
  if (error) {
    setError(GitError::ListBranchesFailedToCreateBranchIterator, error);
    return {};
  }

  if (!it) {
    LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
    return {};
  }

  git_reference *ref = nullptr;

  while (!quitFlag) {
    error = git_branch_next(&ref, &branchType, it);
    if (error == GIT_ITEROVER)
      break;

    if (error) {
      setError(GitError::ListBranchesFailedToReadNextBranch, error);
      break;
    }

    if (!ref) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
      continue;
    }

    const char *branchName = nullptr;

    error = git_branch_name(&branchName, ref);
    if (error == GIT_EINVALID) {
      git_reference_free(ref);
      LOG_ERROR(Log::ErrorId::Git,
                "Reference is not a local or a remote branch.");
      continue;
    }

    if (error) {
      git_reference_free(ref);
      setError(GitError::ListBranchesFailedToReadBranchName, error);
      break;
    }

    if (!branchName) {
      git_reference_free(ref);
      LOG_ERROR(Log::ErrorId::Git, "Invalid branch name.");
      continue;
    }

    activeBranches += branchName;

    git_reference_free(ref);
  };

  git_branch_iterator_free(it);

  LOG_PERF(0, "Exit");

  return activeBranches;
}

QStringList listLocalBranches(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Local, quitFlag);
}

QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Remote, quitFlag);
}

QVector<BranchInfo> listRemoteBranchesWithStatus(git_repository *repo,
                                                 const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QVector<BranchInfo> branchesWithStatus;

  // First get all remote branches
  const QStringList remoteBranches = listRemoteBranches(repo, quitFlag);

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // For each remote branch, calculate its status
  for (const QString &remoteBranchName : remoteBranches) {
    if (quitFlag) {
      break;
    }

    BranchInfo branchInfo;
    branchInfo.name = remoteBranchName;

    // Get the remote reference
    const QString fullRemoteRefName =
        QString("refs/remotes/%1").arg(remoteBranchName);
    git_reference *remoteRef = nullptr;
    int error = git_reference_lookup(&remoteRef, repo,
                                     fullRemoteRefName.toStdString().c_str());
    if (error) {
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      git_reference_free(remoteRef);
      branchesWithStatus.append(branchInfo);
      continue;
    }

    const git_oid *remoteRefTargetOid = git_reference_target(remoteRef);
    if (!remoteRefTargetOid) {
      // Remote reference does not point to a valid OID
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      git_reference_free(remoteRef);
      branchesWithStatus.append(branchInfo);
      continue;
    }

    // Get the remote OID
    git_oid remoteOid;
    git_oid_cpy(&remoteOid, remoteRefTargetOid);
    git_reference_free(remoteRef);

    // Extract the local branch name from the remote branch name
    // Remote branches have format: "origin/branchname"
    const auto parts = remoteBranchName.split('/');
    QString localBranchName;
    if (parts.length() > 1) {
      localBranchName = parts.last();
    } else {
      localBranchName = remoteBranchName;
    }

    // Try to get the local branch that tracks this remote branch
    git_reference *localRef = nullptr;
    error = git_branch_lookup(&localRef, repo,
                              localBranchName.toStdString().c_str(),
                              GIT_BRANCH_LOCAL);

    if (error) {
      // No local tracking branch exists
      branchInfo.statusInfo.status = BranchStatusType::NoUpstream;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      continue;
    }

    const git_oid *localRefTargetOid = git_reference_target(localRef);
    if (!localRefTargetOid) {
      // Local reference does not point to a valid OID
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      git_reference_free(localRef);
      continue;
    }

    // Get the local OID
    git_oid localOid;
    git_oid_cpy(&localOid, localRefTargetOid);
    git_reference_free(localRef);

    // Calculate ahead/behind
    size_t ahead = 0, behind = 0;
    error =
        git_graph_ahead_behind(&ahead, &behind, repo, &localOid, &remoteOid);

    if (error) {
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      setError(
          GitError::ListRemoteBranchesWithStatusFailedToCalculateAheadBehind,
          error);
      continue;
    }

    // Determine the status type
    if (ahead == 0 && behind == 0) {
      branchInfo.statusInfo.status = BranchStatusType::UpToDate;
    } else if (ahead > 0 && behind == 0) {
      branchInfo.statusInfo.status = BranchStatusType::Ahead;
    } else if (ahead == 0 && behind > 0) {
      branchInfo.statusInfo.status = BranchStatusType::Behind;
    } else {
      branchInfo.statusInfo.status = BranchStatusType::Diverged;
    }

    branchInfo.statusInfo.aheadCount = ahead;
    branchInfo.statusInfo.behindCount = behind;

    branchesWithStatus.append(branchInfo);
  }

  LOG_PERF(0, "Exit");

  return branchesWithStatus;
}

QStringList listStashes(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QStringList stashes;

  const auto callback = [](size_t index, const char *message,
                           const git_oid *stash_id, void *payload) {
    if (!payload) {
      return -1;
    }

    const auto payloadStashes = static_cast<QStringList *>(payload);
    if (message) {
      *payloadStashes += QString::fromUtf8(message);
    } else {
      *payloadStashes += QString("stash@{%1}").arg(index);
    }

    return 0;
  };

  if (const int error = git_stash_foreach(repo, callback, &stashes)) {
    setError(GitError::ListStashesFailedToListStashes, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_PERF(0, "Exit");

  return stashes;
}

GitCommitStatus getStashStatus(git_repository *repo, int stashIndex) {
  LOG_PERF(0, "Enter");

  GitCommitStatus status;

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get stash commit object
  git_object *stash_obj = nullptr;
  git_commit *stash_commit = nullptr;
  git_tree *stash_tree = nullptr;
  git_tree *parent_tree = nullptr;
  git_commit *parent_commit = nullptr;
  git_tree *parent1_tree = nullptr;
  git_tree *parent2_tree = nullptr;
  git_commit *parent1_commit = nullptr;
  git_commit *parent2_commit = nullptr;
  git_diff *diff = nullptr;

  // Use git_revparse_single to get the stash commit
  QString stashRef = QString("stash@{%1}").arg(stashIndex);
  int error =
      git_revparse_single(&stash_obj, repo, stashRef.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get stash ref at index {}: error={}", stashIndex,
              error);
    setError(GitError::GetStashStatusFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Convert to commit
  git_object *peeled_obj = nullptr;
  error = git_object_peel(&peeled_obj, stash_obj, GIT_OBJ_COMMIT);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to peel stash object to commit at index {}: error={}",
              stashIndex, error);
    setError(GitError::GetStashStatusFailedToPeelObjectToCommit, error);
    git_object_free(stash_obj);
    LOG_PERF(0, "Exit");
    return {};
  }

  stash_commit = (git_commit *)peeled_obj;
  git_object_free(stash_obj);

  // Get stash tree
  error = git_commit_tree(&stash_tree, stash_commit);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get stash tree: error={}", error);
    setError(GitError::GetStashStatusFailedToGetCommitTree, error);
    git_commit_free(stash_commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Check if stash tree is empty (common for stash commits which are merge
  // commits). If empty, try to use parent 1 (index state) for tracked changes.
  // Parent 2 contains untracked files and is handled separately below.
  git_tree *tracked_working_tree = stash_tree;

  if (git_tree_entrycount(stash_tree) == 0) {
    LOG_DEBUG("Stash tree is empty, trying parent 1 (index state)");

    // Try to get parent 1 commit (the index state in the stash)
    error = git_commit_parent(&parent1_commit, stash_commit, 1);
    if (error == 0) {
      error = git_commit_tree(&parent1_tree, parent1_commit);
      if (error == 0 && git_tree_entrycount(parent1_tree) > 0) {
        LOG_DEBUG("Using parent 1 tree (index state) with {} entries",
                  git_tree_entrycount(parent1_tree));
        tracked_working_tree = parent1_tree;
      } else if (error == 0) {
        LOG_DEBUG("Parent 1 tree is also empty");
        git_tree_free(parent1_tree);
        parent1_tree = nullptr;
        git_commit_free(parent1_commit);
        parent1_commit = nullptr;
        tracked_working_tree = nullptr;
      } else {
        tracked_working_tree = nullptr;
      }
    } else {
      tracked_working_tree = nullptr;
    }
  }

  // Get parent commit (the commit the stash was based on)
  error = git_commit_parent(&parent_commit, stash_commit, 0);
  if (error && error != GIT_ENOTFOUND) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get stash parent: error={}", error);
    setError(GitError::GetStashStatusFailedToGetBaseParentCommit, error);
    git_tree_free(stash_tree);
    if (parent1_tree) {
      git_tree_free(parent1_tree);
    }
    if (parent2_tree) {
      git_tree_free(parent2_tree);
    }
    if (parent1_commit) {
      git_commit_free(parent1_commit);
    }
    if (parent2_commit) {
      git_commit_free(parent2_commit);
    }
    git_commit_free(stash_commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (error == GIT_ENOTFOUND) {
    parent_tree = nullptr; // Initial stash
  } else {
    error = git_commit_tree(&parent_tree, parent_commit);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to get parent tree: error={}",
                error);
      setError(GitError::GetStashStatusFailedToGetSecondCommitTree, error);
      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      git_commit_free(stash_commit);
      git_commit_free(parent_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  // Load the untracked-files commit if it exists. It is separate from the
  // tracked-worktree diff and should be shown in the stash viewer as added
  // files.
  error = git_commit_parent(&parent2_commit, stash_commit, 2);
  if (!error) {
    error = git_commit_tree(&parent2_tree, parent2_commit);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to get untracked stash tree: error={}", error);
      setError(GitError::GetStashStatusFailedToGetThirdCommitTree, error);
      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      if (parent_tree) {
        git_tree_free(parent_tree);
      }
      git_commit_free(stash_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      if (parent_commit) {
        git_commit_free(parent_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }
  } else if (error != GIT_ENOTFOUND) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get untracked stash parent: error={}", error);
    setError(GitError::GetStashStatusFailedToGetFourthCommitTree, error);
    git_tree_free(stash_tree);
    if (parent1_tree) {
      git_tree_free(parent1_tree);
    }
    if (parent_tree) {
      git_tree_free(parent_tree);
    }
    git_commit_free(stash_commit);
    if (parent1_commit) {
      git_commit_free(parent1_commit);
    }
    if (parent_commit) {
      git_commit_free(parent_commit);
    }
    LOG_PERF(0, "Exit");
    return {};
  }

  // Generate diff for tracked changes.
  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.flags |= GIT_DIFF_INCLUDE_TYPECHANGE | GIT_DIFF_RECURSE_UNTRACKED_DIRS |
                GIT_DIFF_INCLUDE_UNTRACKED | GIT_DIFF_IGNORE_CASE |
                GIT_DIFF_INCLUDE_CASECHANGE | GIT_DIFF_SHOW_UNTRACKED_CONTENT;

  if (tracked_working_tree) {
    error = git_diff_tree_to_tree(&diff, repo, parent_tree,
                                  tracked_working_tree, &opts);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to create diff for stash: error={}",
                error);
      setError(GitError::GetStashStatusFailedToCreateStashDiff, error);

      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      if (parent_tree) {
        git_tree_free(parent_tree);
      }
      git_commit_free(stash_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      if (parent_commit) {
        git_commit_free(parent_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }

    collectCommitStatusFromDiff(diff, &status);
    git_diff_free(diff);
    diff = nullptr;
  }

  // Untracked files are stored in parent 2. Walk that tree directly so the
  // stash viewer shows them as files in the stash content list.
  if (parent2_tree) {
    QStringList untrackedFiles;
    CollectTreePathsPayload treePayload{&status, &untrackedFiles};
    error = git_tree_walk(parent2_tree, GIT_TREEWALK_PRE,
                          collectTreePathsCallback, &treePayload);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to walk stash untracked tree: error={}", error);
      setError(GitError::GetStashStatusFailedToWalkUntrackedStashTree, error);

      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      if (parent_tree) {
        git_tree_free(parent_tree);
      }
      git_commit_free(stash_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      if (parent_commit) {
        git_commit_free(parent_commit);
      }
      if (diff) {
        git_diff_free(diff);
      }
      LOG_PERF(0, "Exit");
      return {};
    }

    status.untrackedFiles += untrackedFiles;
  }

  // Get stash commit OID
  const git_oid *oid = git_commit_id(stash_commit);
  char oid_str[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oid_str, GIT_OID_HEXSZ + 1, oid);
  status.commit = QString::fromUtf8(oid_str);

  if (const char *summary = git_commit_summary(stash_commit)) {
    status.summary = QString::fromUtf8(summary);
  } else if (const char *message = git_commit_message(stash_commit)) {
    status.summary = QString::fromUtf8(message).section('\n', 0, 0);
  }

  const int parentCount = git_commit_parentcount(stash_commit);
  status.parentShas.reserve(parentCount);
  for (int i = 0; i < parentCount; ++i) {
    const git_oid *parentOid = git_commit_parent_id(stash_commit, i);
    if (!parentOid) {
      continue;
    }

    char parentOidStr[GIT_OID_HEXSZ + 1] = {0};
    git_oid_tostr(parentOidStr, GIT_OID_HEXSZ + 1, parentOid);
    status.parentShas += QString::fromUtf8(parentOidStr);
  }

  if (const git_signature *author = git_commit_author(stash_commit)) {
    if (author->name) {
      status.authorName = QString::fromUtf8(author->name);
    }
    if (author->email) {
      status.authorEmail = QString::fromUtf8(author->email);
    }
  }

  status.committerDate =
      commitSignatureDateToString(git_commit_committer(stash_commit));

  // Cleanup
  git_tree_free(stash_tree);

  if (parent1_tree) {
    git_tree_free(parent1_tree);
  }
  if (parent2_tree) {
    git_tree_free(parent2_tree);
  }
  if (parent_tree) {
    git_tree_free(parent_tree);
  }

  if (parent1_commit) {
    git_commit_free(parent1_commit);
  }
  if (parent2_commit) {
    git_commit_free(parent2_commit);
  }
  if (parent_commit) {
    git_commit_free(parent_commit);
  }

  git_commit_free(stash_commit);

  LOG_DEBUG(
      "Stash status retrieved for index {}: {} new, {} modified, {} deleted",
      stashIndex, status.newFiles.size(), status.modifiedFiles.size(),
      status.deletedFiles.size());
  LOG_PERF(0, "Exit");
  return status;
}

QStringList listTags(git_repository *repo) {
  LOG_PERF(0, "Enter");

  git_strarray tagNames = {0};
  const int error = git_tag_list(&tagNames, repo);
  if (error) {
    setError(GitError::ListTagsFailedToListTags, error);
    return {};
  }

  QStringList tags;

  for (int i = 0; i < tagNames.count; ++i)
    tags += tagNames.strings[i];

  git_strarray_dispose(&tagNames);

  LOG_PERF(0, "Exit");

  return tags;
}

struct ListSubmodulesPayload {
  const char *repoPath = nullptr;
  QStringList submodules;
};

static int listSubmodulesCallback(git_submodule *sm, const char *name,
                                  void *payload) {
  if (!payload) {
    return 0;
  }

  auto *listPayload = static_cast<ListSubmodulesPayload *>(payload);
  Q_ASSERT(listPayload->repoPath);
  if (!listPayload->repoPath) {
    return 0;
  }

  const char *path = git_submodule_path(sm);
  if (!path) {
    return 0;
  }

  const QString fullPath =
      QString("%1/../%2").arg(listPayload->repoPath).arg(path);

  if (const QDir dir(fullPath); !dir.exists()) {
    return 0;
  }

  listPayload->submodules += name;

  return 0;
}

QStringList listSubmodules(git_repository *repo) {
  LOG_PERF(0, "Enter");

  ListSubmodulesPayload payload;
  payload.repoPath = git_repository_path(repo);

  const int error =
      git_submodule_foreach(repo, &listSubmodulesCallback, &payload);
  if (error) {
    setError(GitError::ListSubmodulesFailedToIterateSubmodules, error);
    return {};
  }

  LOG_PERF(0, "Exit");

  return payload.submodules;
}

QVector<SubmoduleInfo> listSubmodulesWithStatus(git_repository *repo,
                                                const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QVector<SubmoduleInfo> submodulesWithStatus;

  // Get all submodules first
  const QStringList submodules = listSubmodules(repo);

  for (const QString &submoduleName : submodules) {
    if (quitFlag) {
      break;
    }

    SubmoduleInfo info;
    info.name = submoduleName;
    info.statusInfo.status = SubmoduleStatusType::Unknown;

    // Try to get submodule status
    git_submodule *submodule = nullptr;
    if (git_submodule_lookup(&submodule, repo,
                             submoduleName.toStdString().c_str()) != 0) {
      info.statusInfo.status = SubmoduleStatusType::Error;
      submodulesWithStatus.push_back(info);
      continue;
    }

    // Get the expected commit (from parent repository's index)
    const git_oid *headId = git_submodule_head_id(submodule);

    // Get the actual commit (from submodule's working directory)
    const git_oid *workdirId = git_submodule_wd_id(submodule);

    if (workdirId) {
      const char *workdirIdStr = git_oid_tostr_s(workdirId);
      if (workdirIdStr) {
        info.commit = QString::fromUtf8(workdirIdStr);
      }
    }

    if (!headId || !workdirId) {
      info.statusInfo.status = SubmoduleStatusType::Error;
      git_submodule_free(submodule);
      submodulesWithStatus.push_back(info);
      continue;
    }

    if (git_oid_equal(headId, workdirId)) {
      info.statusInfo.status = SubmoduleStatusType::UpToDate;
    } else {
      info.statusInfo.status = SubmoduleStatusType::Outdated;
    }

    git_submodule_free(submodule);
    submodulesWithStatus.push_back(info);
  }

  LOG_PERF(0, "Exit");

  return submodulesWithStatus;
}

static QString gitFlowBranchPrefix(GitFlowBranchType branchType) {
  switch (branchType) {
  case GitFlowBranchType::Feature:
    return QStringLiteral("feature");
  case GitFlowBranchType::Release:
    return QStringLiteral("release");
  case GitFlowBranchType::Hotfix:
    return QStringLiteral("hotfix");
  case GitFlowBranchType::Unknown:
  default:
    return QString();
  }
}

static QString gitFlowBranchName(GitFlowBranchType branchType,
                                 const QString &branchName) {
  const QString trimmedBranchName = branchName.trimmed();
  if (trimmedBranchName.isEmpty()) {
    return {};
  }

  const QString prefix = gitFlowBranchPrefix(branchType);
  if (prefix.isEmpty() ||
      trimmedBranchName.startsWith(prefix + QStringLiteral("/"))) {
    return trimmedBranchName;
  }

  return prefix + QStringLiteral("/") + trimmedBranchName;
}

static QString gitFlowConfiguredBranchName(ConfigManager::ConfigKey key) {
  return ConfigManager::ConfigManager::getInstance().getString(key).trimmed();
}

static QString
defaultGitFlowBaseBranchName(git_repository *repo, GitFlowBranchType branchType,
                             const QString &developmentBranchName) {
  const auto branchExists = [repo](const QString &branchName) {
    git_reference *ref = nullptr;
    const int error = git_branch_lookup(
        &ref, repo, branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
    if (!error) {
      git_reference_free(ref);
      return true;
    }
    return false;
  };

  const QString configuredDevelopmentBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName);
  const QString configuredMainBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName);
  const QString configuredMasterBranchName = QStringLiteral("master");
  const QString normalizedDevelopmentBranchName =
      developmentBranchName.trimmed().isEmpty()
          ? configuredDevelopmentBranchName
          : developmentBranchName.trimmed();

  switch (branchType) {
  case GitFlowBranchType::Feature:
  case GitFlowBranchType::Release:
    if (branchExists(normalizedDevelopmentBranchName)) {
      return normalizedDevelopmentBranchName;
    }
    break;
  case GitFlowBranchType::Hotfix:
    if (branchExists(configuredMainBranchName)) {
      return configuredMainBranchName;
    }
    if (branchExists(configuredMasterBranchName)) {
      return configuredMasterBranchName;
    }
    break;
  case GitFlowBranchType::Unknown:
  default:
    break;
  }

  if (branchExists(normalizedDevelopmentBranchName)) {
    return normalizedDevelopmentBranchName;
  }

  if (branchExists(configuredMainBranchName)) {
    return configuredMainBranchName;
  }

  if (branchExists(configuredMasterBranchName)) {
    return configuredMasterBranchName;
  }

  return getActiveBranch(repo, BranchType::Local, false);
}

static QString
defaultGitFlowPrimaryBranchName(git_repository *repo,
                                const QString &developmentBranchName) {
  const auto branchExists = [repo](const QString &branchName) {
    git_reference *ref = nullptr;
    const int error = git_branch_lookup(
        &ref, repo, branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
    if (!error) {
      git_reference_free(ref);
      return true;
    }
    return false;
  };

  const QString configuredMainBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName);
  const QString configuredMasterBranchName = QStringLiteral("master");
  const QString configuredDevelopmentBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName);
  const QString normalizedDevelopmentBranchName =
      developmentBranchName.trimmed().isEmpty()
          ? configuredDevelopmentBranchName
          : developmentBranchName.trimmed();

  if (branchExists(configuredMainBranchName)) {
    return configuredMainBranchName;
  }

  if (branchExists(configuredMasterBranchName)) {
    return configuredMasterBranchName;
  }

  if (branchExists(normalizedDevelopmentBranchName)) {
    return normalizedDevelopmentBranchName;
  }

  return getActiveBranch(repo, BranchType::Local, false);
}

bool createBranchFromCommit(git_repository *repo, const QString &commit,
                            const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid oid;
  git_commit *commit_ = nullptr;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::CreateBranchFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::CreateBranchFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;
  error = git_branch_create(&ref, repo, branchName.toStdString().c_str(),
                            commit_, 0);
  if (error) {
    setError(GitError::CreateBranchFromCommitFailedToCreateBranch, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);
  git_commit_free(commit_);

  // Set upstream to default repository remote with same branch name
  const QString defaultRemote = getDefaultRemote(repo);
  if (!defaultRemote.isEmpty()) {
    LOG_DEBUG("Setting upstream for new branch '{}' to {}/{}",
              branchName.toStdString(), defaultRemote.toStdString(),
              branchName.toStdString());
    setUpstream(repo, defaultRemote, branchName, branchName);
  } else {
    LOG_DEBUG(
        "No default remote configured, skipping upstream setup for new branch "
        "'{}'",
        branchName.toStdString());
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool startGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                        const QString &branchName,
                        const QString &developmentBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName =
      gitFlowBranchName(branchType, branchName);
  if (normalizedBranchName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "GitFlow branch name is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString baseBranchName =
      defaultGitFlowBaseBranchName(repo, branchType, developmentBranchName);
  if (baseBranchName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Unable to determine the base branch for GitFlow branch '{}'",
              normalizedBranchName.toStdString());
    setError(GitError::CheckoutBranchFailedToLookupBranch, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *baseRef = nullptr;
  int error = git_branch_lookup(
      &baseRef, repo, baseBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::CheckoutBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const git_oid *baseOid = git_reference_target(baseRef);
  if (!baseOid) {
    git_reference_free(baseRef);
    setError(GitError::CheckoutBranchFailedToLookupBranch, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  char oidBuffer[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oidBuffer, sizeof(oidBuffer), baseOid);

  const bool created = createBranchFromCommit(
      repo, QString::fromLatin1(oidBuffer), normalizedBranchName);
  git_reference_free(baseRef);
  if (!created) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool checkedOut =
      checkoutBranch(repo, normalizedBranchName, BranchType::Local);
  if (!checkedOut) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool finishGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                         const QString &branchName, const QString &author,
                         const QString &email,
                         const QString &developmentBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName =
      gitFlowBranchName(branchType, branchName);
  if (normalizedBranchName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "GitFlow branch name is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString primaryBranchName =
      defaultGitFlowPrimaryBranchName(repo, developmentBranchName);
  if (primaryBranchName.isEmpty()) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Unable to determine the destination branch for GitFlow branch '{}'",
        normalizedBranchName.toStdString());
    setError(GitError::MergeFailedToLookupDestinationBranch, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString configuredDevelopmentBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName);
  const QString normalizedDevelopmentBranchName =
      developmentBranchName.trimmed().isEmpty()
          ? configuredDevelopmentBranchName
          : developmentBranchName.trimmed();

  const QString destinationBranchName = branchType == GitFlowBranchType::Feature
                                            ? normalizedDevelopmentBranchName
                                            : primaryBranchName;

  if (!merge(repo, normalizedBranchName, destinationBranchName, author,
             email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (branchType == GitFlowBranchType::Release ||
      branchType == GitFlowBranchType::Hotfix) {
    if (normalizedDevelopmentBranchName != destinationBranchName &&
        !merge(repo, normalizedBranchName, normalizedDevelopmentBranchName,
               author, email)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (!deleteBranch(repo, normalizedBranchName)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool deleteStash(git_repository *repo, int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to delete stash at index: {}", stashIndex);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when deleting stash at index {}",
              stashIndex);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (const int error = git_stash_drop(repo, stashIndex)) {
    setError(GitError::DeleteStashFailedToDropStash, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteAllStashes(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to delete all stashes");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when deleting all stashes");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get the count of stashes first
  size_t stashCount = 0;
  if (const int error = git_stash_foreach(
          repo,
          [](size_t index, const char *name, const git_oid *oid,
             void *payload) -> int {
            (*static_cast<size_t *>(payload))++;
            return 0;
          },
          &stashCount)) {
    setError(GitError::DeleteAllStashesFailedToReadObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Found {} stashes to delete", stashCount);

  // Delete stashes from highest index to lowest to avoid index shifting issues
  for (int i = static_cast<int>(stashCount) - 1; i >= 0; --i) {
    if (const int error = git_stash_drop(repo, i)) {
      LOG_WARN("Failed to delete stash at index {}: error code {}", i, error);
      setError(GitError::DeleteAllStashesFailedToDropStash, error);
      // Continue trying to delete other stashes even if one fails
    } else {
      LOG_DEBUG("Deleted stash at index {}", i);
    }
  }

  LOG_PERF(0, "Exit");
  return true;
}

QString getActiveBranch(git_repository *repo, const BranchType type,
                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  const QStringList checkoutBranches =
      listCheckoutBranches(repo, type, quitFlag);

  LOG_PERF(0, "Exit");

  return !checkoutBranches.empty() ? checkoutBranches.first() : QString();
}

bool initRepository(const QString &repoName, const QString &workingDir,
                    const QString &defaultBranch, const QString &author,
                    const QString &email) {
  LOG_PERF(0, "Enter");

  git_repository_init_options opts{};

  int error = git_repository_init_options_init(
      &opts, GIT_REPOSITORY_INIT_OPTIONS_VERSION);
  if (error) {
    setError(GitError::InitRepositoryFailedToInitializeRepositoryOptions,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  opts.flags = GIT_REPOSITORY_INIT_NO_REINIT | GIT_REPOSITORY_INIT_MKDIR;

  git_repository *repo = nullptr;

  const QString repoPath = workingDir + "/" + repoName;

  error = git_repository_init_ext(&repo, repoPath.toStdString().c_str(), &opts);
  if (error) {
    setError(GitError::InitRepositoryFailedToInitializeRepository, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Internal error. Can't initialize git repository.");
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created = createInitialCommit(repo, defaultBranch, author, email);
  if (!created) {
    closeRepository(repo);
    LOG_ERROR(Log::ErrorId::Git, "Can't create initial commit.");
    LOG_PERF(0, "Exit");
    return false;
  }

  closeRepository(repo);

  LOG_PERF(0, "Exit");

  return true;
}

struct PushReferenceStatus {
  bool rejected = false;
  QString refname;
  QString status;
};

thread_local PushReferenceStatus gPushReferenceStatus;

static int pushUpdateReferenceCallback(const char *refname, const char *status,
                                       void *data) {
  Q_UNUSED(data);

  if (!status) {
    return 0;
  }

  gPushReferenceStatus.rejected = true;
  gPushReferenceStatus.refname =
      refname ? QString::fromUtf8(refname) : QString();
  gPushReferenceStatus.status = QString::fromUtf8(status);

  LOG_ERROR(Log::ErrorId::Git, "Remote rejected push update for ref '{}': {}",
            gPushReferenceStatus.refname.toStdString(),
            gPushReferenceStatus.status.toStdString());

  git_error_set_str(GIT_ERROR_NET, status);
  return GIT_EUSER;
}

static int pushSidebandProgressCallback(const char *str, int len, void *data) {
  Q_UNUSED(data);

  if (!str || len <= 0) {
    return 0;
  }

  const QString message = QString::fromUtf8(str, len).trimmed();
  if (!message.isEmpty()) {
    LOG_DEBUG("Remote push output: {}", message.toStdString());
  }

  return 0;
}

static int fetchProgressCallback(const git_indexer_progress *stats,
                                 void *payload) {
  if (!payload || !stats) {
    return 0;
  }

  std::tuple<CloneProgress, GitCredentialPayload> *callbackPayload =
      static_cast<std::tuple<CloneProgress, GitCredentialPayload> *>(payload);
  auto &[clonePayload, _] = *callbackPayload;

  clonePayload.completedSteps = stats->indexed_objects;
  clonePayload.totalSteps = stats->total_objects;
  clonePayload.started = true;
  clonePayload.finished = false;
  clonePayload.stage = CloneProgressStageType::Fetch;

  if (stats->total_objects > 0) {
    const int percentage =
        ((stats->indexed_objects * 1.0f) / stats->total_objects) * 100;
    if (percentage > clonePayload.lastStepPercentage) {
      clonePayload.lastStepPercentage = percentage;
      EventManager::Event progressEvent(
          EventManager::EventType::GitRepoManagerCloneRepoProgress);
      progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
          clonePayload.uuid,
          GitRepoManager::Structs::GitRepoCommandType::Unknown,
          QVariant::fromValue(clonePayload)});
      EventManager::EventManager::getInstance().send(progressEvent);
    }
  } else {
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        clonePayload.uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(clonePayload)});
    EventManager::EventManager::getInstance().send(progressEvent);
  }

  return 0;
}

static void checkoutProgressCallback(const char *path, size_t completed_steps,
                                     const size_t total_steps, void *payload) {
  if (!payload)
    return;

  const auto progress = static_cast<CloneProgress *>(payload);
  progress->completedSteps = completed_steps;
  progress->totalSteps = total_steps;
  progress->started = true;
  progress->finished = false;
  progress->stage = CloneProgressStageType::Checkout;

  if (completed_steps == 0)
    progress->lastStepPercentage = 0;

  if (total_steps <= 0)
    return;

  const int percentage = ((completed_steps * 1.0f) / total_steps) * 100;
  if (percentage > progress->lastStepPercentage) {
    progress->lastStepPercentage = percentage;
    EventManager::Event progressEvent(
        EventManager::EventType::GitRepoManagerCloneRepoProgress);
    progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
        progress->uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
        QVariant::fromValue(*progress)});
    EventManager::EventManager::getInstance().send(progressEvent);
  }
}

static int certificateCheckCallback(git_cert *cert, int valid, const char *host,
                                    void *payload) {
  if (!cert || !host) {
    LOG_ERROR(Log::ErrorId::Network,
              "Certificate check called with null cert or host");
    return GIT_ECERTIFICATE;
  }

  const char *certTypeStr = nullptr;
  switch (cert->cert_type) {
  case GIT_CERT_X509:
    certTypeStr = "X.509";
    break;
  case GIT_CERT_HOSTKEY_LIBSSH2:
    certTypeStr = "SSH hostkey";
    break;
  case GIT_CERT_STRARRAY:
    certTypeStr = "certificate array";
    break;
  default:
    certTypeStr = "unknown type";
    break;
  }

  LOG_INFO("Certificate check for host: '{}', type: {}, valid: {}", host,
           certTypeStr, valid);

  // If the certificate is valid according to libgit2, accept it
  if (valid) {
    LOG_DEBUG("Certificate for host '{}' is valid", host);
    return 0; // Accept
  }

  // Certificate is not valid - log warning with details
  LOG_WARN("Certificate validation failed for host '{}' (type: {}). "
           "Accepting anyway to maintain connectivity. "
           "In production, consider enabling strict SSL verification.",
           host, certTypeStr);

  // For X.509 certificates, perform detailed validation
  if (cert->cert_type == GIT_CERT_X509) {
    git_cert_x509 *x509_cert = reinterpret_cast<git_cert_x509 *>(cert);
    if (x509_cert && x509_cert->data && x509_cert->len > 0) {
      LOG_DEBUG("X.509 certificate data present (size: {} bytes)",
                x509_cert->len);

      // Parse the certificate using OpenSSL
      const unsigned char *cert_data =
          reinterpret_cast<const unsigned char *>(x509_cert->data);
      X509 *x509 = d2i_X509(nullptr, &cert_data, x509_cert->len);

      if (x509) {
        // Get subject name
        char subject[256] = {0};
        X509_NAME_oneline(X509_get_subject_name(x509), subject,
                          sizeof(subject));

        // Get issuer name
        char issuer[256] = {0};
        X509_NAME_oneline(X509_get_issuer_name(x509), issuer, sizeof(issuer));

        LOG_INFO("Certificate subject: {}", subject);
        LOG_INFO("Certificate issuer: {}", issuer);

        // Check if it's self-signed
        bool isSelfSigned = (strcmp(subject, issuer) == 0);
        if (isSelfSigned) {
          LOG_WARN("Certificate for '{}' is self-signed", host);
        }

        // Perform certificate chain validation using OpenSSL
        X509_STORE *store = X509_STORE_new();
        if (store) {
          // Load system default CA certificates
          if (X509_STORE_set_default_paths(store) == 1) {
            LOG_DEBUG("Loaded system CA certificates");

            // Create verification context
            X509_STORE_CTX *ctx = X509_STORE_CTX_new();
            if (ctx) {
              // Initialize context with certificate and store
              if (X509_STORE_CTX_init(ctx, store, x509, nullptr) == 1) {
                // Set hostname for verification
                X509_VERIFY_PARAM *param = X509_STORE_CTX_get0_param(ctx);
                if (param) {
                  // Set hostname verification flags
#ifdef X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS
                  X509_VERIFY_PARAM_set_hostflags(
                      param, X509_CHECK_FLAG_NO_PARTIAL_WILDCARDS);
#else
                  X509_VERIFY_PARAM_set_hostflags(param, 0);
#endif
                  X509_VERIFY_PARAM_set1_host(param, host, 0);
                }

                // Perform verification
                int verifyResult = X509_verify_cert(ctx);
                if (verifyResult == 1) {
                  LOG_INFO(
                      "Certificate chain validation successful for host '{}'",
                      host);
                } else {
                  int error = X509_STORE_CTX_get_error(ctx);
                  const char *errorStr = X509_verify_cert_error_string(error);
                  LOG_WARN(
                      "Certificate chain validation failed for host '{}': {} "
                      "(error code: {})",
                      host, errorStr, error);

                  // Log additional details about the failure
                  if (error == X509_V_ERR_DEPTH_ZERO_SELF_SIGNED_CERT ||
                      error == X509_V_ERR_SELF_SIGNED_CERT_IN_CHAIN) {
                    LOG_WARN("Certificate chain contains self-signed "
                             "certificate");
                  } else if (error == X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT ||
                             error ==
                                 X509_V_ERR_UNABLE_TO_GET_ISSUER_CERT_LOCALLY) {
                    LOG_WARN("Unable to verify certificate issuer");
                  } else if (error == X509_V_ERR_CERT_HAS_EXPIRED ||
                             error == X509_V_ERR_CERT_NOT_YET_VALID) {
                    LOG_WARN("Certificate validity period issue");
                  }
                }
              }
              X509_STORE_CTX_free(ctx);
            } else {
              LOG_WARN("Failed to create X509_STORE_CTX");
            }
          } else {
            LOG_WARN("Failed to load system CA certificates");
          }
          X509_STORE_free(store);
        } else {
          LOG_WARN("Failed to create X509_STORE");
        }

        X509_free(x509);
      } else {
        LOG_WARN("Failed to parse X.509 certificate data");
      }
    }
  }

  // TODO: Add configuration option to enable strict SSL verification
  // For now, accept invalid certificates to maintain compatibility
  // with self-signed certificates and development environments

  // Return 0 to accept the certificate
  // Return GIT_ECERTIFICATE to reject it
  return 0; // Accept even if invalid
}

static void finishFetchProgress(const QString &uuid, const bool error,
                                const bool cloning) {

  // Avoid race condition with progress toast creation
  std::this_thread::sleep_for(1500ms);

  CloneProgress progress;
  progress.uuid = uuid;
  progress.completedSteps = 100;
  progress.totalSteps = 100;
  progress.started = true;
  progress.finished = true;
  progress.error = error;
  progress.cloning = cloning;

  EventManager::Event progressEvent(
      EventManager::EventType::GitRepoManagerCloneRepoProgress);
  progressEvent.addData(GitRepoManager::Structs::GitRepoResult{
      uuid, GitRepoManager::Structs::GitRepoCommandType::Unknown,
      QVariant::fromValue(progress)});
  EventManager::EventManager::getInstance().send(progressEvent);
}

static void finishCredentialsAcquiring(const QString &url) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  EventManager::Event credentialsFinishedEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsFinished);
  credentialsFinishedEvent.addData(qHash(url));

  eventManager.send(credentialsFinishedEvent);
}

static std::tuple<bool, GitPlaintextCredentials>
acquireCredentials(const QString &repoPath, const QString &url,
                   const QString &username, const int allowedTypes) {
  LOG_PERF(0, "Enter");

  const QString normalizedRepoPath = QDir::fromNativeSeparators(repoPath);

  LOG_INFO("acquireCredentials called with repoPath: '{}', url: '{}'",
           normalizedRepoPath, url);

  // First, try to get stored credentials from encrypted storage
  GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();

  QString storedUsername, storedPassword;
  if (credStorage.getCredentials(normalizedRepoPath, storedUsername,
                                 storedPassword)) {
    LOG_INFO("Found stored credentials for repository: {}", normalizedRepoPath);

    // Check if credentials are expired
    const qlonglong expirationTimeout =
        ConfigManager::ConfigManager::getInstance().getLong(
            ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime);

    LOG_INFO("Expiration timeout: {} ms", expirationTimeout);

    if (!credStorage.areCredentialsExpired(normalizedRepoPath,
                                           expirationTimeout)) {
      LOG_INFO("Using stored credentials for repository: {}",
               normalizedRepoPath);
      LOG_PERF(0, "Exit");
      return std::make_tuple(
          true, GitPlaintextCredentials{storedUsername, storedPassword,
                                        QDateTime::currentMSecsSinceEpoch()});
    } else {
      LOG_INFO("Stored credentials expired for repository: {}",
               normalizedRepoPath);
    }
  } else {
    LOG_INFO("No stored credentials found for repository: {}",
             normalizedRepoPath);
  }

  // If no stored credentials or expired, request from user
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();

  const int token = eventManager.startWait();

  GitCredentialsRequest request;
  request.id = qHash(url);
  request.url = url;
  request.username = username;
  request.allowedTypes = allowedTypes;
  request.repoPath = normalizedRepoPath;

  EventManager::Event credentialsEvent(
      EventManager::EventType::GitRepoManagerGetCredentials);
  credentialsEvent.addData(request);

  eventManager.send(credentialsEvent);

  const auto predicate = [&](EventManager::Event event) -> bool {
    bool success = false;
    switch (event.type()) {
    case EventManager::EventType::GitRepoManagerGetCredentialsCancel: {
      const auto [id] = event.data<GitCredentialsErrorResponse>();
      success = id == request.id;
      break;
    }
    case EventManager::EventType::GitRepoManagerGetCredentialsSubmit: {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();
      success = response.id == request.id;
      break;
    }
    default: {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Unknown event type.");
      break;
    }
    }

    return success;
  };

  const std::set<EventManager::EventType> types{
      EventManager::EventType::GitRepoManagerGetCredentialsCancel,
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit};

  const auto &[success, event] =
      eventManager.waitOneOf(token, types, predicate);

  if (success) {
    if (event.type() ==
        EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
      LOG_PERF(0, "Exit");
      return std::make_tuple(false, GitPlaintextCredentials{});
    } else if (event.type() ==
               EventManager::EventType::GitRepoManagerGetCredentialsSubmit) {
      const GitCredentialsResponse response =
          event.data<GitCredentialsResponse>();

      LOG_INFO("Received credentials response: username='{}', "
               "rememberCredentials={}",
               response.username, response.rememberCredentials);

      // Store credentials in encrypted storage if user wants to remember them
      if (response.rememberCredentials) {
        GitCredentialStorage &credStorage = GitCredentialStorage::getInstance();
        credStorage.storeCredentials(normalizedRepoPath, response.username,
                                     response.password, true);
        LOG_INFO("Stored credentials for repository: {}", normalizedRepoPath);
      } else {
        LOG_INFO("User chose NOT to remember credentials");
      }

      LOG_PERF(0, "Exit");
      return std::make_tuple(
          true, GitPlaintextCredentials{response.username, response.password,
                                        QDateTime::currentMSecsSinceEpoch()});
    } else {
      LOG_ERROR(Log::ErrorId::Git, "Internal error. Unknown event type.");
    }
  }

  LOG_PERF(0, "Exit");
  return std::make_tuple(false, GitPlaintextCredentials{});
}

static bool validateSshKey(const QString &privateKeyPath,
                           const QString &publicKeyPath, bool passphrasePresent,
                           const QString &passphrase) {
  LOG_PERF(0, "Enter");

  // Check if files exist
  if (!QFile::exists(privateKeyPath)) {
    LOG_ERROR(Log::ErrorId::Git, "SSH private key not found: {}",
              privateKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!QFile::exists(publicKeyPath)) {
    LOG_ERROR(Log::ErrorId::Git, "SSH public key not found: {}", publicKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate private key using OpenSSL
  FILE *privateKeyFile = fopen(privateKeyPath.toStdString().c_str(), "r");
  if (!privateKeyFile) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open SSH private key: {}",
              privateKeyPath);
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY *privateKey = PEM_read_PrivateKey(
      privateKeyFile, nullptr, nullptr,
      !passphrasePresent ? nullptr : (void *)passphrase.toStdString().c_str());
  fclose(privateKeyFile);

  if (!privateKey) {
    unsigned long err = ERR_get_error();
    char errBuf[256];
    ERR_error_string_n(err, errBuf, sizeof(errBuf));
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to read SSH private key (OpenSSL error): {}", errBuf);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate that it's a valid key by checking if we can get its parameters
  const int keyType = EVP_PKEY_base_id(privateKey);
  if (keyType != EVP_PKEY_RSA && keyType != EVP_PKEY_DSA &&
      keyType != EVP_PKEY_EC && keyType != EVP_PKEY_ED25519) {
    LOG_ERROR(Log::ErrorId::Git, "SSH private key has unsupported type: {}",
              keyType);
    EVP_PKEY_free(privateKey);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate public key
  FILE *publicKeyFile = fopen(publicKeyPath.toStdString().c_str(), "r");
  if (!publicKeyFile) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open SSH public key: {}",
              publicKeyPath);
    EVP_PKEY_free(privateKey);
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY *publicKey =
      PEM_read_PUBKEY(publicKeyFile, nullptr, nullptr, nullptr);
  fclose(publicKeyFile);

  if (!publicKey) {
    // Public key might be in SSH format, not PEM - this is acceptable
    LOG_INFO("SSH public key is not in PEM format (likely SSH format): {}",
             publicKeyPath);
    EVP_PKEY_free(privateKey);
    LOG_PERF(0, "Exit");
    return true; // Accept SSH format public keys
  }

  // Verify that public and private keys match
  const int keyTypePublic = EVP_PKEY_base_id(publicKey);
  if (keyType != keyTypePublic) {
    LOG_ERROR(Log::ErrorId::Git,
              "SSH key pair type mismatch: private={}, public={}", keyType,
              keyTypePublic);
    EVP_PKEY_free(privateKey);
    EVP_PKEY_free(publicKey);
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY_free(privateKey);
  EVP_PKEY_free(publicKey);

  LOG_INFO("SSH key pair validated successfully: {}", privateKeyPath);
  LOG_PERF(0, "Exit");
  return true;
}

static int credentialsAcquireCallback(git_credential **out, const char *url,
                                      const char *username_from_url,
                                      const unsigned int allowed_types,
                                      void *payload) {
  LOG_PERF(0, "Enter");

  if (!out || !url) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  if (((allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) == 0) &&
      ((allowed_types & GIT_CREDENTIAL_SSH_KEY) == 0)) {
    LOG_PERF(0, "Exit");
    return 1;
  }

  // Extract repository path from payload
  QString repoPath;
  if (payload) {
    std::tuple<CloneProgress, GitCredentialPayload> *callbackPayload =
        static_cast<std::tuple<CloneProgress, GitCredentialPayload> *>(payload);
    const auto &[_, credentialsPayload] = *callbackPayload;
    repoPath = credentialsPayload.repoPath;
  }

  if (const QString urlString = QString::fromUtf8(url);
      urlString.startsWith("git@")) {
    // Extract host from git URL (e.g., "git@github.com:user/repo.git" ->
    // "github.com")
    QString host;
    const QRegularExpression hostRegex("^git@([^:]+):");
    const QRegularExpressionMatch match = hostRegex.match(urlString);
    if (match.hasMatch()) {
      host = match.captured(1);
      LOG_DEBUG("Extracted host '{}' from URL", host);
    }

    QString privateKeyPath;
    const GitSshAuthInfo sshAuth =
        getSshAuthInfo(repoPath, QString::fromUtf8(url),
                       username_from_url ? username_from_url : "git");
    if (!sshAuth.valid) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to acquire SSH auth info for: {}",
                QString::fromUtf8(url));
      LOG_PERF(0, "Exit");
      return GIT_EUSER;
    }

    const int error = git_credential_ssh_key_new(
        out, sshAuth.username.toStdString().c_str(),
        sshAuth.publicKeyPath.toStdString().c_str(),
        sshAuth.privateKeyPath.toStdString().c_str(),
        sshAuth.passphrase.isEmpty()
            ? nullptr
            : sshAuth.passphrase.toStdString().c_str());

    LOG_PERF(0, "Exit");
    return error;
  }

  gGitCredentialsMutex.lock();

  if (const auto it = gGitCredentials.find(url); it != gGitCredentials.end()) {
    gGitCredentialsMutex.unlock();

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qlonglong expirationTimeout =
        ConfigManager::ConfigManager::getInstance().getLong(
            ConfigManager::ConfigKey::GitRepoGitCredentialsExpirationTime);
    if (it->createdTime + expirationTimeout < now) {
      const auto &[success, credentials] = acquireCredentials(
          repoPath, url, username_from_url ? username_from_url : "",
          allowed_types);
      if (success) {
        LOG_INFO("Acquired credentials for '{}'", credentials.username);

        gGitCredentialsMutex.lock();

        gGitCredentials[url] = credentials;

        gGitCredentialsMutex.unlock();
      } else {
        LOG_PERF(0, "Exit");
        return -1;
      }
    }
  } else {
    gGitCredentialsMutex.unlock();

    const auto &[success, credentials] = acquireCredentials(
        repoPath, url, username_from_url ? username_from_url : "",
        allowed_types);
    if (success) {
      LOG_INFO("Acquired credentials for '{}'", credentials.username);

      gGitCredentialsMutex.lock();

      gGitCredentials[url] = credentials;

      gGitCredentialsMutex.unlock();
    } else {
      LOG_PERF(0, "Exit");
      return GIT_EUSER;
    }
  }

  gGitCredentialsMutex.lock();

  const auto &credentials = gGitCredentials[url];

  gGitCredentialsMutex.unlock();

  const int error = git_credential_userpass_plaintext_new(
      out, credentials.username.toStdString().c_str(),
      credentials.password.toStdString().c_str());
  if (error) {
    setError(
        GitError::CredentialsAcquireCallbackFailedToCreatePlaintextCredentials,
        error);
    LOG_PERF(0, "Exit");
    return error;
  }

  LOG_PERF(0, "Exit");

  return 0;
}

bool checkoutCommit(git_repository *repo, const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Checking out commit: '{}'", commit.toStdString());

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid oid;
  git_commit *commit_ = nullptr;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to parse commit OID '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::CheckoutCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup commit '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::CheckoutCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Commit '{}' found, retrieving tree", commit.toStdString());

  git_tree *tree = nullptr;
  error = git_commit_tree(&tree, commit_);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get tree from commit: error={}",
              error);
    setError(GitError::CheckoutCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Checking out tree for commit '{}'", commit.toStdString());
  git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
  opts.checkout_strategy = GIT_CHECKOUT_SAFE;

  error = git_checkout_tree(repo, reinterpret_cast<const git_object *>(tree),
                            &opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to checkout tree: error={}", error);
    setError(GitError::CheckoutCommitFailedToCheckoutCommitTree, error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Setting HEAD to detached state at commit '{}'",
            commit.toStdString());
  error = git_repository_set_head_detached(repo, &oid);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set HEAD detached to commit '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::CheckoutCommitFailedToDetachHead, error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully checked out commit '{}'", commit.toStdString());
  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return true;
}

bool cloneRepository(const QString &url, const QString &destinationDir,
                     const QString &uuid) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting repository clone operation");
  LOG_DEBUG("  URL: '{}'", url.toStdString());
  LOG_DEBUG("  Destination directory: '{}'", destinationDir.toStdString());
  LOG_DEBUG("  UUID: '{}'", uuid.toStdString());

  const QDir workingDir;
  const bool workingDirExisted = workingDir.exists(destinationDir);
  LOG_DEBUG("Destination directory already existed: {}", workingDirExisted);

  if (!workingDir.mkpath(destinationDir)) {
    LOG_ERROR(Log::ErrorId::Git, "can't create destination dir '{}'",
              destinationDir.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Destination directory created/validated successfully");

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::CloneRepositoryGitSshUrlRequiresCommercialPlan,
             GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Cloning git@ repositories is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository *repo = nullptr;
  git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;
  git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = true;

  GitCredentialPayload credPayload;
  credPayload.repoPath = destinationDir;
  credPayload.uuid = uuid;
  credPayload.cloning = true;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  checkout_opts.progress_payload = &progressData;
  checkout_opts.progress_cb = &checkoutProgressCallback;
  fetch_opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetch_opts.callbacks.transfer_progress = &fetchProgressCallback;
  fetch_opts.callbacks.certificate_check = &certificateCheckCallback;
  fetch_opts.callbacks.payload = &callbackPayload;
  clone_opts.checkout_opts = checkout_opts;
  clone_opts.fetch_opts = fetch_opts;

  LOG_DEBUG("Initialized clone options, starting git_clone operation");
  const int error =
      git_clone(&repo, url.toStdString().c_str(),
                destinationDir.toStdString().c_str(), &clone_opts);

  LOG_DEBUG("git_clone returned with error code: {}", error);

  finishFetchProgress(uuid, error != 0, true);

  if (commercial) {
    finishCredentialsAcquiring(url);
  }

  if (error || !repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Clone failed: error code={}, repo pointer null={}", error,
              !repo);
    invalidateCredentials(url);

    if (!workingDirExisted) {
      LOG_DEBUG("Cleaning up destination directory as clone failed");
      QDir dirToRemove(destinationDir);
      if (!dirToRemove.removeRecursively()) {
        LOG_WARN(
            "Failed to clean up destination directory after clone failure");
      }
    }

    setError(GitError::CloneRepositoryFailedToCloneRepository, error);
    LOG_ERROR(Log::ErrorId::Git, "Repository clone failed for URL: {}",
              url.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Repository cloned successfully");
  git_repository_free(repo);
  LOG_INFO("Clone operation completed successfully for URL: {}",
           url.toStdString());

  LOG_PERF(0, "Exit");
  return true;
}

GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            const BranchType type, const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting branch history: branchName='{}', type={}",
            branchName.toStdString(), static_cast<int>(type));

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  GitHistory history_;

  git_revwalk *walker = nullptr;

  int error = git_revwalk_new(&walker, repo);
  if (error) {
    setError(GitError::GetBranchHistoryFailedToCreateRevwalk, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
  if (error) {
    setError(GitError::GetBranchHistoryFailedToConfigureRevwalkSorting, error);
    git_revwalk_free(walker);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (type == BranchType::All || branchName == ALL_BRANCHES) {
    git_branch_iterator *it = nullptr;
    git_reference *ref = nullptr;
    git_branch_t branchType;

    if (branchName == ALL_BRANCHES) {
      if (type == BranchType::Local) {
        branchType = GIT_BRANCH_LOCAL;
      } else if (type == BranchType::Remote) {
        branchType = GIT_BRANCH_REMOTE;
      } else {
        git_revwalk_free(walker);
        LOG_PERF(0, "Exit");
        return {};
      }
    } else {
      branchType = GIT_BRANCH_ALL;
    }

    error = git_branch_iterator_new(&it, repo, branchType);
    if (error) {
      setError(GitError::GetBranchHistoryFailedToCreateBranchIterator, error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    while (git_branch_next(&ref, &branchType, it) == 0 && !quitFlag) {
      const char *currentBranchName = nullptr;

      error = git_branch_name(&currentBranchName, ref);
      if (error) {
        setError(GitError::GetBranchHistoryFailedToReadBranchName, error);
        git_reference_free(ref);
        git_revwalk_free(walker);
        git_branch_iterator_free(it);
        LOG_PERF(0, "Exit");
        return {};
      }

      if (!currentBranchName) {
        git_reference_free(ref);
        git_branch_iterator_free(it);
        git_revwalk_free(walker);
        LOG_ERROR(Log::ErrorId::Git, "No branch name found!");
        LOG_PERF(0, "Exit");
        return {};
      }

      QString fullBranchName;

      if (branchType == GIT_BRANCH_LOCAL)
        fullBranchName = QString("refs/heads/%1").arg(currentBranchName);
      else if (branchType == GIT_BRANCH_REMOTE)
        fullBranchName = QString("refs/remotes/%1").arg(currentBranchName);
      else {
        git_reference_free(ref);
        git_branch_iterator_free(it);
        git_revwalk_free(walker);
        LOG_ERROR(Log::ErrorId::Git, "Unknown branch type!");
        LOG_PERF(0, "Exit");
        return {};
      }

      error =
          git_revwalk_push_ref(walker, fullBranchName.toStdString().c_str());
      if (error) {
        setError(GitError::GetBranchHistoryFailedToAddReferenceToRevwalk,
                 error);
        git_reference_free(ref);
        git_branch_iterator_free(it);
        git_revwalk_free(walker);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_reference_free(ref);
    }

    git_branch_iterator_free(it);
  } else if (type == BranchType::Local) {
    if (branchName.isEmpty()) {
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (branchName == HEAD) {
      error = git_revwalk_push_head(walker);
    } else {
      error = git_revwalk_push_ref(
          walker,
          QString("refs/heads/%1").arg(branchName).toStdString().c_str());
    }
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddLocalBranchToRevwalk,
               error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else if (type == BranchType::Remote) {
    if (branchName.isEmpty()) {
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_revwalk_push_ref(
        walker,
        QString("refs/remotes/%1").arg(branchName).toStdString().c_str());
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddRemoteBranchToRevwalk,
               error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else if (type == BranchType::Tag) {
    if (branchName.isEmpty()) {
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_revwalk_push_ref(
        walker, QString("refs/tags/%1").arg(branchName).toStdString().c_str());
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddTagToRevwalk, error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else {
    git_revwalk_free(walker);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_oid oid;

  QSet<QString> processed;

  while (git_revwalk_next(&oid, walker) == 0 && !quitFlag) {
    const char *str = git_oid_tostr_s(&oid);
    if (!str) {
      LOG_ERROR(Log::ErrorId::Git, "Can't convert oid to string.");
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    const QString oidStr = str;

    git_commit *commit = nullptr;

    error = git_commit_lookup(&commit, repo, &oid);
    if (error) {
      setError(GitError::GetBranchHistoryFailedToLookupCommit, error);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (!commit) {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit with oid \"{}\"",
                oidStr);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (processed.contains(oidStr)) {
      continue;
    }

    processed.insert(oidStr);

    history_.commits.push_back(oidStr);

    const std::size_t parentCount = git_commit_parentcount(commit);

    for (std::size_t i = 0; i < parentCount && !quitFlag; ++i) {
      const git_oid *parentOid = git_commit_parent_id(commit, i);
      if (!parentOid) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Can't lookup parent oid at index {} for oid \"{}\"", i,
                  oidStr);
        git_commit_free(commit);
        git_revwalk_free(walker);
        LOG_PERF(0, "Exit");
        return {};
      }

      str = git_oid_tostr_s(parentOid);
      if (!str) {
        LOG_ERROR(
            Log::ErrorId::Git,
            "Can't convert parent oid at index {} for oid \"{}\" to string.", i,
            oidStr);
        git_commit_free(commit);
        git_revwalk_free(walker);
        LOG_PERF(0, "Exit");
        return {};
      }

      const QString parentOidStr = str;

      if (oidStr == parentOidStr) {
        LOG_ERROR(Log::ErrorId::Git, "The commit \"{}\"is parent of itself",
                  parentOidStr)
        continue;
      }

      history_.commitInfos[oidStr].parents.push_back(parentOidStr);
      history_.commitInfos[parentOidStr].children.push_back(oidStr);
    }

    history_.commitInfos[oidStr].oid = oid;

    if (const char *message = git_commit_message(commit))
      history_.commitInfos[oidStr].message = message;
    else {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit message for oid \"{}\"",
                oidStr);
      git_commit_free(commit);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (const char *summary = git_commit_summary(commit))
      history_.commitInfos[oidStr].summary = summary;
    else {
      LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit summary for oid \"{}\"",
                oidStr);
      git_commit_free(commit);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (const git_signature *signature = git_commit_author(commit)) {
      if (signature->name)
        history_.commitInfos[oidStr].authorName = signature->name;

      if (signature->email)
        history_.commitInfos[oidStr].authorEmail = signature->email;

      history_.commitInfos[oidStr].timeStamp =
          signature->when.time * 1000 + signature->when.offset * 60 * 1000;
    } else {
      LOG_ERROR(Log::ErrorId::Git,
                "Can't lookup commit signature for oid \"{}\"", oidStr);
      git_commit_free(commit);
      git_revwalk_free(walker);
      LOG_PERF(0, "Exit");
      return {};
    }

    QString commitSignature;
    QString commitSignatureType;
    loadCommitSignatureInfo(repo, &oid, &commitSignature, &commitSignatureType);
    history_.commitInfos[oidStr].signedCommit =
        commitSignatureType != QStringLiteral("Unsigned") &&
        commitSignatureType != QStringLiteral("Unavailable") &&
        !commitSignature.isEmpty();
    history_.commitInfos[oidStr].signatureType = commitSignatureType;
    history_.commitInfos[oidStr].signature = commitSignature;

    history_.commitInfos[oidStr].branches += branchName;

    git_commit_free(commit);
  }

  git_revwalk_free(walker);

  if (!quitFlag) {
    std::sort(history_.commits.begin(), history_.commits.end(),
              [&](const QString &a, const QString &b) {
                auto it = history_.commitInfos.find(a);
                if (it == history_.commitInfos.end()) {
                  return false;
                }

                const GitCommitInfo &commitInfoA = it.value();

                it = history_.commitInfos.find(b);
                if (it == history_.commitInfos.end()) {
                  return false;
                }

                const GitCommitInfo &commitInfoB = it.value();

                return commitInfoA.timeStamp < commitInfoB.timeStamp;
              });
  }

  history_.valid = true;

  LOG_PERF(0, "Exit");

  return quitFlag ? GitHistory{} : history_;
}

QStringList getCommitBranches(git_repository *repo, const QString &commitOid,
                              const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  if (commitOid.isEmpty()) {
    LOG_WARN("Commit \"{}\" is empty", commitOid);
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::GetCommitBranchesFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    const char *branchName = nullptr;

    error = git_branch_name(&branchName, ref);
    if (error) {
      setError(GitError::GetCommitBranchesFailedToReadBranchName, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (!branchName) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_commit *branchCommit = nullptr;
    const git_oid *branchOid = nullptr;

    if (git_reference_type(ref) == GIT_REFERENCE_SYMBOLIC) {
      git_reference *symbolicRef = nullptr;
      error = git_reference_resolve(&symbolicRef, ref);
      if (error) {
        setError(GitError::GetCommitBranchesFailedToResolveReference, error);
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_object *branchTip = nullptr;

      error = git_reference_peel(&branchTip, symbolicRef, GIT_OBJECT_COMMIT);
      if (error) {
        setError(GitError::GetCommitBranchesFailedToPeelReference, error);
        git_reference_free(symbolicRef);
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return {};
      }

      branchOid = git_object_id(branchTip);

      git_object_free(branchTip);
      git_reference_free(symbolicRef);
    } else {
      branchOid = git_reference_target(ref);
    }

    if (!branchOid) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      setError(GitError::GetCommitBranchesFailedToParseObjectId, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    // Check if branch contains the commit
    if (const int contains = git_graph_descendant_of(repo, branchOid, &oid);
        contains == 1)
      branches += branchName;

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return branches;
}

QStringList getCommitTags(git_repository *repo, const QString &commitOid,
                          const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList tags;
  git_strarray array = {nullptr, 0};

  const int error = git_tag_list_match(&array, "", repo);
  if (error) {
    setError(GitError::GetCommitTagsFailedToListMatchingTags, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  const QByteArray commitOidUtf8 = commitOid.toUtf8();

  for (size_t i = 0; i < array.count && !quitFlag; ++i) {
    const QString tagName = array.strings[i];
    git_object *object = nullptr;

    const QString spec = QStringLiteral("refs/tags/") + tagName;

    if (git_revparse_single(&object, repo, spec.toStdString().c_str()) != 0) {
      continue;
    }

    git_object *commitObject = nullptr;
    if (git_object_peel(&commitObject, object, GIT_OBJECT_COMMIT) == 0) {
      const git_oid *commitOidValue = git_object_id(commitObject);
      if (commitOidValue &&
          git_oid_streq(commitOidValue, commitOidUtf8.constData())) {
        tags.append(tagName);
      }
      git_object_free(commitObject);
    }

    git_object_free(object);
  }

  git_strarray_dispose(&array);

  LOG_PERF(0, "Exit");
  return tags;
}

QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    setError(GitError::GetCommitHeadsFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    git_object *branchTip = nullptr;
    error = git_reference_peel(&branchTip, ref, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::GetCommitHeadsFailedToPeelReference, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    const char *branchName = nullptr;
    error = git_branch_name(&branchName, ref);
    if (error) {
      setError(GitError::GetCommitHeadsFailedToReadBranchName, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      setError(GitError::GetCommitHeadsFailedToParseObjectId, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_object_free(branchTip);

    // Check if this commit is the HEAD of the branch
    const git_oid *branchHeadOid = git_reference_target(ref);
    if (branchHeadOid) {
      const bool head = git_oid_equal(branchHeadOid, &oid) == 0;
      if (head)
        branches += branchName;
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return branches;
}

bool getCommitIsHead(git_repository *repo, const QString &commitOid,
                     const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    setError(GitError::GetCommitIsHeadFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    git_object *branchTip = nullptr;
    error = git_reference_peel(&branchTip, ref, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::GetCommitIsHeadFailedToPeelReference, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchName = nullptr;
    error = git_branch_name(&branchName, ref);
    if (error) {
      setError(GitError::GetCommitIsHeadFailedToReadBranchName, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      setError(GitError::GetCommitIsHeadFailedToParseObjectId, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(branchTip);

    // Check if this commit is the HEAD of the branch
    const git_oid *branchHeadOid = git_reference_target(ref);
    if (branchHeadOid) {
      const bool head = git_oid_equal(branchHeadOid, &oid) == 0;
      if (head) {
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return true;
      }
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return false;
}

GitStatus getStatus(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting repository status");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  GitStatus status = {};

  const QString repoPath =
      QDir::fromNativeSeparators(git_repository_workdir(repo));
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_status_options opts;
  git_status_options_init(&opts, GIT_STATUS_OPTIONS_VERSION);
  opts.show = GIT_STATUS_SHOW_INDEX_AND_WORKDIR;
  opts.flags =
      GIT_STATUS_OPT_INCLUDE_UNTRACKED | GIT_STATUS_OPT_RECURSE_UNTRACKED_DIRS |
      GIT_STATUS_OPT_INCLUDE_IGNORED | GIT_STATUS_OPT_RENAMES_HEAD_TO_INDEX |
      GIT_STATUS_OPT_SORT_CASE_SENSITIVELY | GIT_STATUS_OPT_INCLUDE_UNREADABLE;

  git_status_list *statusList = nullptr;

  if (const int error = git_status_list_new(&statusList, repo, &opts)) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to create status list: error={}",
              error);
    setError(GitError::GetStatusFailedToCreateStatusList, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  size_t count = git_status_list_entrycount(statusList);
  LOG_DEBUG("Found {} status entries", count);

  if (quitFlag) {
    LOG_DEBUG("Repository status loading cancelled");
    git_status_list_free(statusList);
    LOG_PERF(0, "Exit");
    return {};
  }

  for (size_t i = 0; i < count; ++i) {
    if (quitFlag) {
      LOG_DEBUG("Repository status loading cancelled");
      break;
    }

    const git_status_entry *entry = git_status_byindex(statusList, i);

    Q_ASSERT(entry != nullptr);

    if (!entry) {
      continue;
    }

    const git_diff_delta *delta =
        entry->head_to_index ? entry->head_to_index : entry->index_to_workdir;
    const char *filePath = nullptr;
    if (delta) {
      filePath =
          delta->new_file.path ? delta->new_file.path : delta->old_file.path;
    }

    Q_ASSERT(filePath != nullptr);

    if (!filePath) {
      continue;
    }

    QString unixFilePath = QDir::fromNativeSeparators(filePath);
    const QString fullPath = repoPath + unixFilePath;

    if (QFileInfo(fullPath).isDir()) {
      if (QDir(fullPath)
              .entryList(QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot)
              .isEmpty()) {
        // Skip empty directories
        continue;
      }
    }

    if (unixFilePath.endsWith('/')) {
      unixFilePath = unixFilePath.left(unixFilePath.length() - 1);
    }

    const unsigned int status_flags = entry->status;

    if ((status_flags & GIT_STATUS_WT_NEW) != 0) {
      status.newFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_MODIFIED) != 0) {
      status.modifiedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_DELETED) != 0) {
      status.deletedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_TYPECHANGE) != 0) {
      status.typeChangedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_RENAMED) != 0) {
      status.renamedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_WT_UNREADABLE) != 0) {
      status.unreadableFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_NEW) != 0) {
      status.newStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_MODIFIED) != 0) {
      status.modifiedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_DELETED) != 0) {
      status.deletedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_TYPECHANGE) != 0) {
      status.typeChangedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_INDEX_RENAMED) != 0) {
      status.renamedStagedFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_IGNORED) != 0) {
      status.ignoredFiles += unixFilePath;
    }

    if ((status_flags & GIT_STATUS_CONFLICTED) != 0) {
      status.conflictedFiles += unixFilePath;
    }
  }

  git_status_list_free(statusList);

  LOG_PERF(0, "Exit");

  return status;
}

GitCommitStatus getCommitStatus(git_repository *repo,
                                const QString &commitOid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  GitCommitStatus status;

  git_commit *commit = nullptr;
  git_commit *parent = nullptr;
  git_tree *tree = nullptr;
  git_tree *parent_tree = nullptr;
  git_diff *diff = nullptr;

  // Lookup commit by SHA
  git_oid oid;
  git_oid_fromstr(&oid, commitOid.toStdString().c_str());

  int error = git_commit_lookup(&commit, repo, &oid);
  if (error) {
    setError(GitError::GetCommitStatusFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get commit tree
  error = git_commit_tree(&tree, commit);
  if (error) {
    setError(GitError::GetCommitStatusFailedToGetCommitTree, error);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  int parentCount = git_commit_parentcount(commit);

  // Get parent tree (if exists)
  if (parentCount > 0) {
    error = git_commit_parent(&parent, commit, 0);
    if (error) {
      setError(GitError::GetCommitStatusFailedToGetParentCommit, error);
      git_tree_free(tree);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parent_tree, parent);
    if (error) {
      setError(GitError::GetCommitStatusFailedToGetSecondCommitTree, error);
      git_tree_free(tree);
      git_commit_free(parent);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else {
    parent_tree = nullptr; // Initial commit
  }

  // Generate diff
  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.flags |= GIT_DIFF_INCLUDE_UNTRACKED | GIT_DIFF_INCLUDE_TYPECHANGE |
                GIT_DIFF_RECURSE_UNTRACKED_DIRS;

  error = git_diff_tree_to_tree(&diff, repo, parent_tree, tree, &opts);
  if (error) {
    setError(GitError::GetCommitStatusFailedToCreateCommitDiff, error);

    git_tree_free(tree);

    if (parent_tree) {
      git_tree_free(parent_tree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Detect renames
  git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
  find_opts.flags = GIT_DIFF_FIND_RENAMES;

  error = git_diff_find_similar(diff, &find_opts);
  if (error) {
    setError(GitError::GetCommitStatusFailedToDetectRenames, error);

    git_diff_free(diff);

    if (parent_tree) {
      git_tree_free(parent_tree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  status.commit = commitOid;

  if (const char *summary = git_commit_summary(commit)) {
    status.summary = QString::fromUtf8(summary);
  } else if (const char *message = git_commit_message(commit)) {
    status.summary = QString::fromUtf8(message).section('\n', 0, 0);
  }

  parentCount = git_commit_parentcount(commit);
  status.parentShas.reserve(parentCount);
  for (int i = 0; i < parentCount; ++i) {
    const git_oid *parentOid = git_commit_parent_id(commit, i);
    if (!parentOid) {
      continue;
    }

    char parentOidStr[GIT_OID_HEXSZ + 1] = {0};
    git_oid_tostr(parentOidStr, GIT_OID_HEXSZ + 1, parentOid);
    status.parentShas += QString::fromUtf8(parentOidStr);
  }

  if (const git_signature *author = git_commit_author(commit)) {
    if (author->name) {
      status.authorName = QString::fromUtf8(author->name);
    }
    if (author->email) {
      status.authorEmail = QString::fromUtf8(author->email);
    }
  }

  status.committerDate =
      commitSignatureDateToString(git_commit_committer(commit));

  // Walk the diff
  git_diff_foreach(
      diff,
      [](const git_diff_delta *delta, float, void *payload) {
        auto *status = static_cast<GitCommitStatus *>(payload);

        switch (delta->status) {
        case GIT_DELTA_ADDED:
          status->newFiles += delta->new_file.path;
          break;
        case GIT_DELTA_MODIFIED:
          status->modifiedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_DELETED:
          status->deletedFiles += delta->old_file.path;
          break;
        case GIT_DELTA_RENAMED:
          status->renamedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_TYPECHANGE:
          status->typeChangedFiles += delta->new_file.path;
          break;
        default:
          break;
        }

        return 0;
      },
      nullptr, nullptr, nullptr, &status);

  // Cleanup in reverse order of allocation
  git_diff_free(diff);

  if (parent_tree) {
    git_tree_free(parent_tree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit);

  LOG_PERF(0, "Exit");

  return status;
}

bool stageFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Staging {} files", files.size());

  int fileIndex = 0;
  for (const auto &file : files) {
    LOG_DEBUG("  File {}: '{}'", fileIndex++, file.toStdString());
  }

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository index: error={}",
              error);
    setError(GitError::StageFilesFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString repoPath =
      QDir::fromNativeSeparators(git_repository_workdir(repo));
  LOG_DEBUG("Repository working directory: '{}'", repoPath.toStdString());

  for (const auto &filePath : files) {
    const QString unixFilePath = QDir::fromNativeSeparators(filePath);
    const QString fullPath = QDir(repoPath).filePath(unixFilePath);
    const QString relativePath = QDir(repoPath).relativeFilePath(fullPath);
    LOG_DEBUG("Processing file: '{}' (relative: '{}')", fullPath.toStdString(),
              relativePath.toStdString());

    if (!QFileInfo::exists(fullPath)) {
      LOG_DEBUG("File does not exist, removing from index: '{}'",
                relativePath.toStdString());
      error =
          git_index_remove_bypath(index, relativePath.toStdString().c_str());
      if (error) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Failed to remove file from index '{}': error={}",
                  relativePath.toStdString(), error);
        setError(GitError::StageFilesFailedToRemovePathFromIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      continue;
    }

    git_submodule *submodule = nullptr;
    error = git_submodule_lookup(&submodule, repo,
                                 relativePath.toStdString().c_str());
    if (!error) {
      LOG_DEBUG("Path is a submodule, staging gitlink: '{}'",
                relativePath.toStdString());
      error = git_submodule_add_to_index(submodule, 0);
      git_submodule_free(submodule);
      if (error) {
        setError(GitError::StageFilesFailedToAddSubmoduleToIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else if (error == GIT_ENOTFOUND) {
      giterr_clear();
      error = git_index_add_bypath(index, relativePath.toStdString().c_str());
      if (error) {
        setError(GitError::StageFilesFailedToAddPathToIndex, error);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else {
      setError(GitError::StageFilesFailedToAddMissingPathToIndex, error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_index_write(index);
  if (error) {
    setError(GitError::StageFilesFailedToWriteIndex, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);

  LOG_PERF(0, "Exit");

  return true;
}

bool unstageFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object *headCommit = nullptr;

  // Lookup HEAD commit
  int error = git_revparse_single(&headCommit, repo, "HEAD");
  if (error) {
    setError(GitError::UnstageFilesFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList unixFiles;

  for (const auto &filePath : files) {
    unixFiles += QDir::fromNativeSeparators(filePath);
  }

  git_strarray arr = qstringlist_to_git_strarray(unixFiles);

  // Reset index entries for selected paths to match HEAD
  error = git_reset_default(repo, headCommit, &arr);
  if (error) {
    setError(GitError::UnstageFilesFailedToResetPathsToHead, error);
    git_object_free(headCommit);
    git_strarray_destroy(arr);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(headCommit);
  git_strarray_destroy(arr);

  LOG_PERF(0, "Exit");

  return true;
}

bool commitStaged(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Creating commit: summary='{}', author='{}', email='{}'",
            summary.toStdString(), author.toStdString(), email.toStdString());
  LOG_DEBUG("Message length: {}, cleanup: {}, signoff: {}", message.length(),
            cleanup, signoff);

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Preparing commit: loading index and building tree");
  git_index *index = nullptr;
  git_oid treeId;
  git_oid commitId;
  git_tree *tree = nullptr;
  git_signature *sig = nullptr;
  git_commit *parent = nullptr;
  git_commit *parent2 = nullptr;
  bool isMerge = false;

  // Check if merge is in progress
  if (git_repository_state(repo) == GIT_REPOSITORY_STATE_MERGE) {
    isMerge = true;
    LOG_DEBUG("Merge in progress, will create merge commit with 2 parents");
  }

  // Load index
  int error = git_repository_index(&index, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository index: error={}",
              error);
    setError(GitError::CommitStagedFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write index to tree
  error = git_index_write_tree(&treeId, index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to write index to tree: error={}",
              error);
    setError(GitError::CommitStagedFailedToWriteIndexTree, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeId);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup tree: error={}", error);
    setError(GitError::CommitStagedFailedToLookupTree, error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Tree prepared successfully");

  // Create signature
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CommitStagedFailedToCreateSignature, error);
    git_tree_free(tree);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Lookup HEAD commit (if it exists)
  error = git_repository_head_unborn(repo);
  if (!error) {
    git_reference *headRef = nullptr;
    error = git_repository_head(&headRef, repo);
    if (error) {
      setError(GitError::CommitStagedFailedToGetRepositoryHead, error);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_commit_lookup(&parent, repo, git_reference_target(headRef));
    if (error) {
      setError(GitError::CommitStagedFailedToGetReferenceTarget, error);
      git_reference_free(headRef);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(headRef);

    // If merge in progress, get the MERGE_HEAD as the second parent
    if (isMerge) {
      git_reference *mergeHeadRef = nullptr;
      error = git_reference_dwim(&mergeHeadRef, repo, "MERGE_HEAD");
      if (error) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Failed to read MERGE_HEAD during merge: error={}", error);
        setError(GitError::CommitStagedFailedToResolveMergeHeadReference,
                 error);
        git_commit_free(parent);
        git_signature_free(sig);
        git_tree_free(tree);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      error =
          git_commit_lookup(&parent2, repo, git_reference_target(mergeHeadRef));
      if (error) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Failed to lookup MERGE_HEAD commit: error={}", error);
        setError(GitError::CommitStagedFailedToGetSecondReferenceTarget, error);
        git_reference_free(mergeHeadRef);
        git_commit_free(parent);
        git_signature_free(sig);
        git_tree_free(tree);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_reference_free(mergeHeadRef);
      LOG_DEBUG("Successfully loaded MERGE_HEAD as second parent");
    }
  }

  const QString fullMessage =
      buildCommitMessage(summary, message, author, email, signoff);

  // Create commit with appropriate number of parents
  size_t parentCount = parent ? 1 : 0;
  if (isMerge && parent2) {
    parentCount = 2;
    LOG_DEBUG("Creating merge commit with 2 parents");
  }

  CommitSigningConfig signingConfig;
  QString signingError;
  bool signingConfigLoaded = true;
  if (signCommit) {
    signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::CommitStagedFailedToLoadSigningConfiguration, -1);

      if (parent) {
        git_commit_free(parent);
      }
      if (parent2) {
        git_commit_free(parent2);
      }

      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);

      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const bool doSigning = signCommit && signingConfig.enabled;
  if (doSigning) {
    CommitSigningPayload signingPayload;
    signingPayload.config = signingConfig;
    signingPayload.errorMessage = &signingError;

    git_commit_create_options commitOpts = GIT_COMMIT_CREATE_OPTIONS_INIT;
    commitOpts.allow_empty_commit = 1;
    commitOpts.author = sig;
    commitOpts.committer = sig;
    commitOpts.sign = commitSigningCallback;
    commitOpts.payload = &signingPayload;

    error = git_commit_create_from_stage(
        &commitId, repo, fullMessage.toStdString().c_str(), &commitOpts);
    if (error) {
      if (!signingError.isEmpty()) {
        LOG_ERROR(Log::ErrorId::Git, "Failed to sign commit: {}",
                  signingError.toStdString());
        setError(GitError::CommitStagedFailedToSignCommit, -1);
      } else {
        setError(GitError::CommitStagedFailedToCreateCommitFromStagedChanges,
                 error);
      }

      if (parent) {
        git_commit_free(parent);
      }
      if (parent2) {
        git_commit_free(parent2);
      }

      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);

      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    error = git_commit_create_v(&commitId, repo, "HEAD", sig, sig, nullptr,
                                fullMessage.toStdString().c_str(), tree,
                                parentCount, parent, parent2);
    if (error) {
      setError(GitError::CommitStagedFailedToCreateCommit, error);

      if (parent) {
        git_commit_free(parent);
      }
      if (parent2) {
        git_commit_free(parent2);
      }

      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);

      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (parent) {
    git_commit_free(parent);
  }
  if (parent2) {
    git_commit_free(parent2);
  }

  git_signature_free(sig);
  git_tree_free(tree);
  git_index_free(index);

  if (cleanup) {
    error = git_repository_state_cleanup(repo);
    if (error) {
      setError(GitError::CommitStagedFailedToCleanupRepositoryState, error);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");

  return true;
}

static QString createFullPatchFromContent(const QString &filePath,
                                          const QString &fileContent,
                                          bool newFile,
                                          const QString &indexLine) {
  LOG_PERF(0, "Enter");

  const QString convertedContent = QString(fileContent).replace("\r\n", "\n");
  QStringList lines = convertedContent.split('\n');
  if (!convertedContent.isEmpty() && convertedContent.endsWith('\n')) {
    lines.removeLast();
  }
  const int lineCount = lines.size();

  QString patch;
  QTextStream stream(&patch);

  if (!indexLine.isEmpty()) {
    stream << indexLine << "\n";
  }

  if (newFile) {
    stream << "--- /dev/null\n";
  } else {
    stream << "--- a/" << filePath << "\n";
  }

  stream << "+++ b/" << filePath << "\n";
  stream << "@@ -1,0 +1," << lineCount << " @@\n";

  for (const QString &line : lines) {
    stream << "+" << line << "\n";
  }

  LOG_PERF(0, "Exit");

  return patch;
}

static QString createIndexLineFromContent(const QString &oldFileContent,
                                          const QString &newFileContent) {

  if (oldFileContent.isEmpty() && newFileContent.isEmpty()) {
    return {};
  }

  if (oldFileContent.isEmpty()) {
    const QString normalizedNewContent =
        QString(newFileContent).replace("\r\n", "\n");

    git_oid newOid;

    const QByteArray newBytes = normalizedNewContent.toUtf8();

    git_odb_hash(&newOid, newBytes.constData(),
                 static_cast<size_t>(newBytes.size()), GIT_OBJECT_BLOB);

    char newOidStr[GIT_OID_HEXSZ + 1] = {0};

    git_oid_tostr(newOidStr, sizeof(newOidStr), &newOid);

    return QString("index 0000000..%2")
        .arg(QString::fromUtf8(newOidStr).left(7));
  }

  if (newFileContent.isEmpty()) {
    const QString normalizedOldContent =
        QString(oldFileContent).replace("\r\n", "\n");

    git_oid oldOid;

    const QByteArray oldBytes = normalizedOldContent.toUtf8();

    git_odb_hash(&oldOid, oldBytes.constData(),
                 static_cast<size_t>(oldBytes.size()), GIT_OBJECT_BLOB);

    char oldOidStr[GIT_OID_HEXSZ + 1] = {0};

    git_oid_tostr(oldOidStr, sizeof(oldOidStr), &oldOid);

    return QString("index %1..0000000")
        .arg(QString::fromUtf8(oldOidStr).left(7));
  }

  const QString normalizedOldContent =
      QString(oldFileContent).replace("\r\n", "\n");
  const QString normalizedNewContent =
      QString(newFileContent).replace("\r\n", "\n");

  git_oid oldOid;
  git_oid newOid;

  const QByteArray oldBytes = normalizedOldContent.toUtf8();
  const QByteArray newBytes = normalizedNewContent.toUtf8();

  git_odb_hash(&oldOid, oldBytes.constData(),
               static_cast<size_t>(oldBytes.size()), GIT_OBJECT_BLOB);
  git_odb_hash(&newOid, newBytes.constData(),
               static_cast<size_t>(newBytes.size()), GIT_OBJECT_BLOB);

  char oldOidStr[GIT_OID_HEXSZ + 1] = {0};
  char newOidStr[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oldOidStr, sizeof(oldOidStr), &oldOid);
  git_oid_tostr(newOidStr, sizeof(newOidStr), &newOid);

  return QString("index %1..%2")
      .arg(QString::fromUtf8(oldOidStr).left(7),
           QString::fromUtf8(newOidStr).left(7));
}

static FilesDiffDelta
createFullPatchDeltasFromContent(const QString &filePath,
                                 const QString &fileContent) {
  LOG_PERF(0, "Enter");

  const QString convertedContent = QString(fileContent).replace("\r\n", "\n");
  QStringList lines = convertedContent.split('\n');
  if (!convertedContent.isEmpty() && convertedContent.endsWith('\n')) {
    lines.removeLast();
  }

  FilesDiffDelta delta;
  delta.oldFilePath = filePath;
  delta.newFilePath = filePath;

  FilesDiffHunk hunk;
  hunk.oldStart = 1;
  hunk.newStart = 1;
  hunk.oldLines = 0;
  hunk.newLines = lines.size();

  for (const QString &line : lines) {
    FilesDiffLine diffLine;
    diffLine.origin = '+';
    diffLine.content = line;
    hunk.lines.append(diffLine);
  }

  delta.hunks.append(hunk);

  LOG_PERF(0, "Exit");

  return delta;
}

FilesDiff diffFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_object *headCommit = nullptr;
  git_commit *commit = nullptr;
  git_tree *headTree = nullptr;

  int error = git_revparse_single(&headCommit, repo, "HEAD");
  if (error) {
    setError(GitError::DiffFilesFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_commit_lookup(&commit, repo, git_object_id(headCommit));
  if (error) {
    setError(GitError::DiffFilesFailedToGetHeadObjectId, error);
    git_object_free(headCommit);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_commit_tree(&headTree, commit);
  if (error) {
    setError(GitError::DiffFilesFailedToGetCommitTree, error);
    git_commit_free(commit);
    git_object_free(headCommit);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit_free(commit);
  git_object_free(headCommit);

  git_strarray arr = qstringlist_to_git_strarray(files);

  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.pathspec = arr;

  git_diff *diff = nullptr;
  error = git_diff_tree_to_workdir(&diff, repo, headTree, &opts);
  if (error) {
    setError(GitError::DiffFilesFailedToDiffTreeAgainstWorkdir, error);
    git_tree_free(headTree);
    git_strarray_destroy(arr);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree_free(headTree);
  git_strarray_destroy(arr);

  QString diffOutput;

  QVector<FilesDiffDelta> filesDeltas;

  size_t num_deltas = git_diff_num_deltas(diff);
  for (size_t i = 0; i < num_deltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(diff, i);
    diffOutput += QString("diff --git a/%1 b/%2\n")
                      .arg(delta->old_file.path, delta->new_file.path);

    FilesDiffDelta filesDiffDelta;
    filesDiffDelta.oldFilePath = delta->old_file.path;
    filesDiffDelta.newFilePath = delta->new_file.path;

    git_patch *patch;
    error = git_patch_from_diff(&patch, diff, i);
    if (error) {
      setError(GitError::DiffFilesFailedToCreatePatchFromDiff, error);
      continue;
    }

    const size_t num_hunks = git_patch_num_hunks(patch);
    for (size_t h = 0; h < num_hunks; ++h) {
      const git_diff_hunk *hunk;
      error = git_patch_get_hunk(&hunk, NULL, patch, h);
      if (error) {
        setError(GitError::DiffFilesFailedToReadPatchHunk, error);
        continue;
      }

      diffOutput += QString("@@ -%1,%2 +%3,%4 @@\n")
                        .arg(hunk->old_start)
                        .arg(hunk->old_lines)
                        .arg(hunk->new_start)
                        .arg(hunk->new_lines);

      FilesDiffHunk filesDiffHunk;
      filesDiffHunk.oldStart = hunk->old_start;
      filesDiffHunk.oldLines = hunk->old_lines;
      filesDiffHunk.newStart = hunk->new_start;
      filesDiffHunk.newLines = hunk->new_lines;

      size_t num_lines = git_patch_num_lines_in_hunk(patch, h);
      for (size_t l = 0; l < num_lines; ++l) {
        const git_diff_line *line;
        error = git_patch_get_line_in_hunk(&line, patch, h, l);
        if (error) {
          setError(GitError::DiffFilesFailedToReadPatchLine, error);
          continue;
        }

        const char *start = line->content;
        const char *end = start + line->content_len;
        const char *newline =
            static_cast<const char *>(memchr(start, '\n', end - start));
        if (!newline) {
          newline = end;
        }

        const QString lineContent = QString::fromUtf8(start, newline - start);

        diffOutput += QString("%1%2\n").arg(line->origin, lineContent);

        FilesDiffLine filesDiffLine;
        filesDiffLine.origin = line->origin;
        filesDiffLine.content = lineContent;

        filesDiffHunk.lines += filesDiffLine;
      }

      filesDiffDelta.hunks += filesDiffHunk;
    }

    git_patch_free(patch);

    filesDeltas += filesDiffDelta;
  }

  git_diff_free(diff);

  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    setError(GitError::DiffFilesFailedToGetRepositoryHead, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  headCommit = nullptr;

  error = git_reference_peel(&headCommit, headRef, GIT_OBJECT_COMMIT);
  if (error) {
    setError(GitError::DiffFilesFailedToPeelReference, error);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error =
      git_commit_tree(&tree, reinterpret_cast<const git_commit *>(headCommit));
  if (error) {
    setError(GitError::DiffFilesFailedToReadHeadCommit, error);
    git_reference_free(headRef);
    git_commit_free(reinterpret_cast<git_commit *>(headCommit));
    LOG_PERF(0, "Exit");
    return {};
  }

  const ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong maxBlobSize =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;

  for (const auto &file : files) {
    const QString repoPath = git_repository_workdir(repo);
    const QString filePath = QDir::fromNativeSeparators(file);
    const QString fullPath = repoPath + filePath;

    QFile fileObj(fullPath);
    if (fileObj.open(QIODevice::ReadOnly)) {
      const QByteArray content = fileObj.readAll();
      filesContent.insert(file, QString::fromUtf8(content));
      fileObj.close();
    } else {
      filesContent.insert(file, QString());
    }

    git_tree_entry *entry = nullptr;

    error = git_tree_entry_bypath(&entry, tree, filePath.toStdString().c_str());
    if (error) {
      setError(GitError::DiffFilesFailedToLookupTreeEntryByPath, error);
      repoContent.insert(file, QString());
      continue;
    }

    const git_oid *fullOid = git_tree_entry_id(entry);
    if (!fullOid) {
      setError(GitError::DiffFilesFailedToReadTreeEntryId, error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    git_blob *blob = nullptr;
    error = git_blob_lookup_prefix(&blob, repo, fullOid, 7);
    if (error) {
      setError(GitError::DiffFilesFailedToLookupBlobByPrefix, error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    const char *blobContent =
        static_cast<const char *>(git_blob_rawcontent(blob));
    const size_t blobSize = git_blob_rawsize(blob);
    if (blobSize > static_cast<size_t>(maxBlobSize)) {
      // Blob too large to include in repo content
      repoContent.insert(
          file,
          QString("[[FILE SIZE EXCEEDS LIMIT OF %1 BYTES]]").arg(maxBlobSize));
      git_blob_free(blob);
      git_tree_entry_free(entry);
      continue;
    }

    repoContent.insert(file, QString::fromUtf8(blobContent, blobSize));

    git_blob_free(blob);
    git_tree_entry_free(entry);
  }

  git_tree_free(tree);
  git_commit_free(reinterpret_cast<git_commit *>(headCommit));
  git_reference_free(headRef);

  for (const auto &file : files) {
    const bool newFile = repoContent.value(file).isEmpty();
    if (!newFile) {
      continue;
    }

    const QString indexLine =
        createIndexLineFromContent(repoContent.value(file), filesContent[file]);
    const QString patch = createFullPatchFromContent(file, filesContent[file],
                                                     newFile, indexLine);

    diffOutput += QString("diff --git a/%1 b/%2\n").arg(file, file);
    diffOutput += "new file mode 100644\n";
    diffOutput += patch;

    const FilesDiffDelta patchDeltas =
        createFullPatchDeltasFromContent(file, filesContent[file]);
    filesDeltas += patchDeltas;
  }

  FilesDiff filesDiff;
  filesDiff.diff = diffOutput;
  filesDiff.filesContent = filesContent;
  filesDiff.repoContent = repoContent;
  filesDiff.deltas = filesDeltas;
  filesDiff.valid = true;

  LOG_PERF(0, "Exit");

  return filesDiff;
}

bool discardFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  for (const auto &file : files) {
    git_object *headTree = nullptr;
    int error = git_revparse_single(&headTree, repo, "HEAD^{tree}");
    if (error) {
      setError(GitError::DiscardFilesFailedToResolveRevision, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_strarray arr = qstringlist_to_git_strarray(QStringList() << file);

    // 1. Reset working directory to HEAD
    git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
    checkoutOpts.checkout_strategy =
        GIT_CHECKOUT_FORCE | GIT_CHECKOUT_DISABLE_PATHSPEC_MATCH;
    checkoutOpts.paths = arr;

    error = git_checkout_tree(repo, headTree, &checkoutOpts);
    if (error) {
      setError(GitError::DiscardFilesFailedToCheckoutTree, error);
      git_strarray_destroy(arr);
      git_object_free(headTree);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_strarray_destroy(arr);
    git_object_free(headTree);

    // 2. Reset index entry to HEAD
    git_index *index = nullptr;
    error = git_repository_index(&index, repo);
    if (error) {
      setError(GitError::DiscardFilesFailedToGetRepositoryIndex, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object *headTree2 = nullptr;
    error = git_revparse_single(&headTree2, repo, "HEAD^{tree}");
    if (error) {
      setError(GitError::DiscardFilesFailedToResolveHeadRevision, error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_tree *tree = nullptr;
    error = git_tree_lookup(&tree, repo, git_object_id(headTree2));
    if (error) {
      setError(GitError::DiscardFilesFailedToGetHeadTreeObjectId, error);
      git_object_free(headTree2);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    const std::string path = file.toStdString();

    git_tree_entry *entry = nullptr;
    error = git_tree_entry_bypath(&entry, tree, path.c_str());
    if (error) {
      setError(GitError::DiscardFilesFailedToLookupTreeEntryByPath, error);
      git_tree_free(tree);
      git_object_free(headTree2);
      git_index_free(index);
      continue;
    }

    if (entry) {
      git_blob *blob = nullptr;
      error = git_blob_lookup(&blob, repo, git_tree_entry_id(entry));
      if (error) {
        setError(GitError::DiscardFilesFailedToReadTreeEntryId, error);
        git_tree_entry_free(entry);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      const QString repoPath = git_repository_workdir(repo);
      const QString fullPath =
          QDir::fromNativeSeparators(repoPath + QString::fromStdString(path));

      const QString directoryPath = QFileInfo(fullPath).path();

      QDir fileDir;
      fileDir.mkpath(directoryPath);

      QFile file(fullPath);
      if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        LOG_ERROR(Log::ErrorId::Git, "Can't open file \"{}\" for writing",
                  file.fileName());
        git_blob_free(blob);
        git_tree_entry_free(entry);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      file.write(static_cast<const char *>(git_blob_rawcontent(blob)),
                 git_blob_rawsize(blob));
      file.close();

      git_blob_free(blob);

      // Capture entry data before freeing
      git_index_entry newEntry = {};
      newEntry.mode = git_tree_entry_filemode(entry);
      newEntry.id = *git_tree_entry_id(entry);
      newEntry.path = path.c_str();

      git_tree_entry_free(entry);

      error = git_index_remove(index, path.c_str(), 0); // Remove old entry
      if (error) {
        setError(GitError::DiscardFilesFailedToRemoveIndexEntry, error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }

      error = git_index_add(index, &newEntry); // Add fresh entry
      if (error) {
        setError(GitError::DiscardFilesFailedToAddIndexEntry, error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    } else {
      error = git_index_remove(index, path.c_str(), 0); // Remove if not in HEAD
      if (error) {
        setError(GitError::DiscardFilesFailedToRemoveDeletedIndexEntry, error);
        git_tree_free(tree);
        git_object_free(headTree2);
        git_index_free(index);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    git_tree_free(tree);
    git_object_free(headTree2);

    error = git_index_write(index);
    if (error) {
      setError(GitError::DiscardFilesFailedToWriteIndex, error);
      git_index_free(index);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_index_free(index);
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool pull(git_repository *repo, const QString &activeBranchName, bool rebase) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting {} operation for active branch: '{}'",
            rebase ? "pull with rebase" : "pull",
            activeBranchName.isEmpty() ? "(none)"
                                       : activeBranchName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // If no active branch provided, get current branch from HEAD
  QString targetBranchName = activeBranchName;
  if (targetBranchName.isEmpty()) {
    git_reference *head = nullptr;
    int error = git_repository_head(&head, repo);
    if (error) {
      setError(GitError::PullFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchNameC = nullptr;
    error = git_branch_name(&branchNameC, head);
    if (error != 0 || !branchNameC) {
      setError(GitError::PullFailedToReadBranchName, error);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    targetBranchName = QString::fromUtf8(branchNameC);
    git_reference_free(head);
  }

  LOG_DEBUG("Target branch for pull: '{}'", targetBranchName.toStdString());

  // Get upstream for the active branch
  auto [upstreamSuccess, remoteName, remoteBranchName] =
      getUpstream(repo, targetBranchName);
  if (!upstreamSuccess || remoteName.isEmpty()) {
    setError(GitError::PullNoUpstreamConfigured, GIT_ENOTFOUND);
    LOG_DEBUG("Branch '{}' has no upstream configured",
              targetBranchName.toStdString());
    LOG_PERF(0, "Exit");
    return false; // Nothing to pull, but not an error
  }

  LOG_DEBUG("Branch '{}' will be pulled from {}/{}",
            targetBranchName.toStdString(), remoteName.toStdString(),
            remoteBranchName.toStdString());

  // Fetch from the upstream remote
  const bool commercial = isCommercialPlan();
  int error = 0;

  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::PullFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *url = git_remote_url(remote);
  LOG_DEBUG("Fetching from remote '{}' at URL: '{}'", remoteName.toStdString(),
            url);

  bool gitSchema = false;
  int retries = 0;
  const QString urlStr = QString::fromUtf8(url);
  if (urlStr.startsWith("git@") || urlStr.startsWith("git://")) {
    gitSchema = true;
    LOG_DEBUG("Using git schema for authentication");
  }

  int maxRetries = MAX_RETRIES;
  if (gitSchema) {
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(urlStr);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1;
        LOG_DEBUG("SSH key configured for host '{}', setting max retries to 1",
                  host.toStdString());
      }
    }
  }

  if (!commercial && gitSchema) {
    setError(GitError::PullGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Pulling from git@ repositories is not available on the free "
              "plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString fetchUuid = Uuid::createUuid();

  CloneProgress progressData;
  progressData.uuid = fetchUuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.uuid = fetchUuid;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  // Fetch all heads and tags
  git_strarray refspecs = qstringlist_to_git_strarray(
      QStringList()
      << QString("+refs/heads/*:refs/remotes/%1/*").arg(remoteName)
      << "+refs/tags/*:refs/tags/*");

  // Fetch URL
  const char *fetchUrl = git_remote_url(remote);

  if (fetchUrl) {
    LOG_INFO("Fetch URL: {}", fetchUrl);
  }

  // Push URL (may be NULL if not set separately)
  const char *pushUrl = git_remote_pushurl(remote);

  if (pushUrl) {
    LOG_INFO("Push URL: {}", pushUrl);
  }

  do {
    error = git_remote_fetch(remote, &refspecs, &fetchOpts, nullptr);

    if (error) {
      setError(GitError::PullFailedToFetchRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(fetchUuid, error != 0, false);

  if (commercial) {
    finishCredentialsAcquiring(urlStr);
  }

  if (error) {
    setError(GitError::PullFailedToFetchRemoteWithCredentials, error);
    git_strarray_destroy(refspecs);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  // Merge/rebase the upstream branch into the active branch
  git_reference *head = nullptr;
  error = git_repository_head(&head, repo);
  if (error) {
    setError(GitError::PullFailedToGetRepositoryHeadAfterFetch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Resolving upstream branch {}/{} for current branch '{}'",
            remoteName.toStdString(), remoteBranchName.toStdString(),
            targetBranchName.toStdString());

  // Get upstream tracking reference
  git_reference *upstream = nullptr;
  const QString upstreamRefName =
      QString("refs/remotes/%1/%2").arg(remoteName, remoteBranchName);
  error = git_reference_lookup(&upstream, repo,
                               upstreamRefName.toStdString().c_str());
  if (error != 0) {
    LOG_DEBUG("Failed to lookup upstream reference '{}'",
              upstreamRefName.toStdString());
    setError(GitError::PullFailedToLookupReference, error);
    git_reference_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get annotated commit from upstream
  git_annotated_commit *upstreamCommit = nullptr;
  error = git_annotated_commit_from_ref(&upstreamCommit, repo, upstream);
  if (error) {
    setError(GitError::PullFailedToCreateAnnotatedCommitFromReference, error);
    git_reference_free(upstream);
    git_reference_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool result = false;

  if (rebase) {
    git_rebase_options rebaseOpts = GIT_REBASE_OPTIONS_INIT;
    rebaseOpts.checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;

    git_signature *signature = nullptr;
    error = git_signature_default(&signature, repo);
    if (error) {
      setError(GitError::PullFailedToGetDefaultSignature, error);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_rebase *rebaseSession = nullptr;
    error = git_rebase_init(&rebaseSession, repo, nullptr, upstreamCommit,
                            nullptr, &rebaseOpts);
    if (error) {
      setError(GitError::PullFailedToStartRebase, error);
      git_signature_free(signature);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    bool rebaseSuccess = true;
    git_rebase_operation *operation = nullptr;
    git_oid commitId{};

    while ((error = git_rebase_next(&operation, rebaseSession)) == 0) {
      error = git_rebase_commit(&commitId, rebaseSession, nullptr, signature,
                                nullptr, nullptr);
      if (error == 0) {
        continue;
      }

      if (error == GIT_EUNMERGED) {
        LOG_ERROR(Log::ErrorId::Git,
                  "Rebase stopped because conflicts must be resolved manually");
      } else {
        setError(GitError::PullFailedToCommitRebaseStep, error);
      }
      git_rebase_abort(rebaseSession);
      rebaseSuccess = false;
      break;
    }

    if (rebaseSuccess && error == GIT_ITEROVER) {
      error = git_rebase_finish(rebaseSession, signature);
      if (error) {
        setError(GitError::PullFailedToFinishRebase, error);
        git_rebase_abort(rebaseSession);
        rebaseSuccess = false;
      }
    } else if (rebaseSuccess) {
      setError(GitError::PullFailedToAbortRebase, error);
      rebaseSuccess = false;
    }

    git_rebase_free(rebaseSession);
    git_signature_free(signature);
    result = rebaseSuccess;
  } else {
    // Analyze merge
    git_merge_analysis_t analysis;
    git_merge_preference_t preference;

    error = git_merge_analysis(
        &analysis, &preference, repo,
        const_cast<const git_annotated_commit **>(&upstreamCommit), 1);
    if (error) {
      setError(GitError::PullFailedToCreateAnnotatedCommit, error);
      git_annotated_commit_free(upstreamCommit);
      git_reference_free(upstream);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
      // Fast-forward
      const git_oid *targetOid = git_annotated_commit_id(upstreamCommit);
      if (!targetOid) {
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_ERROR(Log::ErrorId::Git, "Can't get target OID for fast-forward");
        LOG_PERF(0, "Exit");
        return false;
      }

      git_commit *targetCommit = nullptr;
      error = git_commit_lookup(&targetCommit, repo, targetOid);
      if (error) {
        setError(GitError::PullFailedToLookupCommit, error);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
      checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

      error =
          git_checkout_tree(repo, (git_object *)targetCommit, &checkout_opts);
      if (error) {
        setError(GitError::PullFailedToPeelFetchedTargetCommit, error);
        git_commit_free(targetCommit);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_reference *newHead = nullptr;
      error =
          git_reference_set_target(&newHead, head, targetOid, "Fast-forward");
      if (error) {
        setError(GitError::PullFailedToSetReferenceTarget, error);
        git_commit_free(targetCommit);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      git_reference_free(newHead);
      git_commit_free(targetCommit);
      result = true;
    } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
      // Normal merge
      git_merge_options merge_opts = GIT_MERGE_OPTIONS_INIT;
      git_checkout_options checkout_opts = GIT_CHECKOUT_OPTIONS_INIT;
      checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

      error = git_merge(
          repo, const_cast<const git_annotated_commit **>(&upstreamCommit), 1,
          &merge_opts, &checkout_opts);
      if (error) {
        setError(GitError::PullFailedToCreateMergeAnnotatedCommit, error);
        git_annotated_commit_free(upstreamCommit);
        git_reference_free(upstream);
        git_reference_free(head);
        LOG_PERF(0, "Exit");
        return false;
      }

      result = true;
    } else {
      result = true;
    }
  }

  git_annotated_commit_free(upstreamCommit);
  git_reference_free(upstream);
  git_reference_free(head);

  LOG_PERF(0, "Exit");
  return result;
}

static bool fetchRemoteInternal(git_repository *repo,
                                const QString &remoteName) {
  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    return false;
  }

  if (remoteName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name is empty");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_remote *remote = nullptr;
  int error =
      git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::FetchRemoteInternalFailedToLookupRemote, error);
    return false;
  }

  const char *url = git_remote_url(remote);
  LOG_DEBUG("Fetching from remote '{}' at URL: '{}'", remoteName.toStdString(),
            url);

  bool gitSchema = false;
  const QString urlStr = QString::fromUtf8(url);
  if (urlStr.startsWith("git@") || urlStr.startsWith("git://")) {
    gitSchema = true;
    LOG_DEBUG("Using git schema for authentication");
  }

  int maxRetries = MAX_RETRIES;
  if (gitSchema) {
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(urlStr);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1;
        LOG_DEBUG("SSH key configured for host '{}', setting max retries to 1",
                  host.toStdString());
      }
    }
  }

  const bool commercial = isCommercialPlan();
  if (!commercial && gitSchema) {
    setError(GitError::FetchRemoteGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Fetching from git@ repositories is not available on the free "
              "plan.");
    git_remote_free(remote);
    return false;
  }

  const QString fetchUuid = Uuid::createUuid();

  CloneProgress progressData;
  progressData.uuid = fetchUuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.uuid = fetchUuid;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  // Fetch all heads and tags
  git_strarray refspecs = qstringlist_to_git_strarray(
      QStringList()
      << QString("+refs/heads/*:refs/remotes/%1/*").arg(remoteName)
      << "+refs/tags/*:refs/tags/*");

  int retries = 0;
  error = 0;
  do {
    error = git_remote_fetch(remote, &refspecs, &fetchOpts, nullptr);

    if (error) {
      setError(GitError::FetchRemoteInternalFailedToFetchRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(fetchUuid, error != 0, false);

  if (commercial) {
    finishCredentialsAcquiring(urlStr);
  }

  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to fetch from remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::FetchRemoteInternalFailedToFetchRemoteWithCredentials,
             error);
    git_strarray_destroy(refspecs);
    git_remote_free(remote);
    return false;
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);
  return true;
}

bool fetchRemote(git_repository *repo, const QString &remoteName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting fetch operation for remote: '{}'",
            remoteName.toStdString());

  const bool success = fetchRemoteInternal(repo, remoteName);

  LOG_PERF(0, "Exit");
  return success;
}

bool fetchAll(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting fetch all operation");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Fetch every configured remote, not just remotes referenced by local
  // branch upstreams. This keeps Fetch All aligned with the UI expectation
  // that all remotes are refreshed.
  const auto remotes = listRemotes(repo);
  if (remotes.isEmpty()) {
    LOG_DEBUG("No remotes found in repository");
    LOG_PERF(0, "Exit");
    return true;
  }

  QSet<QString> remotesToFetch;
  for (const auto &remote : remotes) {
    if (remote.name.isEmpty()) {
      continue;
    }

    remotesToFetch.insert(remote.name);
    LOG_DEBUG("Adding remote '{}' to fetch list", remote.name.toStdString());
  }

  if (remotesToFetch.isEmpty()) {
    LOG_DEBUG("No fetchable remotes found");
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_DEBUG("Found {} unique remotes to fetch", remotesToFetch.size());

  for (const QString &remoteName : remotesToFetch) {
    if (!fetchRemoteInternal(repo, remoteName)) {
      continue;
    }
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool push(git_repository *repo, const QString &activeBranchName, bool force) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting {}push operation for active branch: '{}'",
            force ? "force " : "",
            activeBranchName.isEmpty() ? "(none)"
                                       : activeBranchName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // If no active branch provided, get current branch from HEAD
  QString targetBranchName = activeBranchName;
  if (targetBranchName.isEmpty()) {
    git_reference *head = nullptr;
    int error = git_repository_head(&head, repo);
    if (error) {
      setError(GitError::PushFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchNameC = nullptr;
    error = git_branch_name(&branchNameC, head);
    if (error != 0 || !branchNameC) {
      setError(GitError::PushFailedToReadBranchName, error);
      git_reference_free(head);
      LOG_PERF(0, "Exit");
      return false;
    }

    targetBranchName = QString::fromUtf8(branchNameC);
    git_reference_free(head);
  }

  LOG_DEBUG("Target branch for push: '{}'", targetBranchName.toStdString());

  // Get upstream for the active branch
  auto [upstreamSuccess, remoteName, remoteBranchName] =
      getUpstream(repo, targetBranchName);
  if (!upstreamSuccess || remoteName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Branch '{}' has no remote configured. Specify a remote for this "
              "branch before pushing.",
              targetBranchName.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Branch '{}' will be pushed to {}/{}",
            targetBranchName.toStdString(), remoteName.toStdString(),
            remoteBranchName.toStdString());

  const QString localRefName =
      QStringLiteral("refs/heads/%1").arg(targetBranchName);
  const QString upstreamRefName =
      QStringLiteral("refs/remotes/%1/%2").arg(remoteName, remoteBranchName);
  git_oid localRefOid = {};
  git_oid upstreamRefOid = {};
  char localOidStr[GIT_OID_HEXSZ + 1] = {0};
  char upstreamOidStr[GIT_OID_HEXSZ + 1] = {0};

  if (git_reference_name_to_id(&localRefOid, repo,
                               localRefName.toStdString().c_str()) == 0) {
    git_oid_tostr(localOidStr, GIT_OID_HEXSZ + 1, &localRefOid);
  }
  if (git_reference_name_to_id(&upstreamRefOid, repo,
                               upstreamRefName.toStdString().c_str()) == 0) {
    git_oid_tostr(upstreamOidStr, GIT_OID_HEXSZ + 1, &upstreamRefOid);
  }

  LOG_DEBUG("Push ref state: local='{}' ({}) upstream='{}' ({})",
            localRefName.toStdString(),
            localOidStr[0] ? localOidStr : "unresolved",
            upstreamRefName.toStdString(),
            upstreamOidStr[0] ? upstreamOidStr : "unresolved");

  // Push to the upstream remote
  const bool commercial = isCommercialPlan();
  int error = 0;

  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::PushFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *url = git_remote_url(remote);
  LOG_DEBUG("Pushing to remote '{}' at URL: '{}'", remoteName.toStdString(),
            url);

  bool gitSchema = false;
  int retries = 0;
  const QString urlStr = QString::fromUtf8(url);
  if (urlStr.startsWith("git@") || urlStr.startsWith("git://")) {
    gitSchema = true;
    LOG_DEBUG("Using git schema for authentication");
  }

  int maxRetries = MAX_RETRIES;
  if (gitSchema) {
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(urlStr);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1;
        LOG_DEBUG("SSH key configured for host '{}', setting max retries to 1",
                  host.toStdString());
      }
    }
  }

  if (!commercial && gitSchema) {
    setError(GitError::PushGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Pushing to git@ repositories is not available on the free "
              "plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!GitRepo::Lfs::uploadLocalObjects(
          repo, repoPath, remoteName, urlStr,
          QStringLiteral("refs/heads/%1").arg(targetBranchName))) {
    setError(GitError::PushFailedToPushRemote, GIT_EUSER);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString pushUuid = Uuid::createUuid();

  CloneProgress progressData;
  progressData.uuid = pushUuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.uuid = pushUuid;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  gPushReferenceStatus = {};

  git_push_options pushOpts;
  git_push_options_init(&pushOpts, GIT_PUSH_OPTIONS_VERSION);
  pushOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  pushOpts.callbacks.transfer_progress = &fetchProgressCallback;
  pushOpts.callbacks.sideband_progress = &pushSidebandProgressCallback;
  pushOpts.callbacks.certificate_check = &certificateCheckCallback;
  pushOpts.callbacks.push_update_reference = &pushUpdateReferenceCallback;
  pushOpts.callbacks.payload = &callbackPayload;

  // Build refspec for pushing the branch to its upstream
  const QString refspec = QString("%1refs/heads/%2:refs/heads/%3")
                              .arg(force ? QStringLiteral("+") : QString(),
                                   targetBranchName, remoteBranchName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);
  LOG_DEBUG("Using push refspec: '{}'", refspec.toStdString());
  LOG_DEBUG("Push source commit SHA: '{}'", localOidStr);

  do {
    gPushReferenceStatus = {};
    error = git_remote_push(remote, &refspecs, &pushOpts);

    if (error == 0 && gPushReferenceStatus.rejected) {
      LOG_ERROR(Log::ErrorId::Git, "Push rejected by remote for ref '{}': {}",
                gPushReferenceStatus.refname.toStdString(),
                gPushReferenceStatus.status.toStdString());
      error = GIT_EUSER;
      git_error_set_str(GIT_ERROR_NET,
                        gPushReferenceStatus.status.toUtf8().constData());
    }

    if (error) {
      setError(GitError::PushFailedToPushRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(pushUuid, error != 0, false);

  if (commercial) {
    finishCredentialsAcquiring(urlStr);
  }

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_PERF(0, "Exit");

  return error == 0;
}

bool pushTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push tag operation for tag: '{}'", tagName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // Verify tag exists
  git_reference *tagRef = nullptr;
  const QString tagRefName = QString("refs/tags/%1").arg(tagName);
  int error =
      git_reference_lookup(&tagRef, repo, tagRefName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Tag '{}' not found: error={}",
              tagName.toStdString(), error);
    setError(GitError::PushTagFailedToLookupReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  git_reference_free(tagRef);
  LOG_DEBUG("Tag '{}' found", tagName.toStdString());

  git_remote *remote = nullptr;
  const QString defaultRemote = getDefaultRemote(repo);

  error = git_remote_lookup(&remote, repo, defaultRemote.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::PushTagFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::PushTagGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(
        Log::ErrorId::Git,
        "Pushing tags to git@ repositories is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!GitRepo::Lfs::uploadLocalObjects(
          repo, repoPath, defaultRemote, url,
          QStringLiteral("refs/tags/%1").arg(tagName))) {
    setError(GitError::PushTagFailedToPushRemote, GIT_EUSER);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.sideband_progress = &pushSidebandProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.push_update_reference = &pushUpdateReferenceCallback;
  opts.callbacks.payload = &callbackPayload;

  // Push tag with refspec: refs/tags/tagName:refs/tags/tagName
  const QString refspec = QString("refs/tags/%1:refs/tags/%1").arg(tagName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  gPushReferenceStatus = {};

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    error = git_remote_push(remote, &refspecs, &opts);
    if (error) {
      setError(GitError::PushTagFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Push tag operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool pushAllTags(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push all tags operation");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // Get list of all tags
  const QStringList allTags = listTags(repo);
  if (allTags.isEmpty()) {
    LOG_INFO("No tags to push");
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_DEBUG("Found {} tags to push", allTags.size());

  git_remote *remote = nullptr;
  const QString defaultRemote = getDefaultRemote(repo);

  int error =
      git_remote_lookup(&remote, repo, defaultRemote.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::PushAllTagsFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::PushAllTagsGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(
        Log::ErrorId::Git,
        "Pushing tags to git@ repositories is not available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!GitRepo::Lfs::uploadLocalObjects(repo, repoPath, defaultRemote, url)) {
    setError(GitError::PushAllTagsFailedToPushRemote, GIT_EUSER);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.sideband_progress = &pushSidebandProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.push_update_reference = &pushUpdateReferenceCallback;
  opts.callbacks.payload = &callbackPayload;

  // Create refspecs for all tags: refs/tags/tagName:refs/tags/tagName
  QStringList refspecList;
  for (const auto &tag : allTags) {
    refspecList += QString("refs/tags/%1:refs/tags/%1").arg(tag);
  }
  git_strarray refspecs = qstringlist_to_git_strarray(refspecList);

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    error = git_remote_push(remote, &refspecs, &opts);
    if (error) {
      setError(GitError::PushAllTagsFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Push all tags operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool stash(git_repository *repo, const QString &note) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid stashId;
  git_signature *signature = nullptr;

  // Get default signature (author info)
  int error = git_signature_default(&signature, repo);
  if (error) {
    setError(GitError::StashFailedToGetDefaultSignature, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QByteArray noteUtf8 = note.toUtf8();
  error = git_stash_save(
      &stashId,  // Optional: receives the stash commit ID
      repo,      // Your git_repository pointer
      signature, // Signature of the stasher
      noteUtf8.isEmpty() ? nullptr : noteUtf8.constData(),
      GIT_STASH_DEFAULT | GIT_STASH_INCLUDE_UNTRACKED // Flags (can be
      // GIT_STASH_INCLUDE_UNTRACKED, etc.)
  );
  if (error) {
    setError(GitError::StashFailedToCreateStash, error);
    git_signature_free(signature);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(signature);

  LOG_PERF(0, "Exit");

  return true;
}

bool setUpstream(git_repository *repo, const QString &remoteName,
                 const QString &remoteBranchName,
                 const QString &localBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  int error = git_branch_lookup(
      &localRef, repo, localBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::SetUpstreamFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Try to set upstream using libgit2's function
  error = git_branch_set_upstream(
      localRef,
      QString("%1/%2").arg(remoteName, remoteBranchName).toStdString().c_str());

  // If it fails because the remote branch doesn't exist yet, set it manually in
  // config This allows users to set upstream on branches that will be created
  // on the next push
  if (error == GIT_ENOTFOUND || error == GIT_EINVALIDSPEC) {
    LOG_WARN("Remote branch '{}' does not exist, setting upstream directly in "
             "config",
             QString("%1/%2").arg(remoteName, remoteBranchName));

    // Get the git config file path
    const QString configPath =
        QString::fromUtf8(git_repository_path(repo)) + "config";

    // Read the existing config file
    QString configContent;
    QFile configFile(configPath);
    if (configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      configContent = QString::fromUtf8(configFile.readAll());
      configFile.close();
    }

    // Parse and update the config
    QStringList lines = configContent.split('\n');
    bool foundBranchSection = false;
    bool foundRemote = false;
    bool foundMerge = false;
    int sectionStart = -1;
    int sectionEnd = -1;

    // Find the branch section
    for (int i = 0; i < lines.size(); ++i) {
      const QString line = lines[i].trimmed();

      if (line == QString("[branch \"%1\"]").arg(localBranchName)) {
        foundBranchSection = true;
        sectionStart = i;

        // Find the end of this section (next [section] or end of file)
        for (int j = i + 1; j < lines.size(); ++j) {
          const QString nextLine = lines[j].trimmed();
          if (nextLine.startsWith('[')) {
            sectionEnd = j - 1;
            break;
          }
        }
        if (sectionEnd == -1) {
          sectionEnd = lines.size() - 1;
        }

        // Look for remote and merge entries in this section
        for (int j = sectionStart + 1; j <= sectionEnd; ++j) {
          const QString sectionLine = lines[j].trimmed();
          if (sectionLine.startsWith("remote =")) {
            lines[j] = QString("\tremote = %1").arg(remoteName);
            foundRemote = true;
          } else if (sectionLine.startsWith("merge =")) {
            lines[j] = QString("\tmerge = refs/heads/%1").arg(remoteBranchName);
            foundMerge = true;
          }
        }
        break;
      }
    }

    // Add missing entries to the section or create the section
    if (foundBranchSection) {
      // Insert missing entries at the end of the section (before sectionEnd)
      int insertPos = sectionEnd + 1;
      if (!foundRemote) {
        lines.insert(insertPos++, QString("\tremote = %1").arg(remoteName));
      }
      if (!foundMerge) {
        lines.insert(insertPos,
                     QString("\tmerge = refs/heads/%1").arg(remoteBranchName));
      }
    } else {
      // Create new section at the end of the file
      if (!configContent.isEmpty() && !configContent.endsWith('\n')) {
        lines.append(""); // Empty line before new section
      }
      lines.append(QString("[branch \"%1\"]").arg(localBranchName));
      lines.append(QString("\tremote = %1").arg(remoteName));
      lines.append(QString("\tmerge = refs/heads/%1").arg(remoteBranchName));
    }

    // Write back to config file
    configFile.setFileName(configPath);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to write .git/config file when setting upstream");
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    configFile.write(lines.join('\n').toUtf8());
    configFile.close();

    LOG_INFO("Set upstream for branch '{}' to '{}/{}' via config file",
             localBranchName, remoteName, remoteBranchName);

    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return true;
  }

  if (error) {
    setError(GitError::SetUpstreamFailedToSetBranchUpstream, error);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(localRef);

  LOG_PERF(0, "Exit");

  return true;
}

std::tuple<bool, QString, QString> getUpstream(git_repository *repo,
                                               const QString &localBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_reference *localRef = nullptr;
  int error = git_branch_lookup(
      &localRef, repo, localBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::GetUpstreamFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_reference *upstreamRef = nullptr;
  error = git_branch_upstream(&upstreamRef, localRef);

  QString remoteName;
  QString remoteBranchName;

  if (error && error != GIT_ENOTFOUND) {
    setError(GitError::GetUpstreamFailedToGetBranchUpstream, error);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (error != GIT_ENOTFOUND) {
    // Successfully got upstream reference from libgit2
    const QString refName = git_reference_name(upstreamRef);
    LOG_DEBUG("Upstream reference: '{}'", refName.toStdString());

    // Parse "refs/remotes/{remoteName}/{remoteBranchName}"
    // remoteBranchName can contain slashes, so we need to split carefully
    const QString prefix = "refs/remotes/";
    if (refName.startsWith(prefix)) {
      const QString withoutPrefix = refName.mid(prefix.length());
      const int firstSlash = withoutPrefix.indexOf('/');
      if (firstSlash != -1) {
        remoteName = withoutPrefix.left(firstSlash);
        remoteBranchName = withoutPrefix.mid(firstSlash + 1);
        LOG_DEBUG("Parsed - Remote: '{}', Branch: '{}'",
                  remoteName.toStdString(), remoteBranchName.toStdString());
      }
    }
    git_reference_free(upstreamRef);
  } else {
    // Fallback: git_branch_upstream returned GIT_ENOTFOUND, so read from git
    // config by iterating through all entries
    const QString configPath = QString::fromUtf8(git_repository_workdir(repo)) +
                               ".git" + QDir::separator() + "config";

    git_config *config = nullptr;
    error = git_config_open_ondisk(&config, configPath.toStdString().c_str());
    if (error) {
      setError(GitError::GetUpstreamFailedToOpenGitConfig, error);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return {};
    }

    const QString remoteConfigKey =
        QString("branch.%1.remote").arg(localBranchName);
    const QString mergeConfigKey =
        QString("branch.%1.merge").arg(localBranchName);

    // Iterate through config entries to find the keys we're looking for
    git_config_iterator *iterator = nullptr;
    error = git_config_iterator_new(&iterator, config);
    if (!error) {
      git_config_entry *entry = nullptr;
      while (!git_config_next(&entry, iterator)) {
        if (entry && entry->name && entry->value) {
          const QString entryName = QString::fromUtf8(entry->name);
          const QString entryValue = QString::fromUtf8(entry->value);

          if (entryName == remoteConfigKey) {
            remoteName = entryValue;
          } else if (entryName == mergeConfigKey) {
            QString mergeRef = entryValue;
            if (mergeRef.startsWith("refs/heads/")) {
              remoteBranchName =
                  mergeRef.mid(11); // Remove "refs/heads/" prefix
            } else {
              remoteBranchName = mergeRef;
            }
          }
        }
      }
      git_config_iterator_free(iterator);
    }

    git_config_free(config);
  }

  git_reference_free(localRef);

  LOG_PERF(0, "Exit");

  return std::make_tuple(!remoteName.isEmpty() && !remoteBranchName.isEmpty(),
                         remoteName, remoteBranchName);
}

bool resetToCommit(git_repository *repo, const QString &commit,
                   ResetType type) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Resetting to commit '{}' with type {}", commit.toStdString(),
            static_cast<int>(type));

  git_object *target = nullptr;

  int error = git_revparse_single(&target, repo, commit.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to parse commit '{}': error={}",
              commit.toStdString(), error);
    setError(GitError::ResetToCommitFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Target commit '{}' parsed successfully", commit.toStdString());

  switch (type) {
  case ResetType::Hard: {
    LOG_DEBUG("Performing HARD reset to commit '{}'", commit.toStdString());
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
    opts.checkout_strategy = GIT_CHECKOUT_FORCE;
    error = git_reset(repo, target, GIT_RESET_HARD, &opts);
    break;
  }
  case ResetType::Mixed: {
    LOG_DEBUG("Performing MIXED reset to commit '{}'", commit.toStdString());
    error = git_reset(repo, target, GIT_RESET_MIXED, nullptr);
    break;
  }
  case ResetType::Soft: {
    LOG_DEBUG("Performing SOFT reset to commit '{}'", commit.toStdString());
    error = git_reset(repo, target, GIT_RESET_SOFT, nullptr);
    break;
  }
  default: {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Unsupported reset type: {}",
              static_cast<int>(type));
    break;
  }
  }

  git_object_free(target);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Reset operation failed: error={}", error);
    setError(GitError::ResetToCommitFailedToResetRepository, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully reset to commit '{}'", commit.toStdString());
  LOG_PERF(0, "Exit");

  return true;
}

bool revertCommit(git_repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;

  int error = git_revparse_single((git_object **)&targetCommit, repo,
                                  commit.toStdString().c_str());
  if (error) {
    setError(GitError::RevertCommitFailedToLookupTargetCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *headCommit = nullptr;
  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    setError(GitError::RevertCommitFailedToGetRepositoryHead, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&headCommit, repo, git_reference_target(headRef));
  if (error) {
    setError(GitError::RevertCommitFailedToGetReferenceTarget, error);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *revertIndex = nullptr;
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
  mergeOpts.file_flags |= GIT_MERGE_FILE_ACCEPT_CONFLICTS;

  error = git_revert_commit(&revertIndex, repo, targetCommit, headCommit,
                            0, // mainline = 0 for non-merge commits
                            &mergeOpts);
  if (error) {
    setError(GitError::RevertCommitFailedToPrepareRevertCommit, error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;

  error = git_checkout_index(repo, revertIndex, &checkoutOpts);
  if (error) {
    setError(GitError::RevertCommitFailedToCheckoutIndex, error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList conflictedFiles;
  if (!collectConflictedFilePathsFromResultIndex(repo, revertIndex,
                                                 &conflictedFiles)) {
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!conflictedFiles.isEmpty()) {
    git_index_free(revertIndex);

    if (!markConflictedFiles(repo, conflictedFiles)) {
      git_commit_free(headCommit);
      git_reference_free(headRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);

    LOG_DEBUG("Revert of commit '{}' completed with conflicts",
              commit.toStdString());
    LOG_PERF(0, "Exit");
    return true;
  }

  git_oid treeOid;

  error = git_index_write_tree_to(&treeOid, revertIndex, repo);
  if (error) {
    setError(GitError::RevertCommitFailedToWriteIndexTreeToRepository, error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree *revertTree = nullptr;

  error = git_tree_lookup(&revertTree, repo, &treeOid);
  if (error) {
    setError(GitError::RevertCommitFailedToLookupTree, error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::RevertCommitFailedToCreateSignature, error);
    git_tree_free(revertTree);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid newCommitOid;

  error = git_commit_create_v(&newCommitOid, repo, "HEAD", sig, sig, nullptr,
                              message.toStdString().c_str(), revertTree, 1,
                              headCommit);
  if (error) {
    setError(GitError::RevertCommitFailedToCreateCommit, error);
    git_signature_free(sig);
    git_tree_free(revertTree);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(revertTree);
  git_index_free(revertIndex);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

static QStringList createPatchTargetFiles(const QString &patchContent) {
  QStringList files;
  const QStringList lines =
      QString(patchContent).replace("\r\n", "\n").split('\n');
  for (const QString &line : lines) {
    if (line.startsWith("+++ ")) {
      const QString path = line.mid(4).trimmed();
      if (path != "/dev/null" && path.startsWith("b/")) {
        files.append(path.mid(2));
      }
    }
  }
  files.removeDuplicates();
  return files;
}

bool applyPatch(git_repository *repo, const QString &patchContent) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString workdirPath = QString::fromUtf8(git_repository_workdir(repo));
  const QStringList targetFiles = patchTargetFiles(patchContent);
  QStringList createdFiles;
  QString errorMessage;

  if (!ensureFilesExistForPatch(workdirPath, targetFiles, createdFiles,
                                &errorMessage)) {
    LOG_ERROR(Log::ErrorId::FileSystem, "{}", errorMessage.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  for (const QString &filePath : targetFiles) {
    const QString fullPath = QDir(workdirPath).filePath(filePath);
    QFileInfo fileInfo(fullPath);
    if (fileInfo.exists() && fileInfo.size() > 0) {
      continue;
    }

    const QString content = reconstructNewFileContent(patchContent, filePath);
    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                   QIODevice::Text)) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Can't write reconstructed file \"{}\" from patch.",
                fullPath.toStdString());
      removeFiles(createdFiles);
      LOG_PERF(0, "Exit");
      return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    file.close();
  }

  git_diff *diff = nullptr;
  const QByteArray buffer = patchContent.toUtf8();

  int error = git_diff_from_buffer(&diff, buffer.constData(), buffer.size());
  if (error) {
    setError(GitError::ApplyPatchFailedToParsePatchBuffer, error);
    removeFiles(createdFiles);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_apply_options opts = GIT_APPLY_OPTIONS_INIT;
  error = git_apply(repo, diff, GIT_APPLY_LOCATION_WORKDIR, &opts);
  if (error) {
    setError(GitError::ApplyPatchFailedToApplyPatch, error);
    git_diff_free(diff);
    removeFiles(createdFiles);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_diff_free(diff);

  LOG_PERF(0, "Exit");

  return true;
}

bool cherryPickCommit(git_repository *repo, const QString &commit,
                      const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;

  int error = git_revparse_single((git_object **)&targetCommit, repo,
                                  commit.toStdString().c_str());
  if (error) {
    setError(GitError::CherryPickCommitFailedToLookupTargetCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *headCommit = nullptr;
  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    setError(GitError::CherryPickCommitFailedToGetRepositoryHead, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&headCommit, repo, git_reference_target(headRef));
  if (error) {
    setError(GitError::CherryPickCommitFailedToGetReferenceTarget, error);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *cherryPickIndex = nullptr;
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
  mergeOpts.file_flags |= GIT_MERGE_FILE_ACCEPT_CONFLICTS;

  error = git_cherrypick_commit(&cherryPickIndex, repo, targetCommit,
                                headCommit, 0, &mergeOpts);
  if (error) {
    setError(GitError::CherryPickCommitFailedToPrepareCherryPickCommit, error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;

  error = git_checkout_index(repo, cherryPickIndex, &checkoutOpts);
  if (error) {
    setError(GitError::CherryPickCommitFailedToCheckoutIndex, error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList conflictedFiles;
  if (!collectConflictedFilePathsFromResultIndex(repo, cherryPickIndex,
                                                 &conflictedFiles)) {
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!conflictedFiles.isEmpty()) {
    git_index_free(cherryPickIndex);

    if (!markConflictedFiles(repo, conflictedFiles)) {
      git_commit_free(headCommit);
      git_reference_free(headRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);

    LOG_DEBUG("Cherry-pick of commit '{}' completed with conflicts",
              commit.toStdString());
    LOG_PERF(0, "Exit");
    return true;
  }

  git_oid treeOid;

  error = git_index_write_tree_to(&treeOid, cherryPickIndex, repo);
  if (error) {
    setError(GitError::CherryPickCommitFailedToWriteIndexTreeToRepository,
             error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree *cherryPickTree = nullptr;

  error = git_tree_lookup(&cherryPickTree, repo, &treeOid);
  if (error) {
    setError(GitError::CherryPickCommitFailedToLookupTree, error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CherryPickCommitFailedToCreateSignature, error);
    git_tree_free(cherryPickTree);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid newCommitOid;
  const char *commitMessage = git_commit_message(targetCommit);

  error = git_commit_create_v(&newCommitOid, repo, "HEAD", sig, sig, nullptr,
                              commitMessage, cherryPickTree, 1, headCommit);
  if (error) {
    setError(GitError::CherryPickCommitFailedToCreateCommit, error);
    git_signature_free(sig);
    git_tree_free(cherryPickTree);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(cherryPickTree);
  git_index_free(cherryPickIndex);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

bool renameBranch(git_repository *repo, const QString &oldName,
                  const QString &newName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;

  int error = git_branch_lookup(&ref, repo, oldName.toStdString().c_str(),
                                GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::RenameBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_move(&ref, ref, newName.toStdString().c_str(), 0);
  if (error) {
    setError(GitError::RenameBranchFailedToRenameBranch, error);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteBranch(git_repository *repo, const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;

  int error = git_branch_lookup(&ref, repo, branchName.toStdString().c_str(),
                                GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::DeleteBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_delete(ref);
  if (error) {
    setError(GitError::DeleteBranchFailedToDeleteBranch, error);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteRemoteBranch(git_repository *repo, const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting delete remote branch operation for branch: '{}'",
            branchName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_remote *remote = nullptr;

  auto [ok, remoteName, _] = getUpstream(repo, branchName);
  if (!ok || remoteName.isEmpty()) {
    remoteName = "origin";
  }

  int error =
      git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::DeleteRemoteBranchFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::DeleteRemoteBranchGitSshUrlRequiresCommercialPlan,
             GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Deleting remote branches from git@ repositories is not "
              "available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Delete branch with refspec: :refs/heads/branchName (empty source = delete)
  // Strip remote prefix (e.g., "origin/branch" -> "branch")
  QString cleanBranchName = branchName;
  if (branchName.startsWith("origin/")) {
    cleanBranchName =
        branchName.mid(7); // Remove "origin/" prefix (7 characters)
  }

  const QString refspec = QString(":refs/heads/%1").arg(cleanBranchName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  LOG_DEBUG("Using refspec for delete: '{}' (original branch name: '{}')",
            refspec.toStdString(), branchName.toStdString());

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    LOG_DEBUG("Attempting deleteRemoteBranch push (attempt {}/{})", retries + 1,
              maxRetries);
    error = git_remote_push(remote, &refspecs, &opts);
    LOG_DEBUG("deleteRemoteBranch git_remote_push returned error code: {}",
              error);

    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "deleteRemoteBranch: git_remote_push failed with error: {}",
                error);
      setError(GitError::DeleteRemoteBranchFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    } else {
      LOG_DEBUG("deleteRemoteBranch: git_remote_push succeeded");
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Delete remote branch operation completed with error code: {}, "
            "returning {}",
            error, (error == 0 ? "success" : "failure"));
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool createTagFromCommit(git_repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid targetOid;

  int error = git_oid_fromstr(&targetOid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  error = git_commit_lookup(&targetCommit, repo, &targetOid);
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToCreateSignature, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid tagOid;

  error = git_tag_create(&tagOid, repo, tagName.toStdString().c_str(),
                         reinterpret_cast<git_object *>(targetCommit), sig,
                         message.toStdString().c_str(), 0);
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToPeelCommitForTag, error);
    git_signature_free(sig);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

QString createPatchFromCommit(git_repository *repo, const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_oid oid;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *parent = nullptr;
  git_tree *parentTree = nullptr;

  if (const int parentCount = git_commit_parentcount(commit_);
      parentCount > 0) {
    error = git_commit_parent(&parent, commit_, 0);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToGetParentCommit, error);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parentTree, parent);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToGetSecondCommitTree,
               error);
      git_commit_free(parent);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  git_diff *diff = nullptr;

  error = git_diff_tree_to_tree(&diff, repo, parentTree, tree, nullptr);
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToCreateCommitDiff, error);

    if (parentTree) {
      git_tree_free(parentTree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  QString patch;
  const size_t count = git_diff_num_deltas(diff);
  for (size_t i = 0; i < count; ++i) {
    git_patch *patch_ = nullptr;

    error = git_patch_from_diff(&patch_, diff, i);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToCreatePatchFromDiff,
               error);
      continue;
    }

    git_buf buf = {nullptr};

    error = git_patch_to_buf(&buf, patch_);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToRenderPatchToBuffer,
               error);
      git_patch_free(patch_);
      continue;
    }

    if (buf.ptr && buf.size > 0) {
      patch += QString::fromUtf8(buf.ptr, buf.size);
    }

    git_buf_dispose(&buf);
    git_patch_free(patch_);
  }

  git_diff_free(diff);

  if (parentTree) {
    git_tree_free(parentTree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return patch;
}

bool renameTag(git_repository *repo, const QString &oldName,
               const QString &newName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *tagRef = nullptr;
  git_object *target = nullptr;
  git_oid newOid;

  int error = git_reference_lookup(
      &tagRef, repo,
      QString("refs/tags/%1").arg(oldName).toStdString().c_str());
  if (error) {
    setError(GitError::RenameTagFailedToLookupReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reference_peel(&target, tagRef, GIT_OBJECT_TAG);
  if (error < 0) {
    // Might be a lightweight tag — try commit or tree
    error = git_reference_peel(&target, tagRef, GIT_OBJECT_ANY);
    if (error) {
      setError(GitError::RenameTagFailedToPeelReference, error);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  // If it's an annotated tag, get message and tagger
  if (git_object_type(target) == GIT_OBJECT_TAG) {
    git_tag *tag = nullptr;
    tag = reinterpret_cast<git_tag *>(target);
    const git_signature *tagger = git_tag_tagger(tag);
    if (!tagger) {
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *message = git_tag_message(tag);

    git_object *tagTarget = nullptr;

    error = git_tag_target(&tagTarget, tag);
    if (error) {
      setError(GitError::RenameTagFailedToReadTagTarget, error);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_tag_create(&newOid, repo, newName.toStdString().c_str(),
                           tagTarget, tagger, message, 0);
    if (error) {
      setError(GitError::RenameTagFailedToCreateAnnotatedTag, error);
      git_object_free(tagTarget);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(tagTarget);
  } else {
    // Lightweight tag
    error = git_tag_create_lightweight(
        &newOid, repo, newName.toStdString().c_str(), target, 0);
    if (error) {
      setError(GitError::RenameTagFailedToCreateLightweightTag, error);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_tag_delete(repo, oldName.toStdString().c_str());
  if (error) {
    setError(GitError::RenameTagFailedToDeleteTag, error);
    git_object_free(target);
    git_reference_free(tagRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(target);
  git_reference_free(tagRef);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (const int error = git_tag_delete(repo, tagName.toStdString().c_str())) {
    setError(GitError::DeleteTagFailedToDeleteTag, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteRemoteTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting delete remote tag operation for tag: '{}'",
            tagName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_remote *remote = nullptr;
  const QString defaultRemoteName = getDefaultRemote(repo);

  int error =
      git_remote_lookup(&remote, repo, defaultRemoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::DeleteRemoteTagFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::DeleteRemoteTagGitSshUrlRequiresCommercialPlan,
             GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git, "Deleting remote tags from git@ repositories "
                                 "is not available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Delete tag with refspec: :refs/tags/tagName (empty source = delete)
  // Note: For tags, we typically don't have a remote prefix, but clean it just
  // in case
  QString cleanTagName = tagName;
  if (const auto parts = tagName.split('/'); parts.length() > 1) {
    cleanTagName = parts.last();
  }

  const QString refspec = QString(":refs/tags/%1").arg(cleanTagName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  LOG_DEBUG("Using refspec for delete: '{}' (original tag name: '{}')",
            refspec.toStdString(), tagName.toStdString());

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    LOG_DEBUG("Attempting deleteRemoteTag push (attempt {}/{})", retries + 1,
              maxRetries);
    error = git_remote_push(remote, &refspecs, &opts);
    LOG_DEBUG("deleteRemoteTag git_remote_push returned: {}", error);

    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "deleteRemoteTag push failed: {}", error);
      setError(GitError::DeleteRemoteTagFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(progressData.uuid, error != 0, false);

  finishCredentialsAcquiring(url);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Delete remote tag operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

QString getSubmodulePath(git_repository *repo, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_submodule *submodule = nullptr;

  if (const int error = git_submodule_lookup(
          &submodule, repo, submoduleName.toStdString().c_str())) {
    setError(GitError::GetSubmodulePathFailedToLookupSubmodule, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  const char *path = git_submodule_path(submodule);
  if (!path) {
    git_submodule_free(submodule);
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString pathStr = path;

  git_submodule_free(submodule);

  LOG_PERF(0, "Exit");

  return pathStr;
}

bool deleteSubmodule(git_repository *repo, const QString &submoduleName,
                     const QString &submodulePath) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string name = submoduleName.toStdString();
  const std::string path = submodulePath.toStdString();

  git_submodule *submodule = nullptr;

  int error = git_submodule_lookup(&submodule, repo, name.c_str());
  if (error) {
    // Submodule lookup failed (e.g., after discarding .gitmodules)
    // Continue with manual cleanup instead of failing
    LOG_DEBUG(
        "Submodule '{}' not found in metadata, proceeding with manual cleanup",
        name);
    submodule = nullptr;
  }

  const char *repositoryPath = git_repository_path(repo);
  if (!repositoryPath) {
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string gitModulesPath =
      QString("%1/../.gitmodules").arg(repositoryPath).toStdString();
  const std::string gitModulesTmpPath =
      QString("%1/../.gitmodules.tmp").arg(repositoryPath).toStdString();

  FILE *src = fopen(gitModulesPath.c_str(), "r");
  FILE *dst = fopen(gitModulesTmpPath.c_str(), "w");

  if (src && dst) {
    char line[1024];
    int skip = 0;
    while (fgets(line, sizeof(line), src)) {
      if (strstr(line, name.c_str())) {
        skip = 1;
        continue;
      }

      if (skip && line[0] == '[') {
        skip = 0;
      }

      if (!skip) {
        fputs(line, dst);
      }
    }

    fclose(src);
    src = nullptr;
    fclose(dst);
    dst = nullptr;

    remove(gitModulesPath.c_str());
    if (rename(gitModulesTmpPath.c_str(), gitModulesPath.c_str()) != 0) {
      // If rename failed, try to clean up the temp file
      remove(gitModulesTmpPath.c_str());
      git_submodule_free(submodule);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (const bool staged = stageFiles(repo, QStringList() << ".gitmodules");
        !staged) {
      git_submodule_free(submodule);
      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    if (src) {
      fclose(src);
    }
    if (dst) {
      fclose(dst);
    }
    // Clean up temp file if it was created
    remove(gitModulesTmpPath.c_str());
  }

  // Remove submodule section from .git/config file
  const std::string gitDirPath = repositoryPath;
  const std::string gitConfigFile =
      gitDirPath +
      (gitDirPath.back() == '/' || gitDirPath.back() == '\\' ? "" : "/") +
      "config";

  src = fopen(gitConfigFile.c_str(), "r");
  dst = fopen((gitConfigFile + ".tmp").c_str(), "w");

  if (src && dst) {
    char line[1024];
    int skipSection = 0;
    while (fgets(line, sizeof(line), src)) {
      // Check if this is the submodule section header
      if (strstr(line, "[submodule") && strstr(line, name.c_str())) {
        skipSection = 1;
        continue;
      }

      // Check if we're leaving the submodule section (new section starts)
      if (skipSection && line[0] == '[') {
        skipSection = 0;
      }

      // Only write lines that are not part of the submodule section
      if (!skipSection) {
        fputs(line, dst);
      }
    }

    fclose(src);
    src = nullptr;
    fclose(dst);
    dst = nullptr;

    remove(gitConfigFile.c_str());
    if (rename((gitConfigFile + ".tmp").c_str(), gitConfigFile.c_str()) != 0) {
      // If rename failed, try to clean up the temp file
      remove((gitConfigFile + ".tmp").c_str());
    }
  } else {
    if (src) {
      fclose(src);
    }
    if (dst) {
      fclose(dst);
    }
    // Clean up temp file if it was created
    remove((gitConfigFile + ".tmp").c_str());
  }

  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    setError(GitError::DeleteSubmoduleFailedToGetRepositoryIndex, error);
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  // Try to remove from index, but don't fail if it's not there
  // (user may have already discarded it)
  error = git_index_remove(index, name.c_str(), 0);
  if (error && error != GIT_ENOTFOUND) {
    setError(GitError::DeleteSubmoduleFailedToRemoveIndexEntry, error);
    git_index_free(index);
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_index_write(index);
  if (error) {
    setError(GitError::DeleteSubmoduleFailedToWriteIndex, error);
    git_index_free(index);
    if (submodule) {
      git_submodule_free(submodule);
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index_free(index);
  if (submodule) {
    git_submodule_free(submodule);
  }

  QDir(submodulePath).removeRecursively();
  QDir(QString("%1/modules/%2").arg(repositoryPath).arg(name))
      .removeRecursively();

  LOG_PERF(0, "Exit");

  return true;
}

// Fetches `branchName` from `remoteName` into `submoduleRepo`, then checks out
// a local branch of the same name tracking the fetched remote branch (never
// leaving the submodule in a detached HEAD state). Submodules must always
// track a branch, whether it was chosen explicitly when the submodule was
// added, or resolved automatically as a sensible default.
static bool checkoutSubmoduleBranch(git_repository *submoduleRepo,
                                    const QString &repoPath,
                                    const QString &remoteName,
                                    const QString &branchName,
                                    const QString &uuid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(submoduleRepo != nullptr);

  if (!submoduleRepo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_remote *remote = nullptr;
  int error = git_remote_lookup(&remote, submoduleRepo,
                                remoteName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *remoteUrl = git_remote_url(remote);
  const QString urlStr = remoteUrl ? QString::fromUtf8(remoteUrl) : QString();
  LOG_DEBUG("Checking out submodule branch '{}' from {}/{} ({})",
            branchName.toStdString(), remoteName.toStdString(),
            branchName.toStdString(), urlStr.toStdString());

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials = &credentialsAcquireCallback;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  const QString refspec =
      QString("+refs/heads/%1:refs/remotes/%2/%1").arg(branchName, remoteName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  int retries = 0;
  const int maxRetries = MAX_RETRIES;

  do {
    error = git_remote_fetch(remote, &refspecs, &fetchOpts, nullptr);

    if (error) {
      setError(GitError::CheckoutSubmoduleBranchFailedToFetchRemote, error);
      gGitCredentialsMutex.lock();
      gGitCredentials.remove(urlStr);
      gGitCredentialsMutex.unlock();
      sendInvalidCredentialsSignal(urlStr);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishFetchProgress(uuid, error != 0, false);
  finishCredentialsAcquiring(urlStr);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  if (error) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString remoteRefName =
      QString("refs/remotes/%1/%2").arg(remoteName, branchName);

  git_reference *remoteRef = nullptr;
  error = git_reference_lookup(&remoteRef, submoduleRepo,
                               remoteRefName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToLookupReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                             remoteRef, GIT_OBJECT_COMMIT);
  git_reference_free(remoteRef);
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToPeelReferenceToCommit,
             error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  error = git_branch_lookup(&localRef, submoduleRepo,
                            branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error == GIT_ENOTFOUND) {
    error =
        git_branch_create(&localRef, submoduleRepo,
                          branchName.toStdString().c_str(), targetCommit, 0);
  }

  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToCreateLocalBranch, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *updatedLocalRef = nullptr;
  error = git_reference_set_target(&updatedLocalRef, localRef,
                                   git_commit_id(targetCommit),
                                   "Update submodule branch");
  git_reference_free(localRef);
  localRef = updatedLocalRef;
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToReadCommitId, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(localRef);
  git_commit_free(targetCommit);

  const QString headRefName = QString("refs/heads/%1").arg(branchName);
  error =
      git_repository_set_head(submoduleRepo, headRefName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToSetHeadToBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy =
      GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;
  // Submodule checkouts are routed through libgit2's filter pipeline, which
  // can hit an internal writer lifecycle assertion on some repositories.
  // Avoid filters here so submodule operations stay reliable.
  checkoutOpts.disable_filters = true;

  error = git_checkout_head(submoduleRepo, &checkoutOpts);
  if (error) {
    setError(GitError::CheckoutSubmoduleBranchFailedToCheckoutHead, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  setUpstream(submoduleRepo, remoteName, branchName, branchName);

  LOG_PERF(0, "Exit");

  return true;
}

static bool checkoutLocalSubmoduleBranch(git_repository *submoduleRepo,
                                         const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(submoduleRepo != nullptr);

  if (!submoduleRepo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *localRef = nullptr;
  int error =
      git_branch_lookup(&localRef, submoduleRepo,
                        branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::CheckoutLocalSubmoduleBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString headRefName = QString("refs/heads/%1").arg(branchName);
  error =
      git_repository_set_head(submoduleRepo, headRefName.toStdString().c_str());
  if (error) {
    setError(GitError::CheckoutLocalSubmoduleBranchFailedToSetHeadToBranch,
             error);
    git_reference_free(localRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy =
      GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;
  // Submodule checkouts are routed through libgit2's filter pipeline, which
  // can hit an internal writer lifecycle assertion on some repositories.
  // Avoid filters here so submodule operations stay reliable.
  checkoutOpts.disable_filters = true;

  error = git_checkout_head(submoduleRepo, &checkoutOpts);
  git_reference_free(localRef);
  if (error) {
    setError(GitError::CheckoutLocalSubmoduleBranchFailedToCheckoutHead, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool updateSubmodule(git_repository *repo, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const std::string name = submoduleName.toStdString();

  git_submodule *submodule = nullptr;
  git_repository *submoduleRepo = nullptr;

  auto cleanup = [&]() {
    if (submoduleRepo) {
      git_repository_free(submoduleRepo);
      submoduleRepo = nullptr;
    }

    if (submodule) {
      git_submodule_free(submodule);
      submodule = nullptr;
    }
  };

  auto fail = [&](const int error) {
    if (error) {
      setError(GitError::UpdateSubmoduleFailedToUpdateSubmodule, error);
    }

    cleanup();
    LOG_PERF(0, "Exit");
    return false;
  };

  int error = git_submodule_lookup(&submodule, repo, name.c_str());
  if (error) {
    return fail(error);
  }

  error = git_submodule_open(&submoduleRepo, submodule);
  if (error) {
    return fail(error);
  }

  // Stage the current local submodule HEAD into the parent repository index.
  // This uses the submodule's working tree state, not the remote branch tip.
  error = git_submodule_add_to_index(submodule, 1);
  if (error) {
    setError(GitError::UpdateSubmoduleFailedToAddSubmoduleToIndex, error);
    cleanup();
    LOG_PERF(0, "Exit");
    return false;
  }

  cleanup();

  LOG_PERF(0, "Exit");

  return true;
}

bool addSubmodule(git_repository *repo, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const char *repoPath = git_repository_path(repo);
  if (!repoPath) {
    LOG_PERF(0, "Exit");
    return false;
  }

  deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));

  git_submodule *subModule = nullptr;

  int error =
      git_submodule_add_setup(&subModule, repo, url.toStdString().c_str(),
                              name.toStdString().c_str(), 1);
  if (error) {
    setError(GitError::AddSubmoduleFailedToSetupSubmodule, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool gitSchema = url.startsWith("git://") || url.startsWith("git@");
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::AddSubmoduleGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git, "Adding submodules to git@ repositories is "
                                 "not available on the free plan.");
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository *subRepo = nullptr;

  CloneProgress progressData;
  progressData.uuid = uuid;
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy = GIT_CHECKOUT_SAFE;
  // Submodule clone also uses checkout filters, which can trigger an internal
  // libgit2 writer assertion on some repositories.
  checkoutOpts.disable_filters = true;
  checkoutOpts.progress_payload = &progressData;
  checkoutOpts.progress_cb = &checkoutProgressCallback;

  git_fetch_options fetchOpts = GIT_FETCH_OPTIONS_INIT;
  fetchOpts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  fetchOpts.callbacks.transfer_progress = &fetchProgressCallback;
  fetchOpts.callbacks.certificate_check = &certificateCheckCallback;
  fetchOpts.callbacks.payload = &callbackPayload;

  git_submodule_update_options cloneOpts = GIT_SUBMODULE_UPDATE_OPTIONS_INIT;
  cloneOpts.fetch_opts = fetchOpts;
  cloneOpts.checkout_opts = checkoutOpts;

  error = git_submodule_clone(&subRepo, subModule, &cloneOpts);
  if (error) {
    setError(GitError::AddSubmoduleFailedToCloneSubmodule, error);
    finishFetchProgress(uuid, true, false);
    finishCredentialsAcquiring(url);
    if (subRepo) {
      git_repository_free(subRepo);
    }
    // Clean up the .gitmodules entry that was added by git_submodule_add_setup
    deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Send completion event to hide progress toast
  // This is needed even if no progress callbacks were fired (e.g., local path
  // submodules)
  finishFetchProgress(uuid, error != 0, false);

  // Dismiss credentials overlay using the URL (same key used for storing the
  // widget)
  finishCredentialsAcquiring(url);

  // Submodules must always track a branch. Use the branch chosen in the add
  // submodule dialog; if none was provided, fall back to whatever branch the
  // clone above already checked out (the remote's default branch).
  QString trackedBranch = branch.trimmed();
  if (trackedBranch.isEmpty()) {
    git_reference *head = nullptr;
    if (!git_repository_head(&head, subRepo)) {
      const char *currentBranch = nullptr;
      if (!git_branch_name(&currentBranch, head) && currentBranch &&
          currentBranch[0]) {
        trackedBranch = QString::fromUtf8(currentBranch);
      }
      git_reference_free(head);
    }
  }

  if (trackedBranch.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Can't determine a branch to track for submodule '{}'",
              name.toStdString());
    git_repository_free(subRepo);
    deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  QString subRepoPath;
  if (const char *workdir = git_repository_workdir(subRepo)) {
    subRepoPath = QString::fromUtf8(workdir);
  }

  if (!checkoutSubmoduleBranch(subRepo, subRepoPath, "origin", trackedBranch,
                               uuid)) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to check out tracked branch '{}' for submodule '{}'",
              trackedBranch.toStdString(), name.toStdString());
    git_repository_free(subRepo);
    deleteSubmodule(repo, name, QString("%1/../%2").arg(repoPath).arg(name));
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Persist the tracked branch in .gitmodules so future updates keep
  // tracking the same branch.
  git_submodule_set_branch(repo, name.toStdString().c_str(),
                           trackedBranch.toStdString().c_str());

  error = git_submodule_add_finalize(subModule);
  if (error) {
    setError(GitError::AddSubmoduleFailedToFinalizeSubmoduleAdd, error);
    git_repository_free(subRepo);
    git_submodule_free(subModule);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_repository_free(subRepo);
  git_submodule_free(subModule);

  LOG_PERF(0, "Exit");

  return true;
}

FilesDiff diffFilesFromCommit(git_repository *repo, const QStringList &files,
                              const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_oid oid;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *parent = nullptr;
  git_tree *parentTree = nullptr;
  git_commit *parent2 = nullptr;
  git_tree *parent2Tree = nullptr;

  if (git_commit_parentcount(commit_) > 0) {
    error = git_commit_parent(&parent, commit_, 0);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetParentCommit, error);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parentTree, parent);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetSecondCommitTree, error);
      git_commit_free(parent);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  if (git_commit_parentcount(commit_) > 2) {
    error = git_commit_parent(&parent2, commit_, 2);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetSecondParentCommit,
               error);
      if (parentTree) {
        git_tree_free(parentTree);
      }
      if (parent) {
        git_commit_free(parent);
      }
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parent2Tree, parent2);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetThirdCommitTree, error);
      if (parent2) {
        git_commit_free(parent2);
      }
      if (parentTree) {
        git_tree_free(parentTree);
      }
      if (parent) {
        git_commit_free(parent);
      }
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  git_diff *diff = nullptr;

  git_strarray arr = qstringlist_to_git_strarray(files);

  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.pathspec = arr;

  error = git_diff_tree_to_tree(&diff, repo, parentTree, tree, &opts);
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToCreateCommitDiff, error);

    git_strarray_destroy(arr);

    if (parentTree) {
      git_tree_free(parentTree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit_);

    LOG_PERF(0, "Exit");
    return {};
  }

  QString diffOutput;

  QVector<FilesDiffDelta> filesDeltas;

  size_t num_deltas = git_diff_num_deltas(diff);
  for (size_t i = 0; i < num_deltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(diff, i);
    diffOutput += QString("diff --git a/%1 b/%2\n")
                      .arg(delta->old_file.path, delta->new_file.path);

    FilesDiffDelta filesDiffDelta;
    filesDiffDelta.oldFilePath = delta->old_file.path;
    filesDiffDelta.newFilePath = delta->new_file.path;

    git_patch *patch;
    error = git_patch_from_diff(&patch, diff, i);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToCreatePatchFromDiff, error);
      continue;
    }

    const size_t num_hunks = git_patch_num_hunks(patch);
    for (size_t h = 0; h < num_hunks; ++h) {
      const git_diff_hunk *hunk;
      error = git_patch_get_hunk(&hunk, NULL, patch, h);
      if (error) {
        setError(GitError::DiffFilesFromCommitFailedToReadPatchHunk, error);
        continue;
      }

      diffOutput += QString("@@ -%1,%2 +%3,%4 @@\n")
                        .arg(hunk->old_start)
                        .arg(hunk->old_lines)
                        .arg(hunk->new_start)
                        .arg(hunk->new_lines);

      FilesDiffHunk filesDiffHunk;
      filesDiffHunk.oldStart = hunk->old_start;
      filesDiffHunk.oldLines = hunk->old_lines;
      filesDiffHunk.newStart = hunk->new_start;
      filesDiffHunk.newLines = hunk->new_lines;

      size_t num_lines = git_patch_num_lines_in_hunk(patch, h);
      for (size_t l = 0; l < num_lines; ++l) {
        const git_diff_line *line;
        error = git_patch_get_line_in_hunk(&line, patch, h, l);
        if (error) {
          setError(GitError::DiffFilesFromCommitFailedToReadPatchLine, error);
          continue;
        }

        const char *start = line->content;
        const char *end = start + line->content_len;
        const char *newline =
            static_cast<const char *>(memchr(start, '\n', end - start));
        if (!newline) {
          newline = end;
        }

        const QString lineContent = QString::fromUtf8(start, newline - start);

        diffOutput += QString("%1%2\n").arg(line->origin, lineContent);

        FilesDiffLine filesDiffLine;
        filesDiffLine.origin = line->origin;
        filesDiffLine.content = lineContent;

        filesDiffHunk.lines += filesDiffLine;
      }

      filesDiffDelta.hunks += filesDiffHunk;
    }

    git_patch_free(patch);

    filesDeltas += filesDiffDelta;
  }

  git_diff_free(diff);
  git_strarray_destroy(arr);

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong maxBlobSize =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QSet<QString> stashUntrackedFiles;

  for (const auto &file : files) {
    const QString filePath = QDir::fromNativeSeparators(file);

    QString repoFileContent;
    if (parentTree && readTreeFileContent(repo, parentTree, filePath,
                                          maxBlobSize, &repoFileContent)) {
      repoContent.insert(file, repoFileContent);
    } else {
      repoContent.insert(file, QString());
    }

    QString filesFileContent;
    if (tree && readTreeFileContent(repo, tree, filePath, maxBlobSize,
                                    &filesFileContent)) {
      filesContent.insert(file, filesFileContent);
      continue;
    }

    if (parent2Tree && readTreeFileContent(repo, parent2Tree, filePath,
                                           maxBlobSize, &filesFileContent)) {
      filesContent.insert(file, filesFileContent);
      stashUntrackedFiles.insert(file);
      continue;
    }

    filesContent.insert(file, QString());
  }

  if (parent2Tree) {
    git_tree_free(parent2Tree);
  }

  if (parent2) {
    git_commit_free(parent2);
  }

  if (parentTree) {
    git_tree_free(parentTree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit_);

  for (const auto &file : files) {
    if (!stashUntrackedFiles.contains(file)) {
      continue;
    }

    const QString indexLine =
        createIndexLineFromContent(QString(), filesContent[file]);
    const QString patch =
        createFullPatchFromContent(file, filesContent[file], true, indexLine);

    diffOutput += QString("diff --git a/%1 b/%2\n").arg(file, file);
    diffOutput += "new file mode 100644\n";
    diffOutput += patch;

    const FilesDiffDelta patchDeltas =
        createFullPatchDeltasFromContent(file, filesContent[file]);
    filesDeltas += patchDeltas;
  }

  FilesDiff filesDiff;
  filesDiff.diff = diffOutput;
  filesDiff.filesContent = filesContent;
  filesDiff.repoContent = repoContent;
  filesDiff.deltas = filesDeltas;
  filesDiff.valid = true;

  LOG_PERF(0, "Exit");

  return filesDiff;
}

BlameReport blame(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  const BlameReport report = createBlameReport(repo, files);

  LOG_PERF(0, "Exit");

  return report;
}

qint64 getFileSizeFromCommit(git_repository *repo, const QString &filePath,
                             const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_oid oid;
  int error;

  // If commit is empty, use HEAD
  if (commit.isEmpty()) {
    git_reference *head = nullptr;
    error = git_repository_head(&head, repo);
    if (error) {
      setError(GitError::GetFileSizeFromCommitFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return -1;
    }

    error = git_reference_name_to_id(&oid, repo, git_reference_name(head));
    git_reference_free(head);
    if (error) {
      setError(GitError::GetFileSizeFromCommitFailedToGetReferenceName, error);
      LOG_PERF(0, "Exit");
      return -1;
    }
  } else {
    error = git_oid_fromstr(&oid, commit.toStdString().c_str());
    if (error) {
      setError(GitError::GetFileSizeFromCommitFailedToParseObjectId, error);
      LOG_PERF(0, "Exit");
      return -1;
    }
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const QString normalizedPath = QDir::fromNativeSeparators(filePath);
  git_tree_entry *entry = nullptr;

  error =
      git_tree_entry_bypath(&entry, tree, normalizedPath.toStdString().c_str());
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToLookupTreeEntryByPath,
             error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const git_oid *blobOid = git_tree_entry_id(entry);
  if (!blobOid) {
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_blob *blob = nullptr;
  error = git_blob_lookup(&blob, repo, blobOid);
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToLookupBlob, error);
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const qint64 blobSize = static_cast<qint64>(git_blob_rawsize(blob));

  git_blob_free(blob);
  git_tree_entry_free(entry);
  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return blobSize;
}

bool merge(git_repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Starting merge operation: source='{}', destination='{}', "
            "author='{}', email='{}'",
            sourceBranch.toStdString(), destinationBranch.toStdString(),
            author.toStdString(), email.toStdString());

  git_reference *srcRef = nullptr;
  git_reference *dstRef = nullptr;

  // Lookup branches
  int error = git_branch_lookup(
      &srcRef, repo, sourceBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to lookup source branch '{}': error={}",
              sourceBranch.toStdString(), error);
    setError(GitError::MergeFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Source branch '{}' lookup successful", sourceBranch.toStdString());

  error = git_branch_lookup(
      &dstRef, repo, destinationBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to lookup destination branch '{}': error={}",
              destinationBranch.toStdString(), error);
    setError(GitError::MergeFailedToLookupDestinationBranch, error);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Destination branch '{}' lookup successful",
            destinationBranch.toStdString());

  git_object *dstObj = nullptr;

  error = git_reference_peel(&dstObj, dstRef, GIT_OBJECT_COMMIT);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to peel destination reference: error={}", error);
    setError(GitError::MergeFailedToPeelReference, error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Checking out destination branch");
  error = git_checkout_tree(repo, dstObj, nullptr);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to checkout tree for destination branch: error={}",
              error);
    setError(GitError::MergeFailedToCheckoutTree, error);
    git_object_free(dstObj);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(dstObj);

  error = git_repository_set_head(repo, git_reference_name(dstRef));
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to set repository HEAD: error={}",
              error);
    setError(GitError::MergeFailedToGetReferenceName, error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Repository HEAD set, preparing to merge");
  // Get annotated commits
  git_annotated_commit *srcCommit = nullptr;

  error = git_annotated_commit_from_ref(&srcCommit, repo, srcRef);
  if (error) {
    setError(GitError::MergeFailedToCreateAnnotatedCommitFromReference, error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *dstCommit = nullptr;

  error = git_annotated_commit_from_ref(&dstCommit, repo, dstRef);
  if (error) {
    setError(GitError::MergeFailedToCreateSecondAnnotatedCommitFromReference,
             error);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_merge_analysis_t analysis;
  git_merge_preference_t preference;

  const git_annotated_commit *commits[1] = {srcCommit};

  // Merge analysis
  error = git_merge_analysis(&analysis, &preference, repo, commits, 1);
  if (error) {
    setError(GitError::MergeFailedToAnalyzeMerge, error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
    LOG_DEBUG("Destination branch is already up to date.");
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return true;
  } else if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
    LOG_DEBUG("Performing fast-forward merge...");

    git_oid targetOid;
    git_commit *srcCommitObj = nullptr;
    error = git_commit_lookup(&srcCommitObj, repo,
                              git_annotated_commit_id(srcCommit));
    if (error) {
      setError(GitError::MergeFailedToReadAnnotatedCommitId, error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference *updatedRef = nullptr;

    // Update destination branch to point to source commit
    error = git_reference_set_target(
        &updatedRef, dstRef, git_commit_id(srcCommitObj), "Fast-forward merge");
    if (error) {
      setError(GitError::MergeFailedToReadCommitId, error);
      git_commit_free(srcCommitObj);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit *newCommit = nullptr;

    error = git_commit_lookup(&newCommit, repo, git_commit_id(srcCommitObj));
    if (error) {
      setError(GitError::MergeFailedToReadSecondCommitId, error);
      git_reference_free(updatedRef);
      git_commit_free(srcCommitObj);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(updatedRef);
    git_commit_free(srcCommitObj);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);

    git_checkout_options ffOpts = GIT_CHECKOUT_OPTIONS_INIT;
    ffOpts.checkout_strategy =
        GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;

    error = git_reset(repo, reinterpret_cast<git_object *>(newCommit),
                      GIT_RESET_HARD, &ffOpts);
    if (error) {
      setError(GitError::MergeFailedToCheckoutDestinationTree, error);
      git_commit_free(newCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(newCommit);

    LOG_PERF(0, "Exit");
    return true;
  } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
    LOG_DEBUG("Performing normal merge...");

    git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;

    error = git_merge(repo, commits, 1, &mergeOpts, &checkoutOpts);
    if (error) {
      setError(GitError::MergeFailedToMerge, error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_index *index = NULL;
    error = git_repository_index(&index, repo);
    if (error) {
      setError(GitError::MergeFailedToGetRepositoryIndex, error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (git_index_has_conflicts(index)) {
      LOG_DEBUG("Merge has conflicts. Resolve them before committing.");
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Write tree
    git_oid treeOid;
    git_tree *tree = nullptr;

    error = git_index_write_tree(&treeOid, index);
    if (error) {
      setError(GitError::MergeFailedToWriteIndexTree, error);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_tree_lookup(&tree, repo, &treeOid);
    if (error) {
      setError(GitError::MergeFailedToLookupTree, error);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Create commit
    git_signature *sig = nullptr;

    error = git_signature_now(&sig, author.toStdString().c_str(),
                              email.toStdString().c_str());
    if (error) {
      setError(GitError::MergeFailedToCreateSignature, error);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_oid commitOid;
    git_commit *dstCommitObj = nullptr;
    git_commit *srcCommitObj = nullptr;

    error =
        git_commit_lookup(&dstCommitObj, repo, git_reference_target(dstRef));
    if (error) {
      setError(GitError::MergeFailedToGetReferenceTarget, error);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error =
        git_commit_lookup(&srcCommitObj, repo, git_reference_target(srcRef));
    if (error) {
      setError(GitError::MergeFailedToGetSecondReferenceTarget, error);
      git_commit_free(dstCommitObj);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    const git_commit *parents[2] = {dstCommitObj, srcCommitObj};

    const QString comment = QString("Merge branch '%1' into '%2'")
                                .arg(sourceBranch, destinationBranch);

    error = git_commit_create(&commitOid, repo, "HEAD", sig, sig, NULL,
                              comment.toStdString().c_str(), tree, 2, parents);
    if (error) {
      setError(GitError::MergeFailedToCreateCommit, error);
      git_commit_free(srcCommitObj);
      git_commit_free(dstCommitObj);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(srcCommitObj);
    git_commit_free(dstCommitObj);
    git_signature_free(sig);
    git_tree_free(tree);
    git_index_free(index);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool rebaseBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (sourceBranch.isEmpty() || destinationBranch.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Source or destination branch is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *srcRef = nullptr;
  git_reference *dstRef = nullptr;
  int error = git_branch_lookup(
      &srcRef, repo, sourceBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::RebaseBranchesFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_lookup(
      &dstRef, repo, destinationBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::RebaseBranchesFailedToLookupDestinationBranch, error);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *srcCommit = nullptr;
  error = git_annotated_commit_from_ref(&srcCommit, repo, srcRef);
  if (error) {
    setError(GitError::RebaseBranchesFailedToCreateAnnotatedCommitFromReference,
             error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *dstCommit = nullptr;
  error = git_annotated_commit_from_ref(&dstCommit, repo, dstRef);
  if (error) {
    setError(GitError::
                 RebaseBranchesFailedToCreateSecondAnnotatedCommitFromReference,
             error);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid baseOid;
  error = git_merge_base(&baseOid, repo, git_annotated_commit_id(srcCommit),
                         git_annotated_commit_id(dstCommit));
  if (error) {
    setError(GitError::RebaseBranchesFailedToReadAnnotatedCommitId, error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *baseCommit = nullptr;
  error = git_annotated_commit_lookup(&baseCommit, repo, &baseOid);
  if (error) {
    setError(GitError::RebaseBranchesFailedToLookupAnnotatedCommit, error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *signature = nullptr;
  error = git_signature_now(&signature, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::RebaseBranchesFailedToCreateSignature, error);
    git_annotated_commit_free(baseCommit);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  CommitSigningConfig signingConfig;
  QString signingError;
  bool signingConfigLoaded = true;
  if (signCommit) {
    signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      git_signature_free(signature);
      git_annotated_commit_free(baseCommit);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::CommitStagedFailedToLoadSigningConfiguration, -1);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_rebase *rebaseSession = nullptr;

  CommitSigningPayload signingPayload;
  signingPayload.config = signingConfig;
  signingPayload.errorMessage = &signingError;
  signingPayload.repo = repo;

  git_rebase_options rebaseOpts = GIT_REBASE_OPTIONS_INIT;
  rebaseOpts.checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
  rebaseOpts.commit_create_cb = commitRebaseSigningCallback;
  rebaseOpts.payload = &signingPayload;

  error = git_rebase_init(&rebaseSession, repo, srcCommit, baseCommit,
                          dstCommit, &rebaseOpts);
  if (error) {
    setError(GitError::RebaseBranchesFailedToStartRebase, error);
    git_signature_free(signature);
    git_annotated_commit_free(baseCommit);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool rebaseSuccess = true;
  git_rebase_operation *operation = nullptr;
  git_oid commitId{};

  while ((error = git_rebase_next(&operation, rebaseSession)) == 0) {
    error = git_rebase_commit(&commitId, rebaseSession, nullptr, signature,
                              nullptr, nullptr);
    if (error == 0) {
      continue;
    }

    if (error == GIT_EUNMERGED) {
      LOG_ERROR(Log::ErrorId::Git,
                "Rebase stopped because conflicts must be resolved manually");
      setError(GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts,
               error);
    } else {
      setError(GitError::RebaseBranchesFailedToCommitRebaseStep, error);
      git_rebase_abort(rebaseSession);
    }
    rebaseSuccess = false;
    break;
  }

  if (rebaseSuccess && error == GIT_ITEROVER) {
    error = git_rebase_finish(rebaseSession, signature);
    if (error) {
      setError(GitError::RebaseBranchesFailedToFinishRebase, error);
      git_rebase_abort(rebaseSession);
      rebaseSuccess = false;
    }
  } else if (rebaseSuccess) {
    setError(GitError::RebaseBranchesFailedToAbortRebase, error);
    rebaseSuccess = false;
  }

  git_rebase_free(rebaseSession);
  git_signature_free(signature);
  git_annotated_commit_free(baseCommit);
  git_annotated_commit_free(dstCommit);
  git_annotated_commit_free(srcCommit);
  git_reference_free(dstRef);
  git_reference_free(srcRef);

  LOG_PERF(0, "Exit");
  return rebaseSuccess;
}

QString listConflictsForStashFile(git_repository *repo, int stashIndex,
                                  const QString &filePath) {
  LOG_PERF(0, "Enter");

  (void)stashIndex;

  if (!repo || filePath.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer or file path is null when reading stash "
              "conflict markers");
    LOG_PERF(0, "Exit");
    return {};
  }

  const char *workdir = git_repository_workdir(repo);
  if (!workdir) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository working directory is unavailable when reading stash "
              "conflict markers");
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString fullPath = QDir::fromNativeSeparators(
      QString::fromUtf8(workdir) + QDir::separator() + filePath);
  QFile file(fullPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    LOG_DEBUG("Could not read stash conflict file '{}': '{}'",
              filePath.toStdString(), fullPath.toStdString());
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString content = QString::fromUtf8(file.readAll());
  file.close();

  LOG_PERF(0, "Exit");
  return content;
}

QString listConflictsForAFile(git_repository *repo, const QString &filePath) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_index *index = nullptr;

  int error = git_repository_index(&index, repo);
  if (error) {
    setError(GitError::ListConflictsForAFileFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_index_conflict_iterator *iterator = nullptr;

  error = git_index_conflict_iterator_new(&iterator, index);
  if (error) {
    setError(GitError::ListConflictsForAFileFailedToCreateConflictIterator,
             error);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return {};
  }

  QString fileContentWithMarkers;

  const std::string filePathStr = filePath.toStdString();

  const git_index_entry *ancestor = nullptr;
  const git_index_entry *ours = nullptr;
  const git_index_entry *theirs = nullptr;

  while (!git_index_conflict_next(&ancestor, &ours, &theirs, iterator)) {
    if (ours == nullptr || ours->path != filePathStr || theirs == nullptr ||
        theirs->path != filePathStr) {
      continue;
    }

    git_blob *blobAncestor = nullptr;
    git_blob *blobOurs = nullptr;
    git_blob *blobTheirs = nullptr;
    git_merge_file_input inputAncestor = GIT_MERGE_FILE_INPUT_INIT;
    git_merge_file_input inputOurs = GIT_MERGE_FILE_INPUT_INIT;
    git_merge_file_input inputTheirs = GIT_MERGE_FILE_INPUT_INIT;
    git_merge_file_options opts = GIT_MERGE_FILE_OPTIONS_INIT;
    git_merge_file_result result = {0};

    if (ancestor) {
      error = git_blob_lookup(&blobAncestor, repo, &ancestor->id);
      if (error) {
        continue;
      }

      inputAncestor.ptr =
          static_cast<const char *>(git_blob_rawcontent(blobAncestor));
      inputAncestor.size = git_blob_rawsize(blobAncestor);
      inputAncestor.path = "file";
      inputAncestor.mode = GIT_FILEMODE_BLOB;
    }

    error = git_blob_lookup(&blobOurs, repo, &ours->id);
    if (error) {
      git_blob_free(blobAncestor);
      continue;
    }

    inputOurs.ptr = static_cast<const char *>(git_blob_rawcontent(blobOurs));
    inputOurs.size = git_blob_rawsize(blobOurs);
    inputOurs.path = "file";
    inputOurs.mode = GIT_FILEMODE_BLOB;

    error = git_blob_lookup(&blobTheirs, repo, &theirs->id);
    if (error) {
      git_blob_free(blobOurs);
      git_blob_free(blobAncestor);
      continue;
    }

    inputTheirs.ptr =
        static_cast<const char *>(git_blob_rawcontent(blobTheirs));
    inputTheirs.size = git_blob_rawsize(blobTheirs);
    inputTheirs.path = "file";
    inputTheirs.mode = GIT_FILEMODE_BLOB;

    opts.flags =
        GIT_MERGE_FILE_STYLE_MERGE; // produces <<<<<<<, =======, >>>>>>>
    opts.favor = GIT_MERGE_FILE_FAVOR_NORMAL;

    // Perform merge
    error = git_merge_file(&result, &inputAncestor, &inputOurs, &inputTheirs,
                           &opts);
    if (error) {
      git_blob_free(blobTheirs);
      git_blob_free(blobOurs);
      git_blob_free(blobAncestor);
      continue;
    }

    fileContentWithMarkers +=
        QString::fromUtf8(result.ptr, static_cast<int>(result.len));

    git_merge_file_result_free(&result);
    git_blob_free(blobTheirs);
    git_blob_free(blobOurs);
    git_blob_free(blobAncestor);

    break;
  }

  git_index_conflict_iterator_free(iterator);
  git_index_free(index);

  LOG_PERF(0, "Exit");

  return fileContentWithMarkers;
}

bool getIsMergeInProgress(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const int state = git_repository_state(repo);

  LOG_PERF(0, "Exit");

  return state == GIT_REPOSITORY_STATE_MERGE ||
         state == GIT_REPOSITORY_STATE_REVERT ||
         state == GIT_REPOSITORY_STATE_CHERRYPICK;
  ;
}

bool abortMerge(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (git_repository_state(repo) != GIT_REPOSITORY_STATE_MERGE) {
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = git_repository_state_cleanup(repo);
  if (error) {
    setError(GitError::AbortMergeFailedToCleanupRepositoryState, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object *head = NULL;

  error = git_revparse_single(&head, repo, "HEAD");
  if (error) {
    setError(GitError::AbortMergeFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reset(repo, head, GIT_RESET_HARD, NULL);
  if (error) {
    setError(GitError::AbortMergeFailedToResetRepository, error);
    git_object_free(head);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(head);

  LOG_PERF(0, "Exit");

  return true;
}

QVector<RemoteInfo> listRemotes(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Listing remotes");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_strarray remote_list;
  int error = git_remote_list(&remote_list, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to list remotes: error={}", error);
    setError(GitError::ListRemotesFailedToListRemotes, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get default remote
  const QString defaultRemote = getDefaultRemote(repo);

  QVector<RemoteInfo> remotes;
  QSet<QString> processedRemotes;

  for (size_t i = 0; i < remote_list.count; ++i) {
    const QString remoteName = QString::fromUtf8(remote_list.strings[i]);

    const QStringList splitRemoteName = remoteName.split('.');

    if (splitRemoteName.isEmpty()) {
      LOG_DEBUG("Skipping empty remote name");
      continue;
    }

    if (splitRemoteName.length() >= 2 &&
        processedRemotes.contains(splitRemoteName[0]) &&
        splitRemoteName[1] == "target") {
      LOG_DEBUG("Skipping duplicate remote '{}'", remoteName.toStdString());
      continue;
    }

    processedRemotes.insert(splitRemoteName[0]);

    git_remote *remote = nullptr;
    error = git_remote_lookup(&remote, repo, remote_list.strings[i]);
    if (error) {
      LOG_DEBUG("Failed to lookup remote '{}': error={}",
                remoteName.toStdString(), error);
      continue;
    }

    const QString fetchUrl = QString::fromUtf8(git_remote_url(remote));
    const QString pushUrl = git_remote_pushurl(remote) != nullptr
                                ? QString::fromUtf8(git_remote_pushurl(remote))
                                : fetchUrl;

    // Create default target for backward compatibility
    QVector<RemoteTarget> targets;
    targets.append(RemoteTarget{QStringLiteral("Default"), fetchUrl, pushUrl});

    // Load additional targets from git config
    git_config *config = nullptr;
    error = git_repository_config(&config, repo);
    if (!error && config) {
      git_config_iterator *iterator = nullptr;
      error = git_config_iterator_new(&iterator, config);
      if (!error) {
        git_config_entry *entry = nullptr;
        const QString targetPrefix =
            QString("remote.%1.target.").arg(remoteName);

        // Map to collect targets: targetName -> (fetchUrl, pushUrl)
        QMap<QString, QPair<QString, QString>> targetMap;

        while (!git_config_next(&entry, iterator)) {
          if (entry && entry->name && entry->value) {
            const QString entryName = QString::fromUtf8(entry->name);
            const QString entryValue = QString::fromUtf8(entry->value);

            // Check if this entry is for a target of this remote
            if (entryName.startsWith(targetPrefix)) {
              // Extract target name and property (fetchUrl or pushUrl)
              const QString suffix = entryName.mid(targetPrefix.length());
              const QStringList parts = suffix.split('.');

              if (parts.size() >= 2) {
                const QString targetName = parts[0];
                const QString property = parts[1];

                if (!targetMap.contains(targetName)) {
                  targetMap[targetName] = {QString(), QString()};
                }

                if (property == "fetchurl") {
                  targetMap[targetName].first = entryValue;
                } else if (property == "pushurl") {
                  targetMap[targetName].second = entryValue;
                }
              }
            }
          }
        }

        // Add all collected targets
        for (auto it = targetMap.constBegin(); it != targetMap.constEnd();
             ++it) {
          const QString targetName = it.key();
          const QString targetFetchUrl = it.value().first;
          const QString targetPushUrl =
              it.value().second.isEmpty() ? targetFetchUrl : it.value().second;

          if (!targetFetchUrl.isEmpty() && targetName != "Default") {
            targets.append(
                RemoteTarget{targetName, targetFetchUrl, targetPushUrl});
          }
        }

        git_config_iterator_free(iterator);
      }
      git_config_free(config);
    }

    remotes.append(RemoteInfo{remoteName, fetchUrl, pushUrl,
                              remoteName == defaultRemote, targets});

    git_remote_free(remote);
  }

  git_strarray_dispose(&remote_list);

  LOG_DEBUG("Listed {} remotes", remotes.size());
  LOG_PERF(0, "Exit");

  return remotes;
}

QString getDefaultRemote(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting default remote");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return "";
  }

  git_branch_t branch_type = GIT_BRANCH_LOCAL;
  git_reference *ref = nullptr;

  // Try to get the upstream branch of the current branch
  git_reference *current_ref = nullptr;
  int error = git_repository_head(&current_ref, repo);

  if (error == 0 && current_ref != nullptr) {
    const char *shorthand = nullptr;
    error = git_branch_upstream(&ref, current_ref);

    if (error == 0 && ref != nullptr) {
      const char *upstream_name = git_reference_shorthand(ref);
      if (upstream_name != nullptr) {
        const QString upstream = QString::fromUtf8(upstream_name);
        const QStringList parts = upstream.split('/');
        if (parts.size() >= 2) {
          const QString remoteName = parts.first();
          LOG_DEBUG("Found upstream remote: '{}'", remoteName.toStdString());
          git_reference_free(ref);
          git_reference_free(current_ref);
          LOG_PERF(0, "Exit");
          return remoteName;
        }
      }
      git_reference_free(ref);
    }

    git_reference_free(current_ref);
  }

  // Fall back to "origin" if upstream not found
  LOG_DEBUG("Using fallback default remote: 'origin'");
  LOG_PERF(0, "Exit");
  return "origin";
}

bool setDefaultRemote(git_repository *repo, const QString &remoteName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Setting default remote to: '{}'", remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify the remote exists
  git_remote *remote = nullptr;
  int error = git_remote_lookup(&remote, repo, remoteName.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Remote '{}' not found: error={}",
              remoteName.toStdString(), error);
    setError(GitError::SetDefaultRemoteFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_remote_free(remote);

  // Set by updating configuration - set the push.default option or store in a
  // custom config We'll use a simple approach: store the default remote name in
  // a custom config variable
  git_config *config = nullptr;
  error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::SetDefaultRemoteFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_config_set_string(config, "sourcerer.defaultremote",
                                remoteName.toUtf8().constData());
  git_config_free(config);

  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set default remote in config: error={}", error);
    setError(GitError::SetDefaultRemoteFailedToWriteDefaultRemoteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully set default remote to: '{}'",
            remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool addRemote(git_repository *repo, const QString &remoteName,
               const QString &fetchUrl, const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding remote '{}' with fetch URL: '{}'", remoteName.toStdString(),
            fetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name and fetch URL cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create the remote
  git_remote *remote = nullptr;
  int error = git_remote_create(&remote, repo, remoteName.toUtf8().constData(),
                                fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to create remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::AddRemoteFailedToCreateRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL if different from fetch URL
  if (!pushUrl.isEmpty() && pushUrl != fetchUrl) {
    error = git_remote_set_pushurl(repo, remoteName.toUtf8().constData(),
                                   pushUrl.toUtf8().constData());
    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to set push URL for remote '{}': error={}",
                remoteName.toStdString(), error);
      setError(GitError::AddRemoteFailedToSetPushUrl, error);
      git_remote_free(remote);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_remote_free(remote);

  LOG_DEBUG("Successfully added remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool removeRemote(git_repository *repo, const QString &remoteName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Removing remote: '{}'", remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  int error = git_remote_delete(repo, remoteName.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to delete remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::RemoveRemoteFailedToDeleteRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully removed remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool updateRemote(git_repository *repo, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Updating remote '{}' -> '{}' with fetch URL: '{}'",
            remoteName.toStdString(), newRemoteName.toStdString(),
            fetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || newRemoteName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote names and fetch URL cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  QString effectiveRemoteName = remoteName;
  if (newRemoteName != remoteName) {
    git_strarray problems = {0};
    const int renameError =
        git_remote_rename(&problems, repo, remoteName.toUtf8().constData(),
                          newRemoteName.toUtf8().constData());
    if (renameError) {
      LOG_ERROR(
          Log::ErrorId::Git, "Failed to rename remote '{}' to '{}': error={}",
          remoteName.toStdString(), newRemoteName.toStdString(), renameError);
      setError(GitError::UpdateRemoteFailedToRenameRemote, renameError);
      LOG_PERF(0, "Exit");
      return false;
    }
    effectiveRemoteName = newRemoteName;
  }

  // Set fetch URL
  int error = git_remote_set_url(repo, effectiveRemoteName.toUtf8().constData(),
                                 fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::UpdateRemoteFailedToSetFetchUrl, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL
  error = git_remote_set_pushurl(repo, effectiveRemoteName.toUtf8().constData(),
                                 pushUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::UpdateRemoteFailedToSetPushUrl, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Successfully updated remote '{}'", remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool addTarget(git_repository *repo, const QString &remoteName,
               const QString &targetName, const QString &fetchUrl,
               const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding target '{}' to remote '{}' with fetch URL: '{}'",
            targetName.toStdString(), remoteName.toStdString(),
            fetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Remote name, target name, and fetch URL cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Prevent overwriting Default target
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Cannot add target with name 'Default' - use updateRemote instead");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::AddTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set fetch URL
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  error = git_config_set_string(config, fetchConfigKey.toUtf8().constData(),
                                fetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::AddTargetFailedToSetFetchUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_set_string(config, pushConfigKey.toUtf8().constData(),
                                pushUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::AddTargetFailedToSetPushUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config_free(config);

  LOG_DEBUG("Successfully added target '{}' to remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool editTarget(git_repository *repo, const QString &remoteName,
                const QString &targetName, const QString &newFetchUrl,
                const QString &newPushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Editing target '{}' on remote '{}' with new fetch URL: '{}'",
            targetName.toStdString(), remoteName.toStdString(),
            newFetchUrl.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty() || newFetchUrl.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Remote name, target name, and fetch URL cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Cannot edit Default target via this function
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot edit 'Default' target - use updateRemote instead");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::EditTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set fetch URL
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  error = git_config_set_string(config, fetchConfigKey.toUtf8().constData(),
                                newFetchUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set fetch URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::EditTargetFailedToSetFetchUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_set_string(config, pushConfigKey.toUtf8().constData(),
                                newPushUrl.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set push URL for target '{}' on remote '{}': "
              "error={}",
              targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::EditTargetFailedToSetPushUrlConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config_free(config);

  LOG_DEBUG("Successfully edited target '{}' on remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool deleteTarget(git_repository *repo, const QString &remoteName,
                  const QString &targetName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Deleting target '{}' from remote '{}'", targetName.toStdString(),
            remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name and target name cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Cannot delete Default target via this function
  if (targetName == QStringLiteral("Default")) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot delete 'Default' target - use removeRemote instead");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::DeleteTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Delete fetch URL
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  error = git_config_delete_entry(config, fetchConfigKey.toUtf8().constData());
  if (error && error != GIT_ENOTFOUND) {
    LOG_WARN("Failed to delete fetch URL config for target '{}' on remote "
             "'{}': error={}",
             targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::DeleteTargetFailedToDeleteFetchUrlConfig, error);
  }

  // Delete push URL
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);
  error = git_config_delete_entry(config, pushConfigKey.toUtf8().constData());
  if (error && error != GIT_ENOTFOUND) {
    LOG_WARN("Failed to delete push URL config for target '{}' on remote "
             "'{}': error={}",
             targetName.toStdString(), remoteName.toStdString(), error);
    setError(GitError::DeleteTargetFailedToDeletePushUrlConfig, error);
  }

  git_config_free(config);

  LOG_DEBUG("Successfully deleted target '{}' from remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool setDefaultTarget(git_repository *repo, const QString &remoteName,
                      const QString &targetName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Setting default target '{}' for remote '{}'",
            targetName.toStdString(), remoteName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (remoteName.isEmpty() || targetName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Remote name and target name cannot be empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (targetName == QStringLiteral("Default")) {
    LOG_DEBUG("Target '{}' is already the default", targetName.toStdString());
    LOG_PERF(0, "Exit");
    return true;
  }

  // Get config
  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository config: error={}",
              error);
    setError(GitError::SetDefaultTargetFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get the new default target's fetch and push URLs
  const QString fetchConfigKey = QStringLiteral("remote.%1.target.%2.fetchUrl")
                                     .arg(remoteName, targetName);
  const QString pushConfigKey =
      QStringLiteral("remote.%1.target.%2.pushUrl").arg(remoteName, targetName);

  git_config_iterator *iter = nullptr;

  error = git_config_iterator_new(&iter, config);
  if (error) {
    setError(GitError::SetDefaultTargetFailedToCreateConfigIterator, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Find the actual stored keys (they might be lowercase due to git config
  // normalization)
  char *newFetchUrl = nullptr;
  char *newPushUrl = nullptr;
  git_config_entry *entry = nullptr;

  const QString targetPrefix =
      QString("remote.%1.target.%2.").arg(remoteName, targetName);

  while ((error = git_config_next(&entry, iter)) == 0) {
    if (entry && entry->name) {
      const QString entryName = QString::fromUtf8(entry->name);
      LOG_DEBUG("Config entry: {}", entryName.toStdString());

      if (entryName.startsWith(targetPrefix, Qt::CaseInsensitive)) {
        const QString suffix = entryName.mid(targetPrefix.length());
        if (suffix.compare("fetchUrl", Qt::CaseInsensitive) == 0) {
          newFetchUrl = strdup(entry->value);
          LOG_DEBUG("Found fetch URL: {}", newFetchUrl ? newFetchUrl : "null");
        } else if (suffix.compare("pushUrl", Qt::CaseInsensitive) == 0) {
          newPushUrl = strdup(entry->value);
          LOG_DEBUG("Found push URL: {}", newPushUrl ? newPushUrl : "null");
        }
      }
    }
  }

  git_config_iterator_free(iter);

  if (!newFetchUrl) {
    if (newPushUrl) {
      free(newPushUrl);
    }
    LOG_ERROR(
        Log::ErrorId::Git,
        "Failed to get fetch URL for target '{}' on remote '{}': key not found",
        targetName.toStdString(), remoteName.toStdString());
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // If push URL not found, use fetch URL as fallback
  if (!newPushUrl) {
    newPushUrl = newFetchUrl;
    LOG_DEBUG("Push URL not set, using fetch URL as fallback");
  }

  // Save current default target's URLs before switching
  char *currentFetchUrl = nullptr;
  char *currentPushUrl = nullptr;

  // Iterate config to find current fetch and push URLs
  git_config_iterator *defaultIter = nullptr;
  error = git_config_iterator_new(&defaultIter, config);
  if (error) {
    LOG_WARN("Failed to create config iterator for default URLs: error={}",
             error);
  } else {
    git_config_entry *defaultEntry = nullptr;
    const QString defaultPrefix = QString("remote.%1.").arg(remoteName);

    while ((error = git_config_next(&defaultEntry, defaultIter)) == 0) {
      if (defaultEntry && defaultEntry->name) {
        const QString entryName = QString::fromUtf8(defaultEntry->name);

        if (entryName.startsWith(defaultPrefix, Qt::CaseInsensitive)) {
          const QString suffix = entryName.mid(defaultPrefix.length());
          if (suffix.compare("url", Qt::CaseInsensitive) == 0) {
            currentFetchUrl = strdup(defaultEntry->value);
            LOG_DEBUG("Found current fetch URL: {}",
                      currentFetchUrl ? currentFetchUrl : "null");
          } else if (suffix.compare("pushurl", Qt::CaseInsensitive) == 0) {
            currentPushUrl = strdup(defaultEntry->value);
            LOG_DEBUG("Found current push URL: {}",
                      currentPushUrl ? currentPushUrl : "null");
          }
        }
      }
    }
    git_config_iterator_free(defaultIter);
  }

  if (!currentFetchUrl) {
    if (newFetchUrl) {
      free(newFetchUrl);
    }
    if (newPushUrl) {
      free(newPushUrl);
    }
    if (currentPushUrl) {
      free(currentPushUrl);
    }

    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get current fetch URL for remote '{}': key not found",
              remoteName.toStdString());
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Set the old default target as a named target
  const QString oldFetchKey =
      QStringLiteral("remote.%1.target.Default.fetchUrl").arg(remoteName);
  error = git_config_set_string(config, oldFetchKey.toUtf8().constData(),
                                currentFetchUrl);
  if (error) {
    LOG_WARN("Failed to backup old default target for remote '{}': error={}",
             remoteName.toStdString(), error);
  }

  if (currentPushUrl) {
    const QString oldPushKey =
        QStringLiteral("remote.%1.target.Default.pushUrl").arg(remoteName);
    git_config_set_string(config, oldPushKey.toUtf8().constData(),
                          currentPushUrl);
  }

  // Set the new target as the default
  const QString defaultFetchKey =
      QStringLiteral("remote.%1.url").arg(remoteName);
  error = git_config_set_string(config, defaultFetchKey.toUtf8().constData(),
                                newFetchUrl);
  if (error) {
    if (newFetchUrl) {
      free(newFetchUrl);
    }
    if (newPushUrl) {
      free(newPushUrl);
    }
    if (currentFetchUrl) {
      free(currentFetchUrl);
    }
    if (currentPushUrl) {
      free(currentPushUrl);
    }

    LOG_ERROR(Log::ErrorId::Git,
              "Failed to set default fetch URL for remote '{}': error={}",
              remoteName.toStdString(), error);
    setError(GitError::SetDefaultTargetFailedToSetDefaultFetchUrl, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString defaultPushKey =
      QStringLiteral("remote.%1.pushurl").arg(remoteName);
  error = git_config_set_string(config, defaultPushKey.toUtf8().constData(),
                                newPushUrl);
  if (error) {
    LOG_WARN("Failed to set default push URL for remote '{}': error={}",
             remoteName.toStdString(), error);
    // Continue even if push URL setting fails
  }

  if (newFetchUrl) {
    free(newFetchUrl);
  }
  if (newPushUrl) {
    free(newPushUrl);
  }
  if (currentFetchUrl) {
    free(currentFetchUrl);
  }
  if (currentPushUrl) {
    free(currentPushUrl);
  }

  git_config_free(config);

  LOG_DEBUG("Successfully set default target to '{}' for remote '{}'",
            targetName.toStdString(), remoteName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

bool amend(git_repository *repo, const QString &summary, const QString &message,
           const QString &author, const QString &email, bool signoff,
           bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *commitToAmend = nullptr;
  git_oid commitOid;
  git_oid newCommitOid;
  git_tree *tree = nullptr;
  const git_signature *authorSig = nullptr;
  git_signature *committerSig = nullptr;

  // 1. Get the HEAD commit (the one to amend)
  int error = git_reference_name_to_id(&commitOid, repo, "HEAD");
  if (error) {
    setError(GitError::AmendFailedToResolveReferenceNameToId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commitToAmend, repo, &commitOid);
  if (error) {
    setError(GitError::AmendFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // 2. Get the tree to commit (your staged changes)
  git_index *index = nullptr;

  error = git_repository_index(&index, repo);
  if (error) {
    setError(GitError::AmendFailedToGetRepositoryIndex, error);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid treeOid;

  error = git_index_write_tree(&treeOid, index);
  if (error) {
    setError(GitError::AmendFailedToWriteIndexTree, error);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_tree_lookup(&tree, repo, &treeOid);
  if (error) {
    setError(GitError::AmendFailedToLookupTree, error);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  // 3. Create committer signature (or use nullptr to keep original)
  error = git_signature_now(&committerSig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::AmendFailedToCreateSignature, error);
    git_tree_free(tree);
    git_index_free(index);
    git_commit_free(commitToAmend);
    LOG_PERF(0, "Exit");
    return false;
  }

  authorSig = git_commit_author(commitToAmend);

  // 4. Amend the commit with the updated message and optional signoff
  const QString fullMessage =
      buildCommitMessage(summary, message, author, email, signoff);

  if (signCommit) {
    QVector<git_commit *> parents;
    const unsigned int parentCount = git_commit_parentcount(commitToAmend);
    parents.reserve(static_cast<int>(parentCount));

    for (unsigned int i = 0; i < parentCount; ++i) {
      git_commit *parent = nullptr;
      error = git_commit_parent(&parent, commitToAmend, i);
      if (error) {
        setError(GitError::AmendFailedToGetParentCommit, error);
        for (git_commit *loadedParent : parents) {
          git_commit_free(loadedParent);
        }
        git_signature_free(committerSig);
        git_tree_free(tree);
        git_index_free(index);
        git_commit_free(commitToAmend);
        LOG_PERF(0, "Exit");
        return false;
      }

      parents.push_back(parent);
    }

    CommitSigningConfig signingConfig;
    QString signingError;
    const bool signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::AmendFailedToLoadSigningConfiguration, -1);
      for (git_commit *loadedParent : parents) {
        git_commit_free(loadedParent);
      }
      git_signature_free(committerSig);
      git_tree_free(tree);
      git_index_free(index);
      git_commit_free(commitToAmend);
      LOG_PERF(0, "Exit");
      return false;
    }

    const bool doSigning = signingConfig.enabled;
    if (doSigning) {
      CommitSigningPayload signingPayload;
      signingPayload.config = signingConfig;
      signingPayload.errorMessage = &signingError;

      git_commit_create_ext_options commitOpts =
          GIT_COMMIT_CREATE_EXT_OPTIONS_INIT;
      commitOpts.sign = commitSigningCallback;
      commitOpts.payload = &signingPayload;

      error = git_commit_create_ext(
          &newCommitOid, repo, authorSig, committerSig,
          fullMessage.toStdString().c_str(), tree, parents.size(),
          parents.isEmpty() ? nullptr : parents.data(), &commitOpts);
      if (!error) {
        git_reference *headRef = nullptr;
        error = git_repository_head(&headRef, repo);
        if (!error) {
          git_reference *updatedRef = nullptr;
          error = git_reference_set_target(&updatedRef, headRef, &newCommitOid,
                                           "Amend commit");
          if (updatedRef) {
            git_reference_free(updatedRef);
          }
          git_reference_free(headRef);
        }
      }
    } else {
      error = git_commit_amend(
          &newCommitOid,
          commitToAmend, // Commit to amend
          "HEAD",        // Update HEAD reference
          authorSig,     // Keep original author (or provide new one)
          committerSig,  // New committer (or nullptr to keep original)
          nullptr,       // Use default encoding
          fullMessage.toStdString().c_str(), // Use updated message
          tree // New tree (or nullptr to keep original)
      );
    }

    for (git_commit *loadedParent : parents) {
      git_commit_free(loadedParent);
    }

    if (error) {
      if (doSigning && !signingError.isEmpty()) {
        LOG_ERROR(Log::ErrorId::Git, "Failed to sign commit: {}",
                  signingError.toStdString());
        setError(GitError::AmendFailedToSignCommit, -1);
      } else {
        setError(GitError::AmendFailedToCreateSignedAmendCommit, error);
      }

      git_signature_free(committerSig);
      git_tree_free(tree);
      git_index_free(index);
      git_commit_free(commitToAmend);
      LOG_PERF(0, "Exit");
      return false;
    }
  } else {
    error = git_commit_amend(
        &newCommitOid,
        commitToAmend, // Commit to amend
        "HEAD",        // Update HEAD reference
        authorSig,     // Keep original author (or provide new one)
        committerSig,  // New committer (or nullptr to keep original)
        nullptr,       // Use default encoding
        fullMessage.toStdString().c_str(), // Use updated message
        tree // New tree (or nullptr to keep original)
    );
    if (error) {
      setError(GitError::AmendFailedToAmendCommit, error);
      git_signature_free(committerSig);
      git_tree_free(tree);

      git_index_free(index);
      git_commit_free(commitToAmend);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  // Cleanup
  git_signature_free(committerSig);
  git_tree_free(tree);
  git_index_free(index);
  git_commit_free(commitToAmend);

  LOG_PERF(0, "Exit");

  return true;
}

bool getIsRebaseInProgress(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const int state = git_repository_state(repo);

  LOG_PERF(0, "Exit");

  return state == GIT_REPOSITORY_STATE_REBASE ||
         state == GIT_REPOSITORY_STATE_REBASE_INTERACTIVE ||
         state == GIT_REPOSITORY_STATE_REBASE_MERGE;
  ;
}

bool commitRebase(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Creating commit: summary='{}', author='{}', email='{}'",
            summary.toStdString(), author.toStdString(), email.toStdString());
  LOG_DEBUG("Message length: {}, cleanup: {}, signoff: {}", message.length(),
            cleanup, signoff);

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  CommitSigningConfig signingConfig;
  QString signingError;
  bool signingConfigLoaded = true;
  if (signCommit) {
    signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::CommitStagedFailedToLoadSigningConfiguration, -1);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  CommitSigningPayload signingPayload;
  signingPayload.config = signingConfig;
  signingPayload.errorMessage = &signingError;
  signingPayload.repo = repo;

  git_rebase *rebase = nullptr;
  git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;
  rebase_opts.commit_create_cb = commitRebaseSigningCallback;
  rebase_opts.payload = &signingPayload;

  int error = git_rebase_open(&rebase, repo, &rebase_opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open rebase session: error={}",
              error);
    setError(GitError::CommitStagedFailedToOpenRebaseSession, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Preparing commit: loading index and building tree");
  git_index *index = nullptr;
  git_oid treeId;
  git_signature *sig = nullptr;

  // Load index
  error = git_repository_index(&index, repo);
  if (error) {
    git_rebase_free(rebase);
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository index: error={}",
              error);
    setError(GitError::CommitStagedFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write index to tree
  error = git_index_write_tree(&treeId, index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to write index to tree: error={}",
              error);
    setError(GitError::CommitStagedFailedToWriteIndexTree, error);
    git_rebase_free(rebase);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Tree prepared successfully");

  // Create signature
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CommitStagedFailedToCreateSignature, error);
    git_rebase_free(rebase);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString fullMessage =
      buildCommitMessage(summary, message, author, email, signoff);

  git_oid oid;

  error = git_rebase_commit(&oid, rebase, sig, sig, nullptr,
                            fullMessage.toStdString().c_str());
  if (error) {
    setError(GitError::CommitStagedFailedToCreateSignature, error);
    git_rebase_free(rebase);
    git_signature_free(sig);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_rebase_operation *operation;
  while ((error = git_rebase_next(&operation, rebase)) == 0) {
    git_rebase_commit(&oid, rebase, sig, sig, nullptr, nullptr);
  }

  if (error == GIT_ITEROVER) {
    git_rebase_finish(rebase, sig);
  }

  git_rebase_free(rebase);
  git_signature_free(sig);
  git_index_free(index);

  if (cleanup && error == 0) {
    error = git_repository_state_cleanup(repo);
    if (error) {
      setError(GitError::CommitStagedFailedToCleanupRepositoryState, error);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");

  return error == 0;
}

bool abortRebase(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const int state = git_repository_state(repo);

  if (state != GIT_REPOSITORY_STATE_REBASE &&
      state != GIT_REPOSITORY_STATE_REBASE_INTERACTIVE &&
      state != GIT_REPOSITORY_STATE_REBASE_MERGE) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_rebase *rebase = nullptr;
  git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;

  int error = git_rebase_open(&rebase, repo, &rebase_opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open rebase session: error={}",
              error);
    setError(GitError::CommitStagedFailedToOpenRebaseSession, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_rebase_abort(rebase);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to abort rebase session: error={}",
              error);
    setError(GitError::CommitStagedFailedToAbortRebaseSession, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return error == 0;
}

} // namespace GitRepo