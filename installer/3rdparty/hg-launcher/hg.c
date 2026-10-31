// Relocatable hg.exe for the Windows package.
//
// The bundled Mercurial lives in <dir of hg.exe>\python, a full Python with
// Mercurial installed in Lib\site-packages. This launcher runs
// "<dir>\python\python.exe -I -m mercurial <arguments>" with the same
// standard handles, so the command server pipes of HgRepo work unchanged.
// -I keeps PYTHONHOME, PYTHONPATH and the user site-packages of the machine
// out of the bundled interpreter.
//
// The child runs in a job object that is killed when the launcher exits, so
// stopping hg.exe (for example closing a repository tab) never leaves an
// orphaned python.exe behind.

#define WIN32_LEAN_AND_MEAN
#define UNICODE
#define _UNICODE
#include <windows.h>

#include <stdio.h>
#include <wchar.h>

#define LAUNCHER_FAILURE 255

// Returns the part of the launcher's command line after the program name,
// with the quoting of every argument kept exactly as it was passed.
static const wchar_t *argumentsAfterProgramName(const wchar_t *commandLine) {
  const wchar_t *p = commandLine;
  if (*p == L'"') {
    ++p;
    while (*p && *p != L'"') {
      ++p;
    }
    if (*p == L'"') {
      ++p;
    }
  } else {
    while (*p && *p != L' ' && *p != L'\t') {
      ++p;
    }
  }
  while (*p == L' ' || *p == L'\t') {
    ++p;
  }
  return p;
}

static void printError(const wchar_t *what, DWORD error) {
  fwprintf(stderr, L"hg launcher: %ls (error %lu)\n", what, error);
}

int main(void) {
  static wchar_t pythonPath[32768];
  DWORD length = GetModuleFileNameW(NULL, pythonPath, ARRAYSIZE(pythonPath));
  if (length == 0 || length >= ARRAYSIZE(pythonPath)) {
    printError(L"cannot resolve the path of hg.exe", GetLastError());
    return LAUNCHER_FAILURE;
  }

  wchar_t *lastSeparator = wcsrchr(pythonPath, L'\\');
  if (lastSeparator == NULL) {
    printError(L"unexpected path of hg.exe", ERROR_BAD_PATHNAME);
    return LAUNCHER_FAILURE;
  }
  *(lastSeparator + 1) = L'\0';
  if (wcscat_s(pythonPath, ARRAYSIZE(pythonPath), L"python\\python.exe") != 0) {
    printError(L"path of the bundled Python is too long", ERROR_BAD_PATHNAME);
    return LAUNCHER_FAILURE;
  }
  if (GetFileAttributesW(pythonPath) == INVALID_FILE_ATTRIBUTES) {
    fwprintf(stderr, L"hg launcher: bundled Python not found: %ls\n",
             pythonPath);
    return LAUNCHER_FAILURE;
  }

  const wchar_t *arguments = argumentsAfterProgramName(GetCommandLineW());
  static const wchar_t format[] = L"\"%ls\" -I -m mercurial %ls";
  size_t commandLineSize =
      wcslen(pythonPath) + wcslen(arguments) + ARRAYSIZE(format);
  wchar_t *commandLine = (wchar_t *)HeapAlloc(
      GetProcessHeap(), 0, commandLineSize * sizeof(wchar_t));
  if (commandLine == NULL) {
    printError(L"out of memory", ERROR_NOT_ENOUGH_MEMORY);
    return LAUNCHER_FAILURE;
  }
  swprintf(commandLine, commandLineSize, format, pythonPath, arguments);

  // Ctrl+C and Ctrl+Break reach the child through the shared console; the
  // launcher only waits for it and returns its exit code.
  SetConsoleCtrlHandler(NULL, TRUE);

  // The launcher joins the job itself and the child inherits it, so the
  // child is never created suspended. Without the job the child still runs;
  // it is only not killed with us.
  HANDLE job = CreateJobObjectW(NULL, NULL);
  if (job != NULL) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
    ZeroMemory(&limits, sizeof(limits));
    limits.BasicLimitInformation.LimitFlags =
        JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation,
                                 &limits, sizeof(limits)) ||
        !AssignProcessToJobObject(job, GetCurrentProcess())) {
      CloseHandle(job);
      job = NULL;
    }
  }

  STARTUPINFOW startupInfo;
  ZeroMemory(&startupInfo, sizeof(startupInfo));
  startupInfo.cb = sizeof(startupInfo);
  startupInfo.dwFlags = STARTF_USESTDHANDLES;
  startupInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
  startupInfo.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
  startupInfo.hStdError = GetStdHandle(STD_ERROR_HANDLE);

  PROCESS_INFORMATION processInfo;
  if (!CreateProcessW(pythonPath, commandLine, NULL, NULL, TRUE,
                      CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &startupInfo,
                      &processInfo)) {
    printError(L"cannot start the bundled Python", GetLastError());
    return LAUNCHER_FAILURE;
  }
  CloseHandle(processInfo.hThread);

  WaitForSingleObject(processInfo.hProcess, INFINITE);
  DWORD exitCode = LAUNCHER_FAILURE;
  GetExitCodeProcess(processInfo.hProcess, &exitCode);
  CloseHandle(processInfo.hProcess);
  return (int)exitCode;
}
