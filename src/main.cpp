#include <ComponentManager.h>
#include <GitRepoManager.h>
#include <GuiManager.h>
#include <NewsTracker.h>
#include <SingleInstanceGuard.h>
#include <SvnRepoManager.h>
#include <UpdateManager.h>
#include <UpdateStaging.h>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStringList>
#include <QtCore/Qt>

namespace {

#ifdef Q_OS_LINUX
QString resolveRunningAppImagePath() {
  const QString envAppImage =
      QString::fromLocal8Bit(qgetenv("ARGV0")).trimmed();
  if (!envAppImage.isEmpty()) {
    const QFileInfo envInfo(envAppImage);
    if (envInfo.exists() && envInfo.isFile() &&
        envInfo.fileName().endsWith(QStringLiteral(".AppImage"),
                                    Qt::CaseInsensitive)) {
      return envAppImage;
    }
  }

  return QString();
}
#endif

QString installationDirectory() {
#ifdef Q_OS_LINUX
  const QString runningAppImagePath = resolveRunningAppImagePath();
  if (!runningAppImagePath.isEmpty()) {
    return QFileInfo(runningAppImagePath).absolutePath();
  }
#endif
  return QCoreApplication::applicationDirPath();
}

QString stagedUpdateFile(const QString &installDir) {
#ifdef Q_OS_MACOS
  Q_UNUSED(installDir);
  return Util::UpdateStaging::macosStagedUpdateFile();
#else
#ifdef Q_OS_LINUX
  const QString pattern = QStringLiteral("Sourcerer-Update*.AppImage");
#else
  const QString pattern = QStringLiteral("Sourcerer-Update*.exe");
#endif
  const QDir dir(installDir);
  const QFileInfoList files =
      dir.entryInfoList({pattern}, QDir::Files | QDir::Readable, QDir::Time);
  if (files.isEmpty()) {
    return QString();
  }

  return files.first().absoluteFilePath();
#endif
}

QString launcherPath() {
  const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
  return QDir(appDir).absoluteFilePath(QStringLiteral("SourcererLauncher.exe"));
#else
  return QDir(appDir).absoluteFilePath(QStringLiteral("SourcererLauncher"));
#endif
}

bool launcherAvailable() {
  return QFileInfo::exists(launcherPath());
}

bool launchLauncherDetached(const QStringList &arguments) {
  return QProcess::startDetached(launcherPath(), arguments,
                                 QCoreApplication::applicationDirPath());
}

void appendChromiumFlag(QByteArray &chromiumFlags, const QByteArray &flag) {
  if (chromiumFlags.contains(flag)) {
    return;
  }

  if (!chromiumFlags.isEmpty() && !chromiumFlags.endsWith(' ')) {
    chromiumFlags.append(' ');
  }
  chromiumFlags.append(flag);
}

QByteArray chromiumFlagsWithWebEngineLogging() {
  QByteArray chromiumFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  return chromiumFlags;
}

struct WebEngineLoggingInitializer {
  WebEngineLoggingInitializer() {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlagsWithWebEngineLogging());
  }
};

const WebEngineLoggingInitializer webEngineLoggingInitializer;

// Bundled tools live under <application directory>/3rdparty/<tool> in every
// package, so the app never depends on system-installed git or svn.
void prependBundledToolToPath(const QString &toolSubdir) {
  const QString toolBin = QDir::toNativeSeparators(
      QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(toolSubdir));
  if (!QFileInfo::exists(toolBin)) {
    return;
  }

  // Prepended so the bundled tool wins; the existing PATH stays as fallback.
  const QString current = QString::fromLocal8Bit(qgetenv("PATH"));
  const QString updated =
      current.isEmpty() ? toolBin
                        : toolBin + QString(QDir::listSeparator()) + current;
  qputenv("PATH", updated.toLocal8Bit());
}

void prependBundledToolsToPath() {
  // MinGit keeps git.exe in cmd/ and its DLLs in mingw64/bin, so cmd/ is used there.
#ifdef Q_OS_WIN
  prependBundledToolToPath(QStringLiteral("3rdparty/git/cmd"));
#else
  prependBundledToolToPath(QStringLiteral("3rdparty/git/bin"));
#endif
  prependBundledToolToPath(QStringLiteral("3rdparty/svn/bin"));
}

#ifdef Q_OS_MACOS
// Applications started from Finder/Dock inherit launchd's minimal PATH
// (/usr/bin:/bin:/usr/sbin:/sbin), which lacks the locations where Homebrew
// installs git-lfs and friends. They are appended, so system tools still win.
void extendPathForGuiLaunch() {
  QByteArray path = qgetenv("PATH");
  const QList<QByteArray> existing = path.split(':');
  for (const char *extra :
       {"/opt/homebrew/bin", "/opt/homebrew/sbin", "/usr/local/bin"}) {
    if (existing.contains(QByteArray(extra))) {
      continue;
    }
    if (!path.isEmpty()) {
      path.append(':');
    }
    path.append(extra);
  }
  qputenv("PATH", path);
}
#endif

} // namespace

int main(int argc, char *argv[]) {
#ifdef Q_OS_MACOS
  extendPathForGuiLaunch();
#endif

#ifndef Q_OS_MACOS
  // Software rendering is a workaround for the Windows/Linux GPU stacks. On
  // macOS Qt uses Metal, and forcing software OpenGL would only slow it down.
  QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
#endif

  // Lives as long as the process, so the lock is held until it exits.
  Util::SingleInstanceGuard instanceGuard;

  {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Sourcerer"));
    prependBundledToolsToPath();

    // Only one process may use the data directory and its WebEngine profiles.
    // Before the update check, so a second launch never applies an update
    // underneath the running instance.
    if (!instanceGuard.acquire()) {
      if (instanceGuard.activateRunningInstance(app.arguments().mid(1))) {
        return 0;
      }
      if (!instanceGuard.isPrimary()) {
        return 1;
      }
    }

#ifdef QT_NO_DEBUG
    const QString installDir = installationDirectory();
    const QString updatePath = stagedUpdateFile(installDir);
    // The launcher passes --skip-update when it could not apply the staged
    // update, otherwise the two executables would start each other forever.
    const bool skipUpdate =
        app.arguments().contains(Util::UpdateStaging::skipUpdateArgument());
    if (!skipUpdate && !updatePath.isEmpty() && launcherAvailable()) {
      if (launchLauncherDetached(app.arguments().mid(1))) {
        return 0;
      }
    }
#endif
  }

  QApplication app(argc, argv);
  instanceGuard.listen();

  ComponentManager::ComponentManager componentManager;
  componentManager.addThread<GitRepoManager::GitRepoManager>();
  componentManager.addThread<NewsTracker::NewsTracker>();
  componentManager.addThread<SvnRepoManager::SvnRepoManager>();
  componentManager.addThread<UpdateManager::UpdateManager>();
  componentManager.startProcess<GuiManager::GuiManager>();
  componentManager.stopAll();

  return 0;
}
