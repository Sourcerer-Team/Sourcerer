#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <svn_hash.h>
#include <svn_props.h>
#include <svn_string.h>
#include <svn_path.h>
#include <svn_wc.h>

#include <Log.h>

#include <QDir>
#include <QFileInfo>
#include <QMap>

#define THREAD_ID "SvnRepo"

namespace SvnRepo {

using namespace Private;

namespace {

const QString kRemoteName = QStringLiteral("origin");

struct ExternalDefinition {
  // Directory the svn:externals property is set on, relative to the working
  // copy root ("" for the root).
  QString definingDirectory;
  // Path of the external relative to the working copy root.
  QString name;
  QString url;
  svn_revnum_t revision = SVN_INVALID_REVNUM;
};

static QString joinRelative(const QString &directory, const QString &name) {
  if (directory.isEmpty() || directory == QStringLiteral(".")) {
    return name;
  }
  return directory + QStringLiteral("/") + name;
}

// External items defined by a property value.
static svn_error_t *parseExternals(QVector<ExternalDefinition> *externals,
                                   const QString &wcPath,
                                   const QString &definingDirectory,
                                   const QByteArray &description,
                                   apr_pool_t *pool) {
  const QByteArray definingBytes =
      absoluteFilePath(wcPath, definingDirectory).toUtf8();
  apr_array_header_t *items = nullptr;
  SVN_ERR(svn_wc_parse_externals_description3(
      &items, definingBytes.constData(), description.constData(), FALSE, pool));

  for (int i = 0; items && i < items->nelts; ++i) {
    const auto *item = APR_ARRAY_IDX(items, i, svn_wc_external_item2_t *);
    ExternalDefinition definition;
    definition.definingDirectory = definingDirectory;
    definition.name =
        joinRelative(definingDirectory, QString::fromUtf8(item->target_dir));
    definition.url = QString::fromUtf8(item->url);
    if (item->revision.kind == svn_opt_revision_number) {
      definition.revision = item->revision.value.number;
    }
    externals->append(definition);
  }

  return SVN_NO_ERROR;
}

// svn:externals properties of the working copy, keyed by the defining
// directory relative to the working copy root.
static svn_error_t *readExternalsProperties(QMap<QString, QByteArray> *values,
                                            const QString &wcPath,
                                            const QString &target,
                                            svn_depth_t depth,
                                            svn_client_ctx_t *ctx,
                                            apr_pool_t *pool) {
  const QByteArray targetBytes = absoluteFilePath(wcPath, target).toUtf8();
  const svn_opt_revision_t peg = revisionUnspecified();
  const svn_opt_revision_t revision = revisionWorking();
  apr_hash_t *properties = nullptr;
  SVN_ERR(svn_client_propget5(&properties, nullptr, SVN_PROP_EXTERNALS,
                              targetBytes.constData(), &peg, &revision, nullptr,
                              depth, nullptr, ctx, pool, pool));

  for (apr_hash_index_t *index = apr_hash_first(pool, properties); index;
       index = apr_hash_next(index)) {
    const void *key = nullptr;
    void *value = nullptr;
    apr_hash_this(index, &key, nullptr, &value);
    const auto *propertyValue = static_cast<const svn_string_t *>(value);
    const QString definingDirectory = relativeFilePath(
        wcPath, QString::fromUtf8(static_cast<const char *>(key)));
    values->insert(
        definingDirectory == QStringLiteral(".") ? QString()
                                                 : definingDirectory,
        QByteArray(propertyValue->data, static_cast<int>(propertyValue->len)));
  }

  return SVN_NO_ERROR;
}

static bool readExternals(QVector<ExternalDefinition> *externals,
                          const QString &wcPath) {
  return runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QMap<QString, QByteArray> values;
        if (!reportError(SvnError::PropertyFailed,
                         readExternalsProperties(&values, wcPath, QString(),
                                                 svn_depth_infinity, ctx,
                                                 pool))) {
          return false;
        }

        for (auto it = values.cbegin(); it != values.cend(); ++it) {
          if (!reportError(SvnError::PropertyFailed,
                           parseExternals(externals, wcPath, it.key(),
                                          it.value(), pool))) {
            return false;
          }
        }

        return true;
      },
      wcPath);
}

