// MIT License
// Copyright (c) 2026 Sourcerer

// A TLS server on the loopback addresses (IPv4 and IPv6) that presents a
// certificate, answers every request with 404 and closes: enough for a
// client to reach its server certificate check, without a Subversion server
// behind it. It runs in its own thread, because the client blocks the test
// thread.

#pragma once

#include <QAtomicInt>
#include <QMutex>
#include <QSemaphore>
#include <QSslCertificate>
#include <QSslConfiguration>
#include <QSslKey>
#include <QSslSocket>
#include <QTcpServer>
#include <QThread>

class TlsHandshakeServer : public QThread {
public:
  TlsHandshakeServer(const QSslCertificate &certificate, const QSslKey &key)
      : _certificate(certificate), _key(key) {}

  ~TlsHandshakeServer() override {
    quit();
    wait();
  }

  // Starts listening; false if it couldn't.
  bool startServer() {
    start();
    _ready.acquire();
    return _port != 0;
  }

  quint16 port() const { return _port; }

  // Connections whose TLS handshake completed.
  int handshakes() const { return _handshakes.loadRelaxed(); }

protected:
  void run() override {
    // "localhost" may resolve to ::1 or 127.0.0.1: both, on one port.
    Server server(this);
    Server server6(this);
    if (server.listen(QHostAddress::LocalHost)) {
      _port = server.serverPort();
      server6.listen(QHostAddress::LocalHostIPv6, _port);
    }
    _ready.release();
    if (_port != 0) {
      exec();
    }
  }

private:
  class Server : public QTcpServer {
  public:
    explicit Server(TlsHandshakeServer *owner) : _owner(owner) {}

  protected:
    void incomingConnection(qintptr descriptor) override {
      auto *socket = new QSslSocket(this);
      if (!socket->setSocketDescriptor(descriptor)) {
        delete socket;
        return;
      }
      QSslConfiguration tls = QSslConfiguration::defaultConfiguration();
      tls.setLocalCertificate(_owner->_certificate);
      tls.setPrivateKey(_owner->_key);
      tls.setPeerVerifyMode(QSslSocket::VerifyNone);
      socket->setSslConfiguration(tls);
      QObject::connect(socket, &QSslSocket::encrypted, socket, [this] {
        _owner->_handshakes.fetchAndAddRelaxed(1);
      });
      QObject::connect(socket, &QSslSocket::readyRead, socket, [socket] {
        socket->readAll();
        socket->write("HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n"
                      "Connection: close\r\n\r\n");
        socket->disconnectFromHost();
      });
      QObject::connect(socket, &QSslSocket::disconnected, socket,
                       &QObject::deleteLater);
      socket->startServerEncryption();
    }

  private:
    TlsHandshakeServer *_owner;
  };

  QSslCertificate _certificate;
  QSslKey _key;
  QSemaphore _ready;
  quint16 _port = 0;
  QAtomicInt _handshakes;
};
