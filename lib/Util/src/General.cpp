#include "General.h"

#include <pthread.h>

namespace General {

void setThreadName(const QString &name) {
  pthread_setname_np(pthread_self(), name.toStdString().c_str());
}

} // namespace General
