#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

#include <svn_client.h>

namespace SvnRepo {

enum class SvnError : int {
  Unknown = 0,
  InitializationFailed,
  CreateClientContextFailed,
  CheckoutFailed,
  UpdateFailed,
  StatusFailed,
  AddFailed,
  DeleteFailed,
  RevertFailed,
  CommitFailed,
  HistoryFailed,
  Last
};

struct SvnErrorContext {
  SvnError error = SvnError::Unknown;
  int errorCode = -1;
  QString errorMessage;
};

struct SvnStatus {
  QString path;
  QStringList addedFiles;
  QStringList modifiedFiles;
  QStringList deletedFiles;
  QStringList conflictedFiles;
  QStringList unversionedFiles;
  QStringList ignoredFiles;

  bool hasChanges() const;
};

struct SvnCommitInfo {
  qint64 revision = -1;
  QString author;
  QString message;
  qint64 timeStamp = -1;
};

struct SvnHistory {
  bool valid = false;
  QVector<SvnCommitInfo> commits;
};

SvnErrorContext getLastError();

void initForCurrentThread();
void shutdownForCurrentThread();

bool init();

bool checkoutRepository(const QString &url, const QString &destinationDir);
bool updateWorkingCopy(const QString &path);

SvnStatus getStatus(const QString &path, bool recurse = true,
                    bool includeUnversioned = true);

bool addFiles(const QString &path, const QStringList &files);
bool deleteFiles(const QString &path, const QStringList &files);
bool revertFiles(const QString &path, const QStringList &files);

bool commit(const QString &path, const QString &message);

SvnHistory getHistory(const QString &path, int limit = 100);

} // namespace SvnRepo

bool svn_init();
