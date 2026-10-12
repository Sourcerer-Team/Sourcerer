#pragma once

#include <QString>

namespace GuiManager::ReleaseNotes {

// Reads the release notes bundled into the application (Markdown).
QString load();

// Returns the sections of 'markdown' whose version is greater than
// 'lastVersion' and not greater than 'currentVersion', newest first.
// Sections start with a "## <version>" heading. An empty 'lastVersion' means
// no lower bound and an empty 'currentVersion' means no upper bound.
// Returns an empty string if there is nothing to show.
QString sectionsBetween(const QString &markdown, const QString &lastVersion,
                        const QString &currentVersion);

} // namespace GuiManager::ReleaseNotes
