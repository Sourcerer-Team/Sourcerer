#include "JWTDecoder.h"

#include <Log.h>

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>

#define THREAD_ID "JWTDecoder"

namespace Util {

JWTDecoder::Claims JWTDecoder::decodeClaims(const QString &jwt) {
  LOG_PERF(0, "Enter");

  Claims claims;

  // Validate JWT format
  if (!isValidJwtFormat(jwt)) {
    LOG_WARN("Invalid JWT format");
    LOG_PERF(0, "Exit");
    return claims;
  }

  // JWT format: header.payload.signature
  // We need to decode the payload (second part)
  const QStringList parts = jwt.split('.');
  if (parts.size() < 2) {
    LOG_WARN("JWT does not have payload part");
    LOG_PERF(0, "Exit");
    return claims;
  }

  // Decode the payload (base64url encoded)
  const QByteArray decodedPayload = decodeBase64Url(parts.at(1));
  const QJsonDocument jsonDoc = QJsonDocument::fromJson(decodedPayload);

  if (!jsonDoc.isObject()) {
    LOG_WARN("JWT payload is not a JSON object");
    LOG_PERF(0, "Exit");
    return claims;
  }

  const QJsonObject claimsObj = jsonDoc.object();

  // Extract standard claims
  claims.givenName = claimsObj.value("first_name").toString();
  claims.familyName = claimsObj.value("last_name").toString();
  claims.email = claimsObj.value("email").toString();
  claims.plan = claimsObj.value("plan").toString();
  claims.roles = claimsObj.value("roles").toString();
  claims.exp = claimsObj.value("exp").toVariant().toLongLong();

  LOG_INFO("Extracted JWT claims - Given Name: {}, Family Name: {}, Email: {}, "
           "Roles: {}",
           claims.givenName, claims.familyName, claims.email, claims.roles);

  LOG_PERF(0, "Exit");
  return claims;
}

bool JWTDecoder::isValidJwtFormat(const QString &jwt) {
  // A valid JWT should have 3 parts separated by dots
  const int dotCount = jwt.count('.');
  return dotCount == 2;
}

QByteArray JWTDecoder::decodeBase64Url(const QString &base64url) {
  // Decode from base64url to base64 (replace - with +, _ with /)
  QString payload = base64url;
  payload.replace('-', '+').replace('_', '/');

  // Add padding if necessary
  switch (payload.length() % 4) {
  case 1:
    payload += "===";
    break;
  case 2:
    payload += "==";
    break;
  case 3:
    payload += "=";
    break;
  }

  // Decode base64
  return QByteArray::fromBase64(payload.toLatin1(), QByteArray::Base64Encoding);
}

} // namespace Util
