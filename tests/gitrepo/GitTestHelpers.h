// Helpers shared by the GitRepo test suites: the git executable for setting
// up repositories, file access and an isolated git configuration.

#pragma once

#include <GitRepo.h>

#include <ConfigKey.h>
#include <ConfigManager.h>

#include <git2.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QStringList>
#include <QtTest>

#include <memory>

namespace GitTest {

inline const QString kAuthor = QStringLiteral("Tester");
inline const QString kEmail = QStringLiteral("tester@example.com");
inline const QString kOtherAuthor = QStringLiteral("Other Author");
inline const QString kOtherEmail = QStringLiteral("other@example.com");
inline const QString kNullCommit = QString(40, QLatin1Char('0'));

inline bool writeFile(const QString &path, const QByteArray &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(content) == content.size();
}

inline QByteArray readFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

inline QString lastErrorMessage() {
  return GitRepo::getLastError().errorMessage;
}

inline GitRepo::GitError lastError() { return GitRepo::getLastError().error; }

// Non-context lines of a delta as origin + content, e.g. "-old", "+new".
inline QStringList changedLines(const GitRepo::FilesDiffDelta &delta) {
  QStringList lines;
  for (const GitRepo::FilesDiffHunk &hunk : delta.hunks) {
    for (const GitRepo::FilesDiffLine &line : hunk.lines) {
      if (line.origin == QLatin1Char('+') || line.origin == QLatin1Char('-')) {
        lines += line.origin + line.content;
      }
    }
  }
  return lines;
}

struct RepoCloser {
  void operator()(git_repository *repo) const {
    GitRepo::closeRepository(repo);
  }
};

using RepoPtr = std::unique_ptr<git_repository, RepoCloser>;

// Git and libgit2 must not read the configuration of the user running the
// tests: the global and system levels point into `home`.
class IsolatedConfig {
public:
  void setUp(const QString &home) {
    _home = home;
    QDir().mkpath(home);
    _globalConfig = home + QStringLiteral("/.gitconfig");
    writeFile(_globalConfig, QByteArray());
    const QByteArray native = QDir::toNativeSeparators(home).toUtf8();
    git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL,
                     native.constData());
    git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG,
                     native.constData());
    git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_SYSTEM,
                     native.constData());
    git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_PROGRAMDATA,
                     native.constData());
  }

  QString home() const { return _home; }
  QString globalConfig() const { return _globalConfig; }

  QProcessEnvironment environment() const {
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("GIT_CONFIG_GLOBAL"), _globalConfig);
    env.insert(QStringLiteral("GIT_CONFIG_NOSYSTEM"), QStringLiteral("1"));
    env.insert(QStringLiteral("GIT_TERMINAL_PROMPT"), QStringLiteral("0"));
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    env.insert(QStringLiteral("HOME"), _home);
    env.remove(QStringLiteral("GIT_DIR"));
    env.remove(QStringLiteral("GIT_WORK_TREE"));
    return env;
  }

private:
  QString _home;
  QString _globalConfig;
};

// Runs git in `dir`. Fails loudly unless `ok` is given.
inline QString gitIn(const IsolatedConfig &config, const QString &dir,
                     const QStringList &args, bool *ok = nullptr) {
  QProcess process;
  process.setWorkingDirectory(dir);
  process.setProcessEnvironment(config.environment());
  process.start(QStringLiteral("git"),
                QStringList{QStringLiteral("-c"),
                            QStringLiteral("user.name=") + kAuthor,
                            QStringLiteral("-c"),
                            QStringLiteral("user.email=") + kEmail,
                            QStringLiteral("-c"),
                            QStringLiteral("core.autocrlf=false"),
                            QStringLiteral("-c"),
                            QStringLiteral("init.defaultBranch=main"),
                            QStringLiteral("-c"),
                            QStringLiteral("protocol.file.allow=always")} +
                    args);
  const bool success = process.waitForFinished(60 * 1000) &&
                       process.exitStatus() == QProcess::NormalExit &&
                       process.exitCode() == 0;
  if (ok) {
    *ok = success;
  } else if (!success) {
    qWarning() << "git" << args << "failed in" << dir << ":"
               << process.readAllStandardError();
  }
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

// Makes every file below `dir` writable. Git keeps its objects read-only and
// QTemporaryDir can't remove them on Windows otherwise.
inline void makeWritable(const QString &dir) {
  QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::System,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    QFile::setPermissions(it.next(), QFile::ReadOwner | QFile::WriteOwner);
  }
}

// Sets a ConfigManager value for the lifetime of the object.
class ConfigOverride {
public:
  ConfigOverride(ConfigManager::ConfigKey key, qlonglong value)
      : _key(key), _kind(Kind::Long) {
    auto &config = ConfigManager::ConfigManager::getInstance();
    _previousLong = config.getLong(key);
    config.setLong(key, value);
  }
  ConfigOverride(ConfigManager::ConfigKey key, int value)
      : ConfigOverride(key, static_cast<qlonglong>(value)) {}
  ConfigOverride(ConfigManager::ConfigKey key, const char *value) = delete;
  ConfigOverride(ConfigManager::ConfigKey key, const QString &value)
      : _key(key), _kind(Kind::String) {
    auto &config = ConfigManager::ConfigManager::getInstance();
    _previousString = config.getString(key);
    config.setString(key, value);
  }
  ConfigOverride(ConfigManager::ConfigKey key, bool value)
      : _key(key), _kind(Kind::Boolean) {
    auto &config = ConfigManager::ConfigManager::getInstance();
    _previousBoolean = config.getBoolean(key);
    config.setBoolean(key, value);
  }
  ~ConfigOverride() {
    auto &config = ConfigManager::ConfigManager::getInstance();
    switch (_kind) {
    case Kind::Long:
      config.setLong(_key, _previousLong);
      break;
    case Kind::String:
      config.setString(_key, _previousString);
      break;
    case Kind::Boolean:
      config.setBoolean(_key, _previousBoolean);
      break;
    }
  }
  ConfigOverride(const ConfigOverride &) = delete;
  ConfigOverride &operator=(const ConfigOverride &) = delete;

private:
  enum class Kind { Long, String, Boolean };

  ConfigManager::ConfigKey _key;
  Kind _kind;
  qlonglong _previousLong = 0;
  QString _previousString;
  bool _previousBoolean = false;
};

// GitCredentialStorage keeps its master key in the OS keychain. Tests use a
// fixed one instead, so they don't add entries to the user's keychain.
inline void useTestCredentialsKey() {
  GitRepo::Testing::setCredentialsMasterKey(
      QByteArray("sourcerer-tests-credentials-key!"));
}

// Full message of a commit as stored, read with libgit2.
inline QString commitMessage(git_repository *repo, const QString &revision) {
  git_object *object = nullptr;
  if (git_revparse_single(&object, repo, revision.toUtf8().constData()) != 0) {
    return {};
  }
  git_commit *commit = nullptr;
  QString message;
  if (git_object_peel(reinterpret_cast<git_object **>(&commit), object,
                      GIT_OBJECT_COMMIT) == 0) {
    message = QString::fromUtf8(git_commit_message(commit));
    git_commit_free(commit);
  }
  git_object_free(object);
  return message;
}

} // namespace GitTest
