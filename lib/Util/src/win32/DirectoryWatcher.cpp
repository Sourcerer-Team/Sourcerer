#include "win32/DirectoryWatcher.h"

#include <Log.h>

#include <windows.h>
#include <Winbase.h>

#define THREAD_ID "DirectoryWatcher"

#define BUFFER_SIZE 8192

namespace Util {

DirectoryWatcher::DirectoryWatcher(const QString &path, QObject *parent)
    : QObject(parent), _thread(nullptr), _quit(false), _path(path) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

DirectoryWatcher::~DirectoryWatcher() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

bool DirectoryWatcher::startWatching() {
  LOG_PERF(0, "Enter");

  stopWatching();

  _quit = false;

  _thread = QThread::create([&]() { run(); });

  connect(_thread, &QThread::finished, _thread, &QObject::deleteLater);

  _thread->start();

  LOG_PERF(0, "Exit");

  return true;
}

bool DirectoryWatcher::stopWatching() {
  LOG_PERF(0, "Enter");

  _quit = true;

  if (_thread) {
    _thread->wait();
    _thread = nullptr;
  }

  LOG_PERF(0, "Exit");

  return true;
}

void DirectoryWatcher::run() {
  LOG_PERF(0, "Enter");

  const HANDLE dirHandle = CreateFile(
      reinterpret_cast<LPCWSTR>(_path.utf16()), FILE_LIST_DIRECTORY,
      FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
      OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, NULL);

  if (dirHandle == INVALID_HANDLE_VALUE) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to open directory handle for {}. Error: {}",
              _path.toStdString(), GetLastError());
    LOG_PERF(0, "Exit");
    return;
  }

  BYTE buffer[BUFFER_SIZE];

  std::memset(&buffer[0], 0, sizeof(buffer));

  while (!_quit) {
    DWORD bytesReturned = 0;
    if (ReadDirectoryChangesW(
            dirHandle, &buffer[0], sizeof(buffer), TRUE,
            FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
                FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
            &bytesReturned, nullptr, nullptr) == 0) {
      LOG_ERROR(Log::ErrorId::FileSystem,
                "Failed to read directory changes for {}. Error: {}",
                _path.toStdString(), GetLastError());
      break;
    }

    if (bytesReturned < sizeof(FILE_NOTIFY_INFORMATION)) {
      continue;
    }

    const auto startingInfo =
        reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer);
    auto info = startingInfo;

    do {
      QString fileName = QString::fromWCharArray(
          info->FileName, info->FileNameLength / sizeof(WCHAR));
      LOG_INFO("Directory change detected: {} - {}", fileName.toStdString(),
               info->Action);

      // Emit signal or handle the change as needed
      emit directoryChanged(fileName);

      if (info->NextEntryOffset == 0) {
        break;
      }

      if (reinterpret_cast<char *>(info) -
              reinterpret_cast<char *>(startingInfo) + info->NextEntryOffset >=
          bytesReturned) {
        break;
      }

      info = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(
          reinterpret_cast<BYTE *>(info) + info->NextEntryOffset);
    } while (true);
  }

  ::CloseHandle(dirHandle);

  LOG_PERF(0, "Exit");
}

} // namespace Util
