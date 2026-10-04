#include "MacUpdater.h"

#include <UpdateStaging.h>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QOperatingSystemVersion>
#include <QProcess>
#include <QSysInfo>
#include <QTemporaryDir>
#include <QVersionNumber>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <unistd.h>

namespace MacUpdater {

namespace {

constexpr int SHORT_TIMEOUT_MS = 2 * 60 * 1000;
constexpr int LONG_TIMEOUT_MS = 30 * 60 * 1000;

// Exit codes of the privileged helper.
constexpr int HELPER_OK = 0;
constexpr int HELPER_BAD_ARGUMENTS = 64;
constexpr int HELPER_COPY_FAILED = 65;
constexpr int HELPER_VERIFY_FAILED = 66;
constexpr int HELPER_SWAP_FAILED = 67;

struct ProcessResult {
  bool started = false;
  int exitCode = -1;
  QString output; // stdout and stderr, merged

  bool ok() const { return started && exitCode == 0; }
};

// Runs a program to completion while keeping the (optional) GUI responsive.
ProcessResult runProcess(const QString &program, const QStringList &arguments,
                         int timeoutMs = SHORT_TIMEOUT_MS) {
  ProcessResult result;

  QProcess process;
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(program, arguments);
  if (!process.waitForStarted(10000)) {
    result.output = process.errorString();
    return result;
  }
  result.started = true;

  QElapsedTimer timer;
  timer.start();
  while (process.state() != QProcess::NotRunning) {
    process.waitForFinished(100);
    QCoreApplication::processEvents();

    if (timer.elapsed() > timeoutMs) {
      process.kill();
      process.waitForFinished(5000);
      result.output = QStringLiteral("%1 timed out").arg(program);
      return result;
    }
  }

  result.exitCode =
      process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
  result.output = QString::fromLocal8Bit(process.readAll());
  return result;
}

QString shellQuote(const QString &value) {
  QString quoted = value;
  quoted.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
  return QLatin1Char('\'') + quoted + QLatin1Char('\'');
}

QString appleScriptEscape(const QString &value) {
  QString escaped = value;
  escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  return escaped;
}

bool removeTree(const QString &path) {
  if (!QFileInfo(path).exists() && !QFileInfo(path).isSymLink()) {
    return true;
  }
  return QDir(path).removeRecursively();
}

QString plistValue(const QString &bundlePath, const QString &key) {
  const ProcessResult result = runProcess(
      QStringLiteral("/usr/bin/plutil"),
      {QStringLiteral("-extract"), key, QStringLiteral("raw"),
       QStringLiteral("-o"), QStringLiteral("-"),
       QDir(bundlePath).absoluteFilePath(QStringLiteral("Contents/Info.plist"))});
  return result.ok() ? result.output.trimmed() : QString();
}

QVersionNumber versionOf(const QString &text) {
  return QVersionNumber::fromString(text.trimmed());
}

// Team identifier of a signed bundle, empty for ad-hoc / unsigned code.
QString teamIdentifier(const QString &bundlePath) {
  const ProcessResult result =
      runProcess(QStringLiteral("/usr/bin/codesign"),
                 {QStringLiteral("-d"), QStringLiteral("-vv"), bundlePath});
  const QStringList lines = result.output.split(QLatin1Char('\n'));
  for (const QString &line : lines) {
    if (line.startsWith(QStringLiteral("TeamIdentifier="))) {
      const QString team = line.mid(int(strlen("TeamIdentifier="))).trimmed();
      return team == QStringLiteral("not set") ? QString() : team;
    }
  }
  return QString();
}

bool verifySignature(const QString &bundlePath, QString &error) {
  const ProcessResult result = runProcess(
      QStringLiteral("/usr/bin/codesign"),
      {QStringLiteral("--verify"), QStringLiteral("--deep"),
       QStringLiteral("--strict"), bundlePath},
      LONG_TIMEOUT_MS);
  if (!result.ok()) {
    error = QStringLiteral("The code signature of the update is invalid: %1")
                .arg(result.output.trimmed());
    return false;
  }
  return true;
}

// An update may only come from the same publisher as the installed app once
// the app is signed with a Developer ID. Ad-hoc builds carry no identity, so
// there is nothing to compare, and they may migrate to a Developer ID build.
bool checkSamePublisher(const QString &currentBundle, const QString &newBundle,
                        QString &error) {
  const QString currentTeam = teamIdentifier(currentBundle);
  if (currentTeam.isEmpty()) {
    return true;
  }

  const QString newTeam = teamIdentifier(newBundle);
  if (newTeam != currentTeam) {
    error = QStringLiteral("The update is signed by a different publisher "
                           "(%1) than the installed application (%2).")
                .arg(newTeam.isEmpty() ? QStringLiteral("unsigned") : newTeam,
                     currentTeam);
    return false;
  }
  return true;
}

void stripQuarantine(const QString &bundlePath) {
  // Failure is harmless: the attribute is normally absent.
  runProcess(QStringLiteral("/usr/bin/xattr"),
             {QStringLiteral("-dr"), QStringLiteral("com.apple.quarantine"),
              bundlePath});
}

bool copyBundle(const QString &source, const QString &destination,
                QString &error) {
  if (!removeTree(destination)) {
    error = QStringLiteral("Unable to remove '%1'.").arg(destination);
    return false;
  }

  const ProcessResult result =
      runProcess(QStringLiteral("/usr/bin/ditto"), {source, destination},
                 LONG_TIMEOUT_MS);
  if (!result.ok()) {
    error = QStringLiteral("Unable to copy the update: %1")
                .arg(result.output.trimmed());
    removeTree(destination);
    return false;
  }
  return true;
}

QString siblingPath(const QString &bundlePath, const QString &suffix) {
  const QFileInfo info(bundlePath);
  return QDir(info.absolutePath())
      .absoluteFilePath(QStringLiteral(".") + info.fileName() + suffix);
}

QString stagedBundlePath(const QString &bundlePath) {
  return siblingPath(bundlePath, QStringLiteral(".new"));
}

// Atomically (per rename) replaces `bundlePath` by `newBundle`, which must be
// on the same volume. The previous version is deleted afterwards; if the
// replacement fails it is put back.
bool swapBundle(const QString &newBundle, const QString &bundlePath,
                QString &error) {
  const QString oldBundle = siblingPath(bundlePath, QStringLiteral(".old"));
  if (!removeTree(oldBundle)) {
    error = QStringLiteral("Unable to remove '%1'.").arg(oldBundle);
    return false;
  }

  if (::rename(bundlePath.toLocal8Bit().constData(),
               oldBundle.toLocal8Bit().constData()) != 0) {
    error = QStringLiteral("Unable to move the installed application aside: %1")
                .arg(QString::fromLocal8Bit(strerror(errno)));
    return false;
  }

  if (::rename(newBundle.toLocal8Bit().constData(),
               bundlePath.toLocal8Bit().constData()) != 0) {
    error = QStringLiteral("Unable to move the new version into place: %1")
                .arg(QString::fromLocal8Bit(strerror(errno)));
    ::rename(oldBundle.toLocal8Bit().constData(),
             bundlePath.toLocal8Bit().constData());
    return false;
  }

  // The previous version is still mapped by running processes; unlinking it is
  // safe. A leftover is cleaned up by the next update.
  removeTree(oldBundle);
  return true;
}

bool canReplaceWithoutPrivileges(const QString &bundlePath) {
  const QFileInfo bundle(bundlePath);
  const QFileInfo parent(bundle.absolutePath());
  return parent.isWritable() && bundle.ownerId() == geteuid();
}

struct MountedImage {
  QString mountPoint;
  bool attached = false;

