// MIT License
// Copyright (c) 2026 Sourcerer

// Helpers shared by the SvnRepo test suites: the svn, svnadmin and svnlook
// executables as oracles and for setup, file access, error checks and an
// isolated Subversion configuration. See docs/SVN_TEST_PLAN.md, section 2.

#pragma once

#include <SvnRepo.h>

#include <ConfigKey.h>
#include <ConfigManager.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QProcess>
#include <QProcessEnvironment>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>
#include <QtTest>

#include <algorithm>

namespace SvnTest {

inline const QString kAuthor = QStringLiteral("Tester");
inline const QString kEmail = QStringLiteral("tester@example.com");
// The changelist SvnRepo stages files in (SvnRepoPrivate.h).
inline const QString kStagingChangelist = QStringLiteral("sourcerer-staged");

// Asserts that the last SvnRepo call failed with kind and said why. Call
// resetLastError() before the failing call, so a stale error can't satisfy it.
#define EXPECT_SVN_ERROR(kind)                                                 \
  do {                                                                         \
    QCOMPARE(SvnTest::lastError(), kind);                                      \
    QVERIFY2(!SvnTest::lastErrorMessage().isEmpty(),                           \
             "the error has no message");                                      \
  } while (false)

inline bool writeFile(const QString &path, const QByteArray &content) {
  QDir().mkpath(QFileInfo(path).absolutePath());
  QFile file(path);
  return file.open(QIODevice::WriteOnly | QIODevice::Truncate) &&
         file.write(content) == content.size();
}

inline QByteArray readFile(const QString &path) {
  QFile file(path);
  return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

inline QString lastErrorMessage() {
  return SvnRepo::getLastError().errorMessage;
}

inline SvnRepo::SvnError lastError() { return SvnRepo::getLastError().error; }

// Successful calls don't clear the last error.
inline void resetLastError() { SvnRepo::Testing::resetLastError(); }

// Non-context lines of a delta as origin + content, e.g. "-old", "+new".
inline QStringList changedLines(const SvnRepo::FilesDiffDelta &delta) {
  QStringList lines;
  for (const SvnRepo::FilesDiffHunk &hunk : delta.hunks) {
    for (const SvnRepo::FilesDiffLine &line : hunk.lines) {
      if (line.origin == QLatin1Char('+') || line.origin == QLatin1Char('-')) {
        lines += line.origin + line.content;
      }
    }
  }
  return lines;
}

// Makes every file below `dir` writable. Subversion keeps its pristine copies
// read-only and QTemporaryDir can't remove them on Windows otherwise.
inline void makeWritable(const QString &dir) {
  QDirIterator it(dir, QDir::Files | QDir::Hidden | QDir::System,
                  QDirIterator::Subdirectories);
  while (it.hasNext()) {
    QFile::setPermissions(it.next(), QFile::ReadOwner | QFile::WriteOwner);
  }
}

// file:/// URL of a local path, as svn expects it.
inline QString fileUrl(const QString &path) {
  return QUrl::fromLocalFile(QDir::cleanPath(QDir::fromNativeSeparators(path)))
      .toString(QUrl::FullyEncoded);
}

// --- Command line tools -----------------------------------------------------

// Full path of a Subversion tool: PATH first, then the TortoiseSVN command
// line tools.
inline QString findTool(const QString &name) {
  QString path = QStandardPaths::findExecutable(name);
#ifdef Q_OS_WIN
  if (path.isEmpty()) {
    path = QStandardPaths::findExecutable(
        name, {QStringLiteral("C:/Program Files/TortoiseSVN/bin")});
  }
#endif
  return path;
}

inline bool toolsAvailable() {
  return !findTool(QStringLiteral("svn")).isEmpty() &&
         !findTool(QStringLiteral("svnadmin")).isEmpty() &&
         !findTool(QStringLiteral("svnlook")).isEmpty();
}

// The configuration area the svn tool and the library under test read. Set by
// the fixture to an empty directory in the test's temporary directory.
inline QString &configDir() {
  static QString dir;
  return dir;
}

inline void setConfigDir(const QString &dir) {
  QDir().mkpath(dir);
  configDir() = dir;
  SvnRepo::Testing::setConfigDirectory(dir);
}

enum class Output {
  Text, // CRLF line endings, which the Windows tools write, become LF
  Raw,  // the bytes as written
};

// Runs a tool in `dir`, feeding it `input`, and returns its standard output.
// A failure is always logged with the tool's message (QtTest's report shows
// it); `ok` lets the caller decide whether it fails the test.
inline QByteArray runTool(const QString &tool, const QString &dir,
                          const QStringList &args, bool *ok = nullptr,
                          Output output = Output::Text,
                          const QByteArray &input = QByteArray()) {
  QProcess process;
  if (!dir.isEmpty()) {
    process.setWorkingDirectory(dir);
  }
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
  env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
  env.remove(QStringLiteral("SVN_EDITOR"));
  env.remove(QStringLiteral("SVN_MERGE"));
  // The 1.14 client's x-shelve commands.
  env.insert(QStringLiteral("SVN_EXPERIMENTAL_COMMANDS"),
             QStringLiteral("shelf3"));
  process.setProcessEnvironment(env);
  // A tool that couldn't load (0xC000007B, 0xC0000135, 0xC0000142: seen with
  // the CLI tools mounted into the build container under load) never ran, so
  // it is started again.
  bool loaded = false;
  for (int attempt = 0; attempt < 3 && !loaded; ++attempt) {
    process.start(findTool(tool), args);
    if (process.waitForStarted(60 * 1000)) {
      if (!input.isEmpty()) {
        process.write(input);
      }
      process.closeWriteChannel();
    }
    process.waitForFinished(60 * 1000);
    const quint32 code = static_cast<quint32>(process.exitCode());
    loaded = code != 0xC000007Bu && code != 0xC0000135u && code != 0xC0000142u;
  }
  const bool success = process.state() == QProcess::NotRunning &&
                       process.exitStatus() == QProcess::NormalExit &&
                       process.exitCode() == 0;
  if (ok) {
    *ok = success;
  }
  if (!success) {
    qWarning() << tool << args << "failed in" << dir << ":"
               << process.exitCode() << process.readAllStandardError();
  }
  QByteArray result = process.readAllStandardOutput();
  if (output == Output::Text) {
    result.replace("\r\n", "\n");
  }
  return result;
}

// svn, always non-interactive, with the isolated configuration area and the
// test author.
inline QString svnIn(const QString &dir, const QStringList &args,
                     bool *ok = nullptr) {
  QStringList fullArgs = args;
  fullArgs << QStringLiteral("--non-interactive");
  if (!configDir().isEmpty()) {
    fullArgs << QStringLiteral("--config-dir") << configDir();
  }
  if (!args.isEmpty() && (args.first() == QLatin1String("commit") ||
                          args.first() == QLatin1String("ci") ||
                          args.first() == QLatin1String("mkdir") ||
                          args.first() == QLatin1String("copy") ||
                          args.first() == QLatin1String("cp") ||
                          args.first() == QLatin1String("move") ||
                          args.first() == QLatin1String("mv") ||
                          args.first() == QLatin1String("delete") ||
                          args.first() == QLatin1String("rm") ||
                          args.first() == QLatin1String("import") ||
                          args.first() == QLatin1String("propset") ||
                          args.first() == QLatin1String("ps"))) {
    fullArgs << QStringLiteral("--username") << kAuthor;
  }
  return QString::fromUtf8(runTool(QStringLiteral("svn"), dir, fullArgs, ok))
      .trimmed();
}

inline QString svn(const QStringList &args, bool *ok = nullptr) {
  return svnIn(QString(), args, ok);
}

// svnadmin and svnlook only read repositories; they take neither
// --non-interactive nor --config-dir.
inline QString svnadmin(const QStringList &args, bool *ok = nullptr) {
  return QString::fromUtf8(
             runTool(QStringLiteral("svnadmin"), QString(), args, ok))
      .trimmed();
}

inline QString svnlook(const QStringList &args, bool *ok = nullptr) {
  return QString::fromUtf8(
             runTool(QStringLiteral("svnlook"), QString(), args, ok))
      .trimmed();
}

// --- Oracles ----------------------------------------------------------------

struct StatusEntry {
  QString item;  // unversioned, added, modified, deleted, missing, ...
  QString props; // none, normal, modified, conflicted
  QString changelist;
  qint64 revision = -1;
  bool copied = false;
  bool treeConflicted = false;
};

// Paths relative to the working copy root with forward slashes; "." is the
// root itself.
using StatusMap = QMap<QString, StatusEntry>;

// `svn status --xml` of the working copy, with every versioned path when
// verbose.
inline StatusMap svnStatusXml(const QString &wc, bool verbose = false) {
  QStringList args{QStringLiteral("status"), QStringLiteral("--xml")};
  if (verbose) {
    args << QStringLiteral("-v");
  }
  args << QStringLiteral(".");
  QStringList fullArgs = args;
  fullArgs << QStringLiteral("--non-interactive");
  if (!configDir().isEmpty()) {
    fullArgs << QStringLiteral("--config-dir") << configDir();
  }
  const QByteArray xml = runTool(QStringLiteral("svn"), wc, fullArgs);

  StatusMap result;
  QXmlStreamReader reader(xml);
  QString changelist;
  QString path;
  while (!reader.atEnd()) {
    reader.readNext();
    if (reader.isStartElement()) {
      if (reader.name() == QLatin1String("changelist")) {
        changelist = reader.attributes().value(QLatin1String("name")).toString();
      } else if (reader.name() == QLatin1String("entry")) {
        path = QDir::fromNativeSeparators(
            reader.attributes().value(QLatin1String("path")).toString());
      } else if (reader.name() == QLatin1String("wc-status")) {
        const QXmlStreamAttributes attributes = reader.attributes();
        StatusEntry entry;
        entry.item = attributes.value(QLatin1String("item")).toString();
        entry.props = attributes.value(QLatin1String("props")).toString();
        entry.changelist = changelist;
        entry.revision =
            attributes.hasAttribute(QLatin1String("revision"))
                ? attributes.value(QLatin1String("revision")).toLongLong()
                : -1;
        entry.copied = attributes.value(QLatin1String("copied")) ==
                       QLatin1String("true");
        entry.treeConflicted =
            attributes.value(QLatin1String("tree-conflicted")) ==
            QLatin1String("true");
        result.insert(path, entry);
      }
    } else if (reader.isEndElement() &&
               reader.name() == QLatin1String("changelist")) {
      changelist.clear();
    }
  }
  if (reader.hasError()) {
    qWarning() << "svn status --xml:" << reader.errorString();
  }
  return result;
}

// Paths of the oracle's status in the changelist.
inline QStringList changelistMembers(const StatusMap &status,
                                     const QString &changelist =
                                         kStagingChangelist) {
  QStringList paths;
  for (auto it = status.cbegin(); it != status.cend(); ++it) {
    if (it->changelist == changelist) {
      paths += it.key();
    }
  }
  return paths;
}

// One field of `svn info`, e.g. "revision", "url", "last-changed-revision".
inline QString svnInfo(const QString &target, const QString &item,
                       const QString &revision = QString()) {
  QStringList args{QStringLiteral("info"), QStringLiteral("--show-item"),
                   item};
  if (!revision.isEmpty()) {
    args << QStringLiteral("-r") << revision;
  }
  args << target;
  bool ok = false;
  const QString value = svn(args, &ok);
  return ok ? value : QString();
}

struct LogEntry {
  bool found = false;
  QString author;
  QString date;
  QString message;
};

// `svn log --xml` of one revision of a URL or working copy path.
inline LogEntry svnLogXml(const QString &target, const QString &revision) {
  QStringList args{QStringLiteral("log"), QStringLiteral("--xml"),
                   QStringLiteral("-r"), revision, target,
                   QStringLiteral("--non-interactive")};
  if (!configDir().isEmpty()) {
    args << QStringLiteral("--config-dir") << configDir();
  }
  const QByteArray xml = runTool(QStringLiteral("svn"), QString(), args);

  LogEntry entry;
  QXmlStreamReader reader(xml);
  while (!reader.atEnd()) {
    reader.readNext();
    if (!reader.isStartElement()) {
      continue;
    }
    if (reader.name() == QLatin1String("logentry")) {
      entry.found = true;
    } else if (reader.name() == QLatin1String("author")) {
      entry.author = reader.readElementText();
    } else if (reader.name() == QLatin1String("date")) {
      entry.date = reader.readElementText();
    } else if (reader.name() == QLatin1String("msg")) {
      entry.message = reader.readElementText();
    }
  }
  return entry;
}

// Value of a versioned property, empty when it isn't set.
inline QString svnPropget(const QString &name, const QString &target) {
  bool ok = false;
  const QString value =
      svn({QStringLiteral("propget"), QStringLiteral("--no-newline"), name,
           target},
          &ok);
  return ok ? value : QString();
}

// The paths a revision changed, as sorted "<action> <path>" lines: "A
// trunk/a.txt" added, "D ..." deleted, "R ..." replaced, "U ..." content
// changed, "_U ..." properties changed, "UU ..." both, like svnlook changed.
// Read from `svn log -v --xml`, because svnlook writes paths in the
// console's code page.
inline QStringList changedPaths(const QString &repoDir,
                                const QString &revision) {
  QStringList args{QStringLiteral("log"), QStringLiteral("--xml"),
                   QStringLiteral("-v"), QStringLiteral("-r"), revision,
                   fileUrl(repoDir), QStringLiteral("--non-interactive")};
  if (!configDir().isEmpty()) {
    args << QStringLiteral("--config-dir") << configDir();
  }
  const QByteArray xml = runTool(QStringLiteral("svn"), QString(), args);

  QStringList lines;
  QXmlStreamReader reader(xml);
  while (!reader.atEnd()) {
    reader.readNext();
    if (!reader.isStartElement() || reader.name() != QLatin1String("path")) {
      continue;
    }
    const QXmlStreamAttributes attributes = reader.attributes();
    const QString action = attributes.value(QLatin1String("action")).toString();
    const bool text =
        attributes.value(QLatin1String("text-mods")) == QLatin1String("true");
    const bool props =
        attributes.value(QLatin1String("prop-mods")) == QLatin1String("true");
    QString path = reader.readElementText();
    if (path.startsWith(QLatin1Char('/'))) {
      path.remove(0, 1);
    }
    QString code = action;
    if (action == QLatin1String("M")) {
      code = QString(text ? QLatin1Char('U') : QLatin1Char('_')) +
             (props ? QStringLiteral("U") : QString());
    }
    lines += code + QLatin1Char(' ') + path;
  }
  std::sort(lines.begin(), lines.end());
  return lines;
}

inline QString svnlookAuthor(const QString &repoDir, const QString &revision) {
  return svnlook({QStringLiteral("author"), QStringLiteral("-r"), revision,
                  repoDir});
}

// The log message; svnlook adds one newline, which is removed. Line endings
// come back as LF even where the stored message has CRLF; svnlookRevprop
// returns the stored bytes.
inline QByteArray svnlookLog(const QString &repoDir, const QString &revision) {
  QByteArray log = runTool(QStringLiteral("svnlook"), QString(),
                           {QStringLiteral("log"), QStringLiteral("-r"),
                            revision, repoDir});
  if (log.endsWith('\n')) {
    log.chop(1);
  }
  return log;
}

inline QString svnlookDate(const QString &repoDir, const QString &revision) {
  return svnlook({QStringLiteral("date"), QStringLiteral("-r"), revision,
                  repoDir});
}

// A revision property exactly as stored (svnlook writes it unchanged), e.g.
// svn:log with CRLF or non-UTF-8 bytes. `found` is false when it isn't set.
inline QByteArray svnlookRevprop(const QString &repoDir,
                                 const QString &revision, const QString &name,
                                 bool *found = nullptr) {
  bool ok = false;
  const QByteArray value =
      runTool(QStringLiteral("svnlook"), QString(),
              {QStringLiteral("propget"), QStringLiteral("--revprop"),
               QStringLiteral("-r"), revision, repoDir, name},
              &ok, Output::Raw);
  if (found) {
    *found = ok;
  }
  return ok ? value : QByteArray();
}

// The youngest revision of the repository, read without the code under test.
inline QString head(const QString &repoDir) {
  return svnlook({QStringLiteral("youngest"), repoDir});
}

// --- Setup ------------------------------------------------------------------

enum class Layout {
  Standard,    // /trunk, /branches, /tags
  ProjectRoot, // /proj/trunk, /proj/branches, /proj/tags
  Flat,        // nothing, no trunk
};

// Creates a repository at `repoDir` with the layout in r1 (none for Flat).
// Returns its file:/// URL, empty on failure.
inline QString makeRepo(const QString &repoDir, Layout layout) {
  bool ok = false;
  svnadmin({QStringLiteral("create"), repoDir}, &ok);
  if (!ok) {
    qWarning() << "svnadmin create failed for" << repoDir;
    return {};
  }

  const QString url = fileUrl(repoDir);
  QString root = url;
  switch (layout) {
  case Layout::Flat:
    return url;
  case Layout::ProjectRoot:
    root += QStringLiteral("/proj");
    break;
  case Layout::Standard:
    break;
  }
  svn({QStringLiteral("mkdir"), QStringLiteral("--parents"),
       QStringLiteral("-m"), QStringLiteral("Layout"),
       root + QStringLiteral("/trunk"), root + QStringLiteral("/branches"),
       root + QStringLiteral("/tags")},
      &ok);
  return ok ? url : QString();
}

// Installs a pre-revprop-change hook that allows or refuses every revision
// property change. Without one, Subversion refuses them all.
inline bool addPreRevpropChangeHook(const QString &repoDir, bool allow) {
#ifdef Q_OS_WIN
  const QString path = repoDir + QStringLiteral("/hooks/pre-revprop-change.bat");
  const QByteArray script =
      allow ? QByteArray("@exit 0\r\n")
            : QByteArray("@echo Revision properties are locked. 1>&2\r\n"
                         "@exit 1\r\n");
#else
  const QString path = repoDir + QStringLiteral("/hooks/pre-revprop-change");
  const QByteArray script =
      allow ? QByteArray("#!/bin/sh\nexit 0\n")
            : QByteArray("#!/bin/sh\n"
                         "echo 'Revision properties are locked.' >&2\n"
                         "exit 1\n");
#endif
  if (!writeFile(path, script)) {
    return false;
  }
  return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner |
                                         QFile::ExeOwner);
}

// Another checkout of the same repository, the counterpart of a second clone.
inline bool secondWc(const QString &url, const QString &dir) {
  // Under load the Windows CLI has failed a checkout now and then (seen once
  // in a parallel Debug ctest run). The failure is logged with the CLI's
  // message, and a second checkout decides: it completes a partial one and
  // only updates a complete one.
  svn({QStringLiteral("checkout"), url, dir});
  bool ok = false;
  svn({QStringLiteral("checkout"), url, dir}, &ok);
  return ok;
}

// Adds every unversioned file and commits the working copy with the CLI. For
// setup only; tests of the library commit with commitAll.
inline bool svnCommitAll(const QString &wc, const QString &message) {
  bool ok = false;
  svnIn(wc, {QStringLiteral("add"), QStringLiteral("--force"),
             QStringLiteral(".")},
        &ok);
  if (!ok) {
    return false;
  }
  svnIn(wc, {QStringLiteral("commit"), QStringLiteral("-m"), message,
             QStringLiteral(".")},
        &ok);
  return ok;
}

// Stages every change through the API under test and commits it. There is no
// update afterwards, so the working copy stays mixed-revision like it does for
// the user.
inline bool commitAll(const QString &wc, const QString &summary,
                      const QString &message = QString()) {
  const bool quit = false;
  const SvnRepo::SvnStatus status = SvnRepo::getStatus(wc, quit);
  const QStringList files = status.newFiles + status.modifiedFiles +
                            status.deletedFiles + status.typeChangedFiles +
                            status.renamedFiles;
  if (!files.isEmpty() && !SvnRepo::stageFiles(wc, files)) {
    return false;
  }
  return SvnRepo::commitStaged(wc, summary, message, kAuthor, kEmail, true,
                               false, false);
}

// --- Dump fixtures (SVN_TEST_PLAN 2.4) -----------------------------------------
//
// Repositories the library can't make: revisions without svn:author or
// svn:date, logs with CRLF or non-UTF-8 bytes, other layouts, replaced nodes.
// They are built here instead of being kept as .dump files, because a dump
// records byte lengths and must keep LF line endings, while the sources are
// kept with CRLF.

// Writes a dump stream (format 2) one revision and node at a time; the
// lengths are computed.
class DumpBuilder {
public:
  DumpBuilder() {
    _data = "SVN-fs-dump-format-version: 2\n\n"
            "UUID: 6c2f8a2e-0000-4000-8000-5ec0de5ec0de\n\n";
    revision(QByteArray(), kDate, QByteArray(), Props::DateOnly);
  }