static svn_error_t *setExternalsProperty(const QString &wcPath,
                                         const QString &definingDirectory,
                                         const QByteArray &value,
                                         svn_client_ctx_t *ctx,
                                         apr_pool_t *pool) {
  const PathArray targets =
      makePathArray(pool, {absoluteFilePath(wcPath, definingDirectory)});
  const svn_string_t *propertyValue =
      value.trimmed().isEmpty()
          ? nullptr
          : svn_string_ncreate(value.constData(),
                               static_cast<apr_size_t>(value.size()), pool);
  return svn_client_propset_local(SVN_PROP_EXTERNALS, propertyValue,
                                  targets.array, svn_depth_empty, FALSE,
                                  nullptr, ctx, pool);
}

// Updates a directory and checks out the externals defined below it,
// respecting pinned revisions. Externals are only processed by recursive
// updates.
static svn_error_t *updateExternals(const QString &wcPath,
                                    const QString &definingDirectory,
                                    svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const PathArray paths =
      makePathArray(pool, {absoluteFilePath(wcPath, definingDirectory)});
  const svn_opt_revision_t head = revisionHead();
  apr_array_header_t *resultRevisions = nullptr;
  return svn_client_update4(&resultRevisions, paths.array, &head,
                            svn_depth_infinity, FALSE, FALSE, TRUE, TRUE, FALSE,
                            ctx, pool);
}

// Updates the working copy of a checked out external to its pinned revision,
// or to HEAD.
static svn_error_t *updateExternal(const QString &externalPath,
                                   const svn_revnum_t revision,
                                   svn_client_ctx_t *ctx, apr_pool_t *pool) {
  const PathArray paths = makePathArray(pool, {externalPath});
  const svn_opt_revision_t target =
      SVN_IS_VALID_REVNUM(revision) ? revisionNumber(revision) : revisionHead();
  apr_array_header_t *resultRevisions = nullptr;
  return svn_client_update4(&resultRevisions, paths.array, &target,
                            svn_depth_infinity, FALSE, FALSE, TRUE, TRUE, FALSE,
                            ctx, pool);
}

static const ExternalDefinition *
findExternal(const QVector<ExternalDefinition> &externals,
             const QString &name) {
  const QString normalizedName =
      QDir::cleanPath(QDir::fromNativeSeparators(name));
  for (const ExternalDefinition &external : externals) {
    if (external.name == normalizedName) {
      return &external;
    }
  }

  setLastError(SvnError::PropertyFailed, -1,
               QStringLiteral("There is no external named '%1'.").arg(name));
  return nullptr;
}

} // namespace

QVector<RemoteInfo> listRemotes(const QString &path) {
  LOG_PERF(0, "Enter");

  WorkingCopyInfo info;
  if (!readInfo(&info, normalizedPath(path))) {
    LOG_PERF(0, "Exit");
    return {};
  }

  RemoteInfo remote;
  remote.name = kRemoteName;
  remote.fetchUrl = trimmedUrl(info.reposRootUrl);
  remote.pushUrl = remote.fetchUrl;
  remote.isDefault = true;

  LOG_PERF(0, "Exit");
  return {remote};
}

QString getDefaultRemote(const QString &) {
  return kRemoteName;
}

bool addRemote(const QString &, const QString &, const QString &,
               const QString &) {
  return setUnsupported("Adding a remote");
}