  MountedImage() = default;
  MountedImage(const MountedImage &) = delete;
  MountedImage &operator=(const MountedImage &) = delete;
  ~MountedImage() { detach(); }

  bool attach(const QString &dmgPath, const QString &where, QString &error) {
    if (!QDir().mkpath(where)) {
      error = QStringLiteral("Unable to create '%1'.").arg(where);
      return false;
    }

    // The image was verified before; -noverify avoids doing it twice.
    const ProcessResult result = runProcess(
        QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("attach"), QStringLiteral("-nobrowse"),
         QStringLiteral("-readonly"), QStringLiteral("-noautoopen"),
         QStringLiteral("-noverify"), QStringLiteral("-mountpoint"), where,
         dmgPath},
        LONG_TIMEOUT_MS);
    if (!result.ok()) {
      error = QStringLiteral("Unable to mount the update image: %1")
                  .arg(result.output.trimmed());
      return false;
    }

    mountPoint = where;
    attached = true;
    return true;
  }

  void detach() {
    if (!attached) {
      return;
    }
    attached = false;

    ProcessResult result = runProcess(
        QStringLiteral("/usr/bin/hdiutil"),
        {QStringLiteral("detach"), QStringLiteral("-quiet"), mountPoint});
    if (!result.ok()) {
      runProcess(QStringLiteral("/usr/bin/hdiutil"),
                 {QStringLiteral("detach"), QStringLiteral("-quiet"),
                  QStringLiteral("-force"), mountPoint});
    }
  }
};

QString findApplication(const QString &mountPoint) {
  const QDir dir(mountPoint);
  if (dir.exists(QStringLiteral("Sourcerer.app"))) {
    return dir.absoluteFilePath(QStringLiteral("Sourcerer.app"));
  }
  return QString();
}

// Cheap checks on the application inside the (read-only) image, run before
// anything is copied.
enum class Inspection { Acceptable, NotNewer, Rejected };

Inspection inspectApplication(const QString &newBundle,
                              const QString &currentBundle, QString &error) {
  const QString expectedId = plistValue(currentBundle, "CFBundleIdentifier");
  if (expectedId.isEmpty()) {
    error = QStringLiteral("Unable to read the installed application's "
                           "bundle identifier.");
    return Inspection::Rejected;
  }

  if (plistValue(newBundle, "CFBundleIdentifier") != expectedId ||
      plistValue(newBundle, "CFBundleExecutable") !=
          QStringLiteral("Sourcerer")) {
    error = QStringLiteral("The image does not contain Sourcerer.");
    return Inspection::Rejected;
  }

  const QDir macOsDir(QDir(newBundle).absoluteFilePath(
      QStringLiteral("Contents/MacOS")));
  if (!macOsDir.exists(QStringLiteral("Sourcerer")) ||
      !macOsDir.exists(QStringLiteral("SourcererLauncher"))) {
    error = QStringLiteral("The update is missing required executables.");
    return Inspection::Rejected;
  }

  const QString architecture = QSysInfo::currentCpuArchitecture();
  const ProcessResult archs =
      runProcess(QStringLiteral("/usr/bin/lipo"),
                 {QStringLiteral("-archs"),
                  macOsDir.absoluteFilePath(QStringLiteral("Sourcerer"))});
  if (!archs.ok() ||
      !archs.output.split(QLatin1Char(' '), Qt::SkipEmptyParts)
           .contains(architecture)) {
    error = QStringLiteral("The update does not support this Mac (%1).")
                .arg(architecture);
    return Inspection::Rejected;
  }

  const QVersionNumber required =
      versionOf(plistValue(newBundle, "LSMinimumSystemVersion"));
  const QOperatingSystemVersion current = QOperatingSystemVersion::current();
  const QVersionNumber running(current.majorVersion(), current.minorVersion(),
                               current.microVersion());
  if (!required.isNull() && QVersionNumber::compare(required, running) > 0) {
    error = QStringLiteral("The update requires macOS %1 or newer.")
                .arg(required.toString());
    return Inspection::Rejected;
  }

  const QVersionNumber newVersion =
      versionOf(plistValue(newBundle, "CFBundleShortVersionString"));
  const QVersionNumber currentVersion =
      versionOf(plistValue(currentBundle, "CFBundleShortVersionString"));
  if (newVersion.isNull()) {
    error = QStringLiteral("The update has no version number.");
    return Inspection::Rejected;
  }
  if (!currentVersion.isNull() &&
      QVersionNumber::compare(newVersion, currentVersion) <= 0) {
    return Inspection::NotNewer;
  }

  return Inspection::Acceptable;
}

// Copies `source` to `destination`, checks the copy's signature and publisher.
// `rejected` is set when the update itself is unacceptable (as opposed to the
// copy failing for environmental reasons).
bool copyAndValidate(const QString &source, const QString &destination,
                     const QString &currentBundle, QString &error,
                     bool &rejected) {
  rejected = false;
  if (!copyBundle(source, destination, error)) {
    return false;
  }

  if (!verifySignature(destination, error) ||
      !checkSamePublisher(currentBundle, destination, error)) {
    rejected = true;
    removeTree(destination);
    return false;
  }

  stripQuarantine(destination);
  return true;
}

bool runElevatedInstall(const QString &verifiedCopy,
                        const QString &currentBundle, const Callbacks &callbacks,
                        Outcome &outcome) {
  const QFileInfo bundleInfo(currentBundle);
  const QString team = teamIdentifier(currentBundle);

  const QStringList helperArguments = {
      shellQuote(QCoreApplication::applicationFilePath()),
      shellQuote(privilegedHelperSwitch()),
      shellQuote(verifiedCopy),
      shellQuote(currentBundle),
      QString::number(bundleInfo.ownerId()),
      QString::number(bundleInfo.groupId()),
      shellQuote(team.isEmpty() ? QStringLiteral("-") : team),
  };

  const QString script =
      QStringLiteral("do shell script \"%1\" with administrator privileges "
                     "with prompt \"%2\"")
          .arg(appleScriptEscape(helperArguments.join(QLatin1Char(' '))),
               appleScriptEscape(QStringLiteral(
                   "Sourcerer needs your permission to install an update.")));

  if (callbacks.setWindowVisible) {
    callbacks.setWindowVisible(false);
  }
  const ProcessResult result = runProcess(
      QStringLiteral("/usr/bin/osascript"), {QStringLiteral("-e"), script},
      LONG_TIMEOUT_MS);
  if (callbacks.setWindowVisible) {
    callbacks.setWindowVisible(true);
  }

  if (result.ok()) {
    return true;
  }

  if (result.output.contains(QStringLiteral("User canceled")) ||
      result.output.contains(QStringLiteral("(-128)"))) {
    outcome.status = Status::Cancelled;
    outcome.message = QStringLiteral("The update was not installed because "
                                     "the administrator password prompt was "
                                     "cancelled.");
  } else {
    outcome.status = Status::Failed;
    outcome.message = QStringLiteral("Installing the update as administrator "
                                     "failed: %1")
                          .arg(result.output.trimmed());
  }
  return false;
}

void report(const Callbacks &callbacks, const QString &text) {
  if (callbacks.status) {
    callbacks.status(text);
  }
  QCoreApplication::processEvents();
}

Outcome failed(const QString &message) {
  Outcome outcome;
  outcome.status = Status::Failed;
  outcome.message = message;
  return outcome;
}

} // namespace

