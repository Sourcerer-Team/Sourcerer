#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

#include <svn_hash.h>
#include <svn_props.h>
#include <svn_string.h>
#include <svn_wc.h>

#include <Log.h>

#include <QDir>
#include <QFileInfo>

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
  const bool success = runWithClientContext(
      [&](svn_client_ctx_t *ctx, apr_pool_t *pool) {
        WorkingCopyInfo info;
        if (!reportError(
                SvnError::InfoFailed,
                readInfo(&info, wcPath, revisionUnspecified(), ctx, pool))) {
          return false;
        }

        const QString oldRoot = trimmedUrl(info.reposRootUrl);
        const QString newRoot = trimmedUrl(fetchUrl);
        if (newRoot.isEmpty() || newRoot == oldRoot) {
          return true;
        }

        const QByteArray wcRoot =
            (info.wcRoot.isEmpty() ? wcPath : info.wcRoot).toUtf8();
        const QByteArray from = oldRoot.toUtf8();
        const QByteArray to = newRoot.toUtf8();
        return reportError(
            SvnError::RelocateFailed,
            svn_client_relocate2(wcRoot.constData(), from.constData(),
                                 to.constData(), FALSE, ctx, pool));
      },
      wcPath);

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

std::tuple<bool, QString, QString> getUpstream(const QString &,
                                               const QString &localBranchName) {
  // A branch is its own upstream.
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

    // Directory externals are working copies of their own.
    const QString externalPath = absoluteFilePath(wcPath, external.name);
    info.statusInfo.status = QFileInfo::exists(externalPath)
                                 ? SubmoduleStatusType::UpToDate
                                 : SubmoduleStatusType::Outdated;
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
  const QString definingDirectory =
      relativeName.contains('/') ? relativeName.section('/', 0, -2) : QString();
  const QString target = relativeName.section('/', -1);

  if (target.isEmpty() || url.trimmed().isEmpty()) {
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
        value += url.trimmed().toUtf8() + ' ' + target.toUtf8() + '\n';

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

        return reportError(
            SvnError::UpdateFailed,
            updateExternals(wcPath, definingDirectory, ctx, pool));
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
    const QString externalPath = submodulePath.isEmpty()
                                     ? absoluteFilePath(wcPath, definition.name)
                                     : absoluteFilePath(wcPath, submodulePath);
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
