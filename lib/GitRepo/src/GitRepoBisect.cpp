// MIT License
// Copyright (c) 2026 Sourcerer

#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QHash>
#include <QSet>
#include <QVector>

#include <memory>
#include <utility>

#define THREAD_ID "GitRepo"

// Bisect sessions.
namespace GitRepo {

using namespace Private;

namespace {
using BisectCommitPtr = std::unique_ptr<git_commit, decltype(&git_commit_free)>;
using BisectObjectPtr = std::unique_ptr<git_object, decltype(&git_object_free)>;
using BisectRefPtr =
    std::unique_ptr<git_reference, decltype(&git_reference_free)>;
using BisectIteratorPtr =
    std::unique_ptr<git_reference_iterator,
                    decltype(&git_reference_iterator_free)>;
using BisectRevwalkPtr =
    std::unique_ptr<git_revwalk, decltype(&git_revwalk_free)>;

constexpr auto BISECT_REF_BAD = "refs/bisect/bad";
constexpr auto BISECT_REF_GOOD_PREFIX = "refs/bisect/good-";
constexpr auto BISECT_REF_SKIP_PREFIX = "refs/bisect/skip-";
constexpr auto BISECT_START_FILE = "BISECT_START";
constexpr auto BISECT_LOG_FILE_NAME = "BISECT_LOG";
constexpr auto BISECT_TERMS_FILE = "BISECT_TERMS";
constexpr auto BISECT_NAMES_FILE = "BISECT_NAMES";

struct BisectMarks {
  bool hasBad = false;
  git_oid bad{};
  QVector<git_oid> good;
  QVector<git_oid> skip;
};
} // namespace

static QString bisectOidToString(const git_oid *oid) {
  char buffer[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(buffer, sizeof(buffer), oid);
  return QString::fromLatin1(buffer);
}

static QString bisectStatePath(git_repository *repo, const char *name) {
  return QDir(QString::fromUtf8(git_repository_path(repo)))
      .filePath(QString::fromLatin1(name));
}

static QString bisectCommitSummary(git_repository *repo, const git_oid *oid) {
  git_commit *rawCommit = nullptr;
  if (git_commit_lookup(&rawCommit, repo, oid) != 0) {
    return {};
  }
  const BisectCommitPtr commit(rawCommit, git_commit_free);

  const char *summary = git_commit_summary(commit.get());
  return summary ? QString::fromUtf8(summary) : QString();
}

static bool bisectWriteFile(const QString &path, const QString &content) {
  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  return file.write(content.toUtf8()) >= 0;
}

static void bisectAppendLog(git_repository *repo, const QString &text) {
  QFile file(bisectStatePath(repo, BISECT_LOG_FILE_NAME));
  if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
    file.write(text.toUtf8());
  }
}

static int bisectListRefs(git_repository *repo,
                          QVector<std::pair<QString, git_oid>> &refs) {
  git_reference_iterator *rawIterator = nullptr;
  int error =
      git_reference_iterator_glob_new(&rawIterator, repo, "refs/bisect/*");
  if (error) {
    return error;
  }
  const BisectIteratorPtr iterator(rawIterator, git_reference_iterator_free);

  git_reference *rawRef = nullptr;
  while ((error = git_reference_next(&rawRef, iterator.get())) == 0) {
    const BisectRefPtr ref(rawRef, git_reference_free);

    const char *name = git_reference_name(ref.get());
    const git_oid *target = git_reference_target(ref.get());
    if (!name || !target) {
      continue;
    }

    refs.append({QString::fromUtf8(name), *target});
  }

  return error == GIT_ITEROVER ? 0 : error;
}

static int bisectReadMarks(git_repository *repo, BisectMarks &marks) {
  QVector<std::pair<QString, git_oid>> refs;
  const int error = bisectListRefs(repo, refs);
  if (error) {
    return error;
  }

  for (const auto &[name, oid] : refs) {
    if (name == QLatin1String(BISECT_REF_BAD)) {
      marks.hasBad = true;
      marks.bad = oid;
    } else if (name.startsWith(QLatin1String(BISECT_REF_GOOD_PREFIX))) {
      marks.good.append(oid);
    } else if (name.startsWith(QLatin1String(BISECT_REF_SKIP_PREFIX))) {
      marks.skip.append(oid);
    }
  }

  return 0;
}

static void bisectRemoveRefs(git_repository *repo) {
  QVector<std::pair<QString, git_oid>> refs;
  if (bisectListRefs(repo, refs) != 0) {
    return;
  }

  for (const auto &ref : refs) {
    git_reference_remove(repo, ref.first.toUtf8().constData());
  }
}

static int bisectCheckout(git_repository *repo, const git_oid *oid,
                          const char *branchRefName) {
  git_object *rawObject = nullptr;
  int error = git_object_lookup(&rawObject, repo, oid, GIT_OBJECT_COMMIT);
  if (error) {
    return error;
  }
  const BisectObjectPtr object(rawObject, git_object_free);

  git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;
  opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  error = git_checkout_tree(repo, object.get(), &opts);
  if (error) {
    return error;
  }

  return branchRefName ? git_repository_set_head(repo, branchRefName)
                       : git_repository_set_head_detached(repo, oid);
}

static bool bisectFinish(git_repository *repo, const git_oid &bad,
                         BisectStatus &status) {
  status.finished = true;
  status.firstBadCommit = bisectOidToString(&bad);
  status.firstBadSummary = bisectCommitSummary(repo, &bad);

  bisectAppendLog(repo,
                  QString("# first bad commit: [%1] %2\n")
                      .arg(status.firstBadCommit, status.firstBadSummary));
  return true;
}

// Finds the next commit to test and checks it out.
static bool bisectAdvance(git_repository *repo, BisectStatus &status) {
  BisectMarks marks;
  int error = bisectReadMarks(repo, marks);
  if (error) {
    setError(GitError::BisectFailedToReadState, error);
    return false;
  }

  status.active = true;
  status.waitingForBad = !marks.hasBad;
  status.waitingForGood = marks.good.isEmpty();
  if (status.waitingForBad || status.waitingForGood) {
    return true;
  }

  git_revwalk *rawWalker = nullptr;
  error = git_revwalk_new(&rawWalker, repo);
  if (error) {
    setError(GitError::BisectFailedToWalkHistory, error);
    return false;
  }
  const BisectRevwalkPtr walker(rawWalker, git_revwalk_free);

  // Parents are listed before their children.
  git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_REVERSE);

