#include "CloudRepo.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <HttpClient.h>
#include <Log.h>

#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QSysInfo>
#include <memory>

#define THREAD_ID "CloudRepo"

namespace Util {

CloudRepo::CloudRepo() {
  LOG_PERF(0, "Enter");
  LOG_INFO("CloudRepo constructor called - initializing singleton");
  LOG_PERF(0, "Exit");
}

CloudRepo::~CloudRepo() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

CloudRepo &CloudRepo::getInstance() {
  static CloudRepo instance;
  return instance;
}

QString CloudRepo::getCloudSigninUrl() const {
  ::ConfigManager::ConfigManager &configManager =
      ::ConfigManager::ConfigManager::getInstance();
  return configManager.getString(::ConfigManager::ConfigKey::CloudSigninUrl);
}

QString CloudRepo::getCloudSignoutUrl() const {
  ::ConfigManager::ConfigManager &configManager =
      ::ConfigManager::ConfigManager::getInstance();
  return configManager.getString(::ConfigManager::ConfigKey::CloudSignoutUrl);
}

QString CloudRepo::getMachineToken() const {
  // Generate machine token from multiple unique system identifiers
  // This ensures the token uniquely identifies a specific machine and user
  const QString machineId = QSysInfo::machineUniqueId();
  const QString hostname = QSysInfo::machineHostName();
  const QString appData =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  const QString osVersion = QSysInfo::kernelVersion();
  const QString productType = QSysInfo::productType();

  // Combine all unique identifiers
  const QByteArray combined =
      (machineId + hostname + appData + osVersion + productType).toUtf8();
  const QByteArray hash =
      QCryptographicHash::hash(combined, QCryptographicHash::Sha256);

  return QString("%1:%2").arg(hostname, QString::fromUtf8(hash.toHex()));
}

QPair<QNetworkReply::NetworkError, QString>
CloudRepo::signin(const QString &email, const QString &password) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Signin requested for email: {}", email);

  _pendingEmail = email;
  _pendingPassword = password;

  // Create JSON payload
  QJsonObject payload;
  payload["email"] = email;
  payload["password"] = password;
  payload["machine_token"] = getMachineToken();

  QJsonDocument doc(payload);
  QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

  // Create HTTP client and send request
  const auto httpClient = std::make_shared<Util::HttpClient>();

  const QString url = getCloudSigninUrl();

  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      "application/json";

  // Start request in background thread

  const auto httpReply =
      httpClient->sendRequest(url, HttpMethod::Post, httpHeaders, jsonData);

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    LOG_ERROR(Log::ErrorId::Network, "Failed to send signin request: {}",
              (int)httpReply.error());
    emit signinError(email, httpReply.error());
    LOG_PERF(0, "Exit");
    return {httpReply.error(), QString()};
  }

  LOG_PERF(0, "Exit");

  return processResponse(
      httpReply.response(),
      200); // Assuming success if no network error. In real implementation,
            // status code should come from HTTP response.
}

QPair<QNetworkReply::NetworkError, QString> CloudRepo::signOut() {
  LOG_PERF(0, "Enter");
  LOG_INFO("Signout requested");

  // Create HTTP client and send request
  const auto httpClient = std::make_shared<Util::HttpClient>();

  const QString url = getCloudSignoutUrl();

  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      "application/json";

  QMap<QString, QString> rawHttpHeaders;
  rawHttpHeaders["authorization"] = "Bearer " + _accessToken;

  // Send signout request with empy payload since server only needs the token
  // for authentication
  QJsonObject payload;
  QJsonDocument doc(payload);
  QByteArray jsonData = doc.toJson(QJsonDocument::Compact);

  const auto httpReply = httpClient->sendRequest(
      url, HttpMethod::Post, httpHeaders, rawHttpHeaders, jsonData);

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    LOG_ERROR(Log::ErrorId::Network, "Failed to send signout request: {}",
              (int)httpReply.error());
    emit signoutError(httpReply.error());
    LOG_PERF(0, "Exit");
    return {httpReply.error(), QString()};
  }

  LOG_INFO("Signout successful");
  _accessToken.clear();
  emit signoutSuccess();

  LOG_PERF(0, "Exit");

  return {QNetworkReply::NetworkError::NoError, QString()};
}

QPair<QNetworkReply::NetworkError, QString>
CloudRepo::processResponse(const QString &response, int statusCode) {
  LOG_PERF(0, "Enter");
  LOG_INFO("Signin response received with status code: {}", statusCode);

  QJsonDocument doc = QJsonDocument::fromJson(response.toUtf8());
  if (!doc.isObject()) {
    LOG_ERROR(Log::ErrorId::Network, "Failed to parse signin response as JSON");
    emit signinFailed(_pendingEmail, "Invalid response format from server");
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError,
            "Invalid response format from server"};
  }

  const QJsonObject obj = doc.object();
  const QString accessToken = obj["access_token"].toString();

  if (accessToken.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Network, "No access_token in signin response");
    emit signinFailed(_pendingEmail, "No token received from server");
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError,
            "No token received from server"};
  }

  LOG_INFO("Signin successful for user: {}", _pendingEmail);

  _accessToken = accessToken;

  emit signinSuccess(_pendingEmail, _pendingPassword, accessToken, response);

  LOG_PERF(0, "Exit");

  return {QNetworkReply::NetworkError::NoError, accessToken};
}

} // namespace Util
