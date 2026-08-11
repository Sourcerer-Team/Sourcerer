#include "AzureRepo.h"

#include "HttpClient.h"

#include <Log.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRandomGenerator>
#include <QSet>
#include <QUrlQuery>

#include <memory>
#include <utility>

#define THREAD_ID "IssueTrackers::AzureRepo"

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

QMap<QNetworkRequest::KnownHeaders, QString> formHeaders() {
  QMap<QNetworkRequest::KnownHeaders, QString> headers;
  headers[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/x-www-form-urlencoded");
  return headers;
}

QMap<QString, QString> bearerHeaders(const QString &accessToken) {
  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("Authorization")] =
      QStringLiteral("Bearer ") + accessToken.trimmed();
  return rawHeaders;
}

QString normalizedAzureTenantId(const QString &tenantId) {
  const QString trimmed = tenantId.trimmed();
  return trimmed;
}

QString azureOrganizationName(const QJsonObject &accountObj) {
  const QString organization =
      accountObj.value(QStringLiteral("accountName")).toString().trimmed();
  if (!organization.isEmpty()) {
    return organization;
  }

  const QString accountUri =
      accountObj.value(QStringLiteral("accountUri")).toString().trimmed();
  if (!accountUri.isEmpty()) {
    const QString path = QUrl(accountUri).path();
    const QStringList pathParts = path.split('/', Qt::SkipEmptyParts);
    if (!pathParts.isEmpty()) {
      return pathParts.last().trimmed();
    }
  }

  return QString();
}

QString azureTenantLabel(const QJsonObject &tenantObj) {
  const QString displayName =
      tenantObj.value(QStringLiteral("displayName")).toString().trimmed();
  const QString tenantId =
      tenantObj.value(QStringLiteral("tenantId")).toString().trimmed();
  const QString id = tenantObj.value(QStringLiteral("id")).toString().trimmed();
  const QString identifier = !tenantId.isEmpty() ? tenantId : id;

  if (displayName.isEmpty()) {
    return identifier;
  }

  return displayName;
}

QString azureTenantId(const QJsonObject &tenantObj) {
  const QString displayName =
      tenantObj.value(QStringLiteral("displayName")).toString().trimmed();
  const QString tenantId =
      tenantObj.value(QStringLiteral("tenantId")).toString().trimmed();
  const QString id = tenantObj.value(QStringLiteral("id")).toString().trimmed();
  const QString identifier = !tenantId.isEmpty() ? tenantId : id;

  return tenantId;
}

QMap<QString, QString>
discoverAzureTenants(const std::shared_ptr<Util::HttpClient> &httpClient,
                     const QString &accessToken,
                     QString *errorMessage = nullptr) {
  const auto tenantsReply = httpClient->sendRequest(
      QStringLiteral(
          "https://management.azure.com/tenants?api-version=2022-12-01"),
      Util::HttpMethod::Get, jsonHeaders(), bearerHeaders(accessToken),
      QByteArray());
  if (tenantsReply.error() != QNetworkReply::NetworkError::NoError) {
    if (errorMessage != nullptr) {
      *errorMessage = responseErrorMessage(
          tenantsReply, QStringLiteral("Failed to request Azure tenants."));
    }
    return {};
  }

  const QJsonDocument tenantsJson =
      QJsonDocument::fromJson(tenantsReply.response());
  if (!tenantsJson.isObject()) {
    if (errorMessage != nullptr) {
      const QString body = QString::fromUtf8(tenantsReply.response()).trimmed();
      *errorMessage = body.isEmpty()
                          ? QStringLiteral("Invalid Azure tenants response.")
                          : QStringLiteral("Invalid Azure tenants response: ") +
                                body.left(200);
    }
    return {};
  }

  QSet<QString> seenTenants;
  QMap<QString, QString> tenants;
  const QJsonArray values =
      tenantsJson.object().value(QStringLiteral("value")).toArray();
  for (const auto &value : values) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject tenantObj = value.toObject();
    const QString tenantId =
        tenantObj.value(QStringLiteral("tenantId")).toString().trimmed();
    if (tenantId.isEmpty() || seenTenants.contains(tenantId)) {
      continue;
    }

    seenTenants.insert(tenantId);
    tenants[azureTenantLabel(tenantObj)] = azureTenantId(tenantObj);
  }

  return tenants;
}

