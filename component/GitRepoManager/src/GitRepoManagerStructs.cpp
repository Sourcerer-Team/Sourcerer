
#include "GitRepoManagerStructs.h"

#include <QCoreApplication>

namespace GitRepoManager {

namespace Structs {

QString translateGitError(int errorCode) {
  // Extract the actual libgit2 error code from the combined error code.
  // The error code is encoded as: (klass & 0x00ff) << 16 | (error & 0xffff)
  // We need the low 16 bits which contain the git_error_code value.
  const int gitErrorCode = errorCode & 0xffff;

  // Handle negative error codes (libgit2 uses negative values for errors)
  // Convert from stored unsigned to signed
  const int signedErrorCode =
      (gitErrorCode > 0x7fff) ? gitErrorCode - 0x10000 : gitErrorCode;

  switch (signedErrorCode) {
  case 0: // GIT_OK
    return QCoreApplication::translate("GitRepoError", "Operation completed successfully.");

  case -1: // GIT_ERROR - Generic error
    return QCoreApplication::translate("GitRepoError", "An error occurred during the operation.");

  case -3: // GIT_ENOTFOUND
    return QCoreApplication::translate("GitRepoError", "The requested object or reference was not found.");

  case -4: // GIT_EEXISTS
    return QCoreApplication::translate("GitRepoError", "An object with that name already exists.");

  case -5: // GIT_EAMBIGUOUS
    return QCoreApplication::translate("GitRepoError", "The reference name is ambiguous.");

  case -6: // GIT_EBUFS
    return QCoreApplication::translate("GitRepoError", "The provided buffer is too small.");

  case -7: // GIT_EUSER
    return QCoreApplication::translate("GitRepoError", "The operation was cancelled by the user.");

  case -8: // GIT_EBAREREPO
    return QCoreApplication::translate("GitRepoError", "This operation cannot be performed on a bare repository.");

  case -9: // GIT_EUNBORNBRANCH
    return QCoreApplication::translate("GitRepoError", "The HEAD refers to an unborn branch.");

  case -10: // GIT_EUNMERGED
    return QCoreApplication::translate("GitRepoError", "There are unmerged changes. Please resolve merge conflicts first.");

  case -11: // GIT_ENONFASTFORWARD
    return QCoreApplication::translate("GitRepoError",
        "The update is not a fast-forward. Please pull the latest changes first.");

  case -12: // GIT_EINVALIDSPEC
    return QCoreApplication::translate("GitRepoError", "Invalid reference or specification provided.");

  case -13: // GIT_ECONFLICT
    return QCoreApplication::translate("GitRepoError",
        "There are conflicts that prevent the operation from completing.");

  case -14: // GIT_ELOCKED
    return QCoreApplication::translate("GitRepoError", "The resource is locked by another process.");

  case -15: // GIT_EMODIFIED
    return QCoreApplication::translate("GitRepoError",
        "The reference value has been modified externally. Please refresh and try again.");

  case -16: // GIT_EAUTH
    return QCoreApplication::translate("GitRepoError",
        "Authentication failed. Please check your credentials.");

  case -17: // GIT_ECERTIFICATE
    return QCoreApplication::translate("GitRepoError",
        "The server certificate verification failed.");

  case -18: // GIT_EAPPLIED
    return QCoreApplication::translate("GitRepoError", "The patch has already been applied.");

  case -19: // GIT_EPEEL
    return QCoreApplication::translate("GitRepoError",
        "The reference cannot be peeled to the requested type.");

  case -20: // GIT_EEOF
    return QCoreApplication::translate("GitRepoError", "Unexpected end of data.");

  case -21: // GIT_EINVALID
    return QCoreApplication::translate("GitRepoError", "Invalid data or parameters provided.");

  case -22: // GIT_EUNCOMMITTED
    return QCoreApplication::translate("GitRepoError",
        "There are uncommitted changes. Please commit or stash them first.");

  case -23: // GIT_EDIRECTORY
    return QCoreApplication::translate("GitRepoError",
        "A directory was found where a file was expected.");

  case -24: // GIT_EMERGECONFLICT
    return QCoreApplication::translate("GitRepoError",
        "Merge conflicts prevent the checkout. Please resolve conflicts first.");

  default:
    return QCoreApplication::translate("GitRepoError",
        "An unexpected error occurred (code: %1).").arg(signedErrorCode);
  }
}

} // namespace Structs
} // namespace GitRepoManager