QString privilegedHelperSwitch() { return QStringLiteral("--apply-update"); }

Outcome installFromDmg(const QString &dmgPath, const Callbacks &callbacks) {
  const QString currentBundle = Util::UpdateStaging::macosBundlePath();
  const QString blocked =
      Util::UpdateStaging::macosSelfUpdateBlockedReason(currentBundle);
  if (!blocked.isEmpty()) {
    return failed(QStringLiteral("The update cannot be installed: %1.")
                      .arg(blocked));
  }

  if (!QFileInfo(dmgPath).isFile()) {
    return failed(QStringLiteral("The downloaded update is missing."));
  }

  report(callbacks, QStringLiteral("Verifying update..."));
  const ProcessResult verify = runProcess(
      QStringLiteral("/usr/bin/hdiutil"),
      {QStringLiteral("verify"), QStringLiteral("-quiet"), dmgPath},
      LONG_TIMEOUT_MS);
  if (!verify.ok()) {
    return failed(QStringLiteral("The downloaded update is corrupt: %1")
                      .arg(verify.output.trimmed()));
  }

  // Declared before the mounted image so that it outlives it.
  QTemporaryDir workDir;
  if (!workDir.isValid()) {
    return failed(QStringLiteral("Unable to create a temporary directory."));
  }

  QString error;
  MountedImage image;
  if (!image.attach(dmgPath, workDir.filePath(QStringLiteral("mnt")), error)) {
    return failed(error);
  }

  const QString mountedApp = findApplication(image.mountPoint);
  if (mountedApp.isEmpty()) {
    return failed(QStringLiteral("The image does not contain Sourcerer.app."));
  }

  switch (inspectApplication(mountedApp, currentBundle, error)) {
  case Inspection::Rejected:
    return failed(error);
  case Inspection::NotNewer: {
    Outcome outcome;
    outcome.status = Status::UpToDate;
    return outcome;
  }
  case Inspection::Acceptable:
    break;
  }

  report(callbacks, QStringLiteral("Installing update..."));

  if (canReplaceWithoutPrivileges(currentBundle)) {
    // Copy next to the installed bundle so the final swap is a rename.
    const QString newBundle = stagedBundlePath(currentBundle);
    bool rejected = false;
    if (copyAndValidate(mountedApp, newBundle, currentBundle, error,
                        rejected)) {
      image.detach();
      if (swapBundle(newBundle, currentBundle, error)) {
        Outcome outcome;
        outcome.status = Status::Installed;
        return outcome;
      }
      removeTree(newBundle);
    }

    // A bad update is final; anything else (typically a permission problem
    // deep inside the bundle) may still work with elevation.
    if (rejected) {
      return failed(error);
    }
    if (!image.attached &&
        !image.attach(dmgPath, workDir.filePath(QStringLiteral("mnt2")),
                      error)) {
      return failed(error);
    }
  }

  // Elevated path: validate a copy that only the current user can reach, then
  // let the helper (running as root) install it.
  const QString verifiedCopy =
      workDir.filePath(QStringLiteral("Sourcerer.app"));
  bool rejectedCopy = false;
  if (!copyAndValidate(findApplication(image.mountPoint), verifiedCopy,
                       currentBundle, error, rejectedCopy)) {
    return failed(error);
  }
  image.detach();

  Outcome outcome;
  if (!runElevatedInstall(verifiedCopy, currentBundle, callbacks, outcome)) {
    return outcome;
  }

  outcome.status = Status::Installed;
  return outcome;
}

