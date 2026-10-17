#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <ConfigManager.h>
#include <LockGuard.h>
#include <Log.h>

#include <QByteArray>
#include <QHash>
#include <QSharedPointer>
#include <QVector>

#include <algorithm>

#define THREAD_ID "GitRepo"

// History and commit details.
namespace GitRepo {

using namespace Private;

namespace {

struct HistoryRef {
  QString key;
  git_oid oid;
};

// Commit order of the history currently paged through, see getBranchHistory.
struct HistoryCursor {
  QString key;
  QVector<git_oid> oids; // Newest first, children before parents.
  QString headCommit;
  QHash<QString, QStringList> refTips;
  QHash<QString, QStringList> refLabels;
};

Util::Mutex gHistoryCursorsMutex;
QMap<git_repository *, QSharedPointer<HistoryCursor>> gHistoryCursors;

} // namespace

namespace Private {

void forgetHistoryCursor(git_repository *repo) {
  LOCKGUARD(gHistoryCursorsMutex);
  gHistoryCursors.remove(repo);
}

} // namespace Private

// Collects the commits pointed to by refs matching `glob`. Names are made
// relative to `prefix`; for remote refs the remote name is stripped as well so
// that "origin/main" and "main" share the same graph color key. Refs that do
// not resolve to a commit (e.g. tags of trees) and symbolic "HEAD" refs are
// skipped.
static void collectHistoryRefs(git_repository *repo, const char *glob,
                               const QString &prefix, const bool stripRemote,
                               QVector<HistoryRef> &out) {
  git_reference_iterator *it = nullptr;
  if (git_reference_iterator_glob_new(&it, repo, glob) != 0) {
    return;
  }

  QVector<HistoryRef> found;
  git_reference *ref = nullptr;
  while (git_reference_next(&ref, it) == 0) {
    const char *name = git_reference_name(ref);

    git_object *obj = nullptr;
    if (name && git_reference_peel(&obj, ref, GIT_OBJECT_COMMIT) == 0) {
      QString key = QString::fromUtf8(name);
      if (key.startsWith(prefix)) {
        key = key.mid(prefix.size());
      }
      if (stripRemote) {
        const int slash = key.indexOf('/');
        if (slash >= 0) {
          key = key.mid(slash + 1);
        }
      }
      if (key != QLatin1String(HEAD)) {
        found.push_back(HistoryRef{key, *git_object_id(obj)});
      }
    }

    if (obj) {
      git_object_free(obj);
    }
    git_reference_free(ref);
  }
  git_reference_iterator_free(it);

  std::sort(
      found.begin(), found.end(),
      [](const HistoryRef &a, const HistoryRef &b) { return a.key < b.key; });
  out += found;
}

// Collects the full names of the refs matching the glob, per commit SHA.
static void collectRefLabels(git_repository *repo, const char *glob,
                             QHash<QString, QStringList> &out) {
  git_reference_iterator *it = nullptr;
  if (git_reference_iterator_glob_new(&it, repo, glob) != 0) {
    return;
  }

  QVector<QPair<QString, QString>> found; // name, sha
  git_reference *ref = nullptr;
  while (git_reference_next(&ref, it) == 0) {
    const char *name = git_reference_name(ref);

    git_object *obj = nullptr;
    if (name && git_reference_peel(&obj, ref, GIT_OBJECT_COMMIT) == 0) {
      const QString fullName = QString::fromUtf8(name);
      // Skip symbolic remote HEADs like refs/remotes/origin/HEAD.
      if (!fullName.endsWith(QStringLiteral("/") + QLatin1String(HEAD))) {
        found.push_back({fullName, QString::fromLatin1(
                                       git_oid_tostr_s(git_object_id(obj)))});
      }
    }

    if (obj) {
      git_object_free(obj);
    }
    git_reference_free(ref);
  }
  git_reference_iterator_free(it);

  std::sort(found.begin(), found.end(),
            [](const QPair<QString, QString> &a,
               const QPair<QString, QString> &b) { return a.first < b.first; });
  for (const auto &[name, sha] : found) {
    out[sha].push_back(name);
  }
}

// Builds the ordered list of commits (newest first, children before parents)
// for the requested history. Returns null on failure.
static QSharedPointer<HistoryCursor>
buildHistoryCursor(git_repository *repo, const QString &branchName,
                   const BranchType type, const QString &cursorKey,
                   const bool &quitFlag) {
  const bool allRefs = type == BranchType::All || (branchName == ALL_BRANCHES &&
                                                   type == BranchType::Local);
  const bool allRemotes =
      branchName == ALL_BRANCHES && type == BranchType::Remote;

  if (branchName == ALL_BRANCHES && !allRefs && !allRemotes) {
    return {};
  }

  if (!allRefs && !allRemotes && branchName.isEmpty()) {
    return {};
  }

  git_revwalk *walker = nullptr;

  int error = git_revwalk_new(&walker, repo);
  if (error) {
    setError(GitError::GetBranchHistoryFailedToCreateRevwalk, error);
    return {};
  }

  error = git_revwalk_sorting(walker, GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);
  if (error) {
    setError(GitError::GetBranchHistoryFailedToConfigureRevwalkSorting, error);
    git_revwalk_free(walker);
    return {};
  }

  // Tips of all branches and tags. They give stable color keys to the graph
  // and are the starting points of the "all branches" history.
  QVector<HistoryRef> refs;
  collectHistoryRefs(repo, "refs/heads/*", QStringLiteral("refs/heads/"), false,
                     refs);
  collectHistoryRefs(repo, "refs/remotes/*", QStringLiteral("refs/remotes/"),
                     true, refs);
  collectHistoryRefs(repo, "refs/tags/*", QStringLiteral("refs/tags/"), false,
                     refs);

  auto cursor = QSharedPointer<HistoryCursor>::create();
  cursor->key = cursorKey;

  for (const HistoryRef &ref : refs) {
    const QString sha = QString::fromLatin1(git_oid_tostr_s(&ref.oid));
    QStringList &keys = cursor->refTips[sha];
    if (!keys.contains(ref.key)) {
      keys.push_back(ref.key);
    }
  }

  git_oid headOid;
  if (git_reference_name_to_id(&headOid, repo, HEAD) == 0) {
    cursor->headCommit = QString::fromLatin1(git_oid_tostr_s(&headOid));
  }

  collectRefLabels(repo, "refs/heads/*", cursor->refLabels);
  collectRefLabels(repo, "refs/remotes/*", cursor->refLabels);
  collectRefLabels(repo, "refs/tags/*", cursor->refLabels);

  if (allRefs) {
    // Unreachable refs are ignored on purpose, the rest of the history is
    // still valid.
    for (const HistoryRef &ref : refs) {
      git_revwalk_push(walker, &ref.oid);
    }
    git_revwalk_push_head(walker);
  } else if (allRemotes) {
    error = git_revwalk_push_glob(walker, "refs/remotes/*");
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddReferenceToRevwalk, error);
      git_revwalk_free(walker);
      return {};
    }
  } else if (type == BranchType::Local) {
    if (branchName == HEAD) {
      error = git_revwalk_push_head(walker);
    } else {
      error = git_revwalk_push_ref(
          walker,
          QString("refs/heads/%1").arg(branchName).toStdString().c_str());
    }
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddLocalBranchToRevwalk,
               error);
      git_revwalk_free(walker);
      return {};
    }
  } else if (type == BranchType::Remote) {
    error = git_revwalk_push_ref(
        walker,
        QString("refs/remotes/%1").arg(branchName).toStdString().c_str());
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddRemoteBranchToRevwalk,
               error);
      git_revwalk_free(walker);
      return {};
    }
  } else if (type == BranchType::Tag) {
    error = git_revwalk_push_ref(
        walker, QString("refs/tags/%1").arg(branchName).toStdString().c_str());
    if (error) {
      setError(GitError::GetBranchHistoryFailedToAddTagToRevwalk, error);
      git_revwalk_free(walker);
      return {};
    }
  } else {
    git_revwalk_free(walker);
    return {};
  }

  git_oid oid;
  while (!quitFlag && git_revwalk_next(&oid, walker) == 0) {
    cursor->oids.push_back(oid);
  }

  git_revwalk_free(walker);

  if (quitFlag) {
    return {};
  }

  return cursor;
}

