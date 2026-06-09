#include "InitResources.h"

#include <Log.h>

#include <QApplication>

#define THREAD_ID "InitResources"

InitResources::InitResources() {
  LOG_PERF(0, "Enter");

  Q_INIT_RESOURCE(resources);

  LOG_PERF(0, "Exit");
}

InitResources::~InitResources() {
  LOG_PERF(0, "Enter");

  Q_CLEANUP_RESOURCE(resources);

  LOG_PERF(0, "Exit");
}
