// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

// Git's smart HTTP protocol (`git http-backend`) behind HTTP Basic
// authentication, for the credential tests. Every request runs
// `git http-backend` as a CGI program. The server runs in its own thread
// because GitRepo blocks the test thread while libgit2 talks to it. With
// useTls() it serves HTTPS, for the certificate tests.

#include <QByteArray>
#include <QElapsedTimer>
#include <QHostAddress>
#include <QList>
#include <QProcess>
#include <QProcessEnvironment>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <atomic>
#include <thread>

class GitHttpServer {
public:
  // Serves the repositories below `root` (e.g. root/remote.git), only to
  // requests with these credentials. An empty username serves everyone.
  GitHttpServer(QString root, const QString &username, const QString &password,
                QProcessEnvironment gitEnvironment)
      : _root(std::move(root)),
        _expected("Basic " + (username + ':' + password).toUtf8().toBase64()),
        _username(username), _environment(std::move(gitEnvironment)) {}

  ~GitHttpServer() {
    _stop = true;
    if (_thread.joinable()) {
      _thread.join();
    }
  }

  GitHttpServer(const GitHttpServer &) = delete;
  GitHttpServer &operator=(const GitHttpServer &) = delete;

  bool start() {
    std::atomic<bool> listening = false;
    _thread = std::thread([this, &listening]() { loop(&listening); });
    QElapsedTimer timer;
    timer.start();
    while (!listening && timer.elapsed() < 10000) {
      QThread::msleep(10);
    }
    return listening;
  }

  // Serves HTTPS with this certificate. Call it before start().
  void useTls(const QSslCertificate &certificate, const QSslKey &key) {
    _tls = QSslConfiguration::defaultConfiguration();
    _tls.setLocalCertificate(certificate);
    _tls.setPrivateKey(key);
    _tls.setPeerVerifyMode(QSslSocket::VerifyNone);
    _useTls = true;
  }

  // URL of a repository below the root, e.g. url("remote.git").
  QString url(const QString &repository) const {
    return QStringLiteral("%1://127.0.0.1:%2/%3")
        .arg(_useTls ? QStringLiteral("https") : QStringLiteral("http"))
        .arg(_port.load())
        .arg(repository);
  }

  // Waits this long before answering each request, for slow operations.
  void setDelay(int milliseconds) { _delay = milliseconds; }

  // TLS handshakes that failed, e.g. because the client rejected the
  // certificate.
  int failedHandshakes() const { return _failedHandshakes; }

  // Requests refused for missing or wrong credentials, and the ones served.
  int refused() const { return _refused; }
  int passed() const { return _passed; }

private:
  struct Request {
    QByteArray method;
    QByteArray path;
    QByteArray query;
    QByteArray authorization;
    QByteArray contentType;
    QByteArray body;
  };

  // Wraps accepted connections in a server-side QSslSocket.
  class Server : public QTcpServer {
  public:
    explicit Server(const GitHttpServer *owner) : _owner(owner) {}

  protected:
    void incomingConnection(qintptr handle) override {
      if (!_owner->_useTls) {
        QTcpServer::incomingConnection(handle);
        return;
      }
      auto *socket = new QSslSocket(this);
      if (!socket->setSocketDescriptor(handle)) {
        delete socket;
        return;
      }
      socket->setSslConfiguration(_owner->_tls);
      socket->startServerEncryption();
      addPendingConnection(socket);
    }

  private:
    const GitHttpServer *_owner;
  };

