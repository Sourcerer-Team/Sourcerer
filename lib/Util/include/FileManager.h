#pragma once

#include <QString>
#include <QStringList>

namespace FileManager {

/// Opens the file manager and reveals the specified file(s).
/// On Windows: Opens Explorer with the file(s) selected.
/// On Linux: Opens the file manager (Nautilus, Dolphin, etc.) in the directory
/// containing the file(s).
/// @param files List of absolute file paths to reveal in the file manager
void revealInFileManager(const QStringList &files);

} // namespace FileManager
