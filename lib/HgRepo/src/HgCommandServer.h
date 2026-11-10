// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

// Client of the Mercurial command server ("hg serve --cmdserver pipe"). Not
// part of the public API.

#include <QByteArray>
#include <QProcess>
#include <QString>
#include <QStringList>

namespace HgRepo::Private {

struct HgCommandResult {
  // The command ran to completion and its exit code is valid.
  bool finished = false;
  // The command was stopped because the quit flag was set.
  bool cancelled = false;
  int exitCode = -1;
  QByteArray out;
  QByteArray err;
};

// Environment hg runs with: plain, untranslated output in UTF-8.
// With the SSH_ASKPASS program of setAskPass.
QProcessEnvironment hgEnvironment();

// Whether setAskPass set a program.
bool hasAskPass();

// Hides the console window hg would open on Windows.
void hideConsoleWindow(QProcess &process);

// One command server process. It is used by a single thread and blocks while
// a command runs, so it needs no event loop.
class HgCommandServer {
public:
  // configArgs are global options ("--config", "section.name=value", ...)
  // that apply to every command.
  HgCommandServer(const QString &executable, const QString &workingDirectory,
                  const QStringList &configArgs);
  ~HgCommandServer();

  HgCommandServer(const HgCommandServer &) = delete;
  HgCommandServer &operator=(const HgCommandServer &) = delete;

  // Starts the server and reads its hello message. Returns false with
  // errorMessage() set on failure.
  bool start();
  // Closes the server's input so it exits, and kills it if it doesn't.
  void stop();
  bool isRunning() const;

  // Runs "hg <args>". input is sent when the command reads its standard
  // input. While *quitFlag is true the server is killed and the result is
  // cancelled; the next call must start() it again.
  HgCommandResult run(const QStringList &args, const QByteArray &input,
                      const bool *quitFlag);

  QString errorMessage() const { return _errorMessage; }

private:
  bool readExactly(QByteArray *data, qint64 size, const bool *quitFlag,
                   bool *cancelled);
  bool writeAll(const QByteArray &data);
  void kill();

  QString _executable;
  QString _workingDirectory;
  QStringList _configArgs;
  QProcess _process;
  QString _errorMessage;
};

} // namespace HgRepo::Private