  // A null author, date or log leaves that revision property out.
  DumpBuilder &revision(const QByteArray &author, const QByteArray &date,
                        const QByteArray &log) {
    return revision(author, date, log, Props::AsGiven);
  }

  // A revision with the usual properties.
  DumpBuilder &revision(const QByteArray &log) {
    return revision(QByteArray("Tester"), kDate, log);
  }

  DumpBuilder &addDir(const QByteArray &path) {
    return node(path, "dir", "add", QByteArray(), true);
  }

  DumpBuilder &addFile(const QByteArray &path, const QByteArray &content) {
    return node(path, "file", "add", content, true);
  }

  DumpBuilder &changeFile(const QByteArray &path, const QByteArray &content) {
    return node(path, "file", "change", content, false);
  }

  // Deletes the node and adds a new one in its place (R).
  DumpBuilder &replaceFile(const QByteArray &path, const QByteArray &content) {
    return node(path, "file", "replace", content, true);
  }

  DumpBuilder &deleteNode(const QByteArray &path) {
    _data += "Node-path: " + path + "\nNode-action: delete\n\n\n";
    return *this;
  }

  // A copy with history; kind is "dir" or "file".
  DumpBuilder &copy(const QByteArray &path, const QByteArray &kind,
                    const QByteArray &fromPath, int fromRevision) {
    _data += "Node-path: " + path + "\nNode-kind: " + kind +
             "\nNode-action: add\nNode-copyfrom-rev: " +
             QByteArray::number(fromRevision) +
             "\nNode-copyfrom-path: " + fromPath + "\n\n\n";
    return *this;
  }