bool updateRemote(const QString &path, const QString &remoteName,
                  const QString &newRemoteName, const QString &fetchUrl,
                  const QString &pushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteName != kRemoteName ||
      (!newRemoteName.isEmpty() && newRemoteName != kRemoteName)) {
    LOG_PERF(0, "Exit");
    return setUnsupported("Renaming the remote");
  }

  if (!pushUrl.isEmpty() && trimmedUrl(pushUrl) != trimmedUrl(fetchUrl)) {
    LOG_PERF(0, "Exit");
    return setUnsupported("A separate push URL");
  }

  // Points the working copy at the repository's new location.
  const QString wcPath = normalizedPath(path);
  WorkingCopyInfo info;
  if (!readInfo(&info, wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }
  const QString oldRoot = trimmedUrl(info.reposRootUrl);
  const QString newRoot = canonicalUrl(fetchUrl);
  if (newRoot.isEmpty() || newRoot == oldRoot) {
    LOG_PERF(0, "Exit");
    return true;
  }
  const QString root = info.wcRoot.isEmpty() ? wcPath : info.wcRoot;
  const QByteArray from = oldRoot.toUtf8();
  const QByteArray to = newRoot.toUtf8();

  // The relocation leaves the working copy's write lock behind (B64); a
  // cleanup in a client context of its own releases it.
  const auto relocate = [&](const QString &workingCopy) {
    const QByteArray bytes = workingCopy.toUtf8();
    return runWithClientContext(
               [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
                 return reportError(
                     SvnError::RelocateFailed,
                     svn_client_relocate2(bytes.constData(), from.constData(),
                                          to.constData(), TRUE, ctx, pool));
               },
               workingCopy) &&
           runWithClientContext(
               [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
                 return reportError(
                     SvnError::RelocateFailed,
                     svn_client_cleanup2(bytes.constData(), TRUE, FALSE, FALSE,
                                         FALSE, FALSE, ctx, pool));
               },
               workingCopy);
  };
  if (!relocate(root)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // The externals of the same repository move along: definitions that name
  // it by its old URL are rewritten (a local change to commit), and their
  // working copies are relocated (B62).
  QStringList externalsToRelocate;
  const bool rewritten = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QMap<QString, QByteArray> values;
        if (!reportError(SvnError::PropertyFailed,
                         readExternalsProperties(&values, root, QString(),
                                                 svn_depth_infinity, ctx,
                                                 pool))) {
          return false;
        }
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
          const QByteArray value = it.value();
          QByteArray updatedValue;
          for (const QByteArray &line : value.split('\n')) {
            QByteArray updated = line;
            const qsizetype at = updated.indexOf(from);
            const qsizetype end = at + from.size();
            if (at >= 0 && (at == 0 || updated.at(at - 1) == ' ') &&
                (end == updated.size() || updated.at(end) == '/' ||
                 updated.at(end) == ' ' || updated.at(end) == '@')) {
              updated.replace(at, from.size(), to);
            }
            updatedValue += updated + '\n';
          }
          updatedValue.chop(1);
          if (updatedValue != value &&
              !reportError(SvnError::PropertyFailed,
                           setExternalsProperty(root, it.key(), updatedValue,
                                                ctx, pool))) {
            return false;
          }

          QVector<ExternalDefinition> definitions;
          svn_error_t *error = parseExternals(&definitions, root, it.key(),
                                              updatedValue, pool);
          if (error) {
            svn_error_clear(error);
            continue;
          }
          for (const ExternalDefinition &definition : definitions) {
            const QString externalPath =
                absoluteFilePath(root, definition.name);
            WorkingCopyInfo external;
            error = readInfo(&external, externalPath, revisionUnspecified(),
                             ctx, pool);
            if (error) {
              svn_error_clear(error);
            } else if (trimmedUrl(external.reposRootUrl) == oldRoot) {
              externalsToRelocate.append(externalPath);
            }
          }
        }
        return true;
      },
      root);
  bool success = rewritten;
  for (const QString &externalPath : externalsToRelocate) {
    success = success && relocate(externalPath);
  }

  LOG_PERF(0, "Exit");
  return success;
}

bool removeRemote(const QString &, const QString &) {
  return setUnsupported("Removing the remote");
}

bool setDefaultRemote(const QString &, const QString &remoteName) {
  // The only remote is the default one.
  return remoteName == kRemoteName || setUnsupported("Changing the remote");
}

bool addTarget(const QString &, const QString &, const QString &,
               const QString &, const QString &) {
  return setUnsupported("Remote targets");
}

bool editTarget(const QString &, const QString &, const QString &,
                const QString &, const QString &) {
  return setUnsupported("Remote targets");
}

