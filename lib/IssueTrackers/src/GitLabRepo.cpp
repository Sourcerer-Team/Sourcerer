#include "GitLabRepo.h"

#include "HttpClient.h"

#include <Log.h>

#include <QByteArray>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkRequest>
#include <QSet>
#include <QUrlQuery>

#include <memory>
#include <utility>

#define THREAD_ID "IssueTrackers::GitLabRepo"

namespace IssueTrackers {
namespace {

QString responseErrorMessage(const Util::HttpReply &httpReply,
                             const QString &fallbackMessage) {
  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (jsonDoc.isObject()) {
    const QJsonObject obj = jsonDoc.object();
    const QString errorDescription =
        obj.value(QStringLiteral("error_description")).toString();
    if (!errorDescription.isEmpty()) {
      return errorDescription;
    }

    const QString error = obj.value(QStringLiteral("error")).toString();
    if (!error.isEmpty()) {
      return error;
    }

    const QString message = obj.value(QStringLiteral("message")).toString();
    if (!message.isEmpty()) {
      return message;
    }
  }

  const QString body = QString::fromUtf8(httpReply.response()).trimmed();
  if (!body.isEmpty()) {
    return body;
  }

  const QString errorString = httpReply.errorString().trimmed();
  if (!errorString.isEmpty()) {
    return fallbackMessage + QStringLiteral(": ") + errorString;
  }

  return fallbackMessage;
}

QMap<QNetworkRequest::KnownHeaders, QString> jsonHeaders() {
  QMap<QNetworkRequest::KnownHeaders, QString> headers;
  headers[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/json");
  return headers;
}

QMap<QString, QString> bearerHeaders(const QString &accessToken) {
  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("Authorization")] =
      QStringLiteral("Bearer ") + accessToken.trimmed();
  return rawHeaders;
}

QString trimmedHost(const QString &host) {
  QString value = host.trimmed();
  while (value.endsWith('/')) {
    value.chop(1);
  }
  return value;
}

} // namespace

GitLabRepo::GitLabRepo(QString owner, QString repository, QString host)
    : _host(std::move(host)), _owner(std::move(owner)),
      _repository(std::move(repository)) {}

GitLabRepo::~GitLabRepo() {
  if (!_accessToken.isEmpty()) {
    _accessToken.fill(QChar(u'\0'));
    _accessToken.clear();
  }
  _accessTokenExpiresAt = QDateTime();
}

QVariantMap GitLabRepo::authenticate(const QVariantMap &parameters) {
  LOG_PERF(0, "Enter");

  QVariantMap result;

  const QString accessToken =
      parameters.value(QStringLiteral("accessToken")).toString().trimmed();
  if (accessToken.isEmpty()) {
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] =
        QStringLiteral("GitLab personal access token is empty.");
    LOG_ERROR(Log::ErrorId::Security, "GitLab personal access token is empty.");
    LOG_PERF(0, "Exit");
    return result;
  }

  _accessToken = accessToken;
  _accessTokenExpiresAt = QDateTime();

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto userReply = httpClient->sendRequest(
      hostToApiBaseUrl(_host) + QStringLiteral("/user"), Util::HttpMethod::Get,
      jsonHeaders(), bearerHeaders(_accessToken), QByteArray());
  if (userReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        userReply, QStringLiteral("Failed to validate the GitLab token."));
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  QString repositoriesErrorMessage;
  const QStringList repositories =
      listUserRepositories(&repositoriesErrorMessage);
  if (repositories.isEmpty()) {
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = repositoriesErrorMessage;
    LOG_PERF(0, "Exit");
    return result;
  }

  result[QStringLiteral("success")] = true;
  result[QStringLiteral("accessToken")] = accessToken;
  result[QStringLiteral("expiresAt")] = _accessTokenExpiresAt;
  result[QStringLiteral("repositories")] = repositories;

  LOG_PERF(0, "Exit");
  return result;
}