  error = git_revwalk_push(walker.get(), &marks.bad);
  for (int i = 0; !error && i < marks.good.size(); ++i) {
    error = git_revwalk_hide(walker.get(), &marks.good[i]);
  }
  if (error) {
    setError(GitError::BisectFailedToWalkHistory, error);
    return false;
  }

  // Candidates are the commits reachable from the bad commit and not from any
  // of the good commits.
  QVector<git_oid> oids;
  QVector<QString> hexes;
  QHash<QString, int> indexOf;

  git_oid oid;
  while ((error = git_revwalk_next(&oid, walker.get())) == 0) {
    const QString hex = bisectOidToString(&oid);
    indexOf.insert(hex, oids.size());
    hexes.append(hex);
    oids.append(oid);
  }
  if (error != GIT_ITEROVER) {
    setError(GitError::BisectFailedToWalkHistory, error);
    return false;
  }

  const int count = oids.size();
  if (count == 0) {
    // The good commit is the bad commit or one of its descendants.
    setError(GitError::BisectInvalidRange, -1);
    return false;
  }

  status.remaining = count;
  status.steps = 0;
  for (int n = count; n > 1; n /= 2) {
    ++status.steps;
  }

  if (count == 1) {
    return bisectFinish(repo, marks.bad, status);
  }

  QSet<QString> skipped;
  for (const auto &skipOid : marks.skip) {
    skipped.insert(bisectOidToString(&skipOid));
  }

  QVector<QVector<int>> parents(count);
  for (int i = 0; i < count; ++i) {
    git_commit *rawCommit = nullptr;
    if (git_commit_lookup(&rawCommit, repo, &oids[i]) != 0) {
      setError(GitError::BisectFailedToWalkHistory, -1);
      return false;
    }
    const BisectCommitPtr commit(rawCommit, git_commit_free);

    const unsigned int parentCount = git_commit_parentcount(commit.get());
    for (unsigned int p = 0; p < parentCount; ++p) {
      const int parentIndex = indexOf.value(
          bisectOidToString(git_commit_parent_id(commit.get(), p)), -1);
      if (parentIndex >= 0) {
        parents[i].append(parentIndex);
      }
    }
  }

  // weight[i] is the number of candidates reachable from candidate i,
  // including itself. It only affects how well the range is halved.
  constexpr int EXACT_MERGE_WEIGHT_LIMIT = 5000;
  QVector<int> weight(count, 1);
  QVector<int> stamp(count, -1);
  for (int i = 0; i < count; ++i) {
    const auto &commitParents = parents[i];
    if (commitParents.isEmpty()) {
      weight[i] = 1;
    } else if (commitParents.size() == 1) {
      weight[i] = weight[commitParents.first()] + 1;
    } else if (count <= EXACT_MERGE_WEIGHT_LIMIT) {
      int reachable = 0;
      QVector<int> stack{i};
      stamp[i] = i;
      while (!stack.isEmpty()) {
        const int current = stack.takeLast();
        ++reachable;
        for (const int parent : parents[current]) {
          if (stamp[parent] != i) {
            stamp[parent] = i;
            stack.append(parent);
          }
        }
      }
      weight[i] = reachable;
    } else {
      int sum = 1;
      for (const int parent : commitParents) {
        sum += weight[parent];
      }
      weight[i] = qMin(sum, count);
    }
  }

