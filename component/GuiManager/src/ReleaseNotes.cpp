#include "ReleaseNotes.h"

#include <Log.h>

#include <QFile>
#include <QStringList>
#include <QVector>
#include <QVersionNumber>

#include <algorithm>

#define THREAD_ID "ReleaseNotes"

namespace GuiManager::ReleaseNotes {

namespace {

struct Section {
  QVersionNumber version;
  QString text;
};

QVersionNumber parseVersion(const QString &text) {
  QString trimmed = text.trimmed();
  if (trimmed.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
    trimmed.remove(0, 1);
  }

  return QVersionNumber::fromString(trimmed);
}

QVector<Section> parseSections(const QString &markdown) {
  QVector<Section> sections;

  bool inCodeFence = false;
  bool inSection = false;
  Section current;

  const auto flush = [&]() {
    if (inSection && !current.version.isNull()) {
      current.text = current.text.trimmed();
      sections.append(current);
    }
    current = Section();
  };

  const QStringList lines = markdown.split(QLatin1Char('\n'));
  for (const QString &rawLine : lines) {
    const QString line =
        rawLine.endsWith(QLatin1Char('\r')) ? rawLine.chopped(1) : rawLine;

    if (line.trimmed().startsWith(QLatin1String("```"))) {
      inCodeFence = !inCodeFence;
    }

    // Only second level headings start a section, "###" does not.
    if (!inCodeFence && line.startsWith(QLatin1String("## "))) {
      flush();
      inSection = true;
      current.version = parseVersion(line.mid(3));
      current.text = line + QLatin1Char('\n');
      continue;
    }

    if (inSection) {
      current.text += line + QLatin1Char('\n');
    }
  }

  flush();

  return sections;
}

} // namespace

QString load() {
  LOG_PERF(0, "Enter");

  QFile file(QStringLiteral(":/release_notes/RELEASE_NOTES.md"));
  if (!file.open(QIODevice::ReadOnly)) {
    LOG_WARN("Failed to open the bundled release notes.");
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString markdown = QString::fromUtf8(file.readAll());

  LOG_PERF(0, "Exit");
  return markdown;
}

QString sectionsBetween(const QString &markdown, const QString &lastVersion,
                        const QString &currentVersion) {
  LOG_PERF(0, "Enter");

  const QVersionNumber last = parseVersion(lastVersion);
  const QVersionNumber current = parseVersion(currentVersion);

  QVector<Section> selected;
  for (const Section &section : parseSections(markdown)) {
    if (!last.isNull() && QVersionNumber::compare(section.version, last) <= 0) {
      continue;
    }

    if (!current.isNull() &&
        QVersionNumber::compare(section.version, current) > 0) {
      continue;
    }

    selected.append(section);
  }

  std::stable_sort(selected.begin(), selected.end(),
                   [](const Section &a, const Section &b) {
                     return QVersionNumber::compare(a.version, b.version) > 0;
                   });

  QStringList texts;
  for (const Section &section : selected) {
    texts.append(section.text);
  }

  LOG_PERF(0, "Exit");
  return texts.join(QStringLiteral("\n\n"));
}

} // namespace GuiManager::ReleaseNotes