// Reads one commit into `history`. Returns false and sets the last error on
// failure.
static bool loadHistoryCommit(git_repository *repo, const git_oid &oid,
                              const QString &branchName, GitHistory &history) {
  const char *str = git_oid_tostr_s(&oid);
  if (!str) {
    LOG_ERROR(Log::ErrorId::Git, "Can't convert oid to string.");
    return false;
  }

  const QString oidStr = str;

  git_commit *commit = nullptr;

  const int error = git_commit_lookup(&commit, repo, &oid);
  if (error) {
    setError(GitError::GetBranchHistoryFailedToLookupCommit, error);
    return false;
  }

  if (!commit) {
    LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit with oid \"{}\"", oidStr);
    return false;
  }

  history.commits.push_back(oidStr);

  GitCommitInfo &info = history.commitInfos[oidStr];
  info.oid = oid;

  const std::size_t parentCount = git_commit_parentcount(commit);

  for (std::size_t i = 0; i < parentCount; ++i) {
    const git_oid *parentOid =
        git_commit_parent_id(commit, static_cast<unsigned int>(i));
    if (!parentOid) {
      LOG_ERROR(Log::ErrorId::Git,
                "Can't lookup parent oid at index {} for oid \"{}\"", i,
                oidStr);
      git_commit_free(commit);
      return false;
    }

    const char *parentStr = git_oid_tostr_s(parentOid);
    if (!parentStr) {
      LOG_ERROR(
          Log::ErrorId::Git,
          "Can't convert parent oid at index {} for oid \"{}\" to string.", i,
          oidStr);
      git_commit_free(commit);
      return false;
    }

    const QString parentOidStr = parentStr;

    if (oidStr == parentOidStr) {
      LOG_ERROR(Log::ErrorId::Git, "The commit \"{}\"is parent of itself",
                parentOidStr)
      continue;
    }

    info.parents.push_back(parentOidStr);
  }

  if (const char *message = git_commit_message(commit)) {
    info.message = message;
  } else {
    LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit message for oid \"{}\"",
              oidStr);
    git_commit_free(commit);
    return false;
  }

  if (const char *summary = git_commit_summary(commit)) {
    info.summary = summary;
  } else {
    LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit summary for oid \"{}\"",
              oidStr);
    git_commit_free(commit);
    return false;
  }

  if (const git_signature *signature = git_commit_author(commit)) {
    if (signature->name) {
      info.authorName = signature->name;
    }

    if (signature->email) {
      info.authorEmail = signature->email;
    }

    info.timeStamp =
        signature->when.time * 1000 + signature->when.offset * 60 * 1000;
  } else {
    LOG_ERROR(Log::ErrorId::Git, "Can't lookup commit signature for oid \"{}\"",
              oidStr);
    git_commit_free(commit);
    return false;
  }

  QString commitSignature;
  QString commitSignatureType;
  loadCommitSignatureInfo(repo, &oid, &commitSignature, &commitSignatureType);
  info.signedCommit = commitSignatureType != QStringLiteral("Unsigned") &&
                      commitSignatureType != QStringLiteral("Unavailable") &&
                      !commitSignature.isEmpty();
  info.signatureType = commitSignatureType;
  info.signature = commitSignature;

  info.branches += branchName;

  git_commit_free(commit);

  return true;
}

