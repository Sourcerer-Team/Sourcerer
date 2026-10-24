#include "SshKnownHosts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTemporaryDir>

namespace GitRepo::SshKnownHosts {

namespace {

// An OpenSSH tool, from PATH or Windows' own OpenSSH.
QString sshTool(const QString &name) {
  QString tool = QStandardPaths::findExecutable(name);
#ifdef Q_OS_WIN
  if (tool.isEmpty()) {
    const QString system =
        qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows")) +
        QStringLiteral("/System32/OpenSSH/") + name + QStringLiteral(".exe");
    if (QFileInfo(system).isExecutable()) {
      tool = system;
    }
  }
#endif
  return tool;
}

struct ToolResult {
  bool started = false;
  int exitCode = -1;
  QByteArray out;
  QByteArray err;
};

ToolResult runTool(const QString &program, const QStringList &args,
                   const int timeoutMs) {
  ToolResult result;
  QProcess process;
#ifdef Q_OS_WIN
  process.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *arguments) {
        // CREATE_NO_WINDOW
        arguments->flags |= 0x08000000;
      });
#endif
  process.start(program, args, QIODevice::ReadOnly);
  if (!process.waitForStarted()) {
    return result;
  }
  result.started = true;
  if (!process.waitForFinished(timeoutMs)) {
    process.kill();
    process.waitForFinished();
    return result;
  }
  result.out = process.readAllStandardOutput();
  result.err = process.readAllStandardError();
  result.exitCode =
      process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
  return result;
}

} // namespace

QString defaultFile() {
  return QDir::homePath() + QStringLiteral("/.ssh/known_hosts");
}

bool isKnown(const QString &host, const QString &knownHostsFile) {
  if (!QFileInfo::exists(knownHostsFile)) {
    return false;
  }
  const QString keygen = sshTool(QStringLiteral("ssh-keygen"));
  if (keygen.isEmpty()) {
    return false;
  }
  const ToolResult result =
      runTool(keygen,
              {QStringLiteral("-F"), host, QStringLiteral("-f"),
               QDir::toNativeSeparators(knownHostsFile)},
              10 * 1000);
  return result.exitCode == 0 && !result.out.trimmed().isEmpty();
}

