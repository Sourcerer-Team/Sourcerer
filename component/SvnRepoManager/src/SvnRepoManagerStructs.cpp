
#include "SvnRepoManagerStructs.h"

#include <QCoreApplication>

namespace SvnRepoManager {

namespace Structs {

QString translateSvnError(int errorCode) {
  const int signedErrorCode = decodeSvnError(errorCode);

  switch (signedErrorCode) {
  default:
    return QCoreApplication::translate(
               "SvnRepoError", "An unexpected error occurred (code: %1).")
        .arg(signedErrorCode);
  }
}

int decodeSvnError(int errorCode) {
  // Extract the actual libgit2 error code from the combined error code.
  // The error code is encoded as: (klass & 0x00ff) << 16 | (error & 0xffff)
  // We need the low 16 bits which contain the git_error_code value.
  const int gitErrorCode = errorCode & 0xffff;

  // Handle negative error codes (libgit2 uses negative values for errors)
  // Convert from stored unsigned to signed
  const int signedErrorCode =
      (gitErrorCode > 0x7fff) ? gitErrorCode - 0x10000 : gitErrorCode;

  return signedErrorCode;
}

} // namespace Structs
} // namespace SvnRepoManager
