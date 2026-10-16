#pragma once

// Header-only helpers shared by the application (UpdateManager, main.cpp) and
// the updater (launcher.cpp), which must agree on where a downloaded update is
// staged. Depends on QtCore only so that the launcher can use it without
// linking the Util library.

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QString>

namespace Util::UpdateStaging {

// Passed by the launcher to the application it starts after an update attempt,
// so that the application does not immediately hand control back to the
// launcher because of a staged update that could not be applied.
inline QString skipUpdateArgument() {
  return QStringLiteral("--skip-update");
}

#ifdef Q_OS_MACOS
// The update payload on macOS is a .dmg. It must never be staged inside the
// application bundle (that would invalidate its code signature) and the
// bundle's directory (/Applications) is usually not writable, so it lives in
// the per-user data directory. GenericDataLocation is used (instead of
// AppDataLocation) because the application name differs between the main
// executable and the launcher.
inline QString macosStagingDirectory() {
  return QDir(QStandardPaths::writableLocation(
                  QStandardPaths::GenericDataLocation))
      .absoluteFilePath(QStringLiteral("Sourcerer/updates"));
}

inline QString macosStagedUpdatePattern() {
  return QStringLiteral("Sourcerer-Update*.dmg");
}

// Returns the newest staged update, or an empty string if there is none.
inline QString macosStagedUpdateFile() {
  const QDir dir(macosStagingDirectory());
  const QFileInfoList files = dir.entryInfoList(
      {macosStagedUpdatePattern()}, QDir::Files | QDir::Readable, QDir::Time);
  return files.isEmpty() ? QString() : files.first().absoluteFilePath();
}

// Path of the running Sourcerer.app, or an empty string when the executable
// does not live in an application bundle (e.g. a bare development build).
inline QString macosBundlePath() {
  QDir dir(QCoreApplication::applicationDirPath()); // <bundle>/Contents/MacOS
  if (!dir.cdUp() || !dir.cdUp()) {
    return QString();
  }
  const QString path = dir.absolutePath();
  return path.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive) ? path
                                                                    : QString();
}

// Explains why the running copy cannot be replaced in place, or returns an
// empty string if it can. A bundle running from a mounted disk image or from
// Gatekeeper's App Translocation sandbox is a read-only copy: replacing it
// would not update what the user installed.
inline QString macosSelfUpdateBlockedReason(const QString &bundlePath) {
  if (bundlePath.isEmpty()) {
    return QStringLiteral("not running from an application bundle");
  }
  if (bundlePath.contains(QStringLiteral("/AppTranslocation/"))) {
    return QStringLiteral("running from a translocated location; move the "
                          "app to /Applications and start it again");
  }
  const QStorageInfo storage(bundlePath);
  if (storage.isValid() && storage.isReadOnly()) {
    return QStringLiteral("running from a read-only volume (disk image); "
                          "copy the app to /Applications first");
  }
  return QString();
}
#endif

} // namespace Util::UpdateStaging
