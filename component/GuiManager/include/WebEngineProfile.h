#pragma once

#if defined(QT_WEBENGINECORE_LIB)
#include <QtWebEngineCore/QWebEngineProfile>
#endif

#include <QString>

namespace GuiManager {

// Absolute path of a WebEngine storage directory below the application data
// directory. Debug builds use their own directories, so they never share
// WebEngine databases with an installed release running at the same time.
QString webEngineStoragePath(const QString &directoryName);

#if defined(QT_WEBENGINECORE_LIB)
QWebEngineProfile *sharedWebEngineProfile();
#endif

} // namespace GuiManager
