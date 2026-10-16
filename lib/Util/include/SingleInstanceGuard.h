#pragma once

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class QLocalServer;
class QLockFile;

namespace Util {

// Keeps a single Sourcerer process per user and data directory. A second
// process hands its command line arguments to the running one and exits, so
// the WebEngine profiles and other on-disk state never have two users.
//
// Usage:
//   SingleInstanceGuard guard;
//   if (!guard.acquire()) {
//     return guard.activateRunningInstance(arguments) ? 0 : 1;
//   }
//   ... create QApplication ...
//   guard.listen();
class SingleInstanceGuard final {
public:
  SingleInstanceGuard();
  ~SingleInstanceGuard();

  SingleInstanceGuard(const SingleInstanceGuard &) = delete;
  SingleInstanceGuard &operator=(const SingleInstanceGuard &) = delete;

  // The guard of this process, or nullptr if there is none.
  static SingleInstanceGuard *instance();

  // Returns true if this process became the primary instance. Needs a
  // QCoreApplication, so the data directory can be resolved.
  bool acquire();

  // Sends arguments to the primary instance, waiting for it while it is still
  // starting. Returns false if it could not be reached; if the primary
  // instance exited meanwhile, this process acquires the lock instead and
  // isPrimary() becomes true.
  bool activateRunningInstance(const QStringList &arguments);

  bool isPrimary() const;

  // Starts accepting activation requests. Call after the QApplication of the
  // primary instance has been created.
  bool listen();

  // Requests that arrive before a handler is set are kept and delivered once
  // it is set.
  void setActivationHandler(
      std::function<void(const QStringList &arguments)> handler);

private:
  bool sendToRunningInstance(const QStringList &arguments) const;
  void handleActivation(const QStringList &arguments);

  QString _serverName;
  std::unique_ptr<QLockFile> _lockFile;
  std::unique_ptr<QLocalServer> _server;
  std::function<void(const QStringList &)> _activationHandler;
  QList<QStringList> _pendingActivations;
};

} // namespace Util
