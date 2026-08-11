#include "BitbucketRepo.h"

#include "HttpClient.h"

#include <Log.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkRequest>
#include <QSet>
#include <QUrlQuery>

#include <memory>
#include <utility>

#define THREAD_ID "IssueTrackers::BitbucketRepo"

namespace IssueTrackers {
namespace {

QString responseErrorMessage(const Util::HttpReply &httpReply,
                             const QString &fallbackMessage) {
  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (jsonDoc.isObject()) {
    const QJsonObject obj = jsonDoc.object();
    const QJsonObject errorObj = obj.value(QStringLiteral("error")).toObject();
    const QString errorMessage =
        errorObj.value(QStringLiteral("message")).toString();
    if (!errorMessage.isEmpty()) {
      return errorMessage;
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

} // namespace

BitbucketRepo::BitbucketRepo(QString owner, QString repository, QString host)
    : _host(std::move(host)), _owner(std::move(owner)),
      _repository(std::move(repository)) {}

BitbucketRepo::~BitbucketRepo() = default;

IssueTrackerRepo::Type BitbucketRepo::type() const {
  return IssueTrackerRepo::Type::Bitbucket;
}

QString BitbucketRepo::serviceName() const {
  return QStringLiteral("Bitbucket");
}

QString BitbucketRepo::host() const {
  return _host;
}

QString BitbucketRepo::owner() const {
  return _owner;
}

QString BitbucketRepo::repository() const {
  return _repository;
}

QString BitbucketRepo::repositoryPath() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return _owner + QStringLiteral("/") + _repository;
}

QString BitbucketRepo::webUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return hostToWebBaseUrl(_host) + QStringLiteral("/") + repositoryPath();
}

QString BitbucketRepo::issuesUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return webUrl() + QStringLiteral("/issues");
}

bool BitbucketRepo::isValid() const {
  return !_host.isEmpty();
}

QUrl BitbucketRepo::authorizationUrl(const QString &authorizeUrl,
                                     const QString &clientId,
                                     const QString &redirectUri,
                                     const QString &scope) {
  if (authorizeUrl.trimmed().isEmpty() || clientId.trimmed().isEmpty()) {
    return QUrl();
  }

  QUrl url(authorizeUrl.trimmed());
  if (!url.isValid()) {
    return QUrl();
  }

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("client_id"), clientId.trimmed());
  query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  if (!redirectUri.trimmed().isEmpty()) {
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri.trimmed());
  }
  if (!scope.trimmed().isEmpty()) {
    query.addQueryItem(QStringLiteral("scope"), scope.trimmed());
  }
  url.setQuery(query);

  return url;
}

