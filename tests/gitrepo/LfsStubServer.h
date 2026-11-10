// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

// A minimal Git LFS server for the LFS tests: the batch API
// (POST <base>/objects/batch), object downloads (GET /objects/<oid>), uploads
// (PUT /objects/<oid>) and verification (POST /verify). Objects are kept in
// memory. It runs in its own thread because GitRepo waits for the replies
// on the test thread.

#include <QByteArray>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QHash>
#include <QHostAddress>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>
#include <QThread>

#include <atomic>
#include <thread>

class LfsStubServer {
public:
  // What the server does wrong, to test the error handling of the client.
  enum class Fault {
    None,
    // The batch request fails with this HTTP status.
    BatchStatus,
    // The batch response is not JSON.
    MalformedBatch,
    // The batch response has no actions for the object.
    NoActions,
    // The download sends other content than requested.
    WrongContent,
    // The download fails with this HTTP status.
    DownloadStatus,
    // The upload fails with this HTTP status.
    UploadStatus,
  };

  LfsStubServer() = default;

  ~LfsStubServer() {
    _stop = true;
    if (_thread.joinable()) {
      _thread.join();
    }
  }

  LfsStubServer(const LfsStubServer &) = delete;
  LfsStubServer &operator=(const LfsStubServer &) = delete;

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

  // http://127.0.0.1:<port>
  QString origin() const {
    return QStringLiteral("http://127.0.0.1:%1").arg(_port.load());
  }

  void setFault(Fault fault, int status = 500) {
    QMutexLocker locker(&_mutex);
    _fault = fault;
    _faultStatus = status;
  }

  // Objects the server has, by oid.
  void addObject(const QByteArray &content) {
    QMutexLocker locker(&_mutex);
    _objects.insert(oidOf(content), content);
  }
  bool hasObject(const QString &oid) const {
    QMutexLocker locker(&_mutex);
    return _objects.contains(oid);
  }
  int objectCount() const {
    QMutexLocker locker(&_mutex);
    return _objects.size();
  }

  // "METHOD /path" of every request so far.
  QStringList requests() const {
    QMutexLocker locker(&_mutex);
    return _requests;
  }
  // Operation and oids of every batch request so far.
  QStringList batches() const {
    QMutexLocker locker(&_mutex);
    return _batches;
  }
  // The Authorization header of the last request.
  QByteArray lastAuthorization() const {
    QMutexLocker locker(&_mutex);
    return _lastAuthorization;
  }
  void clearLog() {
    QMutexLocker locker(&_mutex);
    _requests.clear();
    _batches.clear();
  }

  static QString oidOf(const QByteArray &content) {
    return QString::fromLatin1(
        QCryptographicHash::hash(content, QCryptographicHash::Sha256).toHex());
  }

private:
  struct Request {
    QByteArray method;
    QByteArray path;
    QByteArray body;
    QByteArray authorization;
  };

