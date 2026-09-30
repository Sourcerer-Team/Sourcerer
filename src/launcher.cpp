#include <ConfigManager.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileDevice>
#include <QFileInfo>
#include <QIODevice>
#include <QProcess>
#include <QStringList>
#include <QTextStream>
#include <QtGui/qscreen.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qwidget.h>

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

  QFile mountInfo(QStringLiteral("/proc/self/mountinfo"));
  if (!mountInfo.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }

  QTextStream stream(&mountInfo);
  const QFileInfo appFileInfo(QCoreApplication::applicationFilePath());
  const QString appDir = appFileInfo.absolutePath();

  while (!stream.atEnd()) {
    const QString line = stream.readLine();
    const QStringList parts = line.split(QStringLiteral(" - "));
    if (parts.size() != 2) {
      continue;
    }

    const QStringList mountFields =
        parts.first().split(' ', Qt::SkipEmptyParts);
    const QStringList fsFields = parts.last().split(' ', Qt::SkipEmptyParts);
    if (mountFields.size() < 5 || fsFields.size() < 2) {
      continue;
    }

    const QString mountPoint = mountFields.at(4);
    if (QDir::cleanPath(mountPoint) != QDir::cleanPath(appDir)) {
      continue;
    }

    const QString mountSource = fsFields.at(1);
    if (!mountSource.trimmed().isEmpty()) {
      return mountSource.trimmed();
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

QString executablePath() {
  const QString appDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
  return QDir(appDir).absoluteFilePath(QStringLiteral("Sourcerer.exe"));
#else
  return QDir(appDir).absoluteFilePath(QStringLiteral("Sourcerer"));
#endif
}

QString stagePattern() {
#ifdef Q_OS_WIN
  return QStringLiteral("Sourcerer-Update*.exe");
#else
  return QStringLiteral("Sourcerer-Update*.AppImage");
#endif
}

QString stagedUpdateFile(const QString &installDir) {
  const QDir dir(installDir);
  const QFileInfoList files = dir.entryInfoList(
      {stagePattern()}, QDir::Files | QDir::Readable, QDir::Time);
  if (files.isEmpty()) {
    return QString();
  }

  return files.first().absoluteFilePath();
}

QString nativePath(const QString &path) {
  return QDir::toNativeSeparators(path);
}

bool runUpdate(const QString &updatePath) {
  const QFileInfo updateInfo(updatePath);
  if (!updateInfo.exists() || !updateInfo.isFile()) {
    return false;
  }

#ifdef Q_OS_LINUX
  const QString currentAppImagePath = resolveRunningAppImagePath();
  if (currentAppImagePath.isEmpty()) {
    return false;
  }

  const QFileInfo currentAppImageInfo(currentAppImagePath);
  if (currentAppImageInfo.exists() && currentAppImageInfo.isDir()) {
    return false;
  }

  if (QFile::exists(currentAppImagePath) &&
      !QFile::remove(currentAppImagePath)) {
    return false;
  }

  if (!QFile::rename(updatePath, currentAppImagePath)) {
    if (!QFile::copy(updatePath, currentAppImagePath)) {
      return false;
    }
    if (!QFile::remove(updatePath)) {
      return false;
    }
  }

  QFile currentAppImageFile(currentAppImagePath);
  currentAppImageFile.setPermissions(
      QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
      QFileDevice::ReadUser | QFileDevice::WriteUser | QFileDevice::ExeUser |
      QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther |
      QFileDevice::ExeOther);

  QProcess process;
  process.setProgram(currentAppImagePath);
  process.setArguments({QStringLiteral("--integrate")});
  process.start();
  if (!process.waitForStarted()) {
    return false;
  }

  while (process.state() != QProcess::NotRunning) {
    process.waitForFinished(100); // Wait in 100ms chunks synchronously
    QCoreApplication::processEvents(); // Keep UI redrawing/responsive
  }

  return process.exitStatus() == QProcess::NormalExit &&
         process.exitCode() == 0;
#else
  const QString logDir =
      ConfigManager::ConfigManager::getInstance().getLogsDir();

  QProcess process;
  process.setWorkingDirectory(updateInfo.absolutePath());

  if (updateInfo.suffix().compare(QStringLiteral("msi"), Qt::CaseInsensitive) ==
      0) {
    process.setProgram(QStringLiteral("msiexec"));
    process.setArguments({QStringLiteral("/i"), nativePath(updatePath),
                          QStringLiteral("/quiet"),
                          QStringLiteral("/norestart"), QStringLiteral("/log"),
                          nativePath(QDir(logDir).absoluteFilePath(
                              QStringLiteral("Sourcerer-update.log")))});
  } else if (updateInfo.suffix().compare(QStringLiteral("exe"),
                                         Qt::CaseInsensitive) == 0) {
    process.setProgram(nativePath(updatePath));
    process.setArguments({QStringLiteral("/quiet"),
                          QStringLiteral("/norestart"), QStringLiteral("/log"),
                          nativePath(QDir(logDir).absoluteFilePath(
                              QStringLiteral("Sourcerer-update.log")))});
  } else {
    process.setProgram(nativePath(updatePath));
  }

  process.start();
  if (!process.waitForStarted()) {
    return false;
  }

  while (process.state() != QProcess::NotRunning) {
    process.waitForFinished(100); // Wait in 100ms chunks synchronously
    QCoreApplication::processEvents(); // Keep UI redrawing/responsive
  }

  const bool result =
      process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;

  QDir sourceDir(updatePath);

  // Filter files by extension (.log)
  QStringList nameFilters;
  nameFilters << "*.log";

  // Specify filters: only Files (no directories), ignore hidden files, ignore
  // symbolic links if needed
  const QDir::Filters filters =
      QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot;

  // Get the list of matching file names
  const QFileInfoList logFiles =
      sourceDir.entryInfoList(nameFilters, QDir::Files | QDir::NoDotAndDotDot);

  QDir targetDir(logDir);

  // Create target directory if it doesn't exist
  if (!targetDir.exists()) {
    if (!targetDir.mkpath(".")) {
      return result;
    }
  }

  for (const QFileInfo &fileInfo : logFiles) {
    QString sourceFilePath = fileInfo.absoluteFilePath();
    QString targetFilePath = targetDir.absoluteFilePath(fileInfo.fileName());

    // Overwrite existing file in destination if needed
    if (QFile::exists(targetFilePath)) {
      QFile::remove(targetFilePath);
    }

    // Move (rename) file to destination
    QFile::rename(sourceFilePath, targetFilePath);
  }

  return result;
#endif
}

int launchApplication(const QString &program, const QStringList &arguments) {
  QProcess process;
  process.setWorkingDirectory(QFileInfo(program).absolutePath());
  return process.startDetached(program, arguments);
}

} // namespace

int main(int argc, char *argv[])
{
  QApplication app(argc, argv);

         // Main window setup
  QWidget window;

  // Frameless, always-on-top window
  window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);

  // Styling
  window.setFixedSize(360, 120);
  window.setStyleSheet(
      "QWidget {"
      "   background-color: #1e1e2e;"
      "   border: 1px solid #45475a;"
      "   border-radius: 8px;"
      "}"
      "QLabel {"
      "   color: #cdd6f4;"
      "   font-family: 'Segoe UI', 'Ubuntu', sans-serif;"
      "   font-size: 16px;"
      "   font-weight: bold;"
      "   border: none;"
      "}"
      );

         // Label component
  QLabel *label = new QLabel("Upgrading Sourcerer...", &window);
  label->setAlignment(Qt::AlignCenter);

         // Layout
  QVBoxLayout *layout = new QVBoxLayout(&window);
  layout->addWidget(label);
  layout->setContentsMargins(20, 20, 20, 20);

         // Center window on active primary screen
  window.show();
  if (QScreen *screen = QGuiApplication::primaryScreen()) {
    QRect screenGeometry = screen->geometry();
    int x = screenGeometry.x() + (screenGeometry.width() - window.width()) / 2;
    int y = screenGeometry.y() + (screenGeometry.height() - window.height()) / 2;
    window.move(x, y);
  }

  QCoreApplication::processEvents();

  const QString installDir = installationDirectory();
  const QString updatePath = stagedUpdateFile(installDir);
  const QStringList arguments = app.arguments().mid(1);

  bool updateApplied = false;
  if (!updatePath.isEmpty()) {
    updateApplied = runUpdate(updatePath);
    if (updateApplied) {
      QFile::remove(updatePath);
    }
  }

  if (!updateApplied && !updatePath.isEmpty()) {
    // Keep the staged update available for the next start, but still try to
    // open the app so the user is not blocked.
  }

  const QString appExecutable = executablePath();
  return launchApplication(appExecutable, arguments);
}
