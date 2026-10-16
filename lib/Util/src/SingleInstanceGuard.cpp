#include "SingleInstanceGuard.h"

#include <Log.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QThread>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#define THREAD_ID "SingleInstanceGuard"

namespace Util {

namespace {

SingleInstanceGuard *gInstance = nullptr;

constexpr int kConnectTimeoutMs = 500;
constexpr int kIoTimeoutMs = 2000;
// The primary instance listens only once its QApplication exists, so a second
// process started at the same time waits for it that long.
constexpr int kStartupWaitMs = 10000;
constexpr int kRetryIntervalMs = 200;

const QByteArray kAcknowledgement = QByteArrayLiteral("ok");

// Debug builds are separate instances, so they can run next to an installed
// release. They also keep their own WebEngine data (see GuiManager).
#ifdef QT_DEBUG
const QString kBuildSuffix = QStringLiteral("-debug");
#else
const QString kBuildSuffix;
#endif

QString dataDirectory() {
  return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}

QString userName() {
#ifdef Q_OS_WIN
  return qEnvironmentVariable("USERNAME");
#else
  return qEnvironmentVariable("USER");
#endif
}

// Local server names are global on Windows, so the name includes the user.
QString serverNameFor(const QString &dataDir) {
  const QString identity = QDir::cleanPath(dataDir).toLower() +
                           QLatin1Char('|') + userName() + kBuildSuffix;
  const QByteArray hash =
      QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256)
          .toHex()
          .left(16);
  return QStringLiteral("sourcerer-") + QString::fromLatin1(hash) +
         kBuildSuffix;
}

QByteArray readLine(QLocalSocket &socket) {
  while (!socket.canReadLine()) {
    if (!socket.waitForReadyRead(kIoTimeoutMs)) {
      return {};
    }
  }
  return socket.readLine().trimmed();
}

QByteArray toLine(const QJsonObject &object) {
  return QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
}

} // namespace

SingleInstanceGuard::SingleInstanceGuard() {
  gInstance = this;
}

SingleInstanceGuard::~SingleInstanceGuard() {
  _server.reset();
  if (gInstance == this) {
    gInstance = nullptr;
  }
}

SingleInstanceGuard *SingleInstanceGuard::instance() {
  return gInstance;
}

bool SingleInstanceGuard::acquire() {
  LOG_PERF(0, "Enter");

  const QString dataDir = dataDirectory();
  QDir().mkpath(dataDir);

  _serverName = serverNameFor(dataDir);
  _lockFile = std::make_unique<QLockFile>(QDir(dataDir).absoluteFilePath(
      QStringLiteral("instance%1.lock").arg(kBuildSuffix)));
  // The lock is stale only when its process is gone, not after some time.
  _lockFile->setStaleLockTime(0);

  const bool acquired = _lockFile->tryLock(0);
  LOG_INFO("Single instance lock {}acquired, server name '{}'",
           acquired ? "" : "not ", _serverName.toStdString());

  LOG_PERF(0, "Exit");
  return acquired;
}

bool SingleInstanceGuard::isPrimary() const {
  return _lockFile && _lockFile->isLocked();
}

bool SingleInstanceGuard::activateRunningInstance(
    const QStringList &arguments) {
  LOG_PERF(0, "Enter");

  QElapsedTimer timer;
  timer.start();
  do {
    if (sendToRunningInstance(arguments)) {
      LOG_INFO("Handed over to the running instance");
      LOG_PERF(0, "Exit");
      return true;
    }

    if (_lockFile && _lockFile->tryLock(0)) {
      LOG_INFO("Running instance exited, continuing as the primary instance");
      LOG_PERF(0, "Exit");
      return false;
    }

    QThread::msleep(kRetryIntervalMs);
  } while (timer.elapsed() < kStartupWaitMs);

  LOG_ERROR(Log::ErrorId::Gui,
            "The running instance did not respond within {} ms",
            kStartupWaitMs);
  LOG_PERF(0, "Exit");
  return false;
}

bool SingleInstanceGuard::sendToRunningInstance(
    const QStringList &arguments) const {
  QLocalSocket socket;
  socket.connectToServer(_serverName);
  if (!socket.waitForConnected(kConnectTimeoutMs)) {
    return false;
  }

  // The running instance first sends its process id. This process was started
  // by the user and may bring windows to the front, so it passes that right
  // on; otherwise Windows only flashes the taskbar button.
  const QJsonObject hello = QJsonDocument::fromJson(readLine(socket)).object();
  if (hello.isEmpty()) {
    return false;
  }
#ifdef Q_OS_WIN
  const qint64 processId = hello.value(QStringLiteral("pid")).toInteger();
  if (processId > 0) {
    AllowSetForegroundWindow(static_cast<DWORD>(processId));
  }
#endif

  QJsonObject request;
  request.insert(QStringLiteral("arguments"),
                 QJsonArray::fromStringList(arguments));
  socket.write(toLine(request));
  if (!socket.waitForBytesWritten(kIoTimeoutMs)) {
    return false;
  }

  return readLine(socket) == kAcknowledgement;
}

bool SingleInstanceGuard::listen() {
  LOG_PERF(0, "Enter");

  if (!isPrimary()) {
    LOG_PERF(0, "Exit");
    return false;
  }

  _server = std::make_unique<QLocalServer>();
  _server->setSocketOptions(QLocalServer::UserAccessOption);
  // A crashed instance can leave its socket file behind on Unix. Holding the
  // lock means nobody else uses that name.
  QLocalServer::removeServer(_serverName);
  if (!_server->listen(_serverName)) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to listen on '{}': {}",
              _serverName.toStdString(), _server->errorString().toStdString());
    _server.reset();
    LOG_PERF(0, "Exit");
    return false;
  }

  QObject::connect(
      _server.get(), &QLocalServer::newConnection, _server.get(), [this]() {
        while (QLocalSocket *socket = _server->nextPendingConnection()) {
          QObject::connect(socket, &QLocalSocket::disconnected, socket,
                           &QObject::deleteLater);
          QObject::connect(
              socket, &QLocalSocket::readyRead, socket, [this, socket]() {
                if (!socket->canReadLine()) {
                  return;
                }

                const QJsonObject request =
                    QJsonDocument::fromJson(socket->readLine()).object();
                QStringList arguments;
                const QJsonArray values =
                    request.value(QStringLiteral("arguments")).toArray();
                for (const QJsonValue &value : values) {
                  arguments.append(value.toString());
                }

                socket->write(kAcknowledgement + '\n');
                socket->flush();
                socket->disconnectFromServer();

                handleActivation(arguments);
              });

          QJsonObject hello;
          hello.insert(QStringLiteral("pid"),
                       QCoreApplication::applicationPid());
          socket->write(toLine(hello));
          socket->flush();
        }
      });

  LOG_INFO("Listening for other instances on '{}'", _serverName.toStdString());
  LOG_PERF(0, "Exit");
  return true;
}

void SingleInstanceGuard::setActivationHandler(
    std::function<void(const QStringList &arguments)> handler) {
  _activationHandler = std::move(handler);
  if (!_activationHandler) {
    return;
  }

  const QList<QStringList> pending = std::exchange(_pendingActivations, {});
  for (const QStringList &arguments : pending) {
    _activationHandler(arguments);
  }
}

void SingleInstanceGuard::handleActivation(const QStringList &arguments) {
  LOG_INFO("Activation requested by another instance, arguments: '{}'",
           arguments.join(QLatin1Char(' ')).toStdString());

  if (_activationHandler) {
    _activationHandler(arguments);
  } else {
    _pendingActivations.append(arguments);
  }
}

} // namespace Util
