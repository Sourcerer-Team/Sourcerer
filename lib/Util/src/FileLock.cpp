#include "FileLock.h"

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include <qglobal.h>

#ifdef Q_OS_LINUX

bool checkForAnotherInstance(const std::string& lockName)
{
  const int fd = open(("/data/var/lock/" + lockName + ".lock").c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

  if(fd == -1)
  {
    return false;
  }

  struct flock fl;
  fl.l_type   = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start  = 0;
  fl.l_len    = 0;
  fl.l_pid    = getpid();

  if(fcntl(fd, F_SETLK, &fl) == -1)
  {
    if(errno == EACCES || errno == EAGAIN)
    {
      return true;
    }
  }

  return false;
}

#else //if Q_OS_WIN32

bool checkForAnotherInstance(const std::string& lockName)
{
    return false;
}

#endif
