// MIT License
// Copyright (c) 2026 Sourcerer

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
      // Trailing empty lines are part of the section: they are the spacing
      // the author put between the versions.
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

// Markdown collapses any number of empty lines into one paragraph break. Every
// empty line after the first one is turned into a paragraph holding a no-break
// space, which is rendered as an empty line, so the spacing written in the
// release notes file is kept. Empty lines in code fences are left alone, and
// the ones at the start and at the end are dropped.
QString preserveEmptyLines(const QString &markdown) {
  const QChar nbsp(0x00A0);

  QStringList out;
  bool inCodeFence = false;
  int emptyLines = 0;

  const QStringList lines = markdown.split(QLatin1Char('\n'));
  for (const QString &rawLine : lines) {
    const QString line =
        rawLine.endsWith(QLatin1Char('\r')) ? rawLine.chopped(1) : rawLine;

    if (!inCodeFence && line.trimmed().isEmpty()) {
      ++emptyLines;
      continue;
    }

    if (emptyLines > 0 && !out.isEmpty()) {
      out.append(QString());
      for (int i = 0; i < emptyLines; ++i) {
        out.append(QString(nbsp));
        out.append(QString());
      }
    }
    emptyLines = 0;

    if (line.trimmed().startsWith(QLatin1String("```"))) {
      inCodeFence = !inCodeFence;
    }

    out.append(line);
  }

  return out.join(QLatin1Char('\n'));
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
  return preserveEmptyLines(texts.join(QString()));
}

} // namespace GuiManager::ReleaseNotes