int runPrivilegedHelper(const QStringList &arguments) {
  // <verified copy> <bundle> <owner uid> <owner gid> <team id or "-">
  if (arguments.size() != 5) {
    return HELPER_BAD_ARGUMENTS;
  }

  const QString source = arguments.at(0);
  const QString bundle = arguments.at(1);
  const QString owner = arguments.at(2) + QLatin1Char(':') + arguments.at(3);
  const QString team = arguments.at(4) == QStringLiteral("-")
                           ? QString()
                           : arguments.at(4);

  bool numeric = false;
  arguments.at(2).toUInt(&numeric);
  bool numericGroup = false;
  arguments.at(3).toUInt(&numericGroup);
  if (!numeric || !numericGroup ||
      !bundle.endsWith(QStringLiteral(".app")) ||
      QFileInfo(bundle).fileName() != QStringLiteral("Sourcerer.app") ||
      !QFileInfo(source).isDir()) {
    return HELPER_BAD_ARGUMENTS;
  }

  const QString newBundle = stagedBundlePath(bundle);
  QString error;
  if (!copyBundle(source, newBundle, error)) {
    fprintf(stderr, "%s\n", qPrintable(error));
    return HELPER_COPY_FAILED;
  }

  // Hand the copy to the owner of the installed bundle first, so that nobody
  // else can alter it between the checks below and the swap.
  const ProcessResult chown =
      runProcess(QStringLiteral("/usr/sbin/chown"),
                 {QStringLiteral("-R"), owner, newBundle});
  if (!chown.ok()) {
    fprintf(stderr, "chown failed: %s\n", qPrintable(chown.output.trimmed()));
    removeTree(newBundle);
    return HELPER_COPY_FAILED;
  }

  if (!verifySignature(newBundle, error)) {
    fprintf(stderr, "%s\n", qPrintable(error));
    removeTree(newBundle);
    return HELPER_VERIFY_FAILED;
  }

  if (!team.isEmpty() && teamIdentifier(newBundle) != team) {
    fprintf(stderr, "The update is not signed by team %s.\n", qPrintable(team));
    removeTree(newBundle);
    return HELPER_VERIFY_FAILED;
  }

  stripQuarantine(newBundle);

  if (!swapBundle(newBundle, bundle, error)) {
    fprintf(stderr, "%s\n", qPrintable(error));
    removeTree(newBundle);
    return HELPER_SWAP_FAILED;
  }

  return HELPER_OK;
}

} // namespace MacUpdater