  int best = -1;
  int bestScore = -1;
  for (int i = 0; i < count; ++i) {
    if (git_oid_equal(&oids[i], &marks.bad) || skipped.contains(hexes[i])) {
      continue;
    }

    const int score = qMin(weight[i], count - weight[i]);
    if (score > bestScore) {
      bestScore = score;
      best = i;
    }
  }

  if (best < 0) {
    // Every remaining candidate except the bad commit was skipped.
    status.ambiguous = true;
    return bisectFinish(repo, marks.bad, status);
  }

  error = bisectCheckout(repo, &oids[best], nullptr);
  if (error) {
    setError(GitError::BisectFailedToCheckout, error);
    return false;
  }

  status.currentCommit = hexes[best];
  status.currentSummary = bisectCommitSummary(repo, &oids[best]);

  return true;
}

bool bisect(git_repository *repo, BisectAction action,
            const QString &commitArgument, BisectStatus &status) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  // Copied before `status` is reset: the caller may pass
  // status.currentCommit.
  const QString commit = commitArgument;
  status = BisectStatus();

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString startFilePath = bisectStatePath(repo, BISECT_START_FILE);
  const bool inProgress = QFile::exists(startFilePath);

  if (action == BisectAction::Reset) {
    if (!inProgress) {
      setError(GitError::BisectNotInProgress, -1);
      LOG_PERF(0, "Exit");
      return false;
    }

    QString start;
    {
      QFile startFile(startFilePath);
      if (startFile.open(QIODevice::ReadOnly)) {
        start = QString::fromUtf8(startFile.readAll()).trimmed();
      }
    }

    if (!start.isEmpty()) {
      const QString branchRefName = QString("refs/heads/%1").arg(start);

      git_reference *rawBranchRef = nullptr;
      git_oid startOid;
      int error = 0;
      const bool isBranch =
          git_reference_lookup(&rawBranchRef, repo,
                               branchRefName.toUtf8().constData()) == 0;
      if (isBranch) {
        const BisectRefPtr branchRef(rawBranchRef, git_reference_free);
        git_oid branchOid;
        const git_oid *target = git_reference_target(branchRef.get());
        if (target) {
          branchOid = *target;
          error = bisectCheckout(repo, &branchOid,
                                 branchRefName.toUtf8().constData());
        } else {
          error = -1;
        }
      } else {
        error = git_oid_fromstr(&startOid, start.toUtf8().constData());
        if (!error) {
          error = bisectCheckout(repo, &startOid, nullptr);
        }
      }

      if (error) {
        setError(GitError::BisectFailedToResetRepository, error);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    bisectRemoveRefs(repo);
    QFile::remove(startFilePath);
    // Including the files only git writes, like `git bisect reset`.
    for (const char *name :
         {BISECT_LOG_FILE_NAME, BISECT_TERMS_FILE, BISECT_NAMES_FILE,
          "BISECT_EXPECTED_REV", "BISECT_ANCESTORS_OK", "BISECT_FIRST_PARENT",
          "BISECT_HEAD", "BISECT_RUN"}) {
      QFile::remove(bisectStatePath(repo, name));
    }

    LOG_PERF(0, "Exit");
    return true;
  }

  if (commit.isEmpty()) {
    setError(GitError::BisectInvalidArguments, -1);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object *rawObject = nullptr;
  int error =
      git_revparse_single(&rawObject, repo, commit.toUtf8().constData());
  if (error) {
    setError(GitError::BisectFailedToResolveCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  const BisectObjectPtr resolved(rawObject, git_object_free);

  git_object *rawCommitObject = nullptr;
  error = git_object_peel(&rawCommitObject, resolved.get(), GIT_OBJECT_COMMIT);
  if (error) {
    setError(GitError::BisectFailedToResolveCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  const BisectObjectPtr commitObject(rawCommitObject, git_object_free);
  const git_oid markOid = *git_object_id(commitObject.get());
  const QString markHex = bisectOidToString(&markOid);

  if (!inProgress) {
    if (git_repository_state(repo) != GIT_REPOSITORY_STATE_NONE) {
      LOG_ERROR(Log::ErrorId::Git,
                "Cannot start bisect while another operation is in progress");
      setError(GitError::BisectRepositoryBusy, -1);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Remember where to return to on reset.
    git_reference *rawHead = nullptr;
    error = git_repository_head(&rawHead, repo);
    if (error) {
      setError(GitError::BisectInvalidArguments, error);
      LOG_PERF(0, "Exit");
      return false;
    }
    const BisectRefPtr head(rawHead, git_reference_free);

    QString start;
    if (git_repository_head_detached(repo) == 1) {
      start = bisectOidToString(git_reference_target(head.get()));
    } else {
      start = QString::fromUtf8(git_reference_shorthand(head.get()));
    }

    // Make sure there are no leftovers of a previous session.
    bisectRemoveRefs(repo);

    // BISECT_TERMS and BISECT_NAMES (no pathspec) are what `git bisect start`
    // writes too; `git bisect good` and `bad` need them.
    if (!bisectWriteFile(startFilePath, start + "\n") ||
        !bisectWriteFile(bisectStatePath(repo, BISECT_TERMS_FILE),
                         QStringLiteral("bad\ngood\n")) ||
        !bisectWriteFile(bisectStatePath(repo, BISECT_NAMES_FILE),
                         QStringLiteral("\n")) ||
        !bisectWriteFile(bisectStatePath(repo, BISECT_LOG_FILE_NAME),
                         QStringLiteral("git bisect start\n"))) {
      QFile::remove(startFilePath);
      QFile::remove(bisectStatePath(repo, BISECT_TERMS_FILE));
      QFile::remove(bisectStatePath(repo, BISECT_NAMES_FILE));
      QFile::remove(bisectStatePath(repo, BISECT_LOG_FILE_NAME));
      setError(GitError::BisectFailedToWriteState, -1);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const QString goodRefName =
      QString("%1%2").arg(QLatin1String(BISECT_REF_GOOD_PREFIX), markHex);
  const QString skipRefName =
      QString("%1%2").arg(QLatin1String(BISECT_REF_SKIP_PREFIX), markHex);

  // A commit has a single mark, so a new one replaces the previous one.
  git_reference_remove(repo, goodRefName.toUtf8().constData());
  git_reference_remove(repo, skipRefName.toUtf8().constData());

  QString refName;
  QString stateName;
  switch (action) {
  case BisectAction::Good:
    refName = goodRefName;
    stateName = QStringLiteral("good");
    break;
  case BisectAction::Bad:
    refName = QLatin1String(BISECT_REF_BAD);
    stateName = QStringLiteral("bad");
    break;
  case BisectAction::Skip:
    refName = skipRefName;
    stateName = QStringLiteral("skip");
    break;
  default:
    setError(GitError::BisectInvalidArguments, -1);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *rawMarkRef = nullptr;
  error = git_reference_create(&rawMarkRef, repo, refName.toUtf8().constData(),
                               &markOid, 1, "bisect");
  if (error) {
    setError(GitError::BisectFailedToWriteState, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  git_reference_free(rawMarkRef);

  bisectAppendLog(
      repo, QString("# %1: [%2] %3\ngit bisect %1 %2\n")
                .arg(stateName, markHex, bisectCommitSummary(repo, &markOid)));

  const bool advanced = bisectAdvance(repo, status);

  LOG_PERF(0, "Exit");
  return advanced;
}

QHash<QString, int> getBisectMarks(git_repository *repo) {
  LOG_PERF(0, "Enter");

  QHash<QString, int> result;

  if (!repo || !QFile::exists(bisectStatePath(repo, BISECT_START_FILE))) {
    LOG_PERF(0, "Exit");
    return result;
  }

  BisectMarks marks;
  if (bisectReadMarks(repo, marks) != 0) {
    LOG_PERF(0, "Exit");
    return result;
  }

  for (const auto &oid : marks.good) {
    result.insert(bisectOidToString(&oid), static_cast<int>(BisectMark::Good));
  }
  for (const auto &oid : marks.skip) {
    result.insert(bisectOidToString(&oid), static_cast<int>(BisectMark::Skip));
  }
  if (marks.hasBad) {
    result.insert(bisectOidToString(&marks.bad),
                  static_cast<int>(BisectMark::Bad));
  }

  // The commit being tested is the detached HEAD once the range is known.
  if (marks.hasBad && !marks.good.isEmpty() &&
      git_repository_head_detached(repo) == 1) {
    git_reference *rawHead = nullptr;
    if (git_repository_head(&rawHead, repo) == 0) {
      const BisectRefPtr head(rawHead, git_reference_free);
      if (const git_oid *target = git_reference_target(head.get())) {
        const QString hex = bisectOidToString(target);
        if (!result.contains(hex)) {
          result.insert(hex, static_cast<int>(BisectMark::Current));
        }
      }
    }
  }

  LOG_PERF(0, "Exit");
  return result;
}

} // namespace GitRepo
