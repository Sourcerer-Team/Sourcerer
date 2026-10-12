#include "ProcessRunner.h"

#include <Log.h>

#include <QProcess>

#define THREAD_ID "ProcessRunner"

namespace Util {

ProcessResult runProcess(const QString &program, const QStringList &arguments,
                         const QString &workingDirectory, int timeoutMs) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Running '{}' with arguments '{}' in '{}'", program.toStdString(),
            arguments.join(QLatin1Char(' ')).toStdString(),
            workingDirectory.toStdString());

  ProcessResult result;

  QProcess process;
  process.setWorkingDirectory(workingDirectory);
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start(program, arguments);
  result.started = process.waitForStarted();
  if (!result.started) {
    result.output = process.errorString();
    LOG_ERROR(Log::ErrorId::Gui, "Failed to start '{}': {}",
              program.toStdString(), result.output.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  result.finished = process.waitForFinished(timeoutMs);
  if (!result.finished) {
    process.kill();
    process.waitForFinished();
  }

  result.output = QString::fromUtf8(process.readAll()).trimmed();
  result.exitCode =
      process.exitStatus() == QProcess::NormalExit ? process.exitCode() : -1;
  if (!result.finished) {
    result.exitCode = -1;
  }

  LOG_DEBUG("'{}' finished with exit code {}", program.toStdString(),
            result.exitCode);
  LOG_PERF(0, "Exit");
  return result;
}

QString lastLines(const QString &output, int count) {
  const QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
  return lines.mid(qMax(0, lines.size() - count))
      .join(QLatin1Char('\n'))
      .trimmed();
}

} // namespace Util
