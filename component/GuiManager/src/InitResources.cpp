#include "InitResources.h"

#include <QApplication>

InitResources::InitResources() { Q_INIT_RESOURCE(resources); }

InitResources::~InitResources() { Q_CLEANUP_RESOURCE(resources); }