QVector<HostKey> scan(const QString &host, QString *error, const int port,
                      const int timeoutSeconds) {
  const QString keyscan = sshTool(QStringLiteral("ssh-keyscan"));
  const QString keygen = sshTool(QStringLiteral("ssh-keygen"));
  if (keygen.isEmpty()) {
    *error = QStringLiteral("ssh-keygen was not found. Install OpenSSH.");
    return {};
  }

  ToolResult scanned;
  if (!keyscan.isEmpty()) {
    scanned = runTool(keyscan,
                      {QStringLiteral("-T"), QString::number(timeoutSeconds),
                       QStringLiteral("-p"), QString::number(port), host},
                      (timeoutSeconds + 5) * 1000);
  }
  QStringList lines;
  for (const QString &line :
       QString::fromUtf8(scanned.out).split('\n', Qt::SkipEmptyParts)) {
    const QString trimmed = line.trimmed();
    if (!trimmed.isEmpty() && !trimmed.startsWith('#')) {
      lines << trimmed;
    }
  }
  // ssh-keyscan of Windows' OpenSSH 9.5 fails on hosts offering the
  // sntrup761 key exchange (e.g. github.com). ssh itself records the host key
  // in a scratch known_hosts file when it connects; it doesn't authenticate.
  const QString ssh = sshTool(QStringLiteral("ssh"));
  if (lines.isEmpty() && !ssh.isEmpty()) {
    // ssh replaces the file with a rename, it must not exist yet.
    QTemporaryDir scratchDir;
    if (scratchDir.isValid()) {
      const QString scratch =
          scratchDir.filePath(QStringLiteral("known_hosts"));
      const ToolResult connected = runTool(
          ssh,
          {QStringLiteral("-o"), QStringLiteral("BatchMode=yes"),
           QStringLiteral("-o"),
           QStringLiteral("StrictHostKeyChecking=accept-new"),
           QStringLiteral("-o"),
           QStringLiteral("UserKnownHostsFile=") +
               QDir::fromNativeSeparators(scratch),
           QStringLiteral("-o"), QStringLiteral("GlobalKnownHostsFile=none"),
           QStringLiteral("-o"), QStringLiteral("HashKnownHosts=no"),
           QStringLiteral("-o"),
           QStringLiteral("PreferredAuthentications=none"),
           QStringLiteral("-o"),
           QStringLiteral("ConnectTimeout=") + QString::number(timeoutSeconds),
           QStringLiteral("-p"), QString::number(port), host,
           QStringLiteral("exit")},
          (timeoutSeconds + 5) * 1000);
      QFile recorded(scratch);
      if (recorded.open(QIODevice::ReadOnly)) {
        for (const QString &line : QString::fromUtf8(recorded.readAll())
                                       .split('\n', Qt::SkipEmptyParts)) {
          const QString trimmed = line.trimmed();
          if (!trimmed.isEmpty() && !trimmed.startsWith('#')) {
            lines << trimmed;
          }
        }
      }
      if (lines.isEmpty() && !connected.err.trimmed().isEmpty()) {
        scanned.err = connected.err;
      }
    }
  }

  if (lines.isEmpty()) {
    const QString details = QString::fromUtf8(scanned.err).trimmed();
    *error =
        details.isEmpty()
            ? QStringLiteral("%1 didn't answer on port %2.").arg(host).arg(port)
            : details;
    return {};
  }

  // One fingerprint line per key, in the same order:
  // "256 SHA256:... host (ED25519)".
  // A plain file: ssh-keygen can't open a QTemporaryFile on Windows.
  QTemporaryDir listDir;
  QFile file(listDir.filePath(QStringLiteral("host_keys")));
  if (!listDir.isValid() || !file.open(QIODevice::WriteOnly)) {
    *error = QStringLiteral("Can't write a temporary file.");
    return {};
  }
  file.write(lines.join('\n').toUtf8() + '\n');
  file.close();
  const ToolResult listed = runTool(keygen,
                                    {QStringLiteral("-l"), QStringLiteral("-f"),
                                     QDir::toNativeSeparators(file.fileName())},
                                    10 * 1000);
  const QStringList fingerprints =
      QString::fromUtf8(listed.out).split('\n', Qt::SkipEmptyParts);

  static const QRegularExpression fingerprintLine(
      QStringLiteral("^\\d+\\s+(\\S+)\\s.*\\((\\S+)\\)\\s*$"));
  QVector<HostKey> keys;
  for (int i = 0; i < lines.size(); ++i) {
    HostKey key;
    key.line = lines.at(i);
    const QRegularExpressionMatch match =
        fingerprintLine.match(fingerprints.value(i).trimmed());
    if (match.hasMatch()) {
      key.fingerprint = match.captured(1);
      key.type = match.captured(2);
    } else {
      key.type = key.line.section(' ', 1, 1);
    }
    keys.append(key);
  }
  return keys;
}

bool add(const QVector<HostKey> &keys, QString *error,
         const QString &knownHostsFile) {
  QDir().mkpath(QFileInfo(knownHostsFile).absolutePath());

  // A file without a final newline gets one first.
  QByteArray text;
  QFile existing(knownHostsFile);
  if (existing.open(QIODevice::ReadOnly)) {
    const QByteArray content = existing.readAll();
    if (!content.isEmpty() && !content.endsWith('\n')) {
      text += '\n';
    }
    existing.close();
  }
  for (const HostKey &key : keys) {
    text += key.line.toUtf8() + '\n';
  }

  QFile file(knownHostsFile);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append) ||
      file.write(text) != text.size()) {
    *error =
        QStringLiteral("Can't write %1: %2")
            .arg(QDir::toNativeSeparators(knownHostsFile), file.errorString());
    return false;
  }
  return true;
}

} // namespace GitRepo::SshKnownHosts
