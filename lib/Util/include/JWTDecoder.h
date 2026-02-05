#pragma once

#include <QString>

namespace Util {

class JWTDecoder {
public:
  struct Claims {
    QString givenName;  // given_name claim
    QString familyName; // family_name claim
    QString email;      // email claim
    QString plan;       // subscription plan claim
    QString roles;      // roles claim
    qlonglong exp;      // expiration Unix time claim
  };

  // Decode JWT and extract standard claims
  // Returns Claims struct with extracted values (empty strings if not found)
  static Claims decodeClaims(const QString &jwt);

private:
  // Helper to validate JWT format
  static bool isValidJwtFormat(const QString &jwt);

  // Helper to decode base64url
  static QByteArray decodeBase64Url(const QString &base64url);
};

} // namespace Util
