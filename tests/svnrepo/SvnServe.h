// MIT License
// Copyright (c) 2026 Sourcerer

// Runs svnserve on 127.0.0.1 for the credential tests: one repository with
// the standard layout, anonymous access off, one user with a password.

#pragma once

#include "SvnTestHelpers.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

class SvnServe {
public:
  // Serves root, where the repository "repo" is made.
  explicit SvnServe(const QString &root) : _root(root) {}

  ~SvnServe() { stop(); }

  SvnServe(const SvnServe &) = delete;
  SvnServe &operator=(const SvnServe &) = delete;

  static bool available() {
    return !SvnTest::findTool(QStringLiteral("svnserve")).isEmpty();
  }

  bool start(const QString &username, const QString &password) {
    const QString repo = _root + QStringLiteral("/repo");
    if (SvnTest::makeRepo(repo, SvnTest::Layout::Standard).isEmpty() ||
        !SvnTest::writeFile(repo + QStringLiteral("/conf/svnserve.conf"),
                            "[general]\nanon-access = none\n"
                            "auth-access = write\npassword-db = passwd\n"
                            "realm = sourcerer-tests\n") ||
        !SvnTest::writeFile(repo + QStringLiteral("/conf/passwd"),
                            "[users]\n" + username.toUtf8() + " = " +
                                password.toUtf8() + '\n')) {
      return false;
    }

    // A free port: taken from a server that is closed again right away.
    QTcpServer probe;
    if (!probe.listen(QHostAddress::LocalHost)) {
      return false;
    }
    _port = probe.serverPort();
    probe.close();

    _process.setProgram(SvnTest::findTool(QStringLiteral("svnserve")));
    _process.setArguments({QStringLiteral("-d"), QStringLiteral("--foreground"),
                           QStringLiteral("--listen-host"),
                           QStringLiteral("127.0.0.1"),
                           QStringLiteral("--listen-port"),
                           QString::number(_port), QStringLiteral("-r"),
                           _root});
    _process.start();
    if (!_process.waitForStarted(10000)) {
      return false;
    }

    // Ready once it accepts connections.
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 10000) {
      QTcpSocket socket;
      socket.connectToHost(QHostAddress::LocalHost, _port);
      if (socket.waitForConnected(500)) {
        return true;
      }
      QThread::msleep(100);
    }
    return false;
  }

  void stop() {
    if (_process.state() != QProcess::NotRunning) {
      _process.kill();
      _process.waitForFinished(5000);
    }
  }

  // svn://127.0.0.1:<port>/repo<path>
  QString url(const QString &path = QString()) const {
    return QStringLiteral("svn://127.0.0.1:%1/repo%2").arg(_port).arg(path);
  }

  QString repoDir() const { return _root + QStringLiteral("/repo"); }

private:
  QString _root;
  quint16 _port = 0;
  QProcess _process;
};
