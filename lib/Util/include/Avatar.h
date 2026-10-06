#pragma once

#include <QColor>
#include <QImage>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QStringList>

#include <functional>

class QNetworkAccessManager;

namespace Util::Avatar {

QString initialsForName(const QString &authorName, const QString &authorEmail);
QColor backgroundColorForKey(const QString &key);
QPixmap createInitialsAvatar(const QString &authorName,
                             const QString &authorEmail, const QSize &size);
QPixmap circularCropPixmap(const QImage &image, const QSize &size);
QString avatarUserNameFromEmail(const QString &normalizedEmail);
QStringList avatarUrlsForEmail(const QString &normalizedEmail);

bool requestAvatarPixmap(QNetworkAccessManager *manager,
                         const QString &normalizedEmail, const QSize &size,
                         const std::function<void(const QPixmap &)> &onSuccess,
                         const std::function<void()> &onFailure = {});

} // namespace Util::Avatar