QString azureCodeVerifier() {
  QByteArray verifier;
  verifier.reserve(64);

  static const char alphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
  const int alphabetSize = static_cast<int>(sizeof(alphabet) - 1);
  for (int i = 0; i < 96; ++i) {
    const int index = QRandomGenerator::global()->bounded(alphabetSize);
    verifier.append(alphabet[index]);
  }

  return QString::fromLatin1(verifier);
}

QByteArray azureCodeChallenge(const QString &codeVerifier) {
  const QByteArray verifierBytes = codeVerifier.toUtf8();
  const QByteArray hash =
      QCryptographicHash::hash(verifierBytes, QCryptographicHash::Sha256);
  return hash.toBase64(QByteArray::Base64UrlEncoding |
                       QByteArray::OmitTrailingEquals);
}

QMap<QString, QString>
discoverAzureOrganizations(const std::shared_ptr<Util::HttpClient> &httpClient,
                           const QString &accessToken,
                           QString *errorMessage = nullptr) {
  const auto meReply = httpClient->sendRequest(
      QStringLiteral("https://app.vssps.visualstudio.com/_apis/profile/"
                     "profiles/me?api-version=7.1"),
      Util::HttpMethod::Get, jsonHeaders(), bearerHeaders(accessToken),
      QByteArray());
  if (meReply.error() != QNetworkReply::NetworkError::NoError) {
    if (errorMessage != nullptr) {
      *errorMessage = responseErrorMessage(
          meReply, QStringLiteral("Failed to request Azure organizations."));
    }
    return {};
  }

  const QJsonDocument meJson = QJsonDocument::fromJson(meReply.response());
  if (!meJson.isObject()) {
    if (errorMessage != nullptr) {
      const QString body = QString::fromUtf8(meReply.response()).trimmed();
      *errorMessage = body.isEmpty()
                          ? QStringLiteral("Invalid Azure profile response.")
                          : QStringLiteral("Invalid Azure profile response: ") +
                                body.left(200);
    }
    return {};
  }

  const QString guid = meJson.object().value(QStringLiteral("id")).toString();
  const QString urlStr =
      QString("https://app.vssps.visualstudio.com/_apis/"
              "accounts?memberId=%1&api-version=7.1-preview.1")
          .arg(guid);

  const auto organizationsReply =
      httpClient->sendRequest(urlStr, Util::HttpMethod::Get, jsonHeaders(),
                              bearerHeaders(accessToken), QByteArray());
  if (organizationsReply.error() != QNetworkReply::NetworkError::NoError) {
    if (errorMessage != nullptr) {
      *errorMessage = responseErrorMessage(
          organizationsReply,
          QStringLiteral("Failed to request Azure organizations."));
    }
    return {};
  }

  const QJsonDocument organizationsJson =
      QJsonDocument::fromJson(organizationsReply.response());
  if (!organizationsJson.isObject()) {
    if (errorMessage != nullptr) {
      const QString body =
          QString::fromUtf8(organizationsReply.response()).trimmed();
      *errorMessage =
          body.isEmpty()
          ? QStringLiteral("Invalid Azure organizations response.")
              : QStringLiteral("Invalid Azure organizations response: ") +
                    body.left(200);
    }
    return {};
  }

  QSet<QString> seenOrganizations;
  QMap<QString, QString> organizations;
  const QJsonArray values =
      organizationsJson.object().value(QStringLiteral("value")).toArray();
  for (const auto &value : values) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject accountObj = value.toObject();
    const QString organizationName = azureOrganizationName(accountObj);
    if (organizationName.isEmpty() ||
        seenOrganizations.contains(organizationName)) {
      continue;
    }

    seenOrganizations.insert(organizationName);
    const QString organizationLabel =
        accountObj.value(QStringLiteral("friendlyName")).toString().trimmed();
    organizations[organizationLabel.isEmpty() ? organizationName
                                              : organizationLabel] =
        organizationName;
  }

  return organizations;
}

QByteArray formEncode(const QMap<QString, QString> &fields) {
  QUrlQuery query;
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    query.addQueryItem(it.key(), it.value());
  }
  return query.query(QUrl::FullyEncoded).toUtf8();
}

