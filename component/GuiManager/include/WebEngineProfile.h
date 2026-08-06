#pragma once

#if defined(QT_WEBENGINECORE_LIB)
#include <QtWebEngineCore/QWebEngineProfile>
#endif

namespace GuiManager {

#if defined(QT_WEBENGINECORE_LIB)
QWebEngineProfile *sharedWebEngineProfile();
#endif

} // namespace GuiManager
