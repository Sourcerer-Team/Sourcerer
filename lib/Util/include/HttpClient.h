
#pragma once

#include <LockGuard.h>

#include <QList>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QSslCertificate>
#include <QThread>

class QNetworkAccessManager;
class QSslError;

namespace Util {

template <typename T> class HttpClientWorkerThread : public QThread {
public:
  explicit HttpClientWorkerThread(T *worker, QObject *parent = nullptr)
      : QThread(parent), _worker(worker) {
    _worker->moveToThread(this);
    start();
  }

  virtual ~HttpClientWorkerThread() {
    quit();
    wait();
  }

  T *worker() const { return _worker; }

protected:
  void run() override {
    QThread::run();

    delete _worker;
  }

private:
  T *_worker;
};

enum HttpMethod { Unknown = 0, Delete, Get, Post, Put };

class HttpReply : public QObject {
  Q_OBJECT

public:
  HttpReply() : _error(QNetworkReply::NetworkError::NoError) {}

  HttpReply(QNetworkReply::NetworkError error, const QByteArray &bytes,
            const QString &errorString = QString())
      : _error(error), _response(bytes), _errorString(errorString) {}

  HttpReply &operator=(const HttpReply &that) {
    if (this == &that) {
      return *this;
    }

    _error = that._error;
    _response = that._response;
    _errorString = that._errorString;

    return *this;
  }

  HttpReply(const HttpReply &that) {
    _error = that._error;
    _response = that._response;
    _errorString = that._errorString;
  }

  QNetworkReply::NetworkError error() const { return _error; }

  const QByteArray &response() const { return _response; }

  const QString &errorString() const { return _errorString; }

private:
  QNetworkReply::NetworkError _error;
  QByteArray _response;
  QString _errorString;
};

class HttpClientWorker : public QObject {
  Q_OBJECT

public:
  explicit HttpClientWorker(QObject *parent = nullptr);

  virtual ~HttpClientWorker();

signals:

  void clearConnections();

  Util::HttpReply
  sendRequest(const QString &url, Util::HttpMethod httpMethod,
              const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
              const QMap<QString, QString> &rawHeaders,
              const QByteArray &requestBody);

private slots:

  void clearConnections_();

  void replyFinished();

  void replySslErrors(const QList<QSslError> &errors);

  Util::HttpReply
  sendRequest_(const QString &url, Util::HttpMethod httpMethod,
               const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
               const QMap<QString, QString> &rawHeaders,
               const QByteArray &requestBody);

private:
  void loadClientCertificate();

  void start();

  void stop();

  void sendOnlineEvent(bool online) const;

private:
  QNetworkAccessManager *_accessManager;
  QNetworkReply *_reply;
  Util::Mutex _replyMutex;

  bool _online;
  QSslCertificate _certificate;
};

class HttpClient : public HttpClientWorkerThread<HttpClientWorker> {
  Q_OBJECT

public:
  HttpClient();
  virtual ~HttpClient();

  Util::HttpReply
  sendRequest(const QString &url, HttpMethod httpMethod,
              const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
              const QByteArray &requestBody);

  Util::HttpReply
  sendRequest(const QString &url, HttpMethod httpMethod,
              const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
              const QMap<QString, QString> &rawHeaders,
              const QByteArray &requestBody);

  void clearConnections();
};

} // namespace Util