QVariantMap
requestAzureToken(const std::shared_ptr<Util::HttpClient> &httpClient,
                  const QString &tokenAuthorityUrl, const QString &tenantId,
                  const QString &clientId, const QString &clientSecret,
                  const QString &grantType,
                  const QMap<QString, QString> &extraFields,
                  QString *errorMessage = nullptr) {
  QVariantMap result;

  const QString trimmedTokenAuthorityUrl = tokenAuthorityUrl.trimmed();
  const QString trimmedTenantId = normalizedAzureTenantId(tenantId);
  const QString trimmedClientId = clientId.trimmed();
  if (trimmedTokenAuthorityUrl.isEmpty() || trimmedClientId.isEmpty()) {
    if (errorMessage != nullptr) {
      *errorMessage =
          QStringLiteral("Azure OAuth endpoints are not configured.");
    }
    return result;
  }

  QMap<QString, QString> tokenFields;
  tokenFields[QStringLiteral("client_id")] = trimmedClientId;
  if (!clientSecret.trimmed().isEmpty()) {
    tokenFields[QStringLiteral("client_secret")] = clientSecret.trimmed();
  }
  tokenFields[QStringLiteral("grant_type")] = grantType;
  for (auto it = extraFields.begin(); it != extraFields.end(); ++it) {
    tokenFields[it.key()] = it.value();
  }

  const QString tokenUrl =
      trimmedTokenAuthorityUrl + QStringLiteral("/") +
      QString::fromUtf8(QUrl::toPercentEncoding(trimmedTenantId)) +
      QStringLiteral("/oauth2/token");
  const auto tokenReply = httpClient->sendRequest(
      tokenUrl, Util::HttpMethod::Post, formHeaders(), QMap<QString, QString>(),
      formEncode(tokenFields));
  if (tokenReply.error() != QNetworkReply::NetworkError::NoError) {
    if (errorMessage != nullptr) {
      *errorMessage = responseErrorMessage(
          tokenReply, QStringLiteral("Failed to request Azure access token."));
    }
    return result;
  }

  const QJsonDocument tokenJson =
      QJsonDocument::fromJson(tokenReply.response());
  if (!tokenJson.isObject()) {
    if (errorMessage != nullptr) {
      *errorMessage = QStringLiteral("Invalid Azure access token response.");
    }
    return result;
  }

  const QJsonObject tokenObj = tokenJson.object();
  const QString accessToken =
      tokenObj.value(QStringLiteral("access_token")).toString().trimmed();
  if (accessToken.isEmpty()) {
    if (errorMessage != nullptr) {
      *errorMessage = responseErrorMessage(
          tokenReply,
          QStringLiteral("Azure access token response missing access_token."));
    }
    return result;
  }

  result[QStringLiteral("accessToken")] = accessToken;
  const QString refreshToken =
      tokenObj.value(QStringLiteral("refresh_token")).toString().trimmed();
  if (!refreshToken.isEmpty()) {
    result[QStringLiteral("refreshToken")] = refreshToken;
  }

  const int expiresInSeconds =
      tokenObj.value(QStringLiteral("expires_in")).toInt(0);
  if (expiresInSeconds > 0) {
    result[QStringLiteral("expiresAt")] =
        QDateTime::currentDateTimeUtc().addSecs(expiresInSeconds);
  }

  return result;
}

} // namespace

AzureRepo::AzureRepo(QString owner, QString repository, QString host)
    : _host(std::move(host)), _owner(std::move(owner)),
      _repository(std::move(repository)) {}

AzureRepo::~AzureRepo() = default;

IssueTrackerRepo::Type AzureRepo::type() const {
  return IssueTrackerRepo::Type::Azure;
}

QString AzureRepo::serviceName() const {
  return QStringLiteral("Azure");
}

QString AzureRepo::host() const {
  return _host;
}

QString AzureRepo::owner() const {
  return _owner;
}

QString AzureRepo::repository() const {
  return _repository;
}

QString AzureRepo::repositoryPath() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return _owner + QStringLiteral("/") + _repository;
}

QString AzureRepo::webUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return hostToWebBaseUrl(_host) + QStringLiteral("/") +
         QString::fromUtf8(QUrl::toPercentEncoding(_owner)) +
         QStringLiteral("/") +
         QString::fromUtf8(QUrl::toPercentEncoding(_repository));
}

QString AzureRepo::issuesUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return webUrl() + QStringLiteral("/_workitems");
}

bool AzureRepo::isValid() const {
  return !_host.isEmpty();
}

