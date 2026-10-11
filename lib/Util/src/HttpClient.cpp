#include "HttpClient.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QStandardPaths>
#include <QTimer>

#define THREAD_ID "HttpClient"

namespace Util {

HttpClientWorker::HttpClientWorker(QObject *parent)
    : QObject(parent), _accessManager(nullptr), _reply(nullptr), _online(true) {
  LOG_PERF(0, "Enter");

  loadClientCertificate();

  _accessManager = new QNetworkAccessManager(this);

  connect(this, &HttpClientWorker::clearConnections, this,
          &HttpClientWorker::clearConnections_, Qt::BlockingQueuedConnection);

  connect(this, &HttpClientWorker::sendRequest, this,
          &HttpClientWorker::sendRequest_, Qt::BlockingQueuedConnection);

  connect(this, &HttpClientWorker::downloadFile, this,
          &HttpClientWorker::downloadFile_, Qt::BlockingQueuedConnection);

  start();

  LOG_PERF(0, "Exit");
}

HttpClientWorker::~HttpClientWorker() {
  LOG_PERF(0, "Enter");

  stop();

  delete _accessManager;

  LOG_PERF(0, "Exit");
}

HttpReply HttpClientWorker::sendRequest_(
    const QString &url, HttpMethod httpMethod,
    const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
    const QMap<QString, QString> &rawHeaders, const QByteArray &requestBody) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_replyMutex);

  ::ConfigManager::ConfigManager &configManager =
      ::ConfigManager::ConfigManager::getInstance();

  const qlonglong readTimeout =
      configManager.getLong(::ConfigManager::ConfigKey::HttpClientTimeout);

  QNetworkRequest request;
  request.setUrl(url);
  request.setSslConfiguration(sslConfiguration());

  LOG_DEBUG("Request URL: {}", url.toStdString());

  for (const auto &knownHeader : httpHeaders.keys()) {
    request.setHeader(knownHeader, httpHeaders[knownHeader]);

    LOG_DEBUG("Header: {} ==> {}", (int)knownHeader,
              httpHeaders[knownHeader].toStdString());
  }

  for (const auto &rawHeader : rawHeaders.keys()) {
    request.setRawHeader(rawHeader.toUtf8(), rawHeaders[rawHeader].toUtf8());

    LOG_DEBUG("Raw header: {} ==> {}", rawHeader.toStdString(),
              rawHeaders[rawHeader].toStdString());
  }

  QTimer timer;
  timer.setSingleShot(true);

  QEventLoop eventLoop;

  connect(&timer, SIGNAL(timeout()), &eventLoop, SLOT(quit()));

  _reply = nullptr;

  switch (httpMethod) {
  case HttpMethod::Delete: {
    LOG_DEBUG("Method Delete");
    _reply = _accessManager->deleteResource(request);
    break;
  }
  case HttpMethod::Get: {
    LOG_DEBUG("Method Get");
    _reply = _accessManager->get(request);
    break;
  }
  case HttpMethod::Post: {
    LOG_DEBUG("Method Post");
    _reply = _accessManager->post(request, requestBody);
    break;
  }
  case HttpMethod::Put: {
    LOG_DEBUG("Method Put");
    _reply = _accessManager->put(request, requestBody);
    break;
  }
  default: {
    break;
  }
  }

  connect(_reply, SIGNAL(finished()), &eventLoop, SLOT(quit()));
  connect(_reply, SIGNAL(sslErrors(const QList<QSslError> &)), this,
          SLOT(replySslErrors(const QList<QSslError> &)));

  timer.start(readTimeout);

  eventLoop.exec();

  const HttpReply invalidHttpReply(QNetworkReply::UnknownServerError, {});

  if (!_reply) {
    LOG_ERROR(Log::ErrorId::Network, "Internal error. Unknown HTTP method {}",
              (int)httpMethod);
    LOG_PERF(0, "Exit");
    return invalidHttpReply;
  }

  QByteArray bytes;

  if (timer.isActive()) {
    timer.stop();

    bytes = _reply->readAll();

    if (_reply->error() != QNetworkReply::NetworkError::NoError) {
      if (_online) {
        _online = false;
        LOG_ERROR(Log::ErrorId::Network, "Gone offline! Error: {}, '{}'",
                  static_cast<int>(_reply->error()),
                  _reply->errorString().toStdString());
      }
    } else {
      if (!_online) {
        _online = true;
        LOG_ERROR(Log::ErrorId::Network, "Gone online!");
      }
    }

    const HttpReply httpReply(_reply->error(), bytes, _reply->errorString());

    LOG_PERF(0, "Exit");

    return httpReply;
  } else {
    _reply->abort();

    const HttpReply httpReply(QNetworkReply::TimeoutError, bytes,
                              QStringLiteral("Request timed out"));

    LOG_PERF(0, "Exit");

    return httpReply;
  }
}

