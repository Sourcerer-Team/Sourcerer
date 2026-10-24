#pragma once

// "hg serve" behind a proxy that asks for HTTP Basic authentication, for the
// credential tests. The proxy runs in its own thread because HgRepo blocks the
// test thread while hg talks to it.

#include <HgRepo.h>

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QProcess>
#include <QProcessEnvironment>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <atomic>
#include <thread>

class HgAuthServer {
public:
  HgAuthServer(QString repoDir, const QString &username,
               const QString &password)
      : _repoDir(std::move(repoDir)),
        _expected("Basic " + (username + ':' + password).toUtf8().toBase64()) {}

  ~HgAuthServer() {
    _stop = true;
    if (_thread.joinable()) {
      _thread.join();
    }
    if (_hg.state() != QProcess::NotRunning) {
      _hg.kill();
      _hg.waitForFinished();
    }
  }

  HgAuthServer(const HgAuthServer &) = delete;
  HgAuthServer &operator=(const HgAuthServer &) = delete;

  bool start() {
    _hgPort = freePort();
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("HGPLAIN"), QStringLiteral("1"));
    _hg.setProcessEnvironment(env);
    _hg.setWorkingDirectory(_repoDir);
    _hg.start(HgRepo::findExecutable(),
              {QStringLiteral("serve"), QStringLiteral("--address"),
               QStringLiteral("127.0.0.1"), QStringLiteral("--port"),
               QString::number(_hgPort), QStringLiteral("--config"),
               QStringLiteral("web.push_ssl=False"), QStringLiteral("--config"),
               QStringLiteral("web.allow-push=*")});
    if (!_hg.waitForStarted() || !waitForPort(_hgPort)) {
      return false;
    }

    std::atomic<bool> listening = false;
    _thread = std::thread([this, &listening]() { proxyLoop(&listening); });
    QElapsedTimer timer;
    timer.start();
    while (!listening && timer.elapsed() < 10000) {
      QThread::msleep(10);
    }
    return listening;
  }

  // URL of the proxy, as hg names it in its messages.
  QString url() const {
    return QStringLiteral("http://127.0.0.1:%1/").arg(_proxyPort.load());
  }

  // Requests the proxy refused, and the ones it passed on.
  int refused() const { return _refused; }
  int passed() const { return _passed; }

private:
  static quint16 freePort() {
    QTcpServer server;
    server.listen(QHostAddress::LocalHost, 0);
    return server.serverPort();
  }

  static bool waitForPort(const quint16 port) {
    QElapsedTimer timer;
    timer.start();
    while (timer.elapsed() < 20000) {
      QTcpSocket socket;
      socket.connectToHost(QHostAddress::LocalHost, port);
      if (socket.waitForConnected(200)) {
        return true;
      }
      QThread::msleep(100);
    }
    return false;
  }

  void proxyLoop(std::atomic<bool> *listening) {
    QTcpServer server;
    if (!server.listen(QHostAddress::LocalHost, 0)) {
      return;
    }
    _proxyPort = server.serverPort();
    *listening = true;

    while (!_stop) {
      if (!server.waitForNewConnection(100)) {
        continue;
      }
      while (QTcpSocket *client = server.nextPendingConnection()) {
        serve(client);
        delete client;
      }
    }
  }

  // One request per connection: the response asks to close it.
  void serve(QTcpSocket *client) {
    QByteArray request;
    int headerEnd = -1;
    while ((headerEnd = request.indexOf("\r\n\r\n")) < 0) {
      if (!client->waitForReadyRead(10000)) {
        return;
      }
      request += client->readAll();
    }

    QByteArray authorization;
    qint64 contentLength = 0;
    QList<QByteArray> headers = request.left(headerEnd).split('\n');
    QByteArray forwarded = headers.takeFirst().trimmed() + "\r\n";
    for (const QByteArray &header : headers) {
      const QByteArray line = header.trimmed();
      const int colon = line.indexOf(':');
      const QByteArray name = line.left(colon).trimmed().toLower();
      const QByteArray value = line.mid(colon + 1).trimmed();
      if (name == "authorization") {
        authorization = value;
      } else if (name == "content-length") {
        contentLength = value.toLongLong();
      }
      if (name != "connection") {
        forwarded += line + "\r\n";
      }
    }
    forwarded += "Connection: close\r\n\r\n";

    QByteArray body = request.mid(headerEnd + 4);
    while (body.size() < contentLength) {
      if (!client->waitForReadyRead(10000)) {
        return;
      }
      body += client->readAll();
    }

    if (authorization != _expected) {
      ++_refused;
      client->write("HTTP/1.1 401 Unauthorized\r\n"
                    "WWW-Authenticate: Basic realm=\"sourcerer\"\r\n"
                    "Content-Length: 0\r\n"
                    "Connection: close\r\n\r\n");
      client->waitForBytesWritten(5000);
      client->disconnectFromHost();
      return;
    }

    ++_passed;
    QTcpSocket upstream;
    upstream.connectToHost(QHostAddress::LocalHost, _hgPort);
    if (!upstream.waitForConnected(5000)) {
      return;
    }
    upstream.write(forwarded + body);
    upstream.waitForBytesWritten(5000);
    while (upstream.state() == QAbstractSocket::ConnectedState &&
           upstream.waitForReadyRead(10000)) {
      client->write(upstream.readAll());
    }
    client->write(upstream.readAll());
    client->waitForBytesWritten(5000);
    client->disconnectFromHost();
    if (client->state() != QAbstractSocket::UnconnectedState) {
      client->waitForDisconnected(1000);
    }
  }

  QString _repoDir;
  QByteArray _expected;
  QProcess _hg;
  quint16 _hgPort = 0;
  std::atomic<quint16> _proxyPort = 0;
  std::atomic<bool> _stop = false;
  std::atomic<int> _refused = 0;
  std::atomic<int> _passed = 0;
  std::thread _thread;
};
