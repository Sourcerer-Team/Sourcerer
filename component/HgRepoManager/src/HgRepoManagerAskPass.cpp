// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRepoManagerAskPass.h"

#include <EventManager.h>
#include <GitKeyStorage.h>
#include <GitRepo.h>
#include <HgRepo.h>
#include <Log.h>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMutexLocker>
#include <QPointer>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSemaphore>
#include <QStandardPaths>
#include <QThreadPool>
#include <QUuid>

#include <set>

#define THREAD_ID "HgRepoManagerAskPass"

namespace HgRepoManager::Workers {

namespace {

constexpr int kCheckTimeoutMs = 30 * 1000;

QByteArray randomToken() {
  QByteArray bytes(32, Qt::Uninitialized);
  QRandomGenerator::system()->generate(
      reinterpret_cast<quint32 *>(bytes.data()),
      reinterpret_cast<quint32 *>(bytes.data() + bytes.size()));
  return bytes.toHex();
}

QByteArray okAnswer(const QString &text) {
  return "OK " + text.toUtf8().toBase64() + '\n';
}

const QByteArray kNoAnswer = "NO\n";

QString normalizedPath(const QString &path) {
  return QDir::fromNativeSeparators(QDir::cleanPath(path));
}

// ssh names the key in the prompt cut to 100 characters, the key manager
// knows the full path.
QString resolveKeyPath(const QString &named) {
  const QString path = normalizedPath(named);
  if (QFileInfo::exists(path)) {
    return path;
  }
  for (const GitRepo::GitKeyPair &pair :
       GitRepo::GitKeyStorage::getInstance().getAllKeyPairs()) {
    const QString candidate = normalizedPath(pair.privateKeyPath);
    if (candidate.startsWith(path, Qt::CaseInsensitive)) {
      return candidate;
    }
  }
  return path;
}

QString sshKeygenExecutable() {
  QString keygen = QStandardPaths::findExecutable(QStringLiteral("ssh-keygen"));
#ifdef Q_OS_WIN
  if (keygen.isEmpty()) {
    const QString system =
        qEnvironmentVariable("SystemRoot", QStringLiteral("C:/Windows")) +
        QStringLiteral("/System32/OpenSSH/ssh-keygen.exe");
    if (QFileInfo(system).isExecutable()) {
      keygen = system;
    }
  }
#endif
  return keygen;
}

void sendPromptSignal(const EventManager::EventType type,
                      const std::size_t id) {
  EventManager::Event event(type);
  event.addData(id);
  EventManager::EventManager::getInstance().send(event);
}

} // namespace

AskPassServer::AskPassServer() = default;

AskPassServer::~AskPassServer() {
  stop();
}

QString AskPassServer::defaultProgram() {
#ifdef Q_OS_WIN
  const QString name = QStringLiteral("SourcererAskPass.exe");
#else
  const QString name = QStringLiteral("SourcererAskPass");
#endif
  const QString program =
      QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
  return QFileInfo(program).isExecutable() ? program : QString();
}

bool AskPassServer::start(const QString &program) {
  if (_thread || program.isEmpty()) {
    return false;
  }

  _program = program;
  _token = randomToken();
  _socketName = QStringLiteral("sourcerer-askpass-%1")
                    .arg(QUuid::createUuid().toString(QUuid::Id128));

  // The server lives in a thread of its own with an event loop.
  _thread = std::make_unique<QThread>();
  _thread->setObjectName(QStringLiteral("HgAskPass"));
  _thread->start();

  bool listening = false;
  QSemaphore ready;
  auto *context = new QObject();
  context->moveToThread(_thread.get());
  QObject::connect(_thread.get(), &QThread::finished, context,
                   &QObject::deleteLater);
  QMetaObject::invokeMethod(context, [this, context, &listening, &ready]() {
    _server = new QLocalServer(context);
    _server->setSocketOptions(QLocalServer::UserAccessOption);
    listening = _server->listen(_socketName);
    if (!listening) {
      LOG_WARN("The askpass server can't listen: {}",
               _server->errorString().toStdString());
    }
    QObject::connect(_server, &QLocalServer::newConnection, _server, [this]() {
      while (QLocalSocket *socket = _server->nextPendingConnection()) {
        onConnection(socket);
      }
    });
    ready.release();
  });
  ready.acquire();

  if (!listening) {
    stop();
    return false;
  }

  HgRepo::setAskPass(_program, _socketName, QString::fromLatin1(_token));
  LOG_INFO("SSH passphrases are asked through {}", _program.toStdString());
  return true;
}

void AskPassServer::stop() {
  if (!_thread) {
    return;
  }
  HgRepo::setAskPass(QString(), QString(), QString());

  // A prompt waiting for the user gives up, as if they cancelled.
  _stopping = true;
  if (const std::size_t id = _waitingPromptId.load()) {
    EventManager::Event cancel(
        EventManager::EventType::GitRepoManagerGetCredentialsCancel);
    cancel.addData(GitRepo::GitCredentialsErrorResponse{id});
    EventManager::EventManager::getInstance().send(cancel);
  }
  _pool.waitForDone();

  _thread->quit();
  _thread->wait();
  _thread.reset();
  _server = nullptr;
  _stopping = false;
}

void AskPassServer::onConnection(QLocalSocket *socket) {
  QObject::connect(socket, &QLocalSocket::disconnected, socket,
                   &QObject::deleteLater);

  auto buffer = std::make_shared<QByteArray>();
  QObject::connect(
      socket, &QLocalSocket::readyRead, socket, [this, socket, buffer]() {
        *buffer += socket->readAll();
        if (buffer->count('\n') < 2) {
          return;
        }
        QObject::disconnect(socket, &QLocalSocket::readyRead, nullptr, nullptr);

        const QList<QByteArray> lines = buffer->split('\n');
        const QByteArray token = lines.value(0).trimmed();
        const QString prompt =
            QString::fromUtf8(QByteArray::fromBase64(lines.value(1).trimmed()));

        const auto reply = [socket](const QByteArray &text) {
          socket->write(text);
          socket->flush();
          socket->disconnectFromServer();
        };

        // A passphrase check answers at once: the prompt it serves waits for
        // it.
        {
          QMutexLocker locker(&_stateMutex);
          if (!_checkToken.isEmpty() && token == _checkToken) {
            _checkToken.clear();
            reply(okAnswer(_checkPassphrase));
            return;
          }
        }

        if (token != _token) {
          reply(kNoAnswer);
          return;
        }

        QPointer<QLocalSocket> guard(socket);
        _pool.start([this, guard, prompt]() {
          const QByteArray text = answer(prompt);
          QMetaObject::invokeMethod(
              guard,
              [guard, text]() {
                if (guard) {
                  guard->write(text);
                  guard->flush();
                  guard->disconnectFromServer();
                }
              },
              Qt::QueuedConnection);
        });
      });
}

QByteArray AskPassServer::answer(const QString &prompt) {
  QMutexLocker promptLocker(&_promptMutex);

  // ssh: "Enter passphrase for key '<path>': ".
  static const QRegularExpression passphrasePrompt(
      QStringLiteral("^Enter passphrase for (?:key )?'?(.+?)'?:\\s*$"));
  const QRegularExpressionMatch match =
      passphrasePrompt.match(prompt.trimmed());
  if (!match.hasMatch()) {
    // Unknown host keys are trusted in the SSH key manager.
    if (prompt.contains(QLatin1String("(yes/no"))) {
      LOG_INFO("Declined an SSH host key question: {}", prompt.toStdString());
      return okAnswer(QStringLiteral("no"));
    }
    LOG_INFO("Declined an SSH prompt: {}", prompt.toStdString());
    return kNoAnswer;
  }

  const QString keyPath = resolveKeyPath(match.captured(1));
  {
    QMutexLocker locker(&_stateMutex);
    const auto known = _passphrases.constFind(keyPath);
    if (known != _passphrases.constEnd()) {
      return okAnswer(known.value());
    }
  }

  QString passphrase;
  if (!askPassphrase(keyPath, &passphrase)) {
    return kNoAnswer;
  }

  QMutexLocker locker(&_stateMutex);
  _passphrases.insert(keyPath, passphrase);
  return okAnswer(passphrase);
}

bool AskPassServer::askPassphrase(const QString &keyPath, QString *passphrase) {
  EventManager::EventManager &eventManager =
      EventManager::EventManager::getInstance();
  const std::size_t id = qHash(QStringLiteral("hg-ssh|") + keyPath);

  bool shown = false;
  for (;;) {
    // Waiting starts before the overlay can answer.
    const int token = eventManager.startWait();
    if (shown) {
      sendPromptSignal(
          EventManager::EventType::GitRepoManagerGetCredentialsInvalid, id);
    } else {
      GitRepo::GitCredentialsRequest request;
      request.id = id;
      request.url = keyPath;
      request.allowedTypes = 0;
      request.needsPassphrase = true;

      EventManager::Event event(
          EventManager::EventType::GitRepoManagerGetCredentials);
      event.addData(request);
      eventManager.send(event);
      shown = true;
    }

    const auto predicate = [id](EventManager::Event event) -> bool {
      switch (event.type()) {
      case EventManager::EventType::GitRepoManagerGetCredentialsCancel:
        return event.data<GitRepo::GitCredentialsErrorResponse>().id == id;
      case EventManager::EventType::GitRepoManagerGetCredentialsSubmit:
        return event.data<GitRepo::GitCredentialsResponse>().id == id;
      default:
        return false;
      }
    };
    const std::set<EventManager::EventType> types{
        EventManager::EventType::GitRepoManagerGetCredentialsCancel,
        EventManager::EventType::GitRepoManagerGetCredentialsSubmit};
    // stop() sets _stopping before it reads the id, so one of the two sees
    // the other.
    _waitingPromptId = id;
    if (_stopping) {
      _waitingPromptId = 0;
      return false;
    }
    const auto &[success, event] =
        eventManager.waitOneOf(token, types, predicate);
    _waitingPromptId = 0;
    if (!success ||
        event.type() ==
            EventManager::EventType::GitRepoManagerGetCredentialsCancel) {
      // The overlay closes itself on Cancel.
      return false;
    }

    const auto response = event.data<GitRepo::GitCredentialsResponse>();
    *passphrase = !response.passphrase.isEmpty() ? response.passphrase
                                                 : response.password;
    if (checkPassphrase(keyPath, *passphrase)) {
      sendPromptSignal(
          EventManager::EventType::GitRepoManagerGetCredentialsFinished, id);
      return true;
    }
  }
}

bool AskPassServer::checkPassphrase(const QString &keyPath,
                                    const QString &passphrase) {
  const QString keygen = sshKeygenExecutable();
  if (keygen.isEmpty()) {
    // ssh tells when it is wrong.
    return true;
  }

  const QByteArray checkToken = randomToken();
  {
    QMutexLocker locker(&_stateMutex);
    _checkToken = checkToken;
    _checkPassphrase = passphrase;
  }

  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("SSH_ASKPASS"), _program);
  env.insert(QStringLiteral("SSH_ASKPASS_REQUIRE"), QStringLiteral("force"));
  env.insert(QStringLiteral("SOURCERER_ASKPASS_SOCKET"), _socketName);
  env.insert(QStringLiteral("SOURCERER_ASKPASS_TOKEN"),
             QString::fromLatin1(checkToken));

  QProcess process;
  process.setProcessEnvironment(env);
#ifdef Q_OS_WIN
  process.setCreateProcessArgumentsModifier(
      [](QProcess::CreateProcessArguments *arguments) {
        // CREATE_NO_WINDOW
        arguments->flags |= 0x08000000;
      });
#endif
  process.start(keygen, {QStringLiteral("-y"), QStringLiteral("-f"),
                         QDir::toNativeSeparators(keyPath)});
  const bool opened = process.waitForFinished(kCheckTimeoutMs) &&
                      process.exitStatus() == QProcess::NormalExit &&
                      process.exitCode() == 0;
  if (process.state() != QProcess::NotRunning) {
    process.kill();
    process.waitForFinished();
  }

  QMutexLocker locker(&_stateMutex);
  _checkToken.clear();
  _checkPassphrase.clear();
  return opened;
}

} // namespace HgRepoManager::Workers