bool deleteTarget(const QString &, const QString &, const QString &) {
  return setUnsupported("Remote targets");
}

bool setDefaultTarget(const QString &, const QString &, const QString &) {
  return setUnsupported("Remote targets");
}

std::tuple<bool, QString, QString> getUpstream(const QString &path,
                                               const QString &localBranchName) {
  // An existing branch is its own upstream.
  const bool quit = false;
  if (localBranchName.isEmpty() ||
      !listLocalBranches(path, quit).contains(localBranchName)) {
    return {false, QString(), QString()};
  }
  return {true, kRemoteName, localBranchName};
}

bool setUpstream(const QString &, const QString &, const QString &,
                 const QString &) {
  return setUnsupported("Changing the upstream branch");
}

QStringList listSubmodules(const QString &path) {
  LOG_PERF(0, "Enter");

  QVector<ExternalDefinition> externals;
  if (!readExternals(&externals, normalizedPath(path))) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList names;
  for (const ExternalDefinition &external : externals) {
    names.append(external.name);
  }

  LOG_PERF(0, "Exit");
  return names;
}

QVector<SubmoduleInfo> listSubmodulesWithStatus(const QString &path,
                                                const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString wcPath = normalizedPath(path);
  QVector<ExternalDefinition> externals;
  if (!readExternals(&externals, wcPath)) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QVector<SubmoduleInfo> submodules;
  for (const ExternalDefinition &external : externals) {
    SubmoduleInfo info;
    info.name = external.name;
    info.commit = SVN_IS_VALID_REVNUM(external.revision)
                      ? QString::number(external.revision)
                      : QString();

    // Directory externals are working copies of their own. Local changes
    // in one, or a revision it doesn't have yet (the pinned one, or a newer
    // one upstream), make it Outdated.
    const QString externalPath = absoluteFilePath(wcPath, external.name);
    info.statusInfo.status = QFileInfo::exists(externalPath)
                                 ? SubmoduleStatusType::UpToDate
                                 : SubmoduleStatusType::Outdated;
    if (QFileInfo(externalPath).isDir() && !quitFlag) {
      const SvnStatus local = readStatus(externalPath, &quitFlag);
      bool outdated = !local.modifiedFiles.isEmpty() ||
                      !local.deletedFiles.isEmpty() ||
                      !local.typeChangedFiles.isEmpty() ||
                      !local.newStagedFiles.isEmpty() ||
                      !local.modifiedStagedFiles.isEmpty() ||
                      !local.deletedStagedFiles.isEmpty() ||
                      !local.conflictedFiles.isEmpty();
      runWithClientContext(
          [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
            svn_revnum_t newest = SVN_INVALID_REVNUM;
            WorkingCopyInfo localInfo;
            WorkingCopyInfo upstream;
            svn_error_t *error =
                readMaxRevision(&newest, externalPath, false, ctx, pool);
            if (!error) {
              error = readInfo(&localInfo, externalPath, revisionUnspecified(),
                               ctx, pool);
            }
            if (!error && !SVN_IS_VALID_REVNUM(external.revision)) {
              error = readInfo(&upstream, localInfo.url, revisionHead(), ctx,
                               pool);
            }
            if (error) {
              svn_error_clear(error);
            } else if (SVN_IS_VALID_REVNUM(external.revision)) {
              outdated = outdated || newest != external.revision;
            } else {
              outdated = outdated || upstream.lastChangedRevision > newest;
            }
            return true;
          },
          externalPath);
      if (outdated) {
        info.statusInfo.status = SubmoduleStatusType::Outdated;
      }
    }
    submodules.append(info);
  }

  LOG_PERF(0, "Exit");
  return submodules;
}

QString getSubmodulePath(const QString &path, const QString &submoduleName) {
  return absoluteFilePath(normalizedPath(path), submoduleName);
}