  void loop(std::atomic<bool> *listening) {
    QTcpServer server;
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

  static bool readRequest(QTcpSocket *client, Request *request) {
    QByteArray data;
    int headerEnd = -1;
    while ((headerEnd = data.indexOf("\r\n\r\n")) < 0) {
      if (!client->waitForReadyRead(10000)) {
        return false;
      }
      data += client->readAll();
    }

    QList<QByteArray> lines = data.left(headerEnd).split('\n');
    const QList<QByteArray> requestLine = lines.takeFirst().trimmed().split(' ');
    if (requestLine.size() < 2) {
      return false;
    }
    request->method = requestLine.at(0);
    request->path = requestLine.at(1);

    qint64 contentLength = 0;
    for (const QByteArray &line : lines) {
      const int colon = line.indexOf(':');
      const QByteArray name = line.left(colon).trimmed().toLower();
      const QByteArray value = line.mid(colon + 1).trimmed();
      if (name == "content-length") {
        contentLength = value.toLongLong();
      } else if (name == "authorization") {
        request->authorization = value;
      }
    }

    request->body = data.mid(headerEnd + 4);
    while (request->body.size() < contentLength) {
      if (!client->waitForReadyRead(10000)) {
        return false;
      }
      request->body += client->readAll();
    }
    return true;
  }

  static void reply(QTcpSocket *client, int status, const QByteArray &body,
                    const QByteArray &contentType =
                        "application/vnd.git-lfs+json") {
    QByteArray response = "HTTP/1.1 " + QByteArray::number(status) + " Stub\r\n";
    response += "Content-Type: " + contentType + "\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;
    client->write(response);
    client->waitForBytesWritten(5000);
    client->disconnectFromHost();
    if (client->state() != QAbstractSocket::UnconnectedState) {
      client->waitForDisconnected(1000);
    }
  }

  void serve(QTcpSocket *client) {
    Request request;
    if (!readRequest(client, &request)) {
      return;
    }

    QMutexLocker locker(&_mutex);
    _requests += QString::fromLatin1(request.method + ' ' + request.path);
    _lastAuthorization = request.authorization;

    if (request.method == "POST" && request.path.endsWith("/objects/batch")) {
      serveBatch(client, request);
    } else if (request.method == "GET" && request.path.startsWith("/objects/")) {
      const QString oid = QString::fromLatin1(request.path.mid(9));
      if (_fault == Fault::DownloadStatus) {
        reply(client, _faultStatus, "{}");
      } else if (!_objects.contains(oid)) {
        reply(client, 404, "{\"message\":\"not found\"}");
      } else if (_fault == Fault::WrongContent) {
        QByteArray other = _objects.value(oid);
        if (!other.isEmpty()) {
          other[0] = other[0] == 'x' ? 'y' : 'x';
        }
        reply(client, 200, other, "application/octet-stream");
      } else {
        reply(client, 200, _objects.value(oid), "application/octet-stream");
      }
    } else if (request.method == "PUT" && request.path.startsWith("/objects/")) {
      if (_fault == Fault::UploadStatus) {
        reply(client, _faultStatus, "{}");
        return;
      }
      const QString oid = QString::fromLatin1(request.path.mid(9));
      if (oidOf(request.body) != oid) {
        reply(client, 422, "{\"message\":\"checksum mismatch\"}");
        return;
      }
      _objects.insert(oid, request.body);
      reply(client, 200, QByteArray());
    } else if (request.method == "POST" && request.path == "/verify") {
      const QJsonObject object = QJsonDocument::fromJson(request.body).object();
      const QString oid = object.value(QStringLiteral("oid")).toString();
      reply(client, _objects.contains(oid) ? 200 : 404, "{}");
    } else {
      reply(client, 404, "{}");
    }
  }

  void serveBatch(QTcpSocket *client, const Request &request) {
    if (_fault == Fault::BatchStatus) {
      reply(client, _faultStatus, "{\"message\":\"denied\"}");
      return;
    }
    if (_fault == Fault::MalformedBatch) {
      reply(client, 200, "this is not json");
      return;
    }

    const QJsonObject body = QJsonDocument::fromJson(request.body).object();
    const QString operation = body.value(QStringLiteral("operation")).toString();
    const QString base = origin();
    QStringList oids;
    QJsonArray objects;
    for (const QJsonValue &value :
         body.value(QStringLiteral("objects")).toArray()) {
      const QJsonObject requested = value.toObject();
      const QString oid = requested.value(QStringLiteral("oid")).toString();
      oids += oid;
      QJsonObject object{{"oid", oid},
                         {"size", requested.value(QStringLiteral("size"))}};
      if (_fault != Fault::NoActions) {
        const QJsonObject href{{"href", base + QStringLiteral("/objects/") + oid}};
        if (operation == QStringLiteral("download")) {
          object.insert(QStringLiteral("actions"), QJsonObject{{"download", href}});
        } else if (!_objects.contains(oid)) {
          object.insert(
              QStringLiteral("actions"),
              QJsonObject{{"upload", href},
                          {"verify",
                           QJsonObject{{"href", base + QStringLiteral("/verify")}}}});
        }
      }
      objects.append(object);
    }
    _batches += operation + ':' + oids.join(',');
    reply(client, 200,
          QJsonDocument(QJsonObject{{"transfer", "basic"}, {"objects", objects}})
              .toJson(QJsonDocument::Compact));
  }

  mutable QMutex _mutex;
  QHash<QString, QByteArray> _objects;
  QStringList _requests;
  QStringList _batches;
  QByteArray _lastAuthorization;
  Fault _fault = Fault::None;
  int _faultStatus = 500;
  std::atomic<quint16> _port = 0;
  std::atomic<bool> _stop = false;
  std::thread _thread;
};
