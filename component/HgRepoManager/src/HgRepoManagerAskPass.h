// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QByteArray>
#include <QHash>
#include <QMutex>
#include <QString>
#include <QThread>
#include <QThreadPool>

#include <atomic>
#include <cstddef>

#include <memory>

class QLocalServer;
class QLocalSocket;

namespace HgRepoManager::Workers {

// Answers the askpass program (src/askpass.cpp) that ssh runs for Mercurial
// when it needs a key passphrase. The passphrase is asked with the SSH
// passphrase overlay, checked with ssh-keygen before ssh gets it, and kept in
// memory for the key until Sourcerer quits. Other prompts, e.g. whether to
// trust an unknown host key, are answered with no: host keys are trusted in
// the SSH key manager.
class AskPassServer {
public:
  AskPassServer();
  ~AskPassServer();

  AskPassServer(const AskPassServer &) = delete;
  AskPassServer &operator=(const AskPassServer &) = delete;

  // Listens on a local socket only the user can open and points HgRepo's ssh
  // at program. False when program is missing or the socket can't listen.
  bool start(const QString &program);
  void stop();

  // The askpass program next to the application, empty when it isn't there.
  static QString defaultProgram();

private:
  void onConnection(QLocalSocket *socket);
  // Answer to a prompt, "OK <base64>" or "NO". Runs on a pool thread, one
  // prompt at a time.
  QByteArray answer(const QString &prompt);
  // Asks the user until the passphrase opens the key or they cancel.
  bool askPassphrase(const QString &keyPath, QString *passphrase);
  // Whether passphrase opens keyPath, by letting ssh-keygen read the key
  // through the askpass program with a one-time token.
  bool checkPassphrase(const QString &keyPath, const QString &passphrase);

  QString _program;
  QString _socketName;
  QByteArray _token;
  std::unique_ptr<QThread> _thread;
  QLocalServer *_server = nullptr;
  // Answers prompts, they wait for the user.
  QThreadPool _pool;
  // Overlay a prompt waits for, 0 when none. stop() cancels it.
  std::atomic<std::size_t> _waitingPromptId = 0;
  std::atomic<bool> _stopping = false;

  QMutex _promptMutex;
  QMutex _stateMutex;
  // Passphrases that opened their key, by key path.
  QHash<QString, QString> _passphrases;
  // One-time token of a running check and the passphrase it answers with.
  QByteArray _checkToken;
  QString _checkPassphrase;
};

} // namespace HgRepoManager::Workers
