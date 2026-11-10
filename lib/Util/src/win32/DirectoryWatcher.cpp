// MIT License
// Copyright (c) 2026 Sourcerer

#include "win32/DirectoryWatcher.h"

#include <Log.h>

// clang-format off
#include <windows.h>
#include <Winbase.h>
// clang-format on

#define THREAD_ID "DirectoryWatcher"

namespace Util {

DirectoryWatcher::DirectoryWatcher(const QString &path, QObject *parent)
    : QObject(parent), _thread(nullptr), _quit(true), _path(path) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

DirectoryWatcher::~DirectoryWatcher() {
  LOG_PERF(0, "Enter");

  stopWatching();

  LOG_PERF(0, "Exit");
}

bool DirectoryWatcher::startWatching() {
  LOG_PERF(0, "Enter");

  stopWatching();

  _quit = false;

  _thread = QThread::create([&]() { run(); });

  _thread->start();

  LOG_PERF(0, "Exit");

  return true;
}

bool DirectoryWatcher::stopWatching() {
  LOG_PERF(0, "Enter");

  if (_quit) {
    LOG_PERF(0, "Exit");
    return false;
  }

  _quit = true;

  if (_thread) {
    _thread->wait();
    delete _thread;
    _thread = nullptr;
  }

  LOG_PERF(0, "Exit");

  return true;
}

void DirectoryWatcher::run() {
  LOG_PERF(0, "Enter");

  const HANDLE dirHandle = CreateFileW(
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

  constexpr DWORD BUFFER_SIZE = 64 * 1024; // 64 KB is typical
  static_assert(BUFFER_SIZE >= sizeof(FILE_NOTIFY_INFORMATION),
                "Buffer too small for FILE_NOTIFY_INFORMATION");

  BYTE buffer[BUFFER_SIZE];

  std::memset(&buffer[0], 0, sizeof(buffer));

  OVERLAPPED ol{};
  ol.hEvent = CreateEventW(/*lpEventAttributes*/ nullptr,
                           /*bManualReset*/ TRUE,
                           /*bInitialState*/ FALSE,
                           /*lpName*/ nullptr);
  if (!ol.hEvent) {
    LOG_ERROR(Log::ErrorId::FileSystem,
              "Failed to create overlapped event. Error: {}", GetLastError());
    ::CloseHandle(dirHandle);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto reissueRead = [&](OVERLAPPED &overlapped) -> bool {
    ResetEvent(overlapped.hEvent);

    DWORD bytesReturned = 0;

    const BOOL ok = ReadDirectoryChangesW(
        dirHandle, &buffer[0], sizeof(buffer),
        /*bWatchSubtree*/ TRUE,
        FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME |
            FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE |
            FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION,
        &bytesReturned, // will be 0 in overlapped mode
        &overlapped,
        /*lpCompletionRoutine*/ nullptr);
    if (!ok) {
      const DWORD err = GetLastError();
      if (err != ERROR_IO_PENDING) {
        LOG_ERROR(
            Log::ErrorId::FileSystem,
            "ReadDirectoryChangesW failed to issue overlapped read. Error: {}",
            err);
        return false;
      }
    }
    return true;
  };

  if (!reissueRead(ol)) {
    ::CloseHandle(ol.hEvent);
    ::CloseHandle(dirHandle);
    LOG_PERF(0, "Exit");
    return;
  }

  for (;;) {
    if (_quit.load(std::memory_order_relaxed)) {
      if (!CancelIoEx(dirHandle, &ol)) {
        const DWORD err = GetLastError();
        if (err != ERROR_NOT_FOUND) {
          LOG_ERROR(Log::ErrorId::FileSystem, "CancelIoEx failed. Error: {}",
                    err);
        }
      }

      const DWORD wait = WaitForSingleObject(ol.hEvent, INFINITE);
      (void)wait;

      break;
    }

    const DWORD wait = WaitForSingleObject(ol.hEvent, /*dwMilliseconds*/ 50);

    if (wait == WAIT_OBJECT_0) {
      DWORD transferred = 0;
      if (!GetOverlappedResult(dirHandle, &ol, &transferred, /*bWait*/ FALSE)) {
        const DWORD err = GetLastError();
        if (err == ERROR_OPERATION_ABORTED) {
          if (_quit.load(std::memory_order_relaxed)) {
            break;
          }

          LOG_ERROR(Log::ErrorId::FileSystem,
                    "Overlapped operation aborted unexpectedly");

          if (!reissueRead(ol)) {
            break;
          }

          continue;
        } else {
          LOG_ERROR(Log::ErrorId::FileSystem,
                    "GetOverlappedResult failed. Error: {}", err);

          if (!reissueRead(ol)) {
            break;
          }

          continue;
        }
      }

      if (transferred >= sizeof(FILE_NOTIFY_INFORMATION)) {
        FILE_NOTIFY_INFORMATION *fni =
            reinterpret_cast<FILE_NOTIFY_INFORMATION *>(buffer);

        for (;;) {
          const QString fileName = QString::fromWCharArray(
              fni->FileName, fni->FileNameLength / sizeof(WCHAR));
          LOG_INFO("Directory change detected: {} - {}", fileName.toStdString(),
                   fni->Action);

          emit directoryChanged(fileName);

          if (fni->NextEntryOffset == 0) {
            break;
          }

          fni = reinterpret_cast<FILE_NOTIFY_INFORMATION *>(
              reinterpret_cast<BYTE *>(fni) + fni->NextEntryOffset);
        }
      }

      if (!reissueRead(ol)) {
        break;
      }
    } else if (wait == WAIT_TIMEOUT) {
      continue;
    } else {
      const DWORD err = GetLastError();
      LOG_ERROR(Log::ErrorId::FileSystem,
                "WaitForSingleObject failed. Error: {}", err);

      if (!reissueRead(ol)) {
        break;
      }
    }
  }

  ::CloseHandle(ol.hEvent);
  ::CloseHandle(dirHandle);

  LOG_PERF(0, "Exit");
}
} // namespace Util
