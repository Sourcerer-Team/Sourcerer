#pragma once

// Reads and edits a Mercurial config file (.hg/hgrc) while keeping its
// comments, order and the lines it doesn't understand. Not part of the public
// API.

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>

namespace HgRepo::Private {

class HgRc {
public:
  explicit HgRc(const QString &fileName);

  // A missing file reads as empty.
  bool load();
  bool save();

  QString fileName() const { return _fileName; }
  QString errorMessage() const { return _errorMessage; }

  bool contains(const QString &section, const QString &key) const;
  QString value(const QString &section, const QString &key,
                const QString &defaultValue = QString()) const;
  // Keys and values of a section in file order, the last value of a key
  // repeated in the file wins.
  QList<QPair<QString, QString>> entries(const QString &section) const;

  // Replaces the last definition of key, or appends it to the last block of
  // the section, which is created at the end of the file when missing.
  void setValue(const QString &section, const QString &key,
                const QString &value);
  // Removes every definition of key in the section.
  void remove(const QString &section, const QString &key);
  // Removes every key of the section, and the section headers.
  void removeSection(const QString &section);

private:
  struct Line {
    QString text;
    // Section the line belongs to.
    QString section;
    // Key defined by the line, empty for other lines.
    QString key;
    QString value;
    bool header = false;
    // Indented line continuing the previous value.
    bool continuation = false;
  };

  void parse(const QString &content);
  // Index after the last line of the definition starting at index.
  int definitionEnd(int index) const;

  QString _fileName;
  QString _errorMessage;
  QList<Line> _lines;
};

} // namespace HgRepo::Private
