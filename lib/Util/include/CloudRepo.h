#pragma once

#include <QNetworkReply>
#include <QObject>
#include <QPair>
#include <QString>

namespace Util {

/**
 * @brief Cloud repository for cloud service communication
 *
 * Handles communication with cloud service including signin,
 * authentication, and other cloud operations.
 */
class CloudRepo : public QObject {
  Q_OBJECT

public:
  /**
   * @brief Get the singleton instance
   */
  static CloudRepo &getInstance();

  /**
   * @brief Attempt to signin to cloud service
   * @param email User email
   * @param password User password
   */
  QPair<QNetworkReply::NetworkError, QString> signin(const QString &email,
                                                     const QString &password);

  /**
   * @brief Get the cloud signin URL
   * @return URL for cloud signin endpoint
   */
  QString getCloudSigninUrl() const;

signals:
  /**
   * @brief Emitted when signin is successful
   * @param email User email
   * @param password User password (for potential credential storage)
   * @param accessToken JWT access token returned from server
   * @param response Full response from server
   */
  void signinSuccess(const QString &email, const QString &password,
                     const QString &accessToken, const QString &response);

  /**
   * @brief Emitted when HTTP error occurs during signin
   * @param email User email
   * @param error Error from HTTP client
   */
  void signinError(const QString &email, QNetworkReply::NetworkError error);

  /**
   * @brief Emitted when error occurs during parsing of signin response
   * @param email User email
   * @param error Error message from cloud service
   */
  void signinFailed(const QString &email, const QString &error);

private:
  CloudRepo();
  ~CloudRepo() override;

  // Prevent copying
  CloudRepo(const CloudRepo &) = delete;
  CloudRepo &operator=(const CloudRepo &) = delete;

  /**
   * @brief Handle HTTP response from signin request
   */
  QPair<QNetworkReply::NetworkError, QString>
  processResponse(const QString &response, int statusCode);

private:
  QString _pendingEmail;    // Email for the current signin request
  QString _pendingPassword; // Password for the current signin request
};

} // namespace Util
