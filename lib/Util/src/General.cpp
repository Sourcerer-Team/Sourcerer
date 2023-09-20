#include "General.h"

#include <pthread.h>

namespace General {

void setThreadName(const QString &name) {
#ifdef Q_OS_MACOS
    pthread_setname_np(name.toStdString().c_str());
#else
  pthread_setname_np(pthread_self(), name.toStdString().c_str());
#endif
}

} // namespace General
