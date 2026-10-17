#include "PatchFileUtils.h"

#include "GitRepo.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStringList>

namespace GitRepo {

QStringList patchTargetFiles(const QString &patchContent) {
  QStringList files;
  const QString normalizedPatch = QString(patchContent).replace("\r\n", "\n");
  const QStringList lines = normalizedPatch.split('\n');

  for (const QString &line : lines) {
    if (!line.startsWith("+++ ")) {
      continue;
    }

    const QString path = line.mid(4).trimmed();
    if (path == "/dev/null" || !path.startsWith("b/")) {
      continue;
    }

    files.append(path.mid(2));
  }

  files.removeDuplicates();
  return files;
}

QStringList patchNewFiles(const QString &patchContent) {
  QStringList files;
  const QString normalizedPatch = QString(patchContent).replace("\r\n", "\n");
  const QStringList lines = normalizedPatch.split('\n');
  bool isNewFile = false;

  for (const QString &line : lines) {
    if (line.startsWith("diff --git ")) {
      isNewFile = false;
    } else if (line.startsWith("new file mode ") ||
               line.startsWith("--- /dev/null")) {
      isNewFile = true;
    } else if (isNewFile && line.startsWith("+++ b/")) {
      files.append(line.mid(6).trimmed());
    }
  }

  files.removeDuplicates();
  return files;
}

QString reconstructContentFromDelta(const FilesDiffDelta &delta) {
  QStringList lines;

  for (const auto &hunk : delta.hunks) {
    for (const auto &line : hunk.lines) {
      if (line.origin == QLatin1Char('-')) {
        continue;
      }

      if (line.origin == QLatin1Char('+')) {
        lines.append(line.content);
      }
    }
  }

  return lines.join('\n');
}

QString reconstructNewFileContent(const QString &patchContent,
                                  const QString &filePath) {
  const QString normalizedPatch = QString(patchContent).replace("\r\n", "\n");
  const QStringList lines = normalizedPatch.split('\n');
  QStringList contentLines;
  bool inTargetFile = false;
  bool inHunk = false;
  bool currentIsNewFile = false;

  for (const QString &line : lines) {
    if (line.startsWith("diff --git ")) {
      inTargetFile = line.contains(QString(" a/%1 ").arg(filePath)) ||
                     line.contains(QString(" b/%1 ").arg(filePath)) ||
                     line.endsWith(QString(" b/%1").arg(filePath));
      inHunk = false;
      currentIsNewFile = false;
      continue;
    }

    if (!inTargetFile) {
      continue;
    }

    if (line.startsWith("new file mode ") || line.startsWith("--- /dev/null")) {
      currentIsNewFile = true;
      continue;
    }

    if (line.startsWith("@@ ")) {
      inHunk = true;
      continue;
    }

    if (!inHunk) {
      continue;
    }

    if (line.startsWith("diff --git ") || line.startsWith("--- ") ||
        line.startsWith("+++ ")) {
      continue;
    }

    if (line.startsWith("+") && !line.startsWith("+++ ")) {
      contentLines.append(line.mid(1));
    }
  }

  if (!currentIsNewFile) {
    return {};
  }

  return contentLines.join('\n');
}

bool ensureFilesExistForPatch(const QString &workdirPath,
                              const QStringList &relativeFiles,
                              QStringList &createdFiles,
                              QString *errorMessage) {
  createdFiles.clear();

  for (const QString &relativeFile : relativeFiles) {
    const QString fullPath = QDir(workdirPath).filePath(relativeFile);
    QFileInfo fileInfo(fullPath);
    if (fileInfo.exists()) {
      continue;
    }

    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) {
      if (errorMessage) {
        *errorMessage =
            QString("Can't create parent directory for %1").arg(fullPath);
      }
      removeFiles(createdFiles);
      return false;
    }

    QFile file(fullPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
      if (errorMessage) {
        *errorMessage = QString("Can't create file %1").arg(fullPath);
      }
      removeFiles(createdFiles);
      return false;
    }

    file.close();
    createdFiles.append(fullPath);
  }

  return true;
}

void removeFiles(const QStringList &files) {
  for (const QString &file : files) {
    QFile::remove(file);
  }
}

} // namespace GitRepo