HttpReply HttpClientWorker::downloadFile_(const QString &url,
                                          const QString &filePath) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_replyMutex);

  ::ConfigManager::ConfigManager &configManager =
      ::ConfigManager::ConfigManager::getInstance();

  const qlonglong stallTimeout =
      configManager.getLong(::ConfigManager::ConfigKey::HttpClientTimeout);

  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    LOG_PERF(0, "Exit");
    return HttpReply(QNetworkReply::UnknownContentError, {},
                     QStringLiteral("Unable to open '%1' for writing: %2")
                         .arg(filePath, file.errorString()));
  }

  QNetworkRequest request;
  request.setUrl(url);
  request.setSslConfiguration(sslConfiguration());

  LOG_DEBUG("Download URL: {}", url.toStdString());

  QTimer timer;
  timer.setSingleShot(true);

  QEventLoop eventLoop;

  _reply = _accessManager->get(request);

  bool timedOut = false;
  bool writeFailed = false;

  connect(&timer, &QTimer::timeout, &eventLoop, [&]() {
    timedOut = true;
    eventLoop.quit();
  });
  connect(_reply, &QNetworkReply::readyRead, &eventLoop, [&]() {
    // Any received data restarts the stall timer.
    timer.start(stallTimeout);
    if (file.write(_reply->readAll()) < 0) {
      writeFailed = true;
      eventLoop.quit();
    }
  });
  connect(_reply, &QNetworkReply::finished, &eventLoop, &QEventLoop::quit);
  connect(_reply, SIGNAL(sslErrors(const QList<QSslError> &)), this,
          SLOT(replySslErrors(const QList<QSslError> &)));

  timer.start(stallTimeout);

  eventLoop.exec();

  timer.stop();

  HttpReply httpReply;
  if (timedOut) {
    _reply->abort();
    httpReply = HttpReply(QNetworkReply::TimeoutError, {},
                          QStringLiteral("Download stalled"));
  } else if (writeFailed) {
    _reply->abort();
    httpReply = HttpReply(QNetworkReply::UnknownContentError, {},
                          QStringLiteral("Unable to write to '%1': %2")
                              .arg(filePath, file.errorString()));
  } else {
    if (_reply->error() == QNetworkReply::NoError &&
        file.write(_reply->readAll()) < 0) {
      httpReply = HttpReply(QNetworkReply::UnknownContentError, {},
                            QStringLiteral("Unable to write to '%1': %2")
                                .arg(filePath, file.errorString()));
    } else {
      httpReply = HttpReply(_reply->error(), {}, _reply->errorString());
    }
  }

  file.close();

  _reply = nullptr;

  LOG_PERF(0, "Exit");

  return httpReply;
}

QSslConfiguration HttpClientWorker::sslConfiguration() const {
  QSslConfiguration sslConfig = QSslConfiguration::defaultConfiguration();
  sslConfig.setPeerVerifyMode(QSslSocket::VerifyNone);

  // Only add certificate if it was successfully loaded
  if (!_certificate.isNull()) {
    sslConfig.addCaCertificate(_certificate);
  }

  return sslConfig;
}