QUrl AzureRepo::authorizationUrl(const QString &authorityUrl,
                                 const QString &tenantId,
                                 const QString &clientId,
                                 const QString &redirectUri,
                                 const QString &scope, const QString &state,
                                 const QString &codeChallenge) {
  if (authorityUrl.trimmed().isEmpty() || clientId.trimmed().isEmpty()) {
    return QUrl();
  }

  const QString tenant = normalizedAzureTenantId(tenantId);
  QUrl url(authorityUrl.trimmed());
  if (!url.isValid()) {
    return QUrl();
  }

  url.setPath(QStringLiteral("/") + tenant +
              QStringLiteral("/oauth2/authorize"));

  QUrlQuery query;
  query.addQueryItem(QStringLiteral("client_id"), clientId.trimmed());
  query.addQueryItem(QStringLiteral("response_type"), QStringLiteral("code"));
  query.addQueryItem(QStringLiteral("response_mode"), QStringLiteral("query"));
  if (!redirectUri.trimmed().isEmpty()) {
    query.addQueryItem(QStringLiteral("redirect_uri"), redirectUri.trimmed());
  }
  if (!scope.trimmed().isEmpty()) {
    query.addQueryItem(QStringLiteral("resource"), scope.trimmed());
  }
  if (!codeChallenge.trimmed().isEmpty()) {
    query.addQueryItem(QStringLiteral("code_challenge"),
                       codeChallenge.trimmed());
    query.addQueryItem(QStringLiteral("code_challenge_method"),
                       QStringLiteral("S256"));
  }
  if (!state.trimmed().isEmpty()) {
    query.addQueryItem(QStringLiteral("state"), state.trimmed());
  }
  url.setQuery(query);
  return url;
}

