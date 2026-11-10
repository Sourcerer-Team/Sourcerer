// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QString>
#include <QStringList>

namespace GitRepo {
struct FilesDiffDelta;

QStringList patchTargetFiles(const QString &patchContent);
QStringList patchNewFiles(const QString &patchContent);
QString reconstructContentFromDelta(const FilesDiffDelta &delta);
QString reconstructNewFileContent(const QString &patchContent,
                                  const QString &filePath);
bool ensureFilesExistForPatch(const QString &workdirPath,
                              const QStringList &relativeFiles,
                              QStringList &createdFiles,
                              QString *errorMessage = nullptr);
void removeFiles(const QStringList &files);

} // namespace GitRepo
