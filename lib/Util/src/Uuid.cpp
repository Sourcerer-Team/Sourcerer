
#include "Uuid.h"

#include <QUuid>

namespace Uuid {

QString createUuid() {
  return QUuid::createUuid().toString();
}

} // namespace Uuid
