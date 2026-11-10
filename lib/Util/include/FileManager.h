// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QString>
#include <QStringList>
#include <QUrl>

namespace FileManager {

/// Opens the file manager and reveals the specified file(s).
/// On Windows: Opens Explorer with the file(s) selected.
/// On macOS: Opens Finder with the file(s) selected.
/// On Linux: Opens the file manager (Nautilus, Dolphin, etc.) in the directory
/// containing the file(s).
/// @param files List of absolute file paths to reveal in the file manager
void revealInFileManager(const QStringList &files);

/// Opens the URL in the user's default external handler (usually a browser).
/// On Windows and macOS: uses QDesktopServices.
/// On Linux: uses xdg-open with the AppImage library-path overrides removed
/// from the environment so the host's libraries are loaded.
/// @return true if the handler was launched.
bool openExternalUrl(const QUrl &url);

} // namespace FileManager