bool addSubmodule(const QString &path, const QString &url, const QString &name,
                  const QString &branch, const QString &uuid) {
  LOG_PERF(0, "Enter");

  // The URL names the branch of the external.
  Q_UNUSED(branch);
  Q_UNUSED(uuid);

  const QString wcPath = normalizedPath(path);
  const QString relativeName = QDir::cleanPath(QDir::fromNativeSeparators(
      QFileInfo(name).isAbsolute() ? relativeFilePath(wcPath, name) : name));
  QString definingDirectory =
      relativeName.contains('/') ? relativeName.section('/', 0, -2) : QString();
  QString target = relativeName.section('/', -1);
  // A folder that isn't versioned can't hold the property: the nearest
  // versioned one above it does, with the rest of the path as the target.
  while (!definingDirectory.isEmpty()) {
    WorkingCopyInfo directoryInfo;
    const QString absoluteDirectory =
        absoluteFilePath(wcPath, definingDirectory);
    if (QFileInfo(absoluteDirectory).isDir() &&
        runWithClientContext(
            [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
              svn_error_t *error =
                  readInfo(&directoryInfo, absoluteDirectory,
                           revisionUnspecified(), ctx, pool);
              svn_error_clear(error);
              return !error;
            },
            wcPath)) {
      break;
    }
    target = definingDirectory.section('/', -1) + QLatin1Char('/') + target;
    definingDirectory = definingDirectory.contains('/')
                            ? definingDirectory.section('/', 0, -2)
                            : QString();
  }

  if (target.isEmpty() || url.trimmed().isEmpty() ||
      relativeName.startsWith(QLatin1String("..")) ||
      QFileInfo(relativeName).isAbsolute()) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("The external needs a URL and a path."));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QMap<QString, QByteArray> values;
        if (!reportError(SvnError::PropertyFailed,
                         readExternalsProperties(&values, wcPath,
                                                 definingDirectory,
                                                 svn_depth_empty, ctx, pool))) {
          return false;
        }

        QByteArray value = values.value(definingDirectory);
        if (!value.isEmpty() && !value.endsWith('\n')) {
          value += '\n';
        }
        // A target with a space is quoted; the URL is escaped.
        const QByteArray quotedTarget =
            target.contains(QLatin1Char(' '))
                ? '"' + target.toUtf8() + '"'
                : target.toUtf8();
        QStringList urlParts = url.trimmed().split(QLatin1Char(' '),
                                                   Qt::SkipEmptyParts);
        if (!urlParts.isEmpty() &&
            urlParts.last().contains(QLatin1String("://"))) {
          urlParts.last() = QString::fromUtf8(svn_path_uri_autoescape(
              urlParts.last().toUtf8().constData(), pool));
        }
        value += urlParts.join(QLatin1Char(' ')).toUtf8() + ' ' + quotedTarget +
                 '\n';

        // Validate the new definition before setting it.
        QVector<ExternalDefinition> parsed;
        if (!reportError(SvnError::PropertyFailed,
                         parseExternals(&parsed, wcPath, definingDirectory,
                                        value, pool)) ||
            !reportError(SvnError::PropertyFailed,
                         setExternalsProperty(wcPath, definingDirectory, value,
                                              ctx, pool))) {
          return false;
        }

        // Only the new external is fetched, by checking it out where its
        // definition puts it; nothing else in the working copy moves to
        // another revision. Subversion only fetches externals in recursive
        // updates.
        apr_array_header_t *items = nullptr;
        const QByteArray definingBytes =
            absoluteFilePath(wcPath, definingDirectory).toUtf8();
        const QByteArray line = urlParts.join(QLatin1Char(' ')).toUtf8() +
                                ' ' + quotedTarget;
        if (!reportError(SvnError::PropertyFailed,
                         svn_wc_parse_externals_description3(
                             &items, definingBytes.constData(),
                             line.constData(), FALSE, pool))) {
          return false;
        }
        const auto *item = APR_ARRAY_IDX(items, 0, svn_wc_external_item2_t *);
        WorkingCopyInfo defining;
        if (!reportError(SvnError::InfoFailed,
                         readInfo(&defining,
                                  absoluteFilePath(wcPath, definingDirectory),
                                  revisionUnspecified(), ctx, pool))) {
          return false;
        }
        QString externalUrl = QString::fromUtf8(item->url);
        if (externalUrl.startsWith(QLatin1String("^/"))) {
          externalUrl = trimmedUrl(defining.reposRootUrl) + externalUrl.mid(1);
        } else if (externalUrl.startsWith(QLatin1String("../"))) {
          externalUrl = trimmedUrl(defining.url) + QLatin1Char('/') + externalUrl;
        }
        const QByteArray checkoutUrl = canonicalUrl(externalUrl).toUtf8();
        const QByteArray targetBytes =
            absoluteFilePath(wcPath, joinRelative(definingDirectory, target))
                .toUtf8();
        svn_revnum_t resultRevision = SVN_INVALID_REVNUM;
        const svn_opt_revision_t peg =
            item->peg_revision.kind == svn_opt_revision_unspecified
                ? revisionHead()
                : item->peg_revision;
        const svn_opt_revision_t revision =
            item->revision.kind == svn_opt_revision_unspecified
                ? peg
                : item->revision;
        return reportError(
            SvnError::UpdateFailed,
            svn_client_checkout3(&resultRevision, checkoutUrl.constData(),
                                 targetBytes.constData(), &peg, &revision,
                                 svn_depth_infinity, FALSE, FALSE, ctx, pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

bool deleteSubmodule(const QString &path, const QString &submoduleName,
                     const QString &submodulePath) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  QVector<ExternalDefinition> externals;
  if (!readExternals(&externals, wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const ExternalDefinition *external = findExternal(externals, submoduleName);
  if (!external) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const ExternalDefinition definition = *external;
  const QString externalPath = absoluteFilePath(wcPath, definition.name);
  if (!submodulePath.isEmpty() &&
      absoluteFilePath(wcPath, submodulePath) != externalPath) {
    setLastError(SvnError::InvalidArgument, -1,
                 QStringLiteral("'%1' isn't the folder of the external '%2'.")
                     .arg(submodulePath, submoduleName));
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        QMap<QString, QByteArray> values;
        if (!reportError(SvnError::PropertyFailed,
                         readExternalsProperties(&values, wcPath,
                                                 definition.definingDirectory,
                                                 svn_depth_empty, ctx, pool))) {
          return false;
        }

        // Keep every line, including comments, except the definition of the
        // external.
        QByteArray value;
        for (const QByteArray &line :
             values.value(definition.definingDirectory).split('\n')) {
          QVector<ExternalDefinition> parsed;
          svn_error_t *error =
              line.trimmed().isEmpty() || line.trimmed().startsWith('#')
                  ? SVN_NO_ERROR
                  : parseExternals(&parsed, wcPath,
                                   definition.definingDirectory, line, pool);
          if (error) {
            svn_error_clear(error);
          }

          if (parsed.size() == 1 && parsed.first().name == definition.name) {
            continue;
          }

          if (!line.isEmpty()) {
            value += line + '\n';
          }
        }

        return reportError(SvnError::PropertyFailed,
                           setExternalsProperty(wcPath,
                                                definition.definingDirectory,
                                                value, ctx, pool));
      },
      wcPath);

  if (success) {
    if (QFileInfo(externalPath).isDir() &&
        !QDir(externalPath).removeRecursively()) {
      LOG_WARN("Failed to remove the external directory '{}'.", externalPath);
    }
  }

  LOG_PERF(0, "Exit");
  return success;
}

bool updateSubmodule(const QString &path, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString wcPath = normalizedPath(path);
  QVector<ExternalDefinition> externals;
  if (!readExternals(&externals, wcPath)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const ExternalDefinition *external = findExternal(externals, submoduleName);
  if (!external) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const ExternalDefinition definition = *external;
  const QString externalPath = absoluteFilePath(wcPath, definition.name);
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        // An external that has not been checked out yet is fetched by
        // updating the directory defining it.
        return reportError(
            SvnError::UpdateFailed,
            QFileInfo(externalPath).isDir()
                ? updateExternal(externalPath, definition.revision, ctx, pool)
                : updateExternals(wcPath, definition.definingDirectory, ctx,
                                  pool));
      },
      wcPath);

  LOG_PERF(0, "Exit");
  return success;
}

} // namespace SvnRepo
