#include "FileManager.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
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

} // namespace FileManager
