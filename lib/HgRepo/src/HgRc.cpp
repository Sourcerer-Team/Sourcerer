// MIT License
// Copyright (c) 2026 Sourcerer

#include "HgRc.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace HgRepo::Private {

HgRc::HgRc(const QString &fileName) : _fileName(fileName) {}

bool HgRc::load() {
  _lines.clear();
  _errorMessage.clear();

  QFile file(_fileName);
  if (!file.exists()) {
    return true;
  }
  if (!file.open(QIODevice::ReadOnly)) {
    _errorMessage = QStringLiteral("Failed to read %1: %2")
                        .arg(_fileName, file.errorString());
    return false;
  }

  parse(QString::fromUtf8(file.readAll()));
  return true;
}

bool HgRc::save() {
  QDir().mkpath(QFileInfo(_fileName).absolutePath());

  QString content;
  for (const Line &line : _lines) {
    content += line.text + '\n';
  }

  QSaveFile file(_fileName);
  if (!file.open(QIODevice::WriteOnly) || file.write(content.toUtf8()) < 0 ||
      !file.commit()) {
    _errorMessage = QStringLiteral("Failed to write %1: %2")
                        .arg(_fileName, file.errorString());
    return false;
  }
  return true;
}

void HgRc::parse(const QString &content) {
  static const QRegularExpression sectionPattern(
      QStringLiteral("^\\[([^\\[]+)\\]"));
  static const QRegularExpression itemPattern(
      QStringLiteral("^([^=\\s][^=]*?)\\s*=\\s*(.*\\S|)\\s*$"));

  QString section;
  bool inDefinition = false;

  QStringList texts = QString(content).remove('\r').split('\n');
  if (!texts.isEmpty() && texts.last().isEmpty()) {
    texts.removeLast();
  }

  for (const QString &text : texts) {
    Line line;
    line.text = text;

    if (inDefinition && !text.isEmpty() &&
        (text.at(0) == ' ' || text.at(0) == '\t') &&
        !text.trimmed().isEmpty()) {
      line.continuation = true;
      line.section = section;
      // Mercurial joins continuation lines with a newline.
      for (int i = _lines.size() - 1; i >= 0; --i) {
        if (!_lines[i].continuation) {
          _lines[i].value += '\n' + text.trimmed();
          break;
        }
      }
      _lines.append(line);
      continue;
    }

    inDefinition = false;

    const QRegularExpressionMatch sectionMatch = sectionPattern.match(text);
    if (sectionMatch.hasMatch()) {
      section = sectionMatch.captured(1).trimmed();
      line.header = true;
      line.section = section;
      _lines.append(line);
      continue;
    }

    line.section = section;
    const QString trimmed = text.trimmed();
    if (!trimmed.isEmpty() && !trimmed.startsWith('#') &&
        !trimmed.startsWith(';') && !trimmed.startsWith('%')) {
      const QRegularExpressionMatch itemMatch = itemPattern.match(text);
      if (itemMatch.hasMatch()) {
        line.key = itemMatch.captured(1);
        line.value = itemMatch.captured(2);
        inDefinition = true;
      }
    }
    _lines.append(line);
  }
}

int HgRc::definitionEnd(int index) const {
  int end = index + 1;
  while (end < _lines.size() && _lines[end].continuation) {
    ++end;
  }
  return end;
}

bool HgRc::contains(const QString &section, const QString &key) const {
  for (const Line &line : _lines) {
    if (!line.continuation && line.section == section && line.key == key) {
      return true;
    }
  }
  return false;
}

QString HgRc::value(const QString &section, const QString &key,
                    const QString &defaultValue) const {
  QString result = defaultValue;
  for (const Line &line : _lines) {
    if (!line.continuation && line.section == section && line.key == key) {
      result = line.value;
    }
  }
  return result;
}

QList<QPair<QString, QString>> HgRc::entries(const QString &section) const {
  QList<QPair<QString, QString>> result;
  for (const Line &line : _lines) {
    if (line.continuation || line.section != section || line.key.isEmpty()) {
      continue;
    }
    bool replaced = false;
    for (auto &entry : result) {
      if (entry.first == line.key) {
        entry.second = line.value;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      result.append({line.key, line.value});
    }
  }
  return result;
}

void HgRc::setValue(const QString &section, const QString &key,
                    const QString &value) {
  Line line;
  line.section = section;
  line.key = key;
  line.value = value;
  line.text = key + QStringLiteral(" = ") + value;

  for (int i = _lines.size() - 1; i >= 0; --i) {
    if (!_lines[i].continuation && _lines[i].section == section &&
        _lines[i].key == key) {
      const int end = definitionEnd(i);
      for (int j = end - 1; j > i; --j) {
        _lines.removeAt(j);
      }
      _lines[i] = line;
      return;
    }
  }

  // After the last definition of the section's last block.
  for (int i = _lines.size() - 1; i >= 0; --i) {
    if (_lines[i].section != section) {
      continue;
    }
    int insertAt = i + 1;
    // Keep trailing blank lines and comments of the section after the
    // inserted key.
    while (insertAt > 0 && !_lines[insertAt - 1].header &&
           _lines[insertAt - 1].key.isEmpty() &&
           !_lines[insertAt - 1].continuation) {
      --insertAt;
    }
    _lines.insert(insertAt, line);
    return;
  }

  if (!_lines.isEmpty() && !_lines.last().text.trimmed().isEmpty()) {
    Line blank;
    blank.section = _lines.last().section;
    _lines.append(blank);
  }
  Line header;
  header.header = true;
  header.section = section;
  header.text = QStringLiteral("[%1]").arg(section);
  _lines.append(header);
  _lines.append(line);
}

void HgRc::remove(const QString &section, const QString &key) {
  for (int i = _lines.size() - 1; i >= 0; --i) {
    if (!_lines[i].continuation && _lines[i].section == section &&
        _lines[i].key == key) {
      const int end = definitionEnd(i);
      for (int j = end - 1; j >= i; --j) {
        _lines.removeAt(j);
      }
    }
  }
}

void HgRc::removeSection(const QString &section) {
  for (int i = _lines.size() - 1; i >= 0; --i) {
    if (_lines[i].section == section &&
        (_lines[i].header || !_lines[i].key.isEmpty() ||
         _lines[i].continuation)) {
      _lines.removeAt(i);
    }
  }
}

} // namespace HgRepo::Private