QStringList GitLabRepo::listUserRepositories(QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  QStringList repositories;
  QSet<QString> seenRepositories;

  if (_accessToken.trimmed().isEmpty()) {
    const QString message =
        QStringLiteral("GitLab access token is missing or empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return repositories;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto requestJsonArray = [&](const QUrl &url,
                                    const QString &failureMessage,
                                    QJsonArray *arrayOut) -> bool {
    if (!arrayOut) {
      return false;
    }

    const QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders =
        jsonHeaders();
    const auto httpReply = httpClient->sendRequest(
        url.toString(), Util::HttpMethod::Get, httpHeaders,
        bearerHeaders(_accessToken), QByteArray());
    if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
      const QString message = responseErrorMessage(httpReply, failureMessage);
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
    if (!jsonDoc.isArray()) {
      const QString message = responseErrorMessage(
          httpReply, QStringLiteral("Invalid GitLab repositories response."));
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    *arrayOut = jsonDoc.array();
    return true;
  };

  int page = 1;
  while (true) {
    QUrl url(hostToApiBaseUrl(_host) + QStringLiteral("/projects"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("membership"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("simple"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("order_by"), QStringLiteral("name"));
    query.addQueryItem(QStringLiteral("sort"), QStringLiteral("asc"));
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(query);

    QJsonArray pageArray;
    if (!requestJsonArray(
            url, QStringLiteral("Failed to request GitLab repositories."),
            &pageArray)) {
      LOG_PERF(0, "Exit");
      return repositories;
    }

    const int beforeCount = repositories.size();
    for (const auto &repoValue : pageArray) {
      if (!repoValue.isObject()) {
        continue;
      }

      const QJsonObject repoObj = repoValue.toObject();
      const QString repositoryName =
          repoObj.value(QStringLiteral("path_with_namespace"))
              .toString()
              .trimmed();
      if (repositoryName.isEmpty() ||
          seenRepositories.contains(repositoryName)) {
        continue;
      }

      seenRepositories.insert(repositoryName);
      repositories += repositoryName;
    }

    if (pageArray.size() < 100 || repositories.size() == beforeCount) {
      break;
    }

    ++page;
  }

  if (repositories.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "No GitLab repositories were found for the authenticated account.");
    }
    LOG_INFO("No GitLab repositories found for authenticated account");
  } else if (errorMessage) {
    errorMessage->clear();
  }

  LOG_PERF(0, "Exit");
  return repositories;
}

IssueTrackerRepo::Type GitLabRepo::type() const {
  return IssueTrackerRepo::Type::GitLab;
}

QString GitLabRepo::serviceName() const {
  return QStringLiteral("GitLab");
}

QString GitLabRepo::host() const {
  return _host;
}

QString GitLabRepo::owner() const {
  return _owner;
}

QString GitLabRepo::repository() const {
  return _repository;
}

QString GitLabRepo::repositoryPath() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return _owner + QStringLiteral("/") + _repository;
}

QString GitLabRepo::webUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return hostToWebBaseUrl(_host) + QStringLiteral("/") + repositoryPath();
}

QString GitLabRepo::issuesUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return webUrl() + QStringLiteral("/-/issues");
}

bool GitLabRepo::isValid() const {
  return !_host.trimmed().isEmpty();
}

QString GitLabRepo::hostToWebBaseUrl(const QString &host) {
  const QString value = [&host]() {
    const QString trimmed = trimmedHost(host);
    if (trimmed.isEmpty() || trimmed == QStringLiteral("gitlab.com") ||
        trimmed == QStringLiteral("www.gitlab.com")) {
      return QStringLiteral("https://gitlab.com");
    }

    if (trimmed.startsWith(QStringLiteral("http://")) ||
        trimmed.startsWith(QStringLiteral("https://"))) {
      return trimmed;
    }

    return QStringLiteral("https://") + trimmed;
  }();

  return value;
}

QString GitLabRepo::hostToApiBaseUrl(const QString &host) {
  return hostToWebBaseUrl(host) + QStringLiteral("/api/v4");
}

QByteArray GitLabRepo::formEncode(const QMap<QString, QString> &fields) {
  QUrlQuery query;
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    query.addQueryItem(it.key(), it.value());
  }

  return query.query(QUrl::FullyEncoded).toUtf8();
}

} // namespace IssueTrackers
