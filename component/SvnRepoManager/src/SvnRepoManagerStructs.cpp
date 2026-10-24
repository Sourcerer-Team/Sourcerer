
#include "SvnRepoManagerStructs.h"

#include <QCoreApplication>

namespace SvnRepoManager {

namespace Structs {

QString translateSvnError(int errorCode) {
  const int code = decodeSvnError(errorCode);
  const QString text = SvnRepo::errorCodeText(code);
  if (!text.isEmpty()) {
    return text;
  }
  return QCoreApplication::translate("SvnRepoError",
                                     "An unexpected error occurred (code: %1).")
      .arg(code);
}

int decodeSvnError(int errorCode) {
  // Subversion's error codes (apr_status_t, e.g. 160013) are kept as they
  // are; nothing is packed into them the way the Git component packs
  // libgit2's class and code.
  return errorCode;
}

} // namespace Structs
} // namespace SvnRepoManager
