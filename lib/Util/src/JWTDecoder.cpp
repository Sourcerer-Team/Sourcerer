#include "JWTDecoder.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>

#define THREAD_ID "JWTDecoder"

namespace Util {

JWTDecoder::Claims JWTDecoder::decodeClaims(const QString &jwt) {
  LOG_PERF(0, "Enter");

  Claims claims;

  // Validate JWT format (without signature verification)
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

bool JWTDecoder::verifySignature(const QString &jwt,
                                 const QString &publicKeyPem) {
  LOG_PERF(0, "Enter");

  if (publicKeyPem.isEmpty()) {
    LOG_WARN("Public key is empty, cannot verify JWT signature");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Validate format first (without signature verification)
  if (!isValidJwtFormat(jwt)) {
    LOG_WARN("Invalid JWT format");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QStringList parts = jwt.split('.');
  if (parts.size() != 3) {
    LOG_ERROR(Log::ErrorId::Security, "JWT does not have exactly 3 parts");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString header = parts.at(0);
  const QString payload = parts.at(1);
  const QString signatureBase64Url = parts.at(2);

  // Get the algorithm from header
  const QString algorithm = getJWTAlgorithm(jwt);
  if (algorithm.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Security, "Could not determine JWT algorithm");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify RSA signature (supports RS256, RS384, RS512)
  if (!algorithm.startsWith("RS")) {
    LOG_ERROR(
        Log::ErrorId::Security,
        "Unsupported JWT algorithm: {}. Expected RSA-based algorithm (RS256, "
        "RS384, RS512)",
        algorithm);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create the message that was signed (header.payload)
  const QString message = header + "." + payload;

  bool signatureValid =
      verifyRSASignature(message, signatureBase64Url, publicKeyPem);

  if (!signatureValid) {
    LOG_ERROR(Log::ErrorId::Security,
              "JWT signature verification failed using public key");
  } else {
    LOG_DEBUG("JWT signature verification successful");
  }

  LOG_PERF(0, "Exit");
  return signatureValid;
}

QString JWTDecoder::getJWTPublicKey() {
  LOG_PERF(0, "Enter");

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  QString publicKeyPath =
      configManager.getString(ConfigManager::ConfigKey::CloudJwtPublicKeyPath);
  if (!publicKeyPath.isEmpty()) {
    QFile keyFile(publicKeyPath);
    if (keyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QString publicKey = QString::fromUtf8(keyFile.readAll());
      keyFile.close();
      LOG_PERF(0, "Exit");
      return publicKey;
    }
  }

  LOG_WARN("JWT public key not configured");
  LOG_PERF(0, "Exit");
  return QString();
}

bool JWTDecoder::isValidJwtFormat(const QString &jwt,
                                  const QString &publicKeyPem) {
  // A valid JWT should have 3 parts separated by dots
  const int dotCount = jwt.count('.');
  if (dotCount != 2) {
    return false;
  }

  // If public key is provided, verify signature
  if (!publicKeyPem.isEmpty()) {
    return verifySignature(jwt, publicKeyPem);
  }

  // Only format validation
  return true;
}

QString JWTDecoder::getJWTAlgorithm(const QString &jwt) {
  LOG_PERF(0, "Enter");

  const QStringList parts = jwt.split('.');
  if (parts.isEmpty()) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  const QByteArray decodedHeader = decodeBase64Url(parts.at(0));
  const QJsonDocument jsonDoc = QJsonDocument::fromJson(decodedHeader);

  if (!jsonDoc.isObject()) {
    LOG_PERF(0, "Exit");
    return QString();
  }

  const QString algorithm = jsonDoc.object().value("alg").toString();

  LOG_PERF(0, "Exit");
  return algorithm;
}

bool JWTDecoder::verifyRSASignature(const QString &message,
                                    const QString &signatureBase64Url,
                                    const QString &publicKeyPem) {
  LOG_PERF(0, "Enter");

  // Decode the signature from base64url
  const QByteArray signatureBytes = decodeBase64Url(signatureBase64Url);
  if (signatureBytes.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to decode signature");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Convert public key to UTF-8 bytes (persistent, not temporary)
  const QByteArray keyBytes = publicKeyPem.toUtf8();

  // Load the public key from PEM format
  BIO *keyBio = BIO_new_mem_buf(
      reinterpret_cast<const unsigned char *>(keyBytes.constData()),
      keyBytes.length());
  if (!keyBio) {
    LOG_ERROR(Log::ErrorId::Security, "Failed to create BIO for public key");
    LOG_PERF(0, "Exit");
    return false;
  }

  EVP_PKEY *pubKey = PEM_read_bio_PUBKEY(keyBio, nullptr, nullptr, nullptr);
  BIO_free(keyBio);

  if (!pubKey) {
    unsigned long errCode = ERR_get_error();
    char errBuf[256];
    ERR_error_string_n(errCode, errBuf, sizeof(errBuf));
    LOG_ERROR(
        Log::ErrorId::Security,
        "Failed to read public key from PEM format. Error: {} (0x{:X}). "
        "Key must be in PKCS#8 PEM format (BEGIN PUBLIC KEY...END PUBLIC KEY). "
        "Key length: {} bytes",
        errBuf, errCode, keyBytes.length());
    LOG_DEBUG("First 100 chars of key: {}", keyBytes.left(100).constData());
    LOG_PERF(0, "Exit");
    return false;
  }

  // Create EVP context for verification
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  if (!ctx) {
    EVP_PKEY_free(pubKey);
    LOG_ERROR(Log::ErrorId::Security, "Failed to create EVP_MD_CTX");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Initialize verification context with SHA256
  int result =
      EVP_DigestVerifyInit(ctx, nullptr, EVP_sha256(), nullptr, pubKey);
  if (result != 1) {
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubKey);
    LOG_ERROR(Log::ErrorId::Security, "EVP_DigestVerifyInit failed");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Update with the message that was signed
  const QByteArray messageBytes = message.toUtf8();
  result =
      EVP_DigestVerifyUpdate(ctx, messageBytes.data(), messageBytes.size());
  if (result != 1) {
    EVP_MD_CTX_free(ctx);
    EVP_PKEY_free(pubKey);
    LOG_ERROR(Log::ErrorId::Security, "EVP_DigestVerifyUpdate failed");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Verify the signature
  result = EVP_DigestVerifyFinal(ctx,
                                 reinterpret_cast<unsigned char *>(
                                     const_cast<char *>(signatureBytes.data())),
                                 signatureBytes.size());

  EVP_MD_CTX_free(ctx);
  EVP_PKEY_free(pubKey);

  if (result == 1) {
    LOG_DEBUG("JWT RSA signature verified successfully");
    LOG_PERF(0, "Exit");
    return true;
  } else if (result == 0) {
    LOG_ERROR(Log::ErrorId::Security, "JWT RSA signature verification failed");
    LOG_PERF(0, "Exit");
    return false;
  } else {
    char errBuf[256];
    ERR_error_string_n(ERR_get_error(), errBuf, sizeof(errBuf));
    LOG_ERROR(Log::ErrorId::Security, "EVP_DigestVerifyFinal error: {}",
              errBuf);
    LOG_PERF(0, "Exit");
    return false;
  }
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
