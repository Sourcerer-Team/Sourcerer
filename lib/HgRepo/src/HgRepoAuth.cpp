#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace HgRepo {

namespace {

thread_local HgCredentialsProvider *gCredentialsProvider = nullptr;

QMutex gSshConfigMutex;
QString gSshConfigContent;

QString quoted(const QString &path) {
  return '"' + QDir::toNativeSeparators(path) + '"';
}

QString sshExecutable() {
  QString ssh = QStandardPaths::findExecutable(QStringLiteral("ssh"));
#ifdef Q_OS_WIN
  if (ssh.isEmpty()) {
    const QString system =
        qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows")) +
        QStringLiteral("/System32/OpenSSH/ssh.exe");
    if (QFileInfo(system).isExecutable()) {
      ssh = system;
    }
  }
#endif
  return ssh;
}

// Writes content to Sourcerer's ssh config file when it changed. Returns its
// path, empty when it can't be written.
QString writeSshConfig(const QString &content) {
  const QString path =
      QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
      QStringLiteral("/hg/ssh_config");

  QMutexLocker locker(&gSshConfigMutex);
  if (content == gSshConfigContent && QFileInfo::exists(path)) {
    return path;
  }

  QDir().mkpath(QFileInfo(path).absolutePath());
  QSaveFile file(path);
  if (!file.open(QIODevice::WriteOnly) || file.write(content.toUtf8()) < 0 ||
      !file.commit()) {
    return QString();
  }
  gSshConfigContent = content;
  return path;
}

// Credentials rejected this many times end the command.
constexpr int kMaxAuthAttempts = 5;

// The [auth] prefix of a remote: its URL without user info, query and
// fragment. hg compares it with the start of the URL it connects to.
QString authPrefix(const QString &url) {
  QUrl parsed(url);
  if (!parsed.isValid() || parsed.scheme().isEmpty()) {
    return url;
  }
  parsed.setUserInfo(QString());
  parsed.setQuery(QString());
  parsed.setFragment(QString());
  return parsed.toString(QUrl::FullyEncoded | QUrl::StripTrailingSlash);
}

} // namespace

void setCredentialsProvider(HgCredentialsProvider *provider) {
  gCredentialsProvider = provider;
}

