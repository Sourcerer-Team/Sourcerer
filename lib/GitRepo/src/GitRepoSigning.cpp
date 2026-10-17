#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>

#define THREAD_ID "GitRepo"

// Commit signatures: signing commits and reading their signatures.
namespace GitRepo {

using namespace Private;

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

namespace Private {

bool loadCommitSignatureInfo(git_repository *repo, const git_oid *oid,
                             QString *signature, QString *signatureType) {
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

} // namespace Private

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

namespace Private {

bool loadCommitSigningConfig(CommitSigningConfig *config,
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

} // namespace Private

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

namespace Private {

int commitSigningCallback(git_commitbuilder *builder, git_repository *repo,
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

int commitRebaseSigningCallback(git_oid *out, const git_signature *author,
                                const git_signature *committer,
                                const char *message_encoding,
                                const char *message, const git_tree *tree,
                                size_t parent_count,
                                const git_commit *parents[], void *payload) {
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

} // namespace Private

} // namespace GitRepo