  // Standard layout directories in one revision.
  DumpBuilder &layout(const QByteArray &root = QByteArray()) {
    const QByteArray prefix = root.isEmpty() ? root : root + '/';
    if (!root.isEmpty()) {
      addDir(root);
    }
    return addDir(prefix + "trunk")
        .addDir(prefix + "branches")
        .addDir(prefix + "tags");
  }

  QByteArray data() const { return _data; }

  static inline const QByteArray kDate = "2026-01-02T03:04:05.000000Z";

private:
  enum class Props { AsGiven, DateOnly };

  static QByteArray property(const QByteArray &name, const QByteArray &value) {
    return "K " + QByteArray::number(name.size()) + '\n' + name + "\nV " +
           QByteArray::number(value.size()) + '\n' + value + '\n';
  }

  DumpBuilder &revision(const QByteArray &author, const QByteArray &date,
                        const QByteArray &log, Props props) {
    QByteArray block;
    if (props == Props::AsGiven && !author.isNull()) {
      block += property("svn:author", author);
    }
    if (!date.isNull()) {
      block += property("svn:date", date);
    }
    if (props == Props::AsGiven && !log.isNull()) {
      block += property("svn:log", log);
    }
    block += "PROPS-END\n";
    _data += "Revision-number: " + QByteArray::number(_revision++) +
             "\nProp-content-length: " + QByteArray::number(block.size()) +
             "\nContent-length: " + QByteArray::number(block.size()) +
             "\n\n" + block + '\n';
    return *this;
  }

