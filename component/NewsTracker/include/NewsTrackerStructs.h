#pragma once

#include <QDateTime>
#include <QString>

namespace NewsTracker::Structs {

enum class NewsType { Unknown = 0, General, Update };

struct NewsItem {
  QString id;
  NewsType type{NewsType::Unknown};
  QDateTime publishedAt;
  QString text;
  QDateTime createdAt;
  QDateTime updatedAt;
  QString version;
  QString downloadUrlLinux;
  QString downloadUrlWindows;

  bool isValid() const { return !id.trimmed().isEmpty(); }

  bool isUpdate() const { return type == NewsType::Update; }

  QString typeName() const {
    switch (type) {
    case NewsType::General:
      return QStringLiteral("general");
    case NewsType::Update:
      return QStringLiteral("update");
    default:
      return QStringLiteral("unknown");
    }
  }

  QString title() const {
    if (type == NewsType::Update) {
      return QStringLiteral("Update available");
    }

    return QStringLiteral("News");
  }

  QString downloadUrl() const {
#ifdef Q_OS_WIN
    return downloadUrlWindows;
#else
    return downloadUrlLinux;
#endif
  }
};

} // namespace NewsTracker::Structs

Q_DECLARE_METATYPE(NewsTracker::Structs::NewsItem);