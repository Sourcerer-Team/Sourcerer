#include "GitRepo.h"

#include <QUrl>

namespace Util {

const char *GIT_EXTENSION = ".git";

QString GitRepo::parseRepoName(const QString &url) {
  const QUrl _url(url);
  QString fileName = _url.fileName();
  if (fileName.endsWith(GIT_EXTENSION))
    fileName =
        fileName.left(fileName.length() - strlen(GIT_EXTENSION));

  return fileName;
}

} // namespace Util
