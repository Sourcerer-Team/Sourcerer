#include <ComponentManager.h>
#include <GitRepoManager.h>
#include <GuiManager.h>
#include <NewsTracker.h>
#include <UpdateManager.h>

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

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);

#ifdef QT_NO_DEBUG
  {
    QCoreApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Sourcerer"));

    const QString installDir = installationDirectory();
    const QString updatePath = stagedUpdateFile(installDir);
    if (!updatePath.isEmpty() && launcherAvailable()) {
      if (launchLauncherDetached(app.arguments().mid(1))) {
        return 0;
      }
    }
  }
#endif

  QApplication app(argc, argv);

  ComponentManager::ComponentManager componentManager;
  componentManager.addThread<GitRepoManager::GitRepoManager>();
  componentManager.addThread<NewsTracker::NewsTracker>();
  componentManager.addThread<UpdateManager::UpdateManager>();
  componentManager.startProcess<GuiManager::GuiManager>();
  componentManager.stopAll();

  return 0;
}
