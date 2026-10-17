#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QByteArray>
#include <QDir>
#include <QFile>
#include <QSet>
#include <QTextStream>
#include <QVector>

#define THREAD_ID "GitRepo"

// Diffs, patches and blame.
namespace GitRepo {

using namespace Private;

static QString blameSignatureToString(const git_signature *signature) {
  if (!signature) {
    return QStringLiteral("Unknown");
  }

  const QString name =
      signature->name ? QString::fromUtf8(signature->name) : QString();
  const QString trimmedName = name.trimmed();

  return trimmedName.isEmpty() ? QStringLiteral("Unknown") : trimmedName;
}

static QString blameSignatureEmail(const git_signature *signature) {
  if (!signature || !signature->email) {
    return {};
  }

  return QString::fromUtf8(signature->email).trimmed();
}

static QString blameCommitToString(const git_oid *oid) {
  if (!oid) {
    return QString(GIT_OID_HEXSZ, QChar('0'));
  }

  char buffer[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(buffer, sizeof(buffer), oid);
  return QString::fromLatin1(buffer);
}

static BlameReport createBlameReport(git_repository *repo,
                                     const QStringList &files) {
  BlameReport report;

  git_blame_options blameOptions = GIT_BLAME_OPTIONS_INIT;
  blameOptions.flags = GIT_BLAME_USE_MAILMAP;

  const QString repoWorkdir =
      repo && git_repository_workdir(repo)
          ? QString::fromUtf8(git_repository_workdir(repo))
          : QString();

  for (const QString &file : files) {
    const QString normalizedFile = QDir::fromNativeSeparators(file);
    BlameFileInfo fileInfo;
    fileInfo.filePath = normalizedFile;

    const QByteArray utf8Path = normalizedFile.toUtf8();
    git_blame *blame = nullptr;
    const int error =
        git_blame_file(&blame, repo, utf8Path.constData(), &blameOptions);
    if (error) {
      const git_error *gitError = git_error_last();
      fileInfo.errorMessage = gitError && gitError->message
                                  ? QString::fromUtf8(gitError->message)
                                  : QStringLiteral("Unknown error");
      report.fileInfos += fileInfo;
      continue;
    }

    // Blame the working copy content, so that uncommitted lines get a zero
    // commit id. Filters (e.g. autocrlf) are applied as they would be on
    // commit, so that line endings alone don't count as changes.
    const QString filePath = QDir(repoWorkdir).filePath(normalizedFile);
    QByteArray workdirContent;
    bool workdirRead = false;
    git_filter_list *filters = nullptr;
    if (git_filter_list_load(&filters, repo, nullptr, utf8Path.constData(),
                             GIT_FILTER_TO_ODB, GIT_FILTER_DEFAULT) == 0 &&
        filters) {
      git_buf filtered = GIT_BUF_INIT;
      if (git_filter_list_apply_to_file(&filtered, filters, repo,
                                        utf8Path.constData()) == 0) {
        workdirContent = QByteArray(filtered.ptr, filtered.size);
        workdirRead = true;
      }
      git_buf_dispose(&filtered);
    }
    git_filter_list_free(filters);
    giterr_clear();

    if (!workdirRead) {
      QFile fileObj(filePath);
      if (fileObj.open(QIODevice::ReadOnly)) {
        workdirContent = fileObj.readAll();
        workdirRead = true;
        fileObj.close();
      }
    }

    if (workdirRead && !workdirContent.isEmpty()) {
      git_blame *bufferBlame = nullptr;
      if (git_blame_buffer(&bufferBlame, blame, workdirContent.constData(),
                           static_cast<size_t>(workdirContent.size())) == 0) {
        git_blame_free(blame);
        blame = bufferBlame;
      } else {
        giterr_clear();
      }
    }

    const QString fileContent =
        QString::fromUtf8(workdirContent)
            .replace(QStringLiteral("\r\n"), QStringLiteral("\n"));

    QStringList fileLines;
    if (!fileContent.isEmpty()) {
      fileLines = fileContent.split('\n', Qt::KeepEmptyParts);
      if (fileContent.endsWith('\n') && !fileLines.isEmpty() &&
          fileLines.last().isEmpty()) {
        fileLines.removeLast();
      }
    }

    // libgit2 1.9 git_blame_linecount() counts an end sentinel, and it is not
    // updated by git_blame_buffer(), so count the lines of the content.
    const size_t lineCount = workdirRead
                                 ? static_cast<size_t>(fileLines.size())
                                 : git_blame_linecount(blame);
    for (size_t lineNumber = 1; lineNumber <= lineCount; ++lineNumber) {
      const git_blame_hunk *hunk = git_blame_hunk_byline(blame, lineNumber);
      BlameLineInfo lineInfo;
      lineInfo.filePath = normalizedFile;
      lineInfo.lineNumber = static_cast<qint64>(lineNumber);
      lineInfo.content = lineNumber - 1 < static_cast<size_t>(fileLines.size())
                             ? fileLines.at(static_cast<int>(lineNumber - 1))
                             : QString();
      lineInfo.commit =
          blameCommitToString(hunk ? &hunk->final_commit_id : nullptr);
      lineInfo.author =
          blameSignatureToString(hunk ? hunk->final_signature : nullptr);
      lineInfo.email =
          blameSignatureEmail(hunk ? hunk->final_signature : nullptr);
      lineInfo.summary =
          hunk && hunk->summary ? QString::fromUtf8(hunk->summary) : QString();

      fileInfo.lineInfos += lineInfo;
    }

    git_blame_free(blame);
    report.fileInfos += fileInfo;
  }

  report.valid = true;
  return report;
}

static bool readTreeFileContent(git_repository *repo, git_tree *tree,
                                const QString &filePath,
                                const qint64 maxBlobSize, QString *content) {
  if (!repo || !tree || !content) {
    return false;
  }

  git_tree_entry *entry = nullptr;
  const int error =
      git_tree_entry_bypath(&entry, tree, filePath.toStdString().c_str());
  if (error) {
    return false;
  }

  const git_oid *fullOid = git_tree_entry_id(entry);
  if (!fullOid) {
    git_tree_entry_free(entry);
    return false;
  }

  git_blob *blob = nullptr;
  int blobError = git_blob_lookup(&blob, repo, fullOid);
  if (blobError) {
    git_tree_entry_free(entry);
    return false;
  }

  const size_t blobSize = git_blob_rawsize(blob);
  if (blobSize > static_cast<size_t>(maxBlobSize)) {
    *content =
        QString("[[FILE SIZE EXCEEDS LIMIT OF %1 BYTES]]").arg(maxBlobSize);
    git_blob_free(blob);
    git_tree_entry_free(entry);
    return true;
  }

  const char *blobContent =
      static_cast<const char *>(git_blob_rawcontent(blob));
  *content = QString::fromUtf8(blobContent, blobSize);

  git_blob_free(blob);
  git_tree_entry_free(entry);
  return true;
}

static QString createFullPatchFromContent(const QString &filePath,
                                          const QString &fileContent,
                                          bool newFile,
                                          const QString &indexLine) {
  LOG_PERF(0, "Enter");

  const QString convertedContent = QString(fileContent).replace("\r\n", "\n");
  QStringList lines = convertedContent.split('\n');
  if (!convertedContent.isEmpty() && convertedContent.endsWith('\n')) {
    lines.removeLast();
  }
  const int lineCount = lines.size();

  QString patch;
  QTextStream stream(&patch);

  if (!indexLine.isEmpty()) {
    stream << indexLine << "\n";
  }

  if (newFile) {
    stream << "--- /dev/null\n";
  } else {
    stream << "--- a/" << filePath << "\n";
  }

  stream << "+++ b/" << filePath << "\n";
  stream << "@@ -1,0 +1," << lineCount << " @@\n";

  for (const QString &line : lines) {
    stream << "+" << line << "\n";
  }

  LOG_PERF(0, "Exit");

  return patch;
}

static QString createIndexLineFromContent(const QString &oldFileContent,
                                          const QString &newFileContent) {

  if (oldFileContent.isEmpty() && newFileContent.isEmpty()) {
    return {};
  }

  if (oldFileContent.isEmpty()) {
    const QString normalizedNewContent =
        QString(newFileContent).replace("\r\n", "\n");

    git_oid newOid;

    const QByteArray newBytes = normalizedNewContent.toUtf8();

    git_odb_hash(&newOid, newBytes.constData(),
                 static_cast<size_t>(newBytes.size()), GIT_OBJECT_BLOB);

    char newOidStr[GIT_OID_HEXSZ + 1] = {0};

    git_oid_tostr(newOidStr, sizeof(newOidStr), &newOid);

    return QString("index 0000000..%2")
        .arg(QString::fromUtf8(newOidStr).left(7));
  }

  if (newFileContent.isEmpty()) {
    const QString normalizedOldContent =
        QString(oldFileContent).replace("\r\n", "\n");

    git_oid oldOid;

    const QByteArray oldBytes = normalizedOldContent.toUtf8();

    git_odb_hash(&oldOid, oldBytes.constData(),
                 static_cast<size_t>(oldBytes.size()), GIT_OBJECT_BLOB);

    char oldOidStr[GIT_OID_HEXSZ + 1] = {0};

    git_oid_tostr(oldOidStr, sizeof(oldOidStr), &oldOid);

    return QString("index %1..0000000")
        .arg(QString::fromUtf8(oldOidStr).left(7));
  }

  const QString normalizedOldContent =
      QString(oldFileContent).replace("\r\n", "\n");
  const QString normalizedNewContent =
      QString(newFileContent).replace("\r\n", "\n");

  git_oid oldOid;
  git_oid newOid;

  const QByteArray oldBytes = normalizedOldContent.toUtf8();
  const QByteArray newBytes = normalizedNewContent.toUtf8();

  git_odb_hash(&oldOid, oldBytes.constData(),
               static_cast<size_t>(oldBytes.size()), GIT_OBJECT_BLOB);
  git_odb_hash(&newOid, newBytes.constData(),
               static_cast<size_t>(newBytes.size()), GIT_OBJECT_BLOB);

  char oldOidStr[GIT_OID_HEXSZ + 1] = {0};
  char newOidStr[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oldOidStr, sizeof(oldOidStr), &oldOid);
  git_oid_tostr(newOidStr, sizeof(newOidStr), &newOid);

  return QString("index %1..%2")
      .arg(QString::fromUtf8(oldOidStr).left(7),
           QString::fromUtf8(newOidStr).left(7));
}

static FilesDiffDelta
createFullPatchDeltasFromContent(const QString &filePath,
                                 const QString &fileContent) {
  LOG_PERF(0, "Enter");

  const QString convertedContent = QString(fileContent).replace("\r\n", "\n");
  QStringList lines = convertedContent.split('\n');
  if (!convertedContent.isEmpty() && convertedContent.endsWith('\n')) {
    lines.removeLast();
  }

  FilesDiffDelta delta;
  delta.oldFilePath = filePath;
  delta.newFilePath = filePath;

  FilesDiffHunk hunk;
  hunk.oldStart = 1;
  hunk.newStart = 1;
  hunk.oldLines = 0;
  hunk.newLines = lines.size();

  for (const QString &line : lines) {
    FilesDiffLine diffLine;
    diffLine.origin = '+';
    diffLine.content = line;
    hunk.lines.append(diffLine);
  }

  delta.hunks.append(hunk);

  LOG_PERF(0, "Exit");

  return delta;
}

FilesDiff diffFiles(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_object *headCommit = nullptr;
  git_commit *commit = nullptr;
  git_tree *headTree = nullptr;

  int error = git_revparse_single(&headCommit, repo, "HEAD");
  if (error) {
    setError(GitError::DiffFilesFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_commit_lookup(&commit, repo, git_object_id(headCommit));
  if (error) {
    setError(GitError::DiffFilesFailedToGetHeadObjectId, error);
    git_object_free(headCommit);
    LOG_PERF(0, "Exit");
    return {};
  }

  error = git_commit_tree(&headTree, commit);
  if (error) {
    setError(GitError::DiffFilesFailedToGetCommitTree, error);
    git_commit_free(commit);
    git_object_free(headCommit);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit_free(commit);
  git_object_free(headCommit);

  git_strarray arr = qstringlist_to_git_strarray(files);

  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.pathspec = arr;

  git_diff *diff = nullptr;
  error = git_diff_tree_to_workdir(&diff, repo, headTree, &opts);
  if (error) {
    setError(GitError::DiffFilesFailedToDiffTreeAgainstWorkdir, error);
    git_tree_free(headTree);
    git_strarray_destroy(arr);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree_free(headTree);
  git_strarray_destroy(arr);

  QString diffOutput;

  QVector<FilesDiffDelta> filesDeltas;

  size_t num_deltas = git_diff_num_deltas(diff);
  for (size_t i = 0; i < num_deltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(diff, i);
    diffOutput += QString("diff --git a/%1 b/%2\n")
                      .arg(delta->old_file.path, delta->new_file.path);

    FilesDiffDelta filesDiffDelta;
    filesDiffDelta.oldFilePath = delta->old_file.path;
    filesDiffDelta.newFilePath = delta->new_file.path;

    git_patch *patch;
    error = git_patch_from_diff(&patch, diff, i);
    if (error) {
      setError(GitError::DiffFilesFailedToCreatePatchFromDiff, error);
      continue;
    }

    const size_t num_hunks = git_patch_num_hunks(patch);
    for (size_t h = 0; h < num_hunks; ++h) {
      const git_diff_hunk *hunk;
      error = git_patch_get_hunk(&hunk, NULL, patch, h);
      if (error) {
        setError(GitError::DiffFilesFailedToReadPatchHunk, error);
        continue;
      }

      diffOutput += QString("@@ -%1,%2 +%3,%4 @@\n")
                        .arg(hunk->old_start)
                        .arg(hunk->old_lines)
                        .arg(hunk->new_start)
                        .arg(hunk->new_lines);

      FilesDiffHunk filesDiffHunk;
      filesDiffHunk.oldStart = hunk->old_start;
      filesDiffHunk.oldLines = hunk->old_lines;
      filesDiffHunk.newStart = hunk->new_start;
      filesDiffHunk.newLines = hunk->new_lines;

      size_t num_lines = git_patch_num_lines_in_hunk(patch, h);
      for (size_t l = 0; l < num_lines; ++l) {
        const git_diff_line *line;
        error = git_patch_get_line_in_hunk(&line, patch, h, l);
        if (error) {
          setError(GitError::DiffFilesFailedToReadPatchLine, error);
          continue;
        }

        const char *start = line->content;
        const char *end = start + line->content_len;
        const char *newline =
            static_cast<const char *>(memchr(start, '\n', end - start));
        if (!newline) {
          newline = end;
        }

        const QString lineContent = QString::fromUtf8(start, newline - start);

        diffOutput += QString("%1%2\n").arg(line->origin, lineContent);

        FilesDiffLine filesDiffLine;
        filesDiffLine.origin = line->origin;
        filesDiffLine.content = lineContent;

        filesDiffHunk.lines += filesDiffLine;
      }

      filesDiffDelta.hunks += filesDiffHunk;
    }

    git_patch_free(patch);

    filesDeltas += filesDiffDelta;
  }

  git_diff_free(diff);

  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    setError(GitError::DiffFilesFailedToGetRepositoryHead, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  headCommit = nullptr;

  error = git_reference_peel(&headCommit, headRef, GIT_OBJECT_COMMIT);
  if (error) {
    setError(GitError::DiffFilesFailedToPeelReference, error);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error =
      git_commit_tree(&tree, reinterpret_cast<const git_commit *>(headCommit));
  if (error) {
    setError(GitError::DiffFilesFailedToReadHeadCommit, error);
    git_reference_free(headRef);
    git_commit_free(reinterpret_cast<git_commit *>(headCommit));
    LOG_PERF(0, "Exit");
    return {};
  }

  const ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong maxBlobSize =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  // Files in HEAD, an empty committed file is not a new file.
  QSet<QString> committedFiles;

  for (const auto &file : files) {
    const QString repoPath = git_repository_workdir(repo);
    const QString filePath = QDir::fromNativeSeparators(file);
    const QString fullPath = repoPath + filePath;

    QFile fileObj(fullPath);
    if (fileObj.open(QIODevice::ReadOnly)) {
      const QByteArray content = fileObj.readAll();
      filesContent.insert(file, QString::fromUtf8(content));
      fileObj.close();
    } else {
      filesContent.insert(file, QString());
    }

    git_tree_entry *entry = nullptr;

    error = git_tree_entry_bypath(&entry, tree, filePath.toStdString().c_str());
    if (error) {
      setError(GitError::DiffFilesFailedToLookupTreeEntryByPath, error);
      repoContent.insert(file, QString());
      continue;
    }

    committedFiles.insert(file);

    const git_oid *fullOid = git_tree_entry_id(entry);
    if (!fullOid) {
      setError(GitError::DiffFilesFailedToReadTreeEntryId, error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    git_blob *blob = nullptr;
    error = git_blob_lookup_prefix(&blob, repo, fullOid, 7);
    if (error) {
      setError(GitError::DiffFilesFailedToLookupBlobByPrefix, error);
      git_tree_entry_free(entry);
      repoContent.insert(file, QString());
      continue;
    }

    const char *blobContent =
        static_cast<const char *>(git_blob_rawcontent(blob));
    const size_t blobSize = git_blob_rawsize(blob);
    if (blobSize > static_cast<size_t>(maxBlobSize)) {
      // Blob too large to include in repo content
      repoContent.insert(
          file,
          QString("[[FILE SIZE EXCEEDS LIMIT OF %1 BYTES]]").arg(maxBlobSize));
      git_blob_free(blob);
      git_tree_entry_free(entry);
      continue;
    }

    repoContent.insert(file, QString::fromUtf8(blobContent, blobSize));

    git_blob_free(blob);
    git_tree_entry_free(entry);
  }

  git_tree_free(tree);
  git_commit_free(reinterpret_cast<git_commit *>(headCommit));
  git_reference_free(headRef);

  for (const auto &file : files) {
    const bool newFile = !committedFiles.contains(file);
    if (!newFile) {
      continue;
    }

    const QString indexLine =
        createIndexLineFromContent(repoContent.value(file), filesContent[file]);
    const QString patch = createFullPatchFromContent(file, filesContent[file],
                                                     newFile, indexLine);

    diffOutput += QString("diff --git a/%1 b/%2\n").arg(file, file);
    diffOutput += "new file mode 100644\n";
    diffOutput += patch;

    const FilesDiffDelta patchDeltas =
        createFullPatchDeltasFromContent(file, filesContent[file]);
    filesDeltas += patchDeltas;
  }

  FilesDiff filesDiff;
  filesDiff.diff = diffOutput;
  filesDiff.filesContent = filesContent;
  filesDiff.repoContent = repoContent;
  filesDiff.deltas = filesDeltas;
  filesDiff.valid = true;

  LOG_PERF(0, "Exit");

  return filesDiff;
}

QString createPatchFromCommit(git_repository *repo, const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_oid oid;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *parent = nullptr;
  git_tree *parentTree = nullptr;

  if (const int parentCount = git_commit_parentcount(commit_);
      parentCount > 0) {
    error = git_commit_parent(&parent, commit_, 0);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToGetParentCommit, error);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parentTree, parent);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToGetSecondCommitTree,
               error);
      git_commit_free(parent);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  git_diff *diff = nullptr;

  error = git_diff_tree_to_tree(&diff, repo, parentTree, tree, nullptr);
  if (error) {
    setError(GitError::CreatePatchFromCommitFailedToCreateCommitDiff, error);

    if (parentTree) {
      git_tree_free(parentTree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  QString patch;
  const size_t count = git_diff_num_deltas(diff);
  for (size_t i = 0; i < count; ++i) {
    git_patch *patch_ = nullptr;

    error = git_patch_from_diff(&patch_, diff, i);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToCreatePatchFromDiff,
               error);
      continue;
    }

    git_buf buf = {nullptr};

    error = git_patch_to_buf(&buf, patch_);
    if (error) {
      setError(GitError::CreatePatchFromCommitFailedToRenderPatchToBuffer,
               error);
      git_patch_free(patch_);
      continue;
    }

    if (buf.ptr && buf.size > 0) {
      patch += QString::fromUtf8(buf.ptr, buf.size);
    }

    git_buf_dispose(&buf);
    git_patch_free(patch_);
  }

  git_diff_free(diff);

  if (parentTree) {
    git_tree_free(parentTree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return patch;
}

FilesDiff diffFilesFromCommit(git_repository *repo, const QStringList &files,
                              const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  git_oid oid;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return {};
  }

  git_commit *parent = nullptr;
  git_tree *parentTree = nullptr;
  git_commit *parent2 = nullptr;
  git_tree *parent2Tree = nullptr;

  if (git_commit_parentcount(commit_) > 0) {
    error = git_commit_parent(&parent, commit_, 0);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetParentCommit, error);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parentTree, parent);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetSecondCommitTree, error);
      git_commit_free(parent);
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  if (git_commit_parentcount(commit_) > 2) {
    error = git_commit_parent(&parent2, commit_, 2);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetSecondParentCommit,
               error);
      if (parentTree) {
        git_tree_free(parentTree);
      }
      if (parent) {
        git_commit_free(parent);
      }
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parent2Tree, parent2);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToGetThirdCommitTree, error);
      if (parent2) {
        git_commit_free(parent2);
      }
      if (parentTree) {
        git_tree_free(parentTree);
      }
      if (parent) {
        git_commit_free(parent);
      }
      git_tree_free(tree);
      git_commit_free(commit_);
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  git_diff *diff = nullptr;

  git_strarray arr = qstringlist_to_git_strarray(files);

  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.pathspec = arr;

  error = git_diff_tree_to_tree(&diff, repo, parentTree, tree, &opts);
  if (error) {
    setError(GitError::DiffFilesFromCommitFailedToCreateCommitDiff, error);

    git_strarray_destroy(arr);

    if (parentTree) {
      git_tree_free(parentTree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit_);

    LOG_PERF(0, "Exit");
    return {};
  }

  QString diffOutput;

  QVector<FilesDiffDelta> filesDeltas;

  size_t num_deltas = git_diff_num_deltas(diff);
  for (size_t i = 0; i < num_deltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(diff, i);
    diffOutput += QString("diff --git a/%1 b/%2\n")
                      .arg(delta->old_file.path, delta->new_file.path);

    FilesDiffDelta filesDiffDelta;
    filesDiffDelta.oldFilePath = delta->old_file.path;
    filesDiffDelta.newFilePath = delta->new_file.path;

    git_patch *patch;
    error = git_patch_from_diff(&patch, diff, i);
    if (error) {
      setError(GitError::DiffFilesFromCommitFailedToCreatePatchFromDiff, error);
      continue;
    }

    const size_t num_hunks = git_patch_num_hunks(patch);
    for (size_t h = 0; h < num_hunks; ++h) {
      const git_diff_hunk *hunk;
      error = git_patch_get_hunk(&hunk, NULL, patch, h);
      if (error) {
        setError(GitError::DiffFilesFromCommitFailedToReadPatchHunk, error);
        continue;
      }

      diffOutput += QString("@@ -%1,%2 +%3,%4 @@\n")
                        .arg(hunk->old_start)
                        .arg(hunk->old_lines)
                        .arg(hunk->new_start)
                        .arg(hunk->new_lines);

      FilesDiffHunk filesDiffHunk;
      filesDiffHunk.oldStart = hunk->old_start;
      filesDiffHunk.oldLines = hunk->old_lines;
      filesDiffHunk.newStart = hunk->new_start;
      filesDiffHunk.newLines = hunk->new_lines;

      size_t num_lines = git_patch_num_lines_in_hunk(patch, h);
      for (size_t l = 0; l < num_lines; ++l) {
        const git_diff_line *line;
        error = git_patch_get_line_in_hunk(&line, patch, h, l);
        if (error) {
          setError(GitError::DiffFilesFromCommitFailedToReadPatchLine, error);
          continue;
        }

        const char *start = line->content;
        const char *end = start + line->content_len;
        const char *newline =
            static_cast<const char *>(memchr(start, '\n', end - start));
        if (!newline) {
          newline = end;
        }

        const QString lineContent = QString::fromUtf8(start, newline - start);

        diffOutput += QString("%1%2\n").arg(line->origin, lineContent);

        FilesDiffLine filesDiffLine;
        filesDiffLine.origin = line->origin;
        filesDiffLine.content = lineContent;

        filesDiffHunk.lines += filesDiffLine;
      }

      filesDiffDelta.hunks += filesDiffHunk;
    }

    git_patch_free(patch);

    filesDeltas += filesDiffDelta;
  }

  git_diff_free(diff);
  git_strarray_destroy(arr);

  ConfigManager::ConfigManager &configManager =
      ConfigManager::ConfigManager::getInstance();
  const qlonglong maxBlobSize =
      configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  QMap<QString, QString> filesContent;
  QMap<QString, QString> repoContent;
  QSet<QString> stashUntrackedFiles;

  for (const auto &file : files) {
    const QString filePath = QDir::fromNativeSeparators(file);

    QString repoFileContent;
    if (parentTree && readTreeFileContent(repo, parentTree, filePath,
                                          maxBlobSize, &repoFileContent)) {
      repoContent.insert(file, repoFileContent);
    } else {
      repoContent.insert(file, QString());
    }

    QString filesFileContent;
    if (tree && readTreeFileContent(repo, tree, filePath, maxBlobSize,
                                    &filesFileContent)) {
      filesContent.insert(file, filesFileContent);
      continue;
    }

    if (parent2Tree && readTreeFileContent(repo, parent2Tree, filePath,
                                           maxBlobSize, &filesFileContent)) {
      filesContent.insert(file, filesFileContent);
      stashUntrackedFiles.insert(file);
      continue;
    }

    filesContent.insert(file, QString());
  }

  if (parent2Tree) {
    git_tree_free(parent2Tree);
  }

  if (parent2) {
    git_commit_free(parent2);
  }

  if (parentTree) {
    git_tree_free(parentTree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit_);

  for (const auto &file : files) {
    if (!stashUntrackedFiles.contains(file)) {
      continue;
    }

    const QString indexLine =
        createIndexLineFromContent(QString(), filesContent[file]);
    const QString patch =
        createFullPatchFromContent(file, filesContent[file], true, indexLine);

    diffOutput += QString("diff --git a/%1 b/%2\n").arg(file, file);
    diffOutput += "new file mode 100644\n";
    diffOutput += patch;

    const FilesDiffDelta patchDeltas =
        createFullPatchDeltasFromContent(file, filesContent[file]);
    filesDeltas += patchDeltas;
  }

  FilesDiff filesDiff;
  filesDiff.diff = diffOutput;
  filesDiff.filesContent = filesContent;
  filesDiff.repoContent = repoContent;
  filesDiff.deltas = filesDeltas;
  filesDiff.valid = true;

  LOG_PERF(0, "Exit");

  return filesDiff;
}

BlameReport blame(git_repository *repo, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  const BlameReport report = createBlameReport(repo, files);

  LOG_PERF(0, "Exit");

  return report;
}

qint64 getFileSizeFromCommit(git_repository *repo, const QString &filePath,
                             const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_oid oid;
  int error;

  // If commit is empty, use HEAD
  if (commit.isEmpty()) {
    git_reference *head = nullptr;
    error = git_repository_head(&head, repo);
    if (error) {
      setError(GitError::GetFileSizeFromCommitFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return -1;
    }

    error = git_reference_name_to_id(&oid, repo, git_reference_name(head));
    git_reference_free(head);
    if (error) {
      setError(GitError::GetFileSizeFromCommitFailedToGetReferenceName, error);
      LOG_PERF(0, "Exit");
      return -1;
    }
  } else {
    error = git_oid_fromstr(&oid, commit.toStdString().c_str());
    if (error) {
      setError(GitError::GetFileSizeFromCommitFailedToParseObjectId, error);
      LOG_PERF(0, "Exit");
      return -1;
    }
  }

  git_commit *commit_ = nullptr;

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_tree *tree = nullptr;

  error = git_commit_tree(&tree, commit_);
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToGetCommitTree, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const QString normalizedPath = QDir::fromNativeSeparators(filePath);
  git_tree_entry *entry = nullptr;

  error =
      git_tree_entry_bypath(&entry, tree, normalizedPath.toStdString().c_str());
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToLookupTreeEntryByPath,
             error);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const git_oid *blobOid = git_tree_entry_id(entry);
  if (!blobOid) {
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  git_blob *blob = nullptr;
  error = git_blob_lookup(&blob, repo, blobOid);
  if (error) {
    setError(GitError::GetFileSizeFromCommitFailedToLookupBlob, error);
    git_tree_entry_free(entry);
    git_tree_free(tree);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return -1;
  }

  const qint64 blobSize = static_cast<qint64>(git_blob_rawsize(blob));

  git_blob_free(blob);
  git_tree_entry_free(entry);
  git_tree_free(tree);
  git_commit_free(commit_);

  LOG_PERF(0, "Exit");

  return blobSize;
}

} // namespace GitRepo