  DumpBuilder &node(const QByteArray &path, const QByteArray &kind,
                    const QByteArray &action, const QByteArray &content,
                    bool withProps) {
    const QByteArray props = withProps ? QByteArray("PROPS-END\n") : QByteArray();
    const bool withText = kind == "file";
    _data += "Node-path: " + path + "\nNode-kind: " + kind +
             "\nNode-action: " + action + '\n';
    if (withProps) {
      _data += "Prop-content-length: " + QByteArray::number(props.size()) + '\n';
    }
    if (withText) {
      _data +=
          "Text-content-length: " + QByteArray::number(content.size()) + '\n';
    }
    _data += "Content-length: " +
             QByteArray::number(props.size() +
                                (withText ? content.size() : 0)) +
             "\n\n" + props + (withText ? content : QByteArray()) + "\n\n";
    return *this;
  }

  QByteArray _data;
  int _revision = 0;
};

// Creates a repository at `repoDir` from a dump. Returns its file:/// URL,
// empty on failure.
inline QString loadDump(const QString &repoDir, const QByteArray &dump) {
  bool ok = false;
  svnadmin({QStringLiteral("create"), repoDir}, &ok);
  if (!ok) {
    qWarning() << "svnadmin create failed for" << repoDir;
    return {};
  }
  runTool(QStringLiteral("svnadmin"), QString(),
          {QStringLiteral("load"), QStringLiteral("--quiet"),
           QStringLiteral("--bypass-prop-validation"), repoDir},
          &ok, Output::Text, dump);
  return ok ? fileUrl(repoDir) : QString();
}

namespace Dumps {

// r1 layout and a.txt; r2 has no svn:author, r3 no svn:date, r4 an empty
// log, r5 a CRLF log, r6 a log that isn't UTF-8.
inline QByteArray revpropEdgeCases() {
  return DumpBuilder()
      .revision("Layout")
      .layout()
      .addFile("trunk/a.txt", "1\n")
      .revision(QByteArray(), DumpBuilder::kDate, "No author")
      .changeFile("trunk/a.txt", "2\n")
      .revision("Tester", QByteArray(), "No date")
      .changeFile("trunk/a.txt", "3\n")
      .revision("Tester", DumpBuilder::kDate, "")
      .changeFile("trunk/a.txt", "4\n")
      .revision("Tester", DumpBuilder::kDate, "Summary\r\n\r\nBody\r\n")
      .changeFile("trunk/a.txt", "5\n")
      .revision("Tester", DumpBuilder::kDate, "Caf\xe9 \xff")
      .changeFile("trunk/a.txt", "6\n")
      .data();
}

// r1 /proj/{trunk,branches,tags} and proj/trunk/a.txt.
inline QByteArray projectRootLayout() {
  return DumpBuilder()
      .revision("Layout")
      .layout("proj")
      .addFile("proj/trunk/a.txt", "a\n")
      .data();
}

// r1 /src/a.txt, no trunk.
inline QByteArray noTrunkLayout() {
  return DumpBuilder()
      .revision("Flat")
      .addDir("src")
      .addFile("src/a.txt", "a\n")
      .data();
}

// r1 layout and trunk/a.txt; r2 branches b1, r3 branch b2; r4 changes both
// branches at once; r5 creates tag v1 and touches nothing else.
inline QByteArray branchesAndTags() {
  return DumpBuilder()
      .revision("Layout")
      .layout()
      .addFile("trunk/a.txt", "a\n")
      .revision("Branch b1")
      .copy("branches/b1", "dir", "trunk", 1)
      .revision("Branch b2")
      .copy("branches/b2", "dir", "trunk", 1)
      .revision("Both branches")
      .changeFile("branches/b1/a.txt", "b1\n")
      .changeFile("branches/b2/a.txt", "b2\n")
      .revision("Tag v1")
      .copy("tags/v1", "dir", "trunk", 1)
      .data();
}

// r1 layout, trunk/a.txt and trunk/dir/x.txt; r2 replaces a.txt; r3 moves
// dir to moved (copy and delete).
inline QByteArray replacedAndMoved() {
  return DumpBuilder()
      .revision("Layout")
      .layout()
      .addFile("trunk/a.txt", "old\n")
      .addDir("trunk/dir")
      .addFile("trunk/dir/x.txt", "x\n")
      .revision("Replace a")
      .replaceFile("trunk/a.txt", "new\n")
      .revision("Move dir")
      .copy("trunk/moved", "dir", "trunk/dir", 1)
      .deleteNode("trunk/dir")
      .data();
}

} // namespace Dumps

// --- Configuration and credentials -------------------------------------------

// Sets a ConfigManager value for the lifetime of the object.
class ConfigOverride {
public:
  ConfigOverride(ConfigManager::ConfigKey key, qlonglong value)
      : _key(key), _kind(Kind::Long) {
    auto &config = ConfigManager::ConfigManager::getInstance();
    _previousLong = config.getLong(key);
    config.setLong(key, value);
  }
  ConfigOverride(ConfigManager::ConfigKey key, int value)
      : ConfigOverride(key, static_cast<qlonglong>(value)) {}
  ConfigOverride(ConfigManager::ConfigKey key, const char *value) = delete;
  ConfigOverride(ConfigManager::ConfigKey key, const QString &value)
      : _key(key), _kind(Kind::String) {
    auto &config = ConfigManager::ConfigManager::getInstance();
    _previousString = config.getString(key);
    config.setString(key, value);
  }
  ConfigOverride(ConfigManager::ConfigKey key, bool value)
      : _key(key), _kind(Kind::Boolean) {
    auto &config = ConfigManager::ConfigManager::getInstance();
    _previousBoolean = config.getBoolean(key);
    config.setBoolean(key, value);
  }
  ~ConfigOverride() {
    auto &config = ConfigManager::ConfigManager::getInstance();
    switch (_kind) {
    case Kind::Long:
      config.setLong(_key, _previousLong);
      break;
    case Kind::String:
      config.setString(_key, _previousString);
      break;
    case Kind::Boolean:
      config.setBoolean(_key, _previousBoolean);
      break;
    }
  }
  ConfigOverride(const ConfigOverride &) = delete;
  ConfigOverride &operator=(const ConfigOverride &) = delete;

private:
  enum class Kind { Long, String, Boolean };