QVariantMap AzureRepo::authenticate(const QVariantMap &parameters) {
  LOG_PERF(0, "Enter");

  QVariantMap result;

  const QString clientId =
      parameters.value(QStringLiteral("clientId")).toString().trimmed();
  const QString authorizationCode =
      parameters.value(QStringLiteral("authorizationCode"))
          .toString()
          .trimmed();
  const QString tenantId =
      parameters.value(QStringLiteral("tenantId")).toString().trimmed();
  const QString clientSecret =
      parameters.value(QStringLiteral("clientSecret")).toString().trimmed();
  const QString tokenAuthorityUrl =
      parameters.value(QStringLiteral("tokenUrl")).toString().trimmed();
  const QString redirectUri =
      parameters.value(QStringLiteral("redirectUri")).toString().trimmed();
  const QString scope =
      parameters.value(QStringLiteral("resource")).toString().trimmed();
  const QString codeVerifier =
      parameters.value(QStringLiteral("codeVerifier")).toString().trimmed();

  if (clientId.isEmpty()) {
    const QString message = QStringLiteral("Azure OAuth client id is empty.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  if (authorizationCode.isEmpty()) {
    const QString message =
        QStringLiteral("Azure authorization code is empty.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  if (tokenAuthorityUrl.isEmpty()) {
    const QString message =
        QStringLiteral("Azure OAuth endpoints are not configured.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  if (codeVerifier.isEmpty()) {
    const QString message =
        QStringLiteral("Azure PKCE code verifier is empty.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();

  QMap<QString, QString> tokenFields;
  tokenFields[QStringLiteral("grant_type")] =
      QStringLiteral("authorization_code");
  tokenFields[QStringLiteral("code")] = authorizationCode;
  if (!redirectUri.isEmpty()) {
    tokenFields[QStringLiteral("redirect_uri")] = redirectUri;
  }
  tokenFields[QStringLiteral("resource")] = scope;
  tokenFields[QStringLiteral("code_verifier")] = codeVerifier;

  QString tokenErrorMessage;
  const QVariantMap tokenResult = requestAzureToken(
      httpClient, tokenAuthorityUrl, tenantId, clientId, "",
      QStringLiteral("authorization_code"), tokenFields, &tokenErrorMessage);
  if (!tokenErrorMessage.isEmpty()) {
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = tokenErrorMessage;
    LOG_ERROR(Log::ErrorId::Network, "{}", tokenErrorMessage.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  const QString accessToken =
      tokenResult.value(QStringLiteral("accessToken")).toString().trimmed();
  if (accessToken.isEmpty()) {
    const QString message =
        QStringLiteral("Azure access token response missing access_token.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  const QString refreshToken =
      tokenResult.value(QStringLiteral("refreshToken")).toString().trimmed();
  if (!refreshToken.isEmpty()) {
    result[QStringLiteral("refreshToken")] = refreshToken;
  }

  const QDateTime expiresAt =
      tokenResult.value(QStringLiteral("expiresAt")).toDateTime();

  QString organizationDiscoveryErrorMessage;
  const QMap<QString, QString> organizations = discoverAzureOrganizations(
      httpClient, accessToken, &organizationDiscoveryErrorMessage);
  if (!organizationDiscoveryErrorMessage.isEmpty()) {
    LOG_WARN("Azure organization discovery failed: {}",
             organizationDiscoveryErrorMessage.toStdString());
  }
  if (!organizations.isEmpty()) {
    QVariantMap variantOrgs;
    for (const auto &k : organizations.keys()) {
      variantOrgs[k] = organizations[k];
    }
    result[QStringLiteral("organizations")] = variantOrgs;
  }

  result[QStringLiteral("success")] = true;
  result[QStringLiteral("accessToken")] = accessToken;
  result[QStringLiteral("expiresAt")] = expiresAt;
  result[QStringLiteral("scope")] = scope;

  LOG_PERF(0, "Exit");
  return result;
}

QMap<QString, QString> AzureRepo::discoverTenants(const QString &accessToken,
                                                  QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const QMap<QString, QString> tenants =
      discoverAzureTenants(httpClient, accessToken, errorMessage);

  LOG_PERF(0, "Exit");
  return tenants;
}

QVariantMap AzureRepo::listProjects(const QString &organization,
                                    const QString &accessToken) const {
  LOG_PERF(0, "Enter");

  QVariantMap result;

  const QString trimmedOrganization = organization.trimmed();
  const QString trimmedAccessToken = accessToken.trimmed();
  if (trimmedOrganization.isEmpty()) {
    const QString message = QStringLiteral("Azure organization is empty.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  if (trimmedAccessToken.isEmpty()) {
    const QString message = QStringLiteral("Azure access token is empty.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto urlStr =
      hostToApiBaseUrl(_host) + QStringLiteral("/") +
      QString::fromUtf8(QUrl::toPercentEncoding(trimmedOrganization)) +
      QStringLiteral("/_apis/projects?api-version=7.1-preview.4");
  const auto projectsReply =
      httpClient->sendRequest(urlStr, Util::HttpMethod::Get, jsonHeaders(),
                              bearerHeaders(trimmedAccessToken), QByteArray());
  if (projectsReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        projectsReply, QStringLiteral("Failed to request Azure projects."));
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  const QJsonDocument projectsJson =
      QJsonDocument::fromJson(projectsReply.response());
  if (!projectsJson.isObject()) {
    const QString message = QStringLiteral("Invalid Azure projects response.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return result;
  }

  const QJsonArray values =
      projectsJson.object().value(QStringLiteral("value")).toArray();
  QStringList repositories;
  QSet<QString> seenRepositories;
  for (const auto &value : values) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject projectObj = value.toObject();
    const QString projectName =
        projectObj.value(QStringLiteral("name")).toString().trimmed();
    if (projectName.isEmpty()) {
      continue;
    }

    const QString repositoryName =
        trimmedOrganization + QStringLiteral("/") + projectName;
    if (seenRepositories.contains(repositoryName)) {
      continue;
    }

    seenRepositories.insert(repositoryName);
    repositories += repositoryName;
  }

  repositories.sort(Qt::CaseInsensitive);
  if (repositories.isEmpty()) {
    const QString message = QStringLiteral(
        "No Azure projects were found for the selected organization.");
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = message;
    LOG_INFO("No Azure projects found for selected organization");
    LOG_PERF(0, "Exit");
    return result;
  }

  result[QStringLiteral("success")] = true;
  result[QStringLiteral("organization")] = trimmedOrganization;
  result[QStringLiteral("username")] = trimmedOrganization;
  result[QStringLiteral("repositories")] = repositories;

  LOG_PERF(0, "Exit");
  return result;
}

QString AzureRepo::hostToWebBaseUrl(const QString &host) {
  if (host.isEmpty() || host == QStringLiteral("dev.azure.com") ||
      host == QStringLiteral("www.dev.azure.com")) {
    return QStringLiteral("https://dev.azure.com");
  }

  if (host.startsWith(QStringLiteral("http://")) ||
      host.startsWith(QStringLiteral("https://"))) {
    return host;
  }

  return QStringLiteral("https://") + host;
}

QString AzureRepo::hostToApiBaseUrl(const QString &host) {
  return hostToWebBaseUrl(host);
}

} // namespace IssueTrackers