  void loop(std::atomic<bool> *listening) {
    Server server(this);
    if (!server.listen(QHostAddress::LocalHost, 0)) {
      return;
    }
    _port = server.serverPort();
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

  static bool readMore(QTcpSocket *client, QByteArray *data) {
    if (!client->bytesAvailable() && !client->waitForReadyRead(10000)) {
      return false;
    }
    *data += client->readAll();
    return true;
  }

  static bool readRequest(QTcpSocket *client, Request *request) {
    QByteArray data;
    int headerEnd = -1;
    while ((headerEnd = data.indexOf("\r\n\r\n")) < 0) {
      if (!readMore(client, &data)) {
        return false;
      }
    }

    QList<QByteArray> lines = data.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.takeFirst().trimmed().split(' ');
    if (requestLine.size() < 2) {
      return false;
    }
    request->method = requestLine.at(0);
    const QByteArray target = requestLine.at(1);
    const int question = target.indexOf('?');
    request->path = question < 0 ? target : target.left(question);
    request->query = question < 0 ? QByteArray() : target.mid(question + 1);

    qint64 contentLength = -1;
    bool chunked = false;
    for (const QByteArray &line : lines) {
      const int colon = line.indexOf(':');
      const QByteArray name = line.left(colon).trimmed().toLower();
      const QByteArray value = line.mid(colon + 1).trimmed();
      if (name == "content-length") {
        contentLength = value.toLongLong();
      } else if (name == "transfer-encoding") {
        chunked = value.toLower().contains("chunked");
      } else if (name == "authorization") {
        request->authorization = value;
      } else if (name == "content-type") {
        request->contentType = value;
      }
    }

    QByteArray rest = data.mid(headerEnd + 4);
    if (chunked) {
      // <hex size>\r\n<data>\r\n ... 0\r\n\r\n
      for (;;) {
        int lineEnd = -1;
        while ((lineEnd = rest.indexOf("\r\n")) < 0) {
          if (!readMore(client, &rest)) {
            return false;
          }
        }
        bool ok = false;
        const qint64 size =
            rest.left(lineEnd).split(';').first().trimmed().toLongLong(&ok, 16);
        if (!ok) {
          return false;
        }
        rest.remove(0, lineEnd + 2);
        while (rest.size() < size + 2) {
          if (!readMore(client, &rest)) {
            return false;
          }
        }
        if (size == 0) {
          break;
        }
        request->body += rest.left(size);
        rest.remove(0, size + 2);
      }
    } else if (contentLength > 0) {
      while (rest.size() < contentLength) {
        if (!readMore(client, &rest)) {
          return false;
        }
      }
      request->body = rest.left(contentLength);
    }
    return true;
  }

  static void reply(QTcpSocket *client, const QByteArray &statusLine,
                    const QByteArray &headers, const QByteArray &body) {
    QByteArray response = "HTTP/1.1 " + statusLine + "\r\n" + headers;
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;
    client->write(response);
    client->waitForBytesWritten(10000);
    client->disconnectFromHost();
    if (client->state() != QAbstractSocket::UnconnectedState) {
      client->waitForDisconnected(1000);
    }
  }

  void serve(QTcpSocket *client) {
    if (auto *tls = qobject_cast<QSslSocket *>(client)) {
      if (!tls->waitForEncrypted(10000)) {
        ++_failedHandshakes;
        return;
      }
    }

    Request request;
    if (!readRequest(client, &request)) {
      return;
    }

    if (!_username.isEmpty() && request.authorization != _expected) {
      ++_refused;
      reply(client, "401 Unauthorized",
            "WWW-Authenticate: Basic realm=\"sourcerer\"\r\n", QByteArray());
      return;
    }
    ++_passed;
    if (_delay > 0) {
      QThread::msleep(static_cast<unsigned long>(_delay.load()));
    }

    QProcessEnvironment env = _environment;
    env.insert(QStringLiteral("GIT_PROJECT_ROOT"), _root);
    env.insert(QStringLiteral("GIT_HTTP_EXPORT_ALL"), QStringLiteral("1"));
    env.insert(QStringLiteral("REQUEST_METHOD"),
               QString::fromLatin1(request.method));
    env.insert(QStringLiteral("PATH_INFO"), QString::fromUtf8(request.path));
    env.insert(QStringLiteral("QUERY_STRING"), QString::fromUtf8(request.query));
    env.insert(QStringLiteral("CONTENT_TYPE"),
               QString::fromLatin1(request.contentType));
    env.insert(QStringLiteral("CONTENT_LENGTH"),
               QString::number(request.body.size()));
    env.insert(QStringLiteral("REMOTE_USER"), _username);
    env.insert(QStringLiteral("REMOTE_ADDR"), QStringLiteral("127.0.0.1"));

    QProcess backend;
    backend.setProcessEnvironment(env);
    backend.start(QStringLiteral("git"), {QStringLiteral("http-backend")});
    if (!backend.waitForStarted(10000)) {
      reply(client, "500 Internal Server Error", QByteArray(), QByteArray());
      return;
    }
    backend.write(request.body);
    backend.closeWriteChannel();
    backend.waitForFinished(60000);
    const QByteArray output = backend.readAllStandardOutput();

    // CGI output: headers, an empty line, the body.
    int split = output.indexOf("\r\n\r\n");
    int separator = 4;
    if (split < 0) {
      split = output.indexOf("\n\n");
      separator = 2;
    }
    if (split < 0) {
      reply(client, "500 Internal Server Error", QByteArray(), output);
      return;
    }
    QByteArray statusLine = "200 OK";
    QByteArray headers;
    for (QByteArray line : output.left(split).split('\n')) {
      line = line.trimmed();
      if (line.isEmpty()) {
        continue;
      }
      if (line.toLower().startsWith("status:")) {
        statusLine = line.mid(7).trimmed();
      } else {
        headers += line + "\r\n";
      }
    }
    reply(client, statusLine, headers, output.mid(split + separator));
  }

  QString _root;
  QByteArray _expected;
  QString _username;
  QProcessEnvironment _environment;
  QSslConfiguration _tls;
  bool _useTls = false;
  std::atomic<quint16> _port = 0;
  std::atomic<bool> _stop = false;
  std::atomic<int> _refused = 0;
  std::atomic<int> _passed = 0;
  std::atomic<int> _failedHandshakes = 0;
  std::atomic<int> _delay = 0;
  std::thread _thread;
};
