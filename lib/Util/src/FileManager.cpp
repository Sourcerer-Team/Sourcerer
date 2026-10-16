#include "FileManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSet>
#include <QUrl>

// clang-format off
#ifdef Q_OS_WIN32
#include <windows.h>
#include <shellapi.h>
#endif
// clang-format on

namespace FileManager {

#ifdef Q_OS_WIN32

void revealInFileManager(const QStringList &files) {
  if (files.isEmpty()) {
    return;
  }

  // On Windows, we use ShellExecute with "explorer.exe /select" to select files
  for (const QString &file : files) {
    QFileInfo fileInfo(file);
    if (!fileInfo.exists()) {
      continue;
    }

    // Convert to native Windows path
    QString nativeFile = QDir::toNativeSeparators(fileInfo.absoluteFilePath());

    // Use ShellExecute to open explorer with /select parameter
    // This will open the explorer window with the file selected
    ShellExecuteW(nullptr, L"open", L"explorer.exe",
                  reinterpret_cast<LPCWSTR>(
                      QString("/select,\"%1\"").arg(nativeFile).utf16()),
                  nullptr, SW_SHOW);
  }
}

#elif defined(Q_OS_MACOS)

void revealInFileManager(const QStringList &files) {
  if (files.isEmpty()) {
    return;
  }

  // `open -R` reveals (selects) each item in a Finder window.
  QStringList arguments;
  arguments << QStringLiteral("-R");
  for (const QString &file : files) {
    const QFileInfo fileInfo(file);
    if (fileInfo.exists()) {
      arguments << fileInfo.absoluteFilePath();
    }
  }

  if (arguments.size() > 1) {
    QProcess::startDetached(QStringLiteral("/usr/bin/open"), arguments);
  }
}

#else // Linux and other Unix-like systems

void revealInFileManager(const QStringList &files) {
  if (files.isEmpty()) {
    return;
  }

  // Get the directory containing the files
  QSet<QString> directories;
  for (const QString &file : files) {
    QFileInfo fileInfo(file);
    if (fileInfo.exists()) {
      directories.insert(fileInfo.absolutePath());
    }
  }

  // Open each unique directory in the file manager
  for (const QString &dir : directories) {
    // Try to use xdg-open which will use the default file manager
    QProcess process;
    process.startDetached("xdg-open", QStringList() << dir);
  }
}

#endif

bool openExternalUrl(const QUrl &url) {
#ifdef Q_OS_LINUX
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

  // Unset AppImage environment variables so external processes load host
  // system libraries
  env.remove(QStringLiteral("LD_LIBRARY_PATH"));
  env.remove(QStringLiteral("QT_PLUGIN_PATH"));
  env.remove(QStringLiteral("QML2_IMPORT_PATH"));

  QProcess process;
  process.setProcessEnvironment(env);
  process.setProgram(QStringLiteral("xdg-open"));
  process.setArguments(QStringList() << url.toString());
  return process.startDetached();
#else
  return QDesktopServices::openUrl(url);
#endif
}

} // namespace FileManager
