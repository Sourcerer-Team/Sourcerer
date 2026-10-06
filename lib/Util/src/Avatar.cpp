#include "Avatar.h"

#include <QByteArray>
#include <QChar>
#include <QCryptographicHash>
#include <QFont>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QObject>
#include <QPainter>
#include <QPainterPath>
#include <QUrl>

namespace Util::Avatar {
namespace {

void appendAvatarUrl(QStringList &avatarUrls, const QString &url) {
  if (!url.isEmpty() && !avatarUrls.contains(url)) {
    avatarUrls << url;
  }
}

} // namespace

QString initialsForName(const QString &authorName, const QString &authorEmail) {
  QStringList initialsParts;
  const QString simplifiedName = authorName.simplified();
  if (!simplifiedName.isEmpty()) {
    const QStringList nameParts =
        simplifiedName.split(QChar::fromLatin1(' '), Qt::SkipEmptyParts);
    for (const QString &part : nameParts) {
      if (!part.isEmpty() && part.at(0).isLetterOrNumber()) {
        initialsParts << part.left(1).toUpper();
      }
    }
  }

  QString initials = initialsParts.join(QString());
  if (initials.size() > 2) {
    initials = initials.left(2);
  }

  if (initials.isEmpty()) {
    const QString localPart =
        authorEmail.section(QChar::fromLatin1('@'), 0, 0).simplified();
    for (const QChar ch : localPart) {
      if (ch.isLetterOrNumber()) {
        initials.append(ch.toUpper());
        if (initials.size() == 2) {
          break;
        }
      }
    }
  }

  if (initials.isEmpty()) {
    initials = QStringLiteral("?");
  }

  return initials;
}

QColor backgroundColorForKey(const QString &key) {
  const size_t hash = qHash(key.trimmed().toLower());
  return QColor::fromHsl(static_cast<int>(hash % 360), 170, 120);
}

QPixmap createInitialsAvatar(const QString &authorName,
                             const QString &authorEmail, const QSize &size) {
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setPen(Qt::NoPen);
  painter.setBrush(backgroundColorForKey(
      !authorEmail.trimmed().isEmpty() ? authorEmail : authorName));
  painter.drawEllipse(pixmap.rect().adjusted(1, 1, -1, -1));

  QFont font = painter.font();
  font.setBold(true);
  font.setPointSizeF(size.height() * 0.38);
  painter.setFont(font);
  painter.setPen(Qt::white);
  painter.drawText(pixmap.rect(), Qt::AlignCenter,
                   initialsForName(authorName, authorEmail));

  return pixmap;
}

QPixmap circularCropPixmap(const QImage &image, const QSize &size) {
  if (image.isNull() || size.isEmpty()) {
    return {};
  }

  QPixmap source = QPixmap::fromImage(image.scaled(
      size, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation));
  if (source.isNull()) {
    return {};
  }

  QPixmap target(size);
  target.fill(Qt::transparent);

  QPainter painter(&target);
  painter.setRenderHint(QPainter::Antialiasing, true);
  QPainterPath clipPath;
  clipPath.addEllipse(target.rect().adjusted(1, 1, -1, -1));
  painter.setClipPath(clipPath);

  const QPoint offset((target.width() - source.width()) / 2,
                      (target.height() - source.height()) / 2);
  painter.drawPixmap(offset, source);
  return target;
}

QString avatarUserNameFromEmail(const QString &normalizedEmail) {
  const QString localPart =
      normalizedEmail.section(QChar::fromLatin1('@'), 0, 0);
  const int plusIndex = localPart.indexOf(QChar::fromLatin1('+'));
  if (plusIndex >= 0 && plusIndex + 1 < localPart.size()) {
    return localPart.mid(plusIndex + 1);
  }
  return localPart;
}

QStringList avatarUrlsForEmail(const QString &normalizedEmail) {
  QStringList avatarUrls;
  const QString domain = normalizedEmail.section(QChar::fromLatin1('@'), 1, 1);
  const QString userName = avatarUserNameFromEmail(normalizedEmail);
  const QString encodedUserName =
      QString::fromLatin1(QUrl::toPercentEncoding(userName.trimmed()));

  if (domain.endsWith(QStringLiteral("gmail.com")) ||
      domain.endsWith(QStringLiteral("googlemail.com"))) {
    appendAvatarUrl(
        avatarUrls,
        QStringLiteral(
            "https://profiles.google.com/s2/photos/profile/%1?sz=128")
            .arg(
                QString::fromLatin1(QUrl::toPercentEncoding(normalizedEmail))));
  }

  if (domain.endsWith(QStringLiteral("github.com"))) {
    appendAvatarUrl(avatarUrls,
                    QStringLiteral("https://github.com/%1.png?size=128")
                        .arg(encodedUserName));
  }

  if (domain.endsWith(QStringLiteral("gitlab.com"))) {
    appendAvatarUrl(avatarUrls,
                    QStringLiteral("https://gitlab.com/%1.png?width=128")
                        .arg(encodedUserName));
  }

  if (domain.endsWith(QStringLiteral("bitbucket.org"))) {
    appendAvatarUrl(
        avatarUrls,
        QStringLiteral("https://bitbucket.org/account/%1/avatar/128/")
            .arg(encodedUserName));
  }

  if (domain.endsWith(QStringLiteral("azure.com")) ||
      domain.endsWith(QStringLiteral("visualstudio.com")) ||
      domain.endsWith(QStringLiteral("microsoft.com")) ||
      domain.endsWith(QStringLiteral("outlook.com")) ||
      domain.endsWith(QStringLiteral("hotmail.com")) ||
      domain.endsWith(QStringLiteral("live.com"))) {
    appendAvatarUrl(avatarUrls,
                    QStringLiteral("https://outlook.office.com/owa/service.svc/"
                                   "s/GetPersonaPhoto?email=%1&size=HR64x64")
                        .arg(QString::fromLatin1(
                            QUrl::toPercentEncoding(normalizedEmail))));
  }

  const QString gravatarHash =
      QString::fromLatin1(QCryptographicHash::hash(normalizedEmail.toUtf8(),
                                                   QCryptographicHash::Md5)
                              .toHex());
  appendAvatarUrl(
      avatarUrls,
      QStringLiteral("https://www.gravatar.com/avatar/%1?s=128&d=404")
          .arg(gravatarHash));

  return avatarUrls;
}

bool requestAvatarPixmap(QNetworkAccessManager *manager,
                         const QString &normalizedEmail, const QSize &size,
                         const std::function<void(const QPixmap &)> &onSuccess,
                         const std::function<void()> &onFailure) {
  if (!manager || normalizedEmail.isEmpty() || size.isEmpty()) {
    if (onFailure) {
      onFailure();
    }
    return false;
  }

  const QStringList avatarUrls = avatarUrlsForEmail(normalizedEmail);
  if (avatarUrls.isEmpty()) {
    if (onFailure) {
      onFailure();
    }
    return false;
  }

  auto tryNextSource = std::make_shared<std::function<void(int)>>();
  *tryNextSource = [manager, size, avatarUrls, onSuccess, onFailure,
                    tryNextSource](int index) {
    if (index >= avatarUrls.size()) {
      if (onFailure) {
        onFailure();
      }
      return;
    }

    QNetworkRequest request{QUrl(avatarUrls[index])};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         QByteArrayLiteral("Sourcerer"));
    request.setRawHeader(
        QByteArrayLiteral("Accept"),
        QByteArrayLiteral("image/avif,image/webp,image/*,*/*;q=0.8"));

    QNetworkReply *reply = manager->get(request);
    QObject::connect(
        reply, &QNetworkReply::finished, manager,
        [reply, size, onSuccess, tryNextSource, index]() {
          const int statusCode =
              reply->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                  .toInt();
          const bool httpSuccess = statusCode >= 200 && statusCode < 300;
          if (reply->error() != QNetworkReply::NoError || !httpSuccess) {
            reply->deleteLater();
            (*tryNextSource)(index + 1);
            return;
          }

          const QByteArray payload = reply->readAll();
          const QImage image = QImage::fromData(payload);
          const QPixmap avatar = circularCropPixmap(image, size);
          if (avatar.isNull()) {
            reply->deleteLater();
            (*tryNextSource)(index + 1);
            return;
          }

          if (onSuccess) {
            onSuccess(avatar);
          }
          reply->deleteLater();
        });
  };

  (*tryNextSource)(0);
  return true;
}

} // namespace Util::Avatar
