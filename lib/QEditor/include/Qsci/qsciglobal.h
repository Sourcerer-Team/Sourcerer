#pragma once

#include <QtGlobal>

#if defined(QSCINTILLA_LIBRARY)
#define QSCI_EXPORT Q_DECL_EXPORT
#else
#define QSCI_EXPORT
#endif