void HttpClientWorker::clearConnections_() {
  LOG_PERF(0, "Enter");

  _accessManager->setAutoDeleteReplies(true);
  _accessManager->setStrictTransportSecurityEnabled(false);
  _accessManager->clearConnectionCache();

  LOG_PERF(0, "Exit");
}

void HttpClientWorker::replyFinished() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void HttpClientWorker::replySslErrors(const QList<QSslError> &errors) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(_reply != nullptr);

  if (!_reply) {
    LOG_ERROR(Log::ErrorId::Network,
              "Internal error. The reply is a null pointer!");
    LOG_PERF(0, "Exit");
    return;
  }

  for (const auto &error : errors) {
    LOG_ERROR(Log::ErrorId::Network, "SSL error: {}",
              error.errorString().toStdString());
  }

  _reply->ignoreSslErrors();

  LOG_PERF(0, "Exit");
}

void HttpClientWorker::loadClientCertificate() {
  LOG_PERF(0, "Enter");

  const auto configLocations =
      QStandardPaths::standardLocations(QStandardPaths::AppConfigLocation);
  if (configLocations.isEmpty()) {
    LOG_DEBUG("No standard config locations found");
    LOG_PERF(0, "Exit");
    return;
  }

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();

  const QString clientCertFileName =
      configManager.getString(::ConfigManager::ConfigKey::HttpClientCaCertFile);

  const QString clientCertFilePath =
      QDir(configLocations.first()).absoluteFilePath(clientCertFileName);

  QFile file(clientCertFilePath);
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_DEBUG("Custom certificate file '{}' not found or not accessible. Using "
              "system CA certificates.",
              clientCertFilePath.toStdString());
    LOG_PERF(0, "Exit");
    return;
  }

  const QByteArray bytes = file.readAll();
  file.close();

  _certificate = QSslCertificate(bytes);

  if (_certificate.isNull()) {
    LOG_DEBUG(
        "Failed to load certificate from '{}'. Using system CA certificates.",
        clientCertFilePath.toStdString());
  } else {
    LOG_DEBUG("Successfully loaded custom CA certificate from '{}'",
              clientCertFilePath.toStdString());
  }

  LOG_PERF(0, "Exit");
}

void HttpClientWorker::start() {
  LOG_PERF(0, "Enter");

  clearConnections_();

  _online = true;

  LOG_PERF(0, "Exit");
}

void HttpClientWorker::stop() {
  LOG_PERF(0, "Enter");

  _online = false;

  LOG_PERF(0, "Exit");
}

HttpClient::HttpClient()
    : HttpClientWorkerThread<HttpClientWorker>(new HttpClientWorker(),
                                               nullptr) {
  LOG_PERF(0, "Enter");

  qRegisterMetaType<HttpMethod>("HttpMethod");
  qRegisterMetaType<QMap<QNetworkRequest::KnownHeaders, QString>>(
      "QMap<QNetworkRequest::KnownHeaders, QString>");

  LOG_PERF(0, "Exit");
}

HttpClient::~HttpClient() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

HttpReply HttpClient::sendRequest(
    const QString &url, HttpMethod httpMethod,
    const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
    const QByteArray &requestBody) {
  return sendRequest(url, httpMethod, httpHeaders, {{}}, requestBody);
}

HttpReply HttpClient::sendRequest(
    const QString &url, HttpMethod httpMethod,
    const QMap<QNetworkRequest::KnownHeaders, QString> &httpHeaders,
    const QMap<QString, QString> &rawHeaders, const QByteArray &requestBody) {
  return emit worker() -> sendRequest(url, httpMethod, httpHeaders, rawHeaders,
                                      requestBody);
}

HttpReply HttpClient::downloadFile(const QString &url,
                                   const QString &filePath) {
  return emit worker() -> downloadFile(url, filePath);
}

void HttpClient::clearConnections() {
  emit worker() -> clearConnections();
}

} // namespace Util