namespace Private {

QStringList sshConfigArgs(const std::function<QString()> &userSsh) {
  if (!gCredentialsProvider) {
    return {};
  }

  QString content =
      QStringLiteral("# Written by Sourcerer for Mercurial: the keys chosen "
                     "in SSH keys.\n\n");
  int keys = 0;
  for (const HgSshKey &key : gCredentialsProvider->sshKeys()) {
    if (key.host.isEmpty() || !QFileInfo::exists(key.privateKeyPath)) {
      continue;
    }
    content +=
        QStringLiteral("Host %1\n  IdentityFile \"%2\"\n"
                       "  IdentitiesOnly yes\n\n")
            .arg(key.host, QDir::fromNativeSeparators(key.privateKeyPath));
    ++keys;
  }
  if (keys == 0) {
    return {};
  }

  // -F replaces the user's config, so it is included for everything else.
  const QString userConfig = QDir::homePath() + QStringLiteral("/.ssh/config");
  if (QFileInfo::exists(userConfig)) {
    content += QStringLiteral("Match all\nInclude \"%1\"\n").arg(userConfig);
  }

  // PuTTY's plink (TortoiseHg's default) reads .ppk keys and no ssh config.
  const QString user = userSsh().trimmed();
  if (user.contains(QLatin1String("plink"), Qt::CaseInsensitive)) {
    return {};
  }

  QString base = user;
  if (base.isEmpty()) {
    const QString ssh = sshExecutable();
    if (ssh.isEmpty()) {
      return {};
    }
    base = quoted(ssh);
  }

  const QString path = writeSshConfig(content);
  if (path.isEmpty()) {
    return {};
  }

  // Without an askpass program ssh would wait for a passphrase or a host key
  // answer nobody can give, BatchMode makes it fail instead.
  return {
      QStringLiteral("--config"),
      QStringLiteral("ui.ssh=%1 -F %2%3")
          .arg(base, quoted(path),
               hasAskPass() ? QString() : QStringLiteral(" -o BatchMode=yes"))};
}

QString withSshHint(const QString &message) {
  if (message.contains(QLatin1String("Permission denied (publickey"))) {
    return message +
           QStringLiteral("\n\nThe SSH server didn't accept a key. Choose the "
                          "key for this host in SSH keys.") +
           (hasAskPass()
                ? QString()
                : QStringLiteral(" A key with a passphrase must be loaded in "
                                 "ssh-agent or Pageant."));
  }
  if (message.contains(QLatin1String("Host key verification failed"))) {
    return message +
           QStringLiteral("\n\nThe server's SSH host key isn't known yet. "
                          "Trust it in SSH keys, where the key for this host "
                          "is chosen, or connect to it once with ssh to check "
                          "and accept it.");
  }
  return message;
}

QStringList authConfigArgs(const QVector<HgAuthEntry> &entries) {
  QStringList args;
  for (int i = 0; i < entries.size(); ++i) {
    const HgAuthEntry &entry = entries.at(i);
    const QString group = QStringLiteral("auth.sourcerer%1.").arg(i);
    args << QStringLiteral("--config")
         << group + QStringLiteral("prefix=") + entry.prefix;
    args << QStringLiteral("--config")
         << group + QStringLiteral("username=") + entry.username;
    args << QStringLiteral("--config")
         << group + QStringLiteral("password=") + entry.password;
  }
  return args;
}

AuthFailure authFailure(const QString &errorText, const bool sentCredentials) {
  // "abort: http authorization required for <url>" when hg has no
  // credentials for the remote, "abort: authorization failed" when the remote
  // rejected the ones sent.
  static const QRegularExpression required(
      QStringLiteral("http authorization required for (\\S+)"));
  const QRegularExpressionMatch match = required.match(errorText);
  if (match.hasMatch()) {
    return {true, authPrefix(match.captured(1))};
  }
  if (errorText.contains(QLatin1String("authorization failed"))) {
    return {true, QString()};
  }
  // A remote may refuse an account it knows with 403.
  if (sentCredentials && errorText.contains(QLatin1String("HTTP Error 403"))) {
    return {true, QString()};
  }
  return {};
}

bool runAuthenticated(const QString &repoPath, QVector<HgAuthEntry> *entries,
                      const QStringList &baseConfigArgs,
                      const std::function<QString()> &fallbackUrl,
                      const std::function<bool(const QStringList &configArgs,
                                               QString *errorText)> &run) {
  HgAuthRequest pending;
  bool asked = false;

  for (;;) {
    QString errorText;
    const bool success =
        run(baseConfigArgs + authConfigArgs(*entries), &errorText);
    if (success) {
      if (asked) {
        gCredentialsProvider->finished(pending, HgAuthResult::Accepted);
      }
      return true;
    }

    AuthFailure failure = authFailure(errorText, !entries->isEmpty());
    if (!failure.isAuthFailure) {
      // The credentials got the command past the remote's authorization.
      if (asked) {
        gCredentialsProvider->finished(pending, HgAuthResult::Accepted);
      }
      return false;
    }

    // "authorization failed" names no remote: it is the one just asked
    // about, or the one whose remembered credentials were sent.
    if (failure.url.isEmpty()) {
      if (asked) {
        failure.url = pending.url;
      } else if (!entries->isEmpty()) {
        failure.url = entries->last().prefix;
      } else if (fallbackUrl) {
        failure.url = authPrefix(fallbackUrl());
      }
    }
    if (failure.url.isEmpty()) {
      return false;
    }

    // Credentials sent for this remote were rejected.
    int attempt = 0;
    const auto known = std::find_if(
        entries->begin(), entries->end(),
        [&](const HgAuthEntry &e) { return e.prefix == failure.url; });
    if (known != entries->end()) {
      entries->erase(known);
      attempt = 1;
    }
    if (asked && pending.url == failure.url) {
      attempt = pending.attempt + 1;
      if (attempt >= kMaxAuthAttempts) {
        gCredentialsProvider->finished(pending, HgAuthResult::GaveUp);
        setLastError(HgError::AuthenticationFailed, -1,
                     QStringLiteral("The remote rejected the credentials for "
                                    "%1.")
                         .arg(failure.url));
        return false;
      }
      gCredentialsProvider->finished(pending, HgAuthResult::Rejected);
    } else if (asked) {
      // Another remote, e.g. a subrepo on another host.
      gCredentialsProvider->finished(pending, HgAuthResult::Accepted);
    }

    if (!gCredentialsProvider) {
      setLastError(HgError::AuthenticationFailed, -1,
                   QStringLiteral("%1 needs authorization.").arg(failure.url));
      return false;
    }

    pending = HgAuthRequest{failure.url, repoPath, attempt};
    asked = true;

    HgCredentials credentials;
    if (!gCredentialsProvider->credentials(pending, &credentials)) {
      setLastError(HgError::AuthenticationFailed, -1,
                   QStringLiteral("Authorization for %1 was cancelled.")
                       .arg(failure.url));
      return false;
    }

    entries->append(
        HgAuthEntry{failure.url, credentials.username, credentials.password});
  }
}

} // namespace Private

} // namespace HgRepo