GitHistory getBranchHistory(git_repository *repo, const QString &branchName,
                            const BranchType type, int page,
                            const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Getting branch history: branchName='{}', type={}, page={}",
            branchName.toStdString(), static_cast<int>(type), page);

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  const qint64 pageSize = std::max<qint64>(
      1, ConfigManager::ConfigManager::getInstance().getLong(
             ConfigManager::ConfigKey::GitRepoHistoryPageSize));

  const QString cursorKey =
      QStringLiteral("%1|%2").arg(static_cast<int>(type)).arg(branchName);

  // The commit order is computed once per history (on page 0) and reused for
  // the following pages, so paging does not walk the repository again.
  QSharedPointer<HistoryCursor> cursor;

  if (page > 0) {
    LOCKGUARD(gHistoryCursorsMutex);
    cursor = gHistoryCursors.value(repo);
    if (cursor && cursor->key != cursorKey) {
      cursor.reset();
    }
  }

  if (!cursor) {
    cursor = buildHistoryCursor(repo, branchName, type, cursorKey, quitFlag);
    if (!cursor) {
      LOG_PERF(0, "Exit");
      return {};
    }

    LOCKGUARD(gHistoryCursorsMutex);
    gHistoryCursors[repo] = cursor;
  }

  GitHistory history_;

  const qint64 total = cursor->oids.size();
  const qint64 begin = page > 0 ? std::min<qint64>(total, page * pageSize) : 0;
  const qint64 end =
      page < 0 ? total : std::min<qint64>(total, begin + pageSize);

  history_.commits.reserve(static_cast<int>(end - begin));

  for (qint64 i = begin; i < end && !quitFlag; ++i) {
    if (!loadHistoryCommit(repo, cursor->oids[static_cast<int>(i)], branchName,
                           history_)) {
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  history_.valid = true;
  history_.total = total;
  history_.hasMore = end < total;

  if (page <= 0) {
    history_.headCommit = cursor->headCommit;
    history_.refTips = cursor->refTips;
    history_.refLabels = cursor->refLabels;
    history_.bisectMarks = getBisectMarks(repo);
  }

  LOG_PERF(0, "Exit");

  return history_;
}

QStringList getCommitBranches(git_repository *repo, const QString &commitOid,
                              const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  if (commitOid.isEmpty()) {
    LOG_WARN("Commit \"{}\" is empty", commitOid);
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::GetCommitBranchesFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    const char *branchName = nullptr;

    error = git_branch_name(&branchName, ref);
    if (error) {
      setError(GitError::GetCommitBranchesFailedToReadBranchName, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (!branchName) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    // A copy: the tip object of a symbolic branch is freed below.
    git_oid branchTipOid;
    const git_oid *branchOid = nullptr;

    if (git_reference_type(ref) == GIT_REFERENCE_SYMBOLIC) {
      git_reference *symbolicRef = nullptr;
      error = git_reference_resolve(&symbolicRef, ref);
      if (error) {
        setError(GitError::GetCommitBranchesFailedToResolveReference, error);
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_object *branchTip = nullptr;

      error = git_reference_peel(&branchTip, symbolicRef, GIT_OBJECT_COMMIT);
      if (error) {
        setError(GitError::GetCommitBranchesFailedToPeelReference, error);
        git_reference_free(symbolicRef);
        git_reference_free(ref);
        git_branch_iterator_free(iter);
        LOG_PERF(0, "Exit");
        return {};
      }

      git_oid_cpy(&branchTipOid, git_object_id(branchTip));
      branchOid = &branchTipOid;

      git_object_free(branchTip);
      git_reference_free(symbolicRef);
    } else {
      branchOid = git_reference_target(ref);
    }

    if (!branchOid) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      setError(GitError::GetCommitBranchesFailedToParseObjectId, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    // The branch contains the commit when it is the branch's tip or one of
    // its ancestors; git_graph_descendant_of doesn't count a commit as its own
    // descendant.
    if (git_oid_equal(branchOid, &oid) != 0 ||
        git_graph_descendant_of(repo, branchOid, &oid) == 1) {
      branches += branchName;
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return branches;
}

QStringList getCommitTags(git_repository *repo, const QString &commitOid,
                          const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList tags;
  git_strarray array = {nullptr, 0};

  const int error = git_tag_list_match(&array, "", repo);
  if (error) {
    setError(GitError::GetCommitTagsFailedToListMatchingTags, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  const QByteArray commitOidUtf8 = commitOid.toUtf8();

  for (size_t i = 0; i < array.count && !quitFlag; ++i) {
    const QString tagName = array.strings[i];
    git_object *object = nullptr;

    const QString spec = QStringLiteral("refs/tags/") + tagName;

    if (git_revparse_single(&object, repo, spec.toStdString().c_str()) != 0) {
      continue;
    }

    git_object *commitObject = nullptr;
    if (git_object_peel(&commitObject, object, GIT_OBJECT_COMMIT) == 0) {
      const git_oid *commitOidValue = git_object_id(commitObject);
      if (commitOidValue &&
          git_oid_streq(commitOidValue, commitOidUtf8.constData())) {
        tags.append(tagName);
      }
      git_object_free(commitObject);
    }

    git_object_free(object);
  }

  git_strarray_dispose(&array);

  LOG_PERF(0, "Exit");
  return tags;
}

QStringList getCommitHeads(git_repository *repo, const QString &commitOid,
                           const bool &quitFlag) {
  LOG_PERF(0, "Enter");
  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList branches;

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    setError(GitError::GetCommitHeadsFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    git_object *branchTip = nullptr;
    error = git_reference_peel(&branchTip, ref, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::GetCommitHeadsFailedToPeelReference, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return {};
    }

    const char *branchName = nullptr;
    error = git_branch_name(&branchName, ref);
    if (error) {
      setError(GitError::GetCommitHeadsFailedToReadBranchName, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return {};
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      setError(GitError::GetCommitHeadsFailedToParseObjectId, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return {};
    }

    // The commit is the head of the branch when it is the branch's tip. The
    // peeled tip also covers symbolic branches such as origin/HEAD, which
    // have no direct target. git_oid_equal is non-zero for equal ids.
    if (git_oid_equal(git_object_id(branchTip), &oid) != 0) {
      branches += branchName;
    }

    git_object_free(branchTip);
    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return branches;
}

bool getCommitIsHead(git_repository *repo, const QString &commitOid,
                     const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_branch_iterator *iter = nullptr;
  git_reference *ref = nullptr;
  git_branch_t type;

  int error = git_branch_iterator_new(&iter, repo, GIT_BRANCH_ALL);
  if (error) {
    setError(GitError::GetCommitIsHeadFailedToCreateBranchIterator, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Traverse all branches
  while (git_branch_next(&ref, &type, iter) == 0 && !quitFlag) {
    git_object *branchTip = nullptr;
    error = git_reference_peel(&branchTip, ref, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::GetCommitIsHeadFailedToPeelReference, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *branchName = nullptr;
    error = git_branch_name(&branchName, ref);
    if (error) {
      setError(GitError::GetCommitIsHeadFailedToReadBranchName, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_oid oid;
    error = git_oid_fromstr(&oid, commitOid.toStdString().c_str());
    if (error) {
      setError(GitError::GetCommitIsHeadFailedToParseObjectId, error);
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      git_object_free(branchTip);
      LOG_PERF(0, "Exit");
      return false;
    }

    // The commit is a head when it is the tip of a branch, see
    // getCommitHeads. git_oid_equal is non-zero for equal ids.
    const bool head = git_oid_equal(git_object_id(branchTip), &oid) != 0;
    git_object_free(branchTip);
    if (head) {
      git_reference_free(ref);
      git_branch_iterator_free(iter);
      LOG_PERF(0, "Exit");
      return true;
    }

    git_reference_free(ref);
  }

  git_branch_iterator_free(iter);

  LOG_PERF(0, "Exit");

  return false;
}

GitCommitStatus getCommitStatus(git_repository *repo,
                                const QString &commitOid) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  GitCommitStatus status;

  git_commit *commit = nullptr;
  git_commit *parent = nullptr;
  git_tree *tree = nullptr;
  git_tree *parent_tree = nullptr;
  git_diff *diff = nullptr;

  // Lookup commit by SHA
  git_oid oid;
  git_oid_fromstr(&oid, commitOid.toStdString().c_str());

  int error = git_commit_lookup(&commit, repo, &oid);
  if (error) {
    setError(GitError::GetCommitStatusFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get commit tree
  error = git_commit_tree(&tree, commit);
  if (error) {
    setError(GitError::GetCommitStatusFailedToGetCommitTree, error);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  int parentCount = git_commit_parentcount(commit);

  // Get parent tree (if exists)
  if (parentCount > 0) {
    error = git_commit_parent(&parent, commit, 0);
    if (error) {
      setError(GitError::GetCommitStatusFailedToGetParentCommit, error);
      git_tree_free(tree);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }

    error = git_commit_tree(&parent_tree, parent);
    if (error) {
      setError(GitError::GetCommitStatusFailedToGetSecondCommitTree, error);
      git_tree_free(tree);
      git_commit_free(parent);
      git_commit_free(commit);
      LOG_PERF(0, "Exit");
      return {};
    }
  } else {
    parent_tree = nullptr; // Initial commit
  }

  // Generate diff
  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.flags |= GIT_DIFF_INCLUDE_UNTRACKED | GIT_DIFF_INCLUDE_TYPECHANGE |
                GIT_DIFF_RECURSE_UNTRACKED_DIRS;

  error = git_diff_tree_to_tree(&diff, repo, parent_tree, tree, &opts);
  if (error) {
    setError(GitError::GetCommitStatusFailedToCreateCommitDiff, error);

    git_tree_free(tree);

    if (parent_tree) {
      git_tree_free(parent_tree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Detect renames
  git_diff_find_options find_opts = GIT_DIFF_FIND_OPTIONS_INIT;
  find_opts.flags = GIT_DIFF_FIND_RENAMES;

  error = git_diff_find_similar(diff, &find_opts);
  if (error) {
    setError(GitError::GetCommitStatusFailedToDetectRenames, error);

    git_diff_free(diff);

    if (parent_tree) {
      git_tree_free(parent_tree);
    }

    if (parent) {
      git_commit_free(parent);
    }

    git_tree_free(tree);
    git_commit_free(commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  status.commit = commitOid;

  if (const char *summary = git_commit_summary(commit)) {
    status.summary = QString::fromUtf8(summary);
  } else if (const char *message = git_commit_message(commit)) {
    status.summary = QString::fromUtf8(message).section('\n', 0, 0);
  }

  parentCount = git_commit_parentcount(commit);
  status.parentShas.reserve(parentCount);
  for (int i = 0; i < parentCount; ++i) {
    const git_oid *parentOid =
        git_commit_parent_id(commit, static_cast<unsigned int>(i));
    if (!parentOid) {
      continue;
    }

    char parentOidStr[GIT_OID_HEXSZ + 1] = {0};
    git_oid_tostr(parentOidStr, GIT_OID_HEXSZ + 1, parentOid);
    status.parentShas += QString::fromUtf8(parentOidStr);
  }

  if (const git_signature *author = git_commit_author(commit)) {
    if (author->name) {
      status.authorName = QString::fromUtf8(author->name);
    }
    if (author->email) {
      status.authorEmail = QString::fromUtf8(author->email);
    }
  }

  status.committerDate =
      commitSignatureDateToString(git_commit_committer(commit));

  // Walk the diff
  git_diff_foreach(
      diff,
      [](const git_diff_delta *delta, float, void *payload) {
        auto *status = static_cast<GitCommitStatus *>(payload);

        switch (delta->status) {
        case GIT_DELTA_ADDED:
          status->newFiles += delta->new_file.path;
          break;
        case GIT_DELTA_MODIFIED:
          status->modifiedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_DELETED:
          status->deletedFiles += delta->old_file.path;
          break;
        case GIT_DELTA_RENAMED:
          status->renamedFiles += delta->new_file.path;
          break;
        case GIT_DELTA_TYPECHANGE:
          status->typeChangedFiles += delta->new_file.path;
          break;
        default:
          break;
        }

        return 0;
      },
      nullptr, nullptr, nullptr, &status);

  // Cleanup in reverse order of allocation
  git_diff_free(diff);

  if (parent_tree) {
    git_tree_free(parent_tree);
  }

  if (parent) {
    git_commit_free(parent);
  }

  git_tree_free(tree);
  git_commit_free(commit);

  LOG_PERF(0, "Exit");

  return status;
}

} // namespace GitRepo
