#include "CloudRepo.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <HttpClient.h>
#include <Log.h>

#include <QJsonDocument>
#include <QJsonObject>
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

  emit signinSuccess(_pendingEmail, accessToken, response);

  LOG_PERF(0, "Exit");

  return {QNetworkReply::NetworkError::NoError, accessToken};
}

} // namespace Util
