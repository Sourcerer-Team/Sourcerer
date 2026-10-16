#include "General.h"

#include <qglobal.h>

#if defined(Q_OS_LINUX) || defined(Q_OS_MACOS)
#include <pthread.h>
#endif

namespace General {

void setThreadName(const QString &name) {
#if defined(Q_OS_MACOS)
  pthread_setname_np(name.toStdString().c_str());
#elif defined(Q_OS_LINUX)
  pthread_setname_np(pthread_self(), name.toStdString().c_str());
#else
  Q_UNUSED(name);
#endif
}

} // namespace General
