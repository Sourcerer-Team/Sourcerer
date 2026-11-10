// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QString>
#include <QVector>

namespace Util {

class JWTDecoder {
public:
  struct Claims {
    QString givenName;      // given_name claim
    QString familyName;     // family_name claim
    QString email;          // email claim
    QString plan;           // subscription plan claim
    QVector<QString> roles; // roles claim
    qlonglong exp;          // expiration Unix time claim
  };

  // Decode JWT and extract standard claims
  // Returns Claims struct with extracted values (empty strings if not found)
  static Claims decodeClaims(const QString &jwt);

  // Verify JWT signature using the provided public key (PEM format)
  // Returns true if signature is valid, false otherwise
  static bool verifySignature(const QString &jwt, const QString &publicKeyPem);

  // Get JWT public key from configuration file path
  // Returns the PEM-formatted public key content, or empty string if not
  // configured
  static QString getJWTPublicKey();

private:
  // Helper to validate JWT format and optionally verify signature
  // If publicKeyPem is empty, only validates format without signature
  // verification
  static bool isValidJwtFormat(const QString &jwt,
                               const QString &publicKeyPem = QString());

  // Helper to decode base64url
  static QByteArray decodeBase64Url(const QString &base64url);

  // Helper to decode JWT header and extract algorithm
  static QString getJWTAlgorithm(const QString &jwt);

  // Helper to verify RSA signature using public key
  static bool verifyRSASignature(const QString &message,
                                 const QString &signatureBase64Url,
                                 const QString &publicKeyPem);
};

} // namespace Util
