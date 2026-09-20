#include "GitHubRepo.h"

#include "HttpClient.h"

#include <Log.h>

#include <QApplication>
#include <QByteArray>
#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QMetaObject>
#include <QNetworkRequest>
#include <QPoint>
#include <QPointer>
#ifndef Q_OS_WIN
#include <QProcess>
#include <QProcessEnvironment>
#endif
#include <QPushButton>
#include <QSet>
#include <QThread>
#include <QToolTip>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QVariantMap>

#include <algorithm>
#include <atomic>
#include <functional>
#include <memory>
#include <utility>

#define THREAD_ID "IssueTrackers::GitHubRepo"

namespace IssueTrackers {
namespace {

QString parseErrorDescription(const QJsonObject &obj);

QString responseErrorMessage(const Util::HttpReply &httpReply,
                             const QString &fallbackMessage) {
  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (jsonDoc.isObject()) {
    const QJsonObject obj = jsonDoc.object();
    const QString jsonMessage = parseErrorDescription(obj);
    if (!jsonMessage.isEmpty()) {
      return jsonMessage;
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

QString parseErrorDescription(const QJsonObject &obj) {
  const QString description =
      obj.value(QStringLiteral("error_description")).toString();
  if (!description.isEmpty()) {
    return description;
  }

  const QString error = obj.value(QStringLiteral("error")).toString();
  if (!error.isEmpty()) {
    return error;
  }

  return QStringLiteral("Unknown GitHub OAuth error");
}

#ifndef Q_OS_WIN
void openUrlFromAppImage(const QUrl &url) {
  QProcess process;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

  // Unset AppImage environment variables so external processes load host system libraries
  env.remove("LD_LIBRARY_PATH");
  env.remove("QT_PLUGIN_PATH");
  env.remove("QML2_IMPORT_PATH");

  process.setProcessEnvironment(env);
  process.start("xdg-open", QStringList() << url.toString());
  process.startDetached(); // Allow xdg-open to run independently without blocking
}
#endif

QMap<QNetworkRequest::KnownHeaders, QString> githubJsonHeaders() {
  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/json");
  return httpHeaders;
}

QMap<QString, QString> githubApiHeaders(const QString &accessToken) {
  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("X-GitHub-Api-Version")] =
      QStringLiteral("2022-11-28");
  if (!accessToken.trimmed().isEmpty()) {
    rawHeaders[QStringLiteral("Authorization")] =
        QStringLiteral("Bearer ") + accessToken.trimmed();
  }
  return rawHeaders;
}

} // namespace

GitHubRepo::GitHubRepo(QString owner, QString repository, QString host)
    : _host(std::move(host)), _owner(std::move(owner)),
      _repository(std::move(repository)) {}

GitHubRepo::~GitHubRepo() {
  clearAccessToken();
}

QUrl GitHubRepo::deviceLoginUrl(const QString &host) {
  return QUrl(hostToWebBaseUrl(host) + QStringLiteral("/login/device"));
}

QVariantMap GitHubRepo::authenticate(const QVariantMap &parameters) {
  LOG_PERF(0, "Enter");

  QVariantMap result;

  const QString clientId =
      parameters.value(QStringLiteral("clientId")).toString().trimmed();
  const QString scope =
      parameters.contains(QStringLiteral("scope"))
          ? parameters.value(QStringLiteral("scope")).toString().trimmed()
          : QStringLiteral("repo");

  if (clientId.isEmpty()) {
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] =
        QStringLiteral("GitHub OAuth client id is empty.");
    LOG_ERROR(Log::ErrorId::Security, "GitHub OAuth client id is empty.");
    LOG_PERF(0, "Exit");
    return result;
  }

  DeviceCodeResponse deviceCodeResponse;
  QString errorMessage;
  const auto deviceCodeReply =
      requestDeviceCode(clientId, &deviceCodeResponse, scope, &errorMessage);
  if (deviceCodeReply.first != QNetworkReply::NetworkError::NoError) {
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = errorMessage;
    LOG_PERF(0, "Exit");
    return result;
  }

  const QUrl verificationUrl =
      deviceCodeResponse.verificationUriComplete.isValid()
          ? deviceCodeResponse.verificationUriComplete
          : deviceCodeResponse.verificationUri;

  result[QStringLiteral("verificationUrl")] = verificationUrl;
  result[QStringLiteral("userCode")] = deviceCodeResponse.userCode;
  result[QStringLiteral("deviceCode")] = deviceCodeResponse.deviceCode;

  QPointer<QDialog> verificationDialog;
  const auto cancelled = std::make_shared<std::atomic_bool>(false);

  const auto showVerificationDialog = [verificationUrl,
                                       userCode = deviceCodeResponse.userCode,
                                       cancelled, &verificationDialog]() {
    auto *dialog = new QDialog(nullptr);
    verificationDialog = dialog;
    dialog->setWindowTitle(QStringLiteral("GitHub authentication"));
    dialog->setWindowModality(Qt::ApplicationModal);
    dialog->setMinimumWidth(460);

    auto *mainLayout = new QVBoxLayout(dialog);

    auto *instructionLabel = new QLabel(
        QStringLiteral(
            "Open the GitHub verification page and paste this code:"),
        dialog);
    instructionLabel->setWordWrap(true);
    mainLayout->addWidget(instructionLabel);

    auto *codeLayout = new QHBoxLayout();
    auto *codeEdit = new QLineEdit(dialog);
    codeEdit->setText(userCode);
    codeEdit->setReadOnly(true);
    codeEdit->setAlignment(Qt::AlignCenter);
    codeEdit->setClearButtonEnabled(false);
    codeLayout->addWidget(codeEdit, 1);

    auto *copyButton = new QPushButton(QStringLiteral("Copy"), dialog);
    codeLayout->addWidget(copyButton);
    mainLayout->addLayout(codeLayout);

    if (!verificationUrl.isValid()) {
      auto *urlLabel = new QLabel(
          QStringLiteral(
              "GitHub verification page is unavailable for this host."),
          dialog);
      urlLabel->setWordWrap(true);
      mainLayout->addWidget(urlLabel);
    }

    auto *buttonLayout = new QHBoxLayout();
    auto *openButton =
        new QPushButton(QStringLiteral("Open verification page"), dialog);
    openButton->setEnabled(verificationUrl.isValid());
    auto *cancelButton = new QPushButton(QStringLiteral("Cancel"), dialog);

    buttonLayout->addStretch();
    buttonLayout->addWidget(openButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    QObject::connect(
        copyButton, &QPushButton::clicked, dialog, [userCode, copyButton]() {
          QApplication::clipboard()->setText(userCode);
          QToolTip::showText(
              copyButton->mapToGlobal(QPoint(0, copyButton->height())),
              QStringLiteral("Code copied to clipboard."), copyButton,
              copyButton->rect(), 5000);
        });
    QObject::connect(openButton, &QPushButton::clicked, dialog,
                     [verificationUrl]() {
                       if (verificationUrl.isValid()) {
#ifdef Q_OS_WIN
                         QDesktopServices::openUrl(verificationUrl);
#else
                         openUrlFromAppImage(verificationUrl);
#endif
                       }
                     });
    QObject::connect(cancelButton, &QPushButton::clicked, dialog,
                     [cancelled, verificationDialog]() {
                       cancelled->store(true);
                       if (verificationDialog) {
                         verificationDialog->reject();
                       }
                     });
    QObject::connect(dialog, &QDialog::rejected, dialog,
                     [cancelled]() { cancelled->store(true); });

    dialog->show();
  };

  if (QThread::currentThread() == qApp->thread()) {
    showVerificationDialog();
  } else {
    QMetaObject::invokeMethod(qApp, showVerificationDialog,
                              Qt::BlockingQueuedConnection);
  }

  const auto closeVerificationDialog = [&verificationDialog]() {
    if (!verificationDialog) {
      return;
    }

    QMetaObject::invokeMethod(
        verificationDialog.data(),
        [dialog = verificationDialog]() {
          if (dialog) {
            dialog->close();
            dialog->deleteLater();
          }
        },
        Qt::QueuedConnection);
  };

  const auto tokenReply = pollDeviceAccessToken(
      clientId, deviceCodeResponse.deviceCode,
      deviceCodeResponse.intervalSeconds, deviceCodeResponse.expiresInSeconds,
      &errorMessage, cancelled.get());
  if (cancelled->load() ||
      tokenReply.first == QNetworkReply::NetworkError::OperationCanceledError) {
    closeVerificationDialog();
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("cancelled")] = true;
    result[QStringLiteral("errorMessage")] = QString();
    LOG_PERF(0, "Exit");
    return result;
  }

  if (tokenReply.first != QNetworkReply::NetworkError::NoError) {
    closeVerificationDialog();
    result[QStringLiteral("success")] = false;
    result[QStringLiteral("errorMessage")] = errorMessage;
    LOG_PERF(0, "Exit");
    return result;
  }

  closeVerificationDialog();

  result[QStringLiteral("success")] = true;
  result[QStringLiteral("accessToken")] = accessToken();
  result[QStringLiteral("expiresAt")] = accessTokenExpiresAt();
  result[QStringLiteral("verificationUrl")] = verificationUrl;
  result[QStringLiteral("userCode")] = deviceCodeResponse.userCode;
  result[QStringLiteral("deviceCode")] = deviceCodeResponse.deviceCode;

  LOG_PERF(0, "Exit");
  return result;
}

GitHubRepo::RepositoryVisibility
GitHubRepo::repositoryVisibility(QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  if (_owner.trimmed().isEmpty() || _repository.trimmed().isEmpty()) {
    if (errorMessage) {
      errorMessage->clear();
    }
    LOG_PERF(0, "Exit");
    return RepositoryVisibility::Unknown;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const QString accessToken = hasValidAccessToken() ? _accessToken : QString();
  const QString repoUrl =
      hostToApiBaseUrl(_host) + QStringLiteral("/repos/") +
      QString::fromUtf8(QUrl::toPercentEncoding(_owner)) + QStringLiteral("/") +
      QString::fromUtf8(QUrl::toPercentEncoding(_repository));

  const auto httpReply = httpClient->sendRequest(
      repoUrl, Util::HttpMethod::Get, githubJsonHeaders(),
      githubApiHeaders(accessToken), QByteArray());

  if (httpReply.error() == QNetworkReply::NetworkError::ContentNotFoundError ||
      httpReply.error() == QNetworkReply::NetworkError::ContentAccessDenied) {
    if (errorMessage) {
      errorMessage->clear();
    }
    LOG_INFO("GitHub repository '{}' appears to be private or inaccessible",
             repositoryPath().toStdString());
    LOG_PERF(0, "Exit");
    return RepositoryVisibility::Private;
  }

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        httpReply, QStringLiteral("Failed to request GitHub repository info."));
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return RepositoryVisibility::Unknown;
  }

  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (!jsonDoc.isObject()) {
    const QString message =
        QStringLiteral("Invalid GitHub repository info response.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return RepositoryVisibility::Unknown;
  }

  const QJsonObject obj = jsonDoc.object();
  if (!obj.contains(QStringLiteral("private"))) {
    if (errorMessage) {
      errorMessage->clear();
    }
    LOG_PERF(0, "Exit");
    return RepositoryVisibility::Unknown;
  }

  const bool isPrivate = obj.value(QStringLiteral("private")).toBool(false);
  if (errorMessage) {
    errorMessage->clear();
  }

  LOG_PERF(0, "Exit");
  return isPrivate ? RepositoryVisibility::Private
                   : RepositoryVisibility::Public;
}

QStringList GitHubRepo::listUserRepositories(QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  QStringList repositories;
  QSet<QString> seenRepositories;

  if (!hasValidAccessToken()) {
    const QString message =
        QStringLiteral("GitHub access token is missing or expired.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return repositories;
  }

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto authHeaderValue = QStringLiteral("Bearer ") + _accessToken;

  const auto requestJsonArray = [&](const QUrl &url,
                                    const QString &failureMessage,
                                    QJsonArray *arrayOut) -> bool {
    if (!arrayOut) {
      return false;
    }

    QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
    httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
        QStringLiteral("application/json");

    QMap<QString, QString> rawHeaders;
    rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
    rawHeaders[QStringLiteral("Authorization")] = authHeaderValue;
    rawHeaders[QStringLiteral("X-GitHub-Api-Version")] =
        QStringLiteral("2022-11-28");

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
    if (!jsonDoc.isArray()) {
      const QString message = responseErrorMessage(
          httpReply, QStringLiteral("Invalid GitHub repositories response."));
      if (errorMessage) {
        *errorMessage = message;
      }
      LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
      return false;
    }

    *arrayOut = jsonDoc.array();
    return true;
  };

  const auto appendRepositoryNames = [&](const QJsonArray &repos) {
    for (const auto &repoValue : repos) {
      if (!repoValue.isObject()) {
        continue;
      }

      const QJsonObject repoObj = repoValue.toObject();
      const QString fullName =
          repoObj.value(QStringLiteral("full_name")).toString().trimmed();
      const QString name =
          repoObj.value(QStringLiteral("name")).toString().trimmed();
      const QString repositoryName = !fullName.isEmpty() ? fullName : name;
      if (repositoryName.isEmpty() ||
          seenRepositories.contains(repositoryName)) {
        continue;
      }

      seenRepositories.insert(repositoryName);
      repositories += repositoryName;
    }
  };

  const auto fetchPagedRepositories =
      [&](const std::function<QUrl(int)> &buildUrl,
          const QString &failureMessage) -> bool {
    int page = 1;
    while (true) {
      QJsonArray pageArray;
      if (!requestJsonArray(buildUrl(page), failureMessage, &pageArray)) {
        return false;
      }

      const int beforeCount = repositories.size();
      appendRepositoryNames(pageArray);
      if (pageArray.size() < 100 || repositories.size() == beforeCount) {
        return true;
      }

      ++page;
    }
  };

  const auto userReposUrl = [&](int page) {
    QUrl url(hostToApiBaseUrl(_host) + QStringLiteral("/user/repos"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    query.addQueryItem(
        QStringLiteral("affiliation"),
        QStringLiteral("owner,collaborator,organization_member"));
    query.addQueryItem(QStringLiteral("sort"), QStringLiteral("full_name"));
    url.setQuery(query);
    return url;
  };

  if (!fetchPagedRepositories(
          userReposUrl,
          QStringLiteral("Failed to request GitHub repositories."))) {
    LOG_PERF(0, "Exit");
    return repositories;
  }

  const auto orgsUrl = [&](int page) {
    QUrl url(hostToApiBaseUrl(_host) + QStringLiteral("/user/orgs"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
    query.addQueryItem(QStringLiteral("page"), QString::number(page));
    url.setQuery(query);
    return url;
  };

  int orgPage = 1;
  while (true) {
    QJsonArray orgArray;
    if (!requestJsonArray(
            orgsUrl(orgPage),
            QStringLiteral("Failed to request GitHub organizations."),
            &orgArray)) {
      LOG_PERF(0, "Exit");
      return repositories;
    }

    if (orgArray.isEmpty()) {
      break;
    }

    for (const auto &orgValue : orgArray) {
      if (!orgValue.isObject()) {
        continue;
      }

      const QJsonObject orgObj = orgValue.toObject();
      const QString orgLogin =
          orgObj.value(QStringLiteral("login")).toString().trimmed();
      if (orgLogin.isEmpty()) {
        continue;
      }

      const auto orgReposUrl = [&](int page) {
        QUrl url(hostToApiBaseUrl(_host) + QStringLiteral("/orgs/") + orgLogin +
                 QStringLiteral("/repos"));
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("per_page"), QStringLiteral("100"));
        query.addQueryItem(QStringLiteral("page"), QString::number(page));
        query.addQueryItem(QStringLiteral("type"), QStringLiteral("all"));
        query.addQueryItem(QStringLiteral("sort"), QStringLiteral("full_name"));
        url.setQuery(query);
        return url;
      };

      if (!fetchPagedRepositories(
              orgReposUrl,
              QStringLiteral(
                  "Failed to request GitHub organization repositories."))) {
        LOG_PERF(0, "Exit");
        return repositories;
      }
    }

    if (orgArray.size() < 100) {
      break;
    }

    ++orgPage;
  }

  if (repositories.isEmpty()) {
    if (errorMessage) {
      *errorMessage = QStringLiteral(
          "No GitHub repositories were found for the authenticated account.");
    }
    LOG_INFO("No GitHub repositories found for authenticated account");
  } else if (errorMessage) {
    errorMessage->clear();
  }

  LOG_PERF(0, "Exit");
  return repositories;
}

IssueTrackerRepo::Type GitHubRepo::type() const {
  return IssueTrackerRepo::Type::GitHub;
}

QString GitHubRepo::serviceName() const {
  return QStringLiteral("GitHub");
}

QString GitHubRepo::host() const {
  return _host;
}

QString GitHubRepo::owner() const {
  return _owner;
}

QString GitHubRepo::repository() const {
  return _repository;
}

QString GitHubRepo::repositoryPath() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return _owner + QStringLiteral("/") + _repository;
}

QString GitHubRepo::webUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return hostToWebBaseUrl(_host) + QStringLiteral("/") + repositoryPath();
}

QString GitHubRepo::issuesUrl() const {
  if (_owner.isEmpty() || _repository.isEmpty()) {
    return QString();
  }

  return webUrl() + QStringLiteral("/issues");
}

bool GitHubRepo::isValid() const {
  return !_host.isEmpty();
}

QPair<QNetworkReply::NetworkError, QString> GitHubRepo::requestDeviceCode(
    const QString &clientId, DeviceCodeResponse *response, const QString &scope,
    QString *errorMessage) const {
  LOG_PERF(0, "Enter");

  if (clientId.trimmed().isEmpty()) {
    const QString message = QStringLiteral("GitHub OAuth client id is empty.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  if (!response) {
    const QString message =
        QStringLiteral("Device code response output is null.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Generic, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  QMap<QString, QString> fields;
  fields[QStringLiteral("client_id")] = clientId.trimmed();
  fields[QStringLiteral("scope")] =
      scope.trimmed().isEmpty() ? QStringLiteral("repo") : scope.trimmed();

  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/x-www-form-urlencoded");

  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("X-GitHub-Api-Version")] =
      QStringLiteral("2022-11-28");

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto httpReply = httpClient->sendRequest(
      hostToWebBaseUrl(_host) + QStringLiteral("/login/device/code"),
      Util::HttpMethod::Post, httpHeaders, rawHeaders, formEncode(fields));

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        httpReply, QStringLiteral("Failed to request GitHub device code."));
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
        QStringLiteral("Invalid GitHub device code response.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  const QJsonObject obj = jsonDoc.object();
  if (obj.contains(QStringLiteral("error"))) {
    const QString message = parseErrorDescription(obj);
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  response->deviceCode = obj.value(QStringLiteral("device_code")).toString();
  response->userCode = obj.value(QStringLiteral("user_code")).toString();
  response->verificationUri =
      QUrl(obj.value(QStringLiteral("verification_uri")).toString());
  response->verificationUriComplete =
      QUrl(obj.value(QStringLiteral("verification_uri_complete")).toString());
  response->expiresInSeconds =
      obj.value(QStringLiteral("expires_in")).toInt(900);
  response->intervalSeconds =
      std::max(1, obj.value(QStringLiteral("interval")).toInt(5));

  if (response->deviceCode.isEmpty() || response->userCode.isEmpty() ||
      !response->verificationUri.isValid()) {
    const QString message =
        QStringLiteral("GitHub device code response missing required fields.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  if (errorMessage) {
    errorMessage->clear();
  }

  LOG_INFO("Requested GitHub device code");
  LOG_PERF(0, "Exit");
  return {QNetworkReply::NetworkError::NoError, QString()};
}

QPair<QNetworkReply::NetworkError, QString> GitHubRepo::pollDeviceAccessToken(
    const QString &clientId, const QString &deviceCode, int intervalSeconds,
    int expiresInSeconds, QString *errorMessage,
    const std::atomic_bool *cancelled) {
  LOG_PERF(0, "Enter");

  if (clientId.trimmed().isEmpty() || deviceCode.trimmed().isEmpty()) {
    const QString message =
        QStringLiteral("GitHub OAuth polling parameters are invalid.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Security, "{}", message.toStdString());
    LOG_PERF(0, "Exit");
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  int currentInterval = std::max(1, intervalSeconds);
  const QDateTime deadline =
      QDateTime::currentDateTimeUtc().addSecs(std::max(1, expiresInSeconds));

  const auto isCancelled = [&]() {
    return cancelled != nullptr && cancelled->load();
  };

  const auto sleepWithCancellation = [&](int seconds) {
    for (int i = 0; i < seconds; ++i) {
      if (isCancelled()) {
        const QString cancelledMessage =
            QStringLiteral("GitHub authentication cancelled.");
        if (errorMessage) {
          *errorMessage = cancelledMessage;
        }
        return false;
      }

      QThread::sleep(1);
    }

    return true;
  };

  while (QDateTime::currentDateTimeUtc() < deadline) {
    if (isCancelled()) {
      const QString cancelledMessage =
          QStringLiteral("GitHub authentication cancelled.");
      if (errorMessage) {
        *errorMessage = cancelledMessage;
      }
      LOG_PERF(0, "Exit");
      return {QNetworkReply::NetworkError::OperationCanceledError,
              cancelledMessage};
    }

    const auto tokenReply =
        requestAccessTokenOnce(clientId, deviceCode, errorMessage);
    if (tokenReply.first == QNetworkReply::NetworkError::NoError) {
      LOG_PERF(0, "Exit");
      return tokenReply;
    }

    const QString message = tokenReply.second.trimmed();
    if (message.compare(QStringLiteral("authorization_pending"),
                        Qt::CaseInsensitive) == 0) {
      if (!sleepWithCancellation(currentInterval)) {
        const QString cancelledMessage =
            QStringLiteral("GitHub authentication cancelled.");
        LOG_PERF(0, "Exit");
        return {QNetworkReply::NetworkError::OperationCanceledError,
                cancelledMessage};
      }
      continue;
    }

    if (message.compare(QStringLiteral("slow_down"), Qt::CaseInsensitive) ==
        0) {
      currentInterval += 5;
      if (!sleepWithCancellation(currentInterval)) {
        const QString cancelledMessage =
            QStringLiteral("GitHub authentication cancelled.");
        LOG_PERF(0, "Exit");
        return {QNetworkReply::NetworkError::OperationCanceledError,
                cancelledMessage};
      }
      continue;
    }

    if (message.compare(QStringLiteral("expired_token"), Qt::CaseInsensitive) ==
        0) {
      const QString expiredMessage = QStringLiteral(
          "GitHub device authorization expired. Please try again.");
      if (errorMessage) {
        *errorMessage = expiredMessage;
      }
      LOG_ERROR(Log::ErrorId::Security, "{}", expiredMessage.toStdString());
      LOG_PERF(0, "Exit");
      return {QNetworkReply::NetworkError::ContentNotFoundError,
              expiredMessage};
    }

    if (message.compare(QStringLiteral("access_denied"), Qt::CaseInsensitive) ==
        0) {
      const QString deniedMessage =
          QStringLiteral("GitHub authorization was denied by the user.");
      if (errorMessage) {
        *errorMessage = deniedMessage;
      }
      LOG_ERROR(Log::ErrorId::Security, "{}", deniedMessage.toStdString());
      LOG_PERF(0, "Exit");
      return {QNetworkReply::NetworkError::ContentAccessDenied, deniedMessage};
    }

    LOG_PERF(0, "Exit");
    return tokenReply;
  }

  const QString timeoutMessage =
      QStringLiteral("Timed out waiting for GitHub OAuth authorization.");
  if (errorMessage) {
    *errorMessage = timeoutMessage;
  }
  LOG_ERROR(Log::ErrorId::Network, "{}", timeoutMessage.toStdString());
  LOG_PERF(0, "Exit");
  return {QNetworkReply::NetworkError::TimeoutError, timeoutMessage};
}

void GitHubRepo::setAccessToken(const QString &accessToken,
                                const QDateTime &expiresAt) {
  clearAccessToken();
  _accessToken = accessToken;
  _accessTokenExpiresAt = expiresAt;
}

const QString &GitHubRepo::accessToken() const {
  return _accessToken;
}

QDateTime GitHubRepo::accessTokenExpiresAt() const {
  return _accessTokenExpiresAt;
}

bool GitHubRepo::hasValidAccessToken() const {
  if (_accessToken.isEmpty()) {
    return false;
  }

  if (!_accessTokenExpiresAt.isValid()) {
    return true;
  }

  return _accessTokenExpiresAt > QDateTime::currentDateTimeUtc();
}

void GitHubRepo::clearAccessToken() {
  if (!_accessToken.isEmpty()) {
    _accessToken.fill(QChar(u'\0'));
    _accessToken.clear();
  }

  _accessTokenExpiresAt = QDateTime();
}

QString GitHubRepo::hostToWebBaseUrl(const QString &host) {
  if (host.isEmpty() || host == QStringLiteral("github.com")) {
    return QStringLiteral("https://github.com");
  }

  return QStringLiteral("https://") + host;
}

QString GitHubRepo::hostToApiBaseUrl(const QString &host) {
  if (host.isEmpty() || host == QStringLiteral("github.com")) {
    return QStringLiteral("https://api.github.com");
  }

  if (host.startsWith(QStringLiteral("api."))) {
    return QStringLiteral("https://") + host;
  }

  return QStringLiteral("https://") + host + QStringLiteral("/api/v3");
}

QByteArray GitHubRepo::formEncode(const QMap<QString, QString> &fields) {
  QUrlQuery query;
  for (auto it = fields.begin(); it != fields.end(); ++it) {
    query.addQueryItem(it.key(), it.value());
  }

  return query.query(QUrl::FullyEncoded).toUtf8();
}

QPair<QNetworkReply::NetworkError, QString> GitHubRepo::requestAccessTokenOnce(
    const QString &clientId, const QString &deviceCode, QString *errorMessage) {
  QMap<QString, QString> fields;
  fields[QStringLiteral("client_id")] = clientId.trimmed();
  fields[QStringLiteral("device_code")] = deviceCode.trimmed();
  fields[QStringLiteral("grant_type")] =
      QStringLiteral("urn:ietf:params:oauth:grant-type:device_code");

  QMap<QNetworkRequest::KnownHeaders, QString> httpHeaders;
  httpHeaders[QNetworkRequest::KnownHeaders::ContentTypeHeader] =
      QStringLiteral("application/x-www-form-urlencoded");

  QMap<QString, QString> rawHeaders;
  rawHeaders[QStringLiteral("Accept")] = QStringLiteral("application/json");
  rawHeaders[QStringLiteral("X-GitHub-Api-Version")] =
      QStringLiteral("2022-11-28");

  const auto httpClient = std::make_shared<Util::HttpClient>();
  const auto httpReply = httpClient->sendRequest(
      hostToWebBaseUrl(_host) + QStringLiteral("/login/oauth/access_token"),
      Util::HttpMethod::Post, httpHeaders, rawHeaders, formEncode(fields));

  if (httpReply.error() != QNetworkReply::NetworkError::NoError) {
    const QString message = responseErrorMessage(
        httpReply, QStringLiteral("Failed to request GitHub access token."));
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    return {httpReply.error(), message};
  }

  const QJsonDocument jsonDoc = QJsonDocument::fromJson(httpReply.response());
  if (!jsonDoc.isObject()) {
    const QString message =
        QStringLiteral("Invalid GitHub access token response.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  const QJsonObject obj = jsonDoc.object();
  if (obj.contains(QStringLiteral("error"))) {
    const QString error = obj.value(QStringLiteral("error")).toString();
    const QString message = parseErrorDescription(obj);
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_DEBUG("GitHub access token not ready yet: {}", error.toStdString());
    return {QNetworkReply::NetworkError::UnknownContentError, error};
  }

  const QString accessToken =
      obj.value(QStringLiteral("access_token")).toString();
  if (accessToken.isEmpty()) {
    const QString message =
        QStringLiteral("GitHub access token response missing access_token.");
    if (errorMessage) {
      *errorMessage = message;
    }
    LOG_ERROR(Log::ErrorId::Network, "{}", message.toStdString());
    return {QNetworkReply::NetworkError::ContentNotFoundError, message};
  }

  const int expiresInSeconds = obj.value(QStringLiteral("expires_in")).toInt(0);
  setAccessToken(accessToken,
                 expiresInSeconds > 0
                     ? QDateTime::currentDateTimeUtc().addSecs(expiresInSeconds)
                     : QDateTime());

  if (errorMessage) {
    errorMessage->clear();
  }

  LOG_INFO("Received GitHub OAuth access token");
  return {QNetworkReply::NetworkError::NoError, accessToken};
}

} // namespace IssueTrackers
