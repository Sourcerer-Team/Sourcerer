#pragma once

#include <QString>
#include <QStringList>

namespace Util {

struct ProcessResult {
  bool started = false;
  bool finished = false;
  int exitCode = -1;
  // Standard output and standard error, interleaved.
  QString output;
};

// Runs program in workingDirectory and waits for it to finish. Blocks, so it
// must not be called on the UI thread.
//
// On Windows, scripts such as npm's .cmd shims are run by cmd.exe, which
// interprets characters like & | < > ^ % in arguments. Never pass text typed
// by the user to such a program.
ProcessResult runProcess(const QString &program, const QStringList &arguments,
                         const QString &workingDirectory,
                         int timeoutMs = 10 * 60 * 1000);

// The last lines of output, for error messages.
QString lastLines(const QString &output, int count = 8);

} // namespace Util