  ConfigManager::ConfigKey _key;
  Kind _kind;
  qlonglong _previousLong = 0;
  QString _previousString;
  bool _previousBoolean = false;
};

// SvnRepo keeps credentials in GitRepo's GitCredentialStorage, whose master
// key lives in the OS keychain. Tests use a fixed one instead, so they don't
// add entries to the user's keychain.
inline void useTestCredentialsKey() {
  GitRepo::Testing::setCredentialsMasterKey(
      QByteArray("sourcerer-tests-credentials-key!"));
}

// --- Data tables ------------------------------------------------------------

// The shared edge-case file names (SVN_TEST_PLAN 3.0) as a "name" column.
inline void unusualNames() {
  QTest::addColumn<QString>("name");
  QTest::newRow("space") << QStringLiteral("a b.txt");
  QTest::newRow("space dir") << QStringLiteral("dir with space/x.txt");
  QTest::newRow("latin") << QString::fromUtf16(u"\u00fcn\u00ef.txt");
  QTest::newRow("cjk") << QString::fromUtf16(u"\u65e5\u672c.txt");
  QTest::newRow("emoji") << QString::fromUtf16(u"\U0001F600.txt");
  QTest::newRow("nfd") << QString::fromUtf16(u"u\u0308.txt");
  QTest::newRow("hash") << QStringLiteral("#hash.txt");
  QTest::newRow("percent") << QStringLiteral("100%.txt");
  QTest::newRow("plus") << QStringLiteral("a+b.txt");
  QTest::newRow("comma") << QStringLiteral("x,y.txt");
  QTest::newRow("semicolon") << QStringLiteral("a;b.txt");
  QTest::newRow("equals") << QStringLiteral("a=b.txt");
  QTest::newRow("apostrophe") << QStringLiteral("it's.txt");
  QTest::newRow("parentheses") << QStringLiteral("(1).txt");
  QTest::newRow("bang") << QStringLiteral("a!.txt");
  QTest::newRow("dollar") << QStringLiteral("$x.txt");
  QTest::newRow("ampersand") << QStringLiteral("a&b.txt");
  QTest::newRow("at") << QStringLiteral("a@b.txt");
  QTest::newRow("trailing at") << QStringLiteral("file@");
  QTest::newRow("leading at") << QStringLiteral("@leading.txt");
  QTest::newRow("brackets") << QStringLiteral("file[1].txt");
  QTest::newRow("leading dash") << QStringLiteral("-x.txt");
  QTest::newRow("leading dot") << QStringLiteral(".hidden");
  QTest::newRow("trunk dir") << QStringLiteral("trunk/x.txt");
  QTest::newRow("branches dir") << QStringLiteral("branches/x.txt");
  QTest::newRow("tags dir") << QStringLiteral("tags/x.txt");
#ifndef Q_OS_WIN
  QTest::newRow("question") << QStringLiteral("a?b");
  QTest::newRow("star") << QStringLiteral("a*b.txt");
#endif
}

} // namespace SvnTest