QVariantMap BitbucketRepo::authenticate(const QVariantMap &parameters) {
  LOG_PERF(0, "Enter");

  QVariantMap result;

  const bool useOAuth =
      parameters.contains(QStringLiteral("authorizationCode")) ||
      parameters.contains(QStringLiteral("clientId")) ||
      parameters.contains(QStringLiteral("clientSecret"));
  if (useOAuth) {
    const QString clientId =
        parameters.value(QStringLiteral("clientId")).toString().trimmed();
    const QString clientSecret =
        parameters.value(QStringLiteral("clientSecret")).toString();
    const QString authorizationCode =
        parameters.value(QStringLiteral("authorizationCode"))
            .toString()
            .trimmed();
    const QString authorizeUrl =
        parameters.value(QStringLiteral("authorizeUrl")).toString().trimmed();
    const QString tokenUrl =
        parameters.value(QStringLiteral("tokenUrl")).toString().trimmed();
    const QString redirectUri =
        parameters.value(QStringLiteral("redirectUri")).toString().trimmed();
    const QString scope =
        parameters.contains(QStringLiteral("scope"))
            ? parameters.value(QStringLiteral("scope")).toString().trimmed()
            : QStringLiteral("account repository");

    if (clientId.isEmpty()) {
      result[QStringLiteral("success")] = false;
      result[QStringLiteral("errorMessage")] =
          QStringLiteral("Bitbucket OAuth client id is empty.");
      LOG_ERROR(Log::ErrorId::Security, "Bitbucket OAuth client id is empty.");
      LOG_PERF(0, "Exit");
      return result;
    }

    if (authorizationCode.isEmpty()) {
      result[QStringLiteral("success")] = false;
      result[QStringLiteral("errorMessage")] =
          QStringLiteral("Bitbucket authorization code is empty.");
      LOG_ERROR(Log::ErrorId::Security,
                "Bitbucket authorization code is empty.");
      LOG_PERF(0, "Exit");
      return result;
    }

    QString errorMessage;
    QDateTime expiresAt;
    const auto tokenReply = requestOAuthAccessToken(
        tokenUrl.isEmpty()
            ? QStringLiteral("https://bitbucket.org/site/oauth2/access_token")
            : tokenUrl,
        clientId, clientSecret, authorizationCode, redirectUri,
        QStringLiteral("Bitbucket OAuth access token"), &expiresAt,
        &errorMessage);
    if (tokenReply.first != QNetworkReply::NetworkError::NoError) {
      result[QStringLiteral("success")] = false;
      result[QStringLiteral("errorMessage")] = errorMessage;
      LOG_PERF(0, "Exit");
      return result;
    }

    const QString accessToken = tokenReply.second;
    QString workspaceErrorMessage;
    const QString workspace =
        currentWorkspaceName(accessToken, &workspaceErrorMessage);
    if (workspace.isEmpty()) {
      result[QStringLiteral("success")] = false;
      result[QStringLiteral("errorMessage")] = workspaceErrorMessage;
      LOG_PERF(0, "Exit");
      return result;
    }

    QString repositoriesErrorMessage;
    const QStringList repositories = listRepositoriesForWorkspace(
        workspace, accessToken, &repositoriesErrorMessage);
    if (repositories.isEmpty()) {
      result[QStringLiteral("success")] = false;
      result[QStringLiteral("errorMessage")] = repositoriesErrorMessage;
      LOG_PERF(0, "Exit");
      return result;
    }

    _accessToken = accessToken;
    _username = workspace;

    result[QStringLiteral("success")] = true;
    result[QStringLiteral("accessToken")] = accessToken;
    result[QStringLiteral("expiresAt")] = expiresAt;
    result[QStringLiteral("username")] = workspace;
    result[QStringLiteral("repositories")] = repositories;
    result[QStringLiteral("scope")] = scope;

    LOG_PERF(0, "Exit");
    return result;
  }

  const QString username =
      parameters.value(QStringLiteral("username")).toString().trimmed();
  const QString appPassword =
      parameters.value(QStringLiteral("appPassword")).toString();

  if (username.isEmpty() || appPassword.isEmpty()) {
    const QString message =
        QStringLiteral("Bitbucket username or app password is empty.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  QString errorMessage;
  const QStringList repositories =
      listUserRepositories(username, appPassword, &errorMessage);
  if (repositories.isEmpty()) {
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = errorMessage;
    LOG_PERF(0, "Exit");
    return result;
  }

  _username = username;
  _appPassword = appPassword;

  result[QStringLiteral("success")] = true;
  result[QStringLiteral("username")] = username;
  result[QStringLiteral("appPassword")] = appPassword;
  result[QStringLiteral("repositories")] = repositories;

  LOG_PERF(0, "Exit");
  return result;
}

QStringList BitbucketRepo::listUserRepositories(const QString &username,
                                                const QString &appPassword,
                                                QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  QStringList repositories;
  QSet<QString> seenRepositories;

  if (username.trimmed().isEmpty() || appPassword.isEmpty()) {
    const QString message =
        QStringLiteral("Bitbucket username or app password is empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return repositories;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const QString authHeader = basicAuthHeaderValue(username, appPassword);

  const auto requestJsonObject = [&](const QUrl &url,
                                     const QString &failureMessage,
                                     QJsonObject *objectOut) -> bool {
    if (!objectOut) {
      return false;
    }

    const QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders =
        jsonHeaders();
    QMap<QString, QString> rawHeaders;
    rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
    rawHeaders[QStringLiteral("Authorization")] = authHeader;

    const auto httpReply =
        httpClient->sendRequest(url.toString(), Util::HttpMethod::Get,
                                httpHeaders, rawHeaders, QByteArray());

    if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
      const QString message = responseErrorMessage(httpReply, failureMessage);
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
    if (!jsonDoc.isObject()) {
      const QString message = responseErrorMessage(
          httpReply,
          QStringLiteral("Invalid Bitbucket repositories response."));
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    *objectOut = jsonDoc.object();
    return true;
  };

  QUrl url(hostToApiBaseUrl(_host) + QStringLiteral("/repositories/") +
           QString::fromUtf8(QUrl::toPercentEncoding(username)));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("pagelen"), QStringLiteral("100"));
  query.addQueryItem(QStringLiteral("role"), QStringLiteral("member"));
  url.setQuery(query);

  while (url.isValid()) {
    QJsonObject object;
    if (!requestJsonObject(
            url, QStringLiteral("Failed to request Bitbucket repositories."),
            &object)) {
      LOG_PERF(0, "Exit");
      return repositories;
    }

    const QJsonArray values = object.value(QStringLiteral("values")).toArray();
    for (const auto &repoValue : values) {
      if (!repoValue.isObject()) {
        continue;
      }

      const QJsonObject repoObj = repoValue.toObject();
      const QString fullName =
          repoObj.value(QStringLiteral("full_name")).toString().trimmed();
      const QString slug =
          repoObj.value(QStringLiteral("slug")).toString().trimmed();
      const QString repositoryName = !fullName.isEmpty() ? fullName : slug;
      if (repositoryName.isEmpty() ||
          seenRepositories.contains(repositoryName)) {
        continue;
      }

      seenRepositories.insert(repositoryName);
      repositories += repositoryName;
    }

    const QString nextUrl =
        object.value(QStringLiteral("next")).toString().trimmed();
    if (nextUrl.isEmpty()) {
      break;
    }

    url = QUrl(nextUrl);
  }

  if (repositories.isEmpty()) {
    const QString message = QStringLiteral(
        "No Bitbucket repositories were found for the authenticated account.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_INFO("No Bitbucket repositories found for authenticated account");
  } else if (errorMessage) {
    errorMessage->clear();
  }

  LOG_PERF(0, "Exit");
  return repositories;
}

QStringList BitbucketRepo::listUserRepositoriesWithAccessToken(
    const QString &accessToken, QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  const QString workspace = currentWorkspaceName(accessToken, errorMessage);
  if (workspace.isEmpty()) {
    LOG_PERF(0, "Exit");
    return QStringList();
  }

  const QStringList repositories =
      listRepositoriesForWorkspace(workspace, accessToken, errorMessage);
  LOG_PERF(0, "Exit");
  return repositories;
}

QPair<QNetworkReply::NetworkError, QString>
BitbucketRepo::requestOAuthAccessToken(
    const QString &tokenUrl, const QString &clientId,
    const QString &clientSecret, const QString &authorizationCode,
    const QString &redirectUri, const QString &accessTokenLabel,
    QDateTime *expiresAtOut, QString *errorMessage) {
  LOG_PERF(0, "Enter");

  if (clientId.trimmed().isEmpty()) {
    const QString message =
        QStringLiteral("Bitbucket OAuth client id is empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  if (authorizationCode.trimmed().isEmpty()) {
    const QString message =
        QStringLiteral("Bitbucket authorization code is empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  QMap<QString, QString> fields;
  fields[QStringLiteral("grant_type")] = QStringLiteral("authorization_code");
  fields[QStringLiteral("code")] = authorizationCode.trimmed();
  if (!redirectUri.trimmed().isEmpty()) {
    fields[QStringLiteral("redirect_uri")] = redirectUri.trimmed();
  }

  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/x-www-form-urlencoded");

  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("Authorization")] =
      basicAuthHeaderValue(clientId, clientSecret);

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto httpReply =
      httpClient->sendRequest(tokenUrl, Util::HttpMethod::Post, httpHeaders,
                              rawHeaders, formEncode(fields));

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        httpReply, QStringLiteral("Failed to request Bitbucket access token."));
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {httpReply.error(), message};
  }

  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (!jsonDoc.isObject()) {
    const QString message =
        QStringLiteral("Invalid Bitbucket access token response.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  const QJsonObject obj = jsonDoc.object();
  if (obj.contains(QStringLiteral("error"))) {
    const QString message = responseErrorMessage(httpReply, accessTokenLabel);
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  const QString accessToken =
      obj.value(QStringLiteral("access_token")).toString().trimmed();
  if (accessToken.isEmpty()) {
    const QString message =
        QStringLiteral("Bitbucket access token response missing access_token.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  const int expiresInSeconds = obj.value(QStringLiteral("expires_in")).toInt(0);
  if (expiresAtOut) {
    *expiresAtOut =
        expiresInSeconds > 0
            ? QDateTime::currentDateTimeUtc().addSecs(expiresInSeconds)
            : QDateTime();
  }

  if (errorMessage) {
    errorMessage->clear();
  }

  LOG_INFO("Received Bitbucket OAuth access token");
  LOG_PERF(0, "Exit");
  return {QNetworkReply::NetworkError::NoError, accessToken};
}

QStringList
BitbucketRepo::listRepositoriesForWorkspace(const QString &workspace,
                                            const QString &accessToken,
                                            QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  QStringList repositories;
  QSet<QString> seenRepositories;

  if (workspace.trimmed().isEmpty() || accessToken.isEmpty()) {
    const QString message =
        QStringLiteral("Bitbucket workspace or access token is empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return repositories;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const QString authHeader = bearerAuthHeaderValue(accessToken);

  const auto requestJsonObject = [&](const QUrl &url,
                                     const QString &failureMessage,
                                     QJsonObject *objectOut) -> bool {
    if (!objectOut) {
      return false;
    }

    const QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders =
        jsonHeaders();
    QMap<QString, QString> rawHeaders;
    rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
    rawHeaders[QStringLiteral("Authorization")] = authHeader;

    const auto httpReply =
        httpClient->sendRequest(url.toString(), Util::HttpMethod::Get,
                                httpHeaders, rawHeaders, QByteArray());

    if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
      const QString message = responseErrorMessage(httpReply, failureMessage);
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
    if (!jsonDoc.isObject()) {
      const QString message = responseErrorMessage(
          httpReply,
          QStringLiteral("Invalid Bitbucket repositories response."));
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    *objectOut = jsonDoc.object();
    return true;
  };

  QUrl url(hostToApiBaseUrl(_host) + QStringLiteral("/repositories/") +
           QString::fromUtf8(QUrl::toPercentEncoding(workspace)));
  QUrlQuery query;
  query.addQueryItem(QStringLiteral("pagelen"), QStringLiteral("100"));
  query.addQueryItem(QStringLiteral("role"), QStringLiteral("member"));
  url.setQuery(query);

  while (url.isValid()) {
    QJsonObject object;
    if (!requestJsonObject(
            url, QStringLiteral("Failed to request Bitbucket repositories."),
            &object)) {
      LOG_PERF(0, "Exit");
      return repositories;
    }

    const QJsonArray values = object.value(QStringLiteral("values")).toArray();
    for (const auto &repoValue : values) {
      if (!repoValue.isObject()) {
        continue;
      }

      const QJsonObject repoObj = repoValue.toObject();
      const QString fullName =
          repoObj.value(QStringLiteral("full_name")).toString().trimmed();
      const QString slug =
          repoObj.value(QStringLiteral("slug")).toString().trimmed();
      const QString repositoryName = !fullName.isEmpty() ? fullName : slug;
      if (repositoryName.isEmpty() ||
          seenRepositories.contains(repositoryName)) {
        continue;
      }

      seenRepositories.insert(repositoryName);
      repositories += repositoryName;
    }

    const QString nextUrl =
        object.value(QStringLiteral("next")).toString().trimmed();
    if (nextUrl.isEmpty()) {
      break;
    }

    url = QUrl(nextUrl);
  }

  if (repositories.isEmpty()) {
    const QString message = QStringLiteral(
        "No Bitbucket repositories were found for the authenticated account.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_INFO("No Bitbucket repositories found for authenticated account");
  } else if (errorMessage) {
    errorMessage->clear();
  }

  LOG_PERF(0, "Exit");
  return repositories;
}

QString BitbucketRepo::currentWorkspaceName(const QString &accessToken,
                                            QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  if (accessToken.isEmpty()) {
    const QString message = QStringLiteral("Bitbucket access token is empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return QString();
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/json");

  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("Authorization")] =
      bearerAuthHeaderValue(accessToken);

  const auto httpReply = httpClient->sendRequest(
      hostToApiBaseUrl(_host) + QStringLiteral("/user"), Util::HttpMethod::Get,
      httpHeaders, rawHeaders, QByteArray());

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        httpReply, QStringLiteral("Failed to request Bitbucket user info."));
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return QString();
  }

  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (!jsonDoc.isObject()) {
    const QString message = QStringLiteral("Invalid Bitbucket user response.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return QString();
  }

  const QJsonObject obj = jsonDoc.object();
  QString workspace =
      obj.value(QStringLiteral("username")).toString().trimmed();
  if (workspace.isEmpty()) {
    workspace = obj.value(QStringLiteral("nickname")).toString().trimmed();
  }
  if (workspace.isEmpty()) {
    workspace = obj.value(QStringLiteral("display_name")).toString().trimmed();
  }

  if (workspace.isEmpty()) {
    const QString message = QStringLiteral(
        "Bitbucket user response did not contain a workspace name.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return QString();
  }

  if (errorMessage) {
    errorMessage->clear();
  }

  LOG_PERF(0, "Exit");
  return workspace;
}

QString BitbucketRepo::hostToWebBaseUrl(const QString &host) {
  if (host.isEmpty() || host == QStringLiteral("bitbucket.org") ||
      host == QStringLiteral("www.bitbucket.org")) {
    return QStringLiteral("https://bitbucket.org");
  }

  return QStringLiteral("https://") + host;
}

QString BitbucketRepo::hostToApiBaseUrl(const QString &host) {
  if (host.isEmpty() || host == QStringLiteral("bitbucket.org") ||
      host == QStringLiteral("www.bitbucket.org")) {
    return QStringLiteral("https://api.bitbucket.org/2.0");
  }

  if (host.startsWith(QStringLiteral("api."))) {
    return QStringLiteral("https://") + host;
  }

  return QStringLiteral("https://api.") + host + QStringLiteral("/2.0");
}

QByteArray BitbucketRepo::formEncode(const QMap<QString, QString> &fields) {
  QUrlQuery query;
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    query.addQueryItem(it.key(), it.value());
  }

  return query.query(QUrl::FullyEncoded).toUtf8();
}

QString BitbucketRepo::basicAuthHeaderValue(const QString &username,
                                            const QString &appPassword) {
  QByteArray credentials;
  credentials.append(username.trimmed().toUtf8());
  credentials.append(':');
  credentials.append(appPassword.toUtf8());
  return QStringLiteral("Basic ") + QString::fromLatin1(credentials.toBase64());
}

QString BitbucketRepo::bearerAuthHeaderValue(const QString &accessToken) {
  return QStringLiteral("Bearer ") + accessToken.trimmed();
}

} // namespace IssueTrackers
