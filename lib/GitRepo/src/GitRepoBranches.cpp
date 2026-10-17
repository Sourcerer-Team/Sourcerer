#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include "GitKeyStorage.h"
#include "GitLfs.h"

#include <Log.h>
#include <Uuid.h>

#include <git2/sys/errors.h>

#include <QRegularExpression>
#include <QVector>

#include <tuple>

#define THREAD_ID "GitRepo"

// Branches and tags.
namespace GitRepo {

using namespace Private;

// The local branch name for a remote branch such as "origin/feature/x": the
// name without its remote, "feature/x".
static QString localNameOfRemoteBranch(git_repository *repo,
                                       const QString &remoteBranchName) {
  const QByteArray refName =
      QStringLiteral("refs/remotes/%1").arg(remoteBranchName).toUtf8();
  git_buf remoteName = GIT_BUF_INIT;
  if (git_branch_remote_name(&remoteName, repo, refName.constData()) == 0) {
    const QString remote = QString::fromUtf8(remoteName.ptr);
    git_buf_dispose(&remoteName);
    if (remoteBranchName.startsWith(remote + '/')) {
      return remoteBranchName.mid(remote.length() + 1);
    }
  }
  git_buf_dispose(&remoteName);

  // No configured remote matches, take the first component as its name.
  const int slash = remoteBranchName.indexOf('/');
  return slash >= 0 ? remoteBranchName.mid(slash + 1) : remoteBranchName;
}

bool checkoutBranch(git_repository *repo, const QString &branchOrTagName,
                    const BranchType type) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to checkout branch/tag: '{}', type: {}",
            branchOrTagName.toStdString(), static_cast<int>(type));

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when checking out branch '{}'",
              branchOrTagName.toStdString());
    LOG_PERF(0, "Exit");
    return false;
  }

  if (type == BranchType::Local) {
    LOG_DEBUG("Checking out local branch: '{}'", branchOrTagName.toStdString());
    git_tree *tree = nullptr;
    git_reference *localRef = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    int error = git_branch_lookup(&localRef, repo,
                                  branchOrTagName.toStdString().c_str(),
                                  GIT_BRANCH_LOCAL);
    if (error) {
      setError(GitError::CheckoutBranchFailedToLookupBranch, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Resolve the reference to a commit
    git_commit *targetCommit = nullptr;
    error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                               localRef, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::CheckoutBranchFailedToPeelLocalBranchToCommit, error);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 3. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      setError(GitError::CheckoutBranchFailedToGetCommitTree, error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 4. Set checkout strategy
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 5. Checkout the tree
    error =
        git_checkout_tree(repo, reinterpret_cast<git_object *>(tree), &opts);
    if (error) {
      setError(GitError::CheckoutBranchFailedToCheckoutLocalBranchTree, error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      setError(GitError::CheckoutBranchFailedToGetReferenceName, error);
      git_commit_free(targetCommit);
      git_reference_free(localRef);
      git_tree_free(tree);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_commit_free(targetCommit);
    git_reference_free(localRef);
    git_tree_free(tree);
  } else if (type == BranchType::Remote) {
    git_tree *tree = nullptr;
    git_reference *localRef = nullptr;
    git_reference *remoteRef = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    const QString localName = localNameOfRemoteBranch(repo, branchOrTagName);

    const QString fullBranchName =
        QString("refs/remotes/%1").arg(branchOrTagName);

    // 1. Lookup the branch reference
    int error = git_reference_lookup(&remoteRef, repo,
                                     fullBranchName.toStdString().c_str());
    if (error) {
      setError(GitError::CheckoutBranchFailedToLookupReference, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Resolve the reference to a commit
    git_commit *targetCommit = nullptr;
    error = git_reference_peel(reinterpret_cast<git_object **>(&targetCommit),
                               remoteRef, GIT_OBJECT_COMMIT);
    if (error) {
      setError(GitError::CheckoutBranchFailedToPeelRemoteBranchToCommit, error);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 3. Create local branch that tracks the remote branch
    bool trackRemote = true;
    error = git_branch_create(&localRef, repo, localName.toStdString().c_str(),
                              targetCommit, 0);
    if (error) {
      if (error == GIT_EEXISTS) {
        // 3.1 Branch already exists, try to lookup it
        error = git_branch_lookup(
            &localRef, repo, localName.toStdString().c_str(), GIT_BRANCH_LOCAL);
        if (error) {
          setError(GitError::CheckoutBranchFailedToLookupDestinationBranch,
                   error);
          git_commit_free(targetCommit);
          git_reference_free(remoteRef);
          LOG_PERF(0, "Exit");
          return false;
        }

        // 3.2 Like `git checkout <branch>`, the existing branch is checked out
        // where it is, it is not moved to the remote commit.
        git_commit *localCommit = nullptr;
        error = git_reference_peel(reinterpret_cast<git_object **>(&localCommit),
                                   localRef, GIT_OBJECT_COMMIT);
        if (error) {
          setError(GitError::CheckoutBranchFailedToPeelLocalBranchToCommit,
                   error);
          git_reference_free(localRef);
          git_commit_free(targetCommit);
          git_reference_free(remoteRef);
          LOG_PERF(0, "Exit");
          return false;
        }
        git_commit_free(targetCommit);
        targetCommit = localCommit;

        // 3.3 An upstream the branch already has is kept.
        git_reference *upstreamRef = nullptr;
        if (git_branch_upstream(&upstreamRef, localRef) == 0) {
          trackRemote = false;
          git_reference_free(upstreamRef);
        }
      } else {
        setError(GitError::CheckoutBranchFailedToCreateLocalBranchFromRemote,
                 error);
        git_commit_free(targetCommit);
        git_reference_free(remoteRef);
        LOG_PERF(0, "Exit");
        return false;
      }
    }

    // 4. Set up tracking
    error = trackRemote ? git_branch_set_upstream(
                              localRef, branchOrTagName.toStdString().c_str())
                        : 0;
    if (error) {
      setError(GitError::CheckoutBranchFailedToSetBranchUpstream, error);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 5. Get the tree from the commit
    error = git_commit_tree(&tree, targetCommit);
    if (error) {
      setError(GitError::CheckoutBranchFailedToGetSecondCommitTree, error);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Set checkout strategy
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 7. Checkout the tree
    error =
        git_checkout_tree(repo, reinterpret_cast<git_object *>(tree), &opts);
    if (error) {
      setError(GitError::CheckoutBranchFailedToCheckoutTagTree, error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 6. Move HEAD to the branch
    error = git_repository_set_head(repo, git_reference_name(localRef));
    if (error) {
      setError(GitError::CheckoutBranchFailedToSetHeadToTagReference, error);
      git_tree_free(tree);
      git_reference_free(localRef);
      git_commit_free(targetCommit);
      git_reference_free(remoteRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Cleanup
    git_tree_free(tree);
    git_reference_free(localRef);
    git_commit_free(targetCommit);
    git_reference_free(remoteRef);
  } else if (type == BranchType::Tag) {
    git_object *tagTarget = nullptr;
    git_checkout_options opts = GIT_CHECKOUT_OPTIONS_INIT;

    // 1. Resolve the tag to its target commit/tree
    int error = git_revparse_single(&tagTarget, repo,
                                    branchOrTagName.toStdString().c_str());
    if (error) {
      setError(GitError::CheckoutBranchFailedToResolveRevision, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 2. Set checkout options
    opts.checkout_strategy = GIT_CHECKOUT_SAFE;

    // 3. Checkout the tree
    error = git_checkout_tree(repo, tagTarget, &opts);
    if (error) {
      setError(GitError::CheckoutBranchFailedToCheckoutTree, error);
      git_object_free(tagTarget);
      LOG_PERF(0, "Exit");
      return false;
    }

    // 4. Detach HEAD to the tag's commit
    error = git_repository_set_head_detached(repo, git_object_id(tagTarget));
    if (error) {
      setError(GitError::CheckoutBranchFailedToSetHeadDetachedAtTag, error);
      git_object_free(tagTarget);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(tagTarget);
  } else {
    LOG_ERROR(Log::ErrorId::Git, "Internal error! Unsupported branch type {}",
              static_cast<int>(type));
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

static QStringList listCheckoutBranches(git_repository *repo,
                                        const BranchType type,
                                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  bool tagRequested = false;

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
  case BranchType::Local: {
    branchType = GIT_BRANCH_LOCAL;
    break;
  }
  case BranchType::Remote: {
    branchType = GIT_BRANCH_REMOTE;
    break;
  }
  case BranchType::Tag: {
    tagRequested = true;
    break;
  }
  case BranchType::All:
  default: {
    break;
  }
  }

  if (!tagRequested && branchType == GIT_BRANCH_ALL) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList activeBranches;

  if (!tagRequested) {
    git_branch_iterator *it = nullptr;

    int error = git_branch_iterator_new(&it, repo, branchType);
    if (error) {
      setError(GitError::ListCheckoutBranchesFailedToCreateBranchIterator,
               error);
      return {};
    }

    if (!it) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
      return {};
    }

    git_reference *ref = nullptr;

    while (!quitFlag) {
      error = git_branch_next(&ref, &branchType, it);
      if (error == GIT_ITEROVER)
        break;

      if (error) {
        setError(GitError::ListCheckoutBranchesFailedToReadNextBranch, error);
        break;
      }

      if (!ref) {
        LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
        continue;
      }

      const char *branchName = nullptr;

      error = git_branch_name(&branchName, ref);
      if (error == GIT_EINVALID) {
        git_reference_free(ref);
        LOG_ERROR(Log::ErrorId::Git,
                  "Reference is not a local or a remote branch.");
        continue;
      }

      if (error) {
        git_reference_free(ref);
        setError(GitError::ListCheckoutBranchesFailedToReadBranchName, error);
        break;
      }

      if (!branchName) {
        git_reference_free(ref);
        LOG_ERROR(Log::ErrorId::Git, "Invalid branch name.");
        continue;
      }

      // git_branch_is_head() looks at this repository's HEAD only. Unlike
      // git_branch_is_checked_out() it ignores branches that are checked out
      // in other worktrees, which must not be reported as active here.
      error = git_branch_is_head(ref);
      if (error == 1) {
        activeBranches += branchName;
      } else if (error != 0) {
        git_reference_free(ref);
        setError(
            GitError::ListCheckoutBranchesFailedToCheckIfBranchIsCheckedOut,
            error);
        break;
      }

      git_reference_free(ref);
    };

    git_branch_iterator_free(it);
  }

  // Include HEAD if detached for local branches or if tag requested
  if ((type == BranchType::Local && activeBranches.isEmpty()) || tagRequested) {
    git_reference *headRef = nullptr;

    int error = git_repository_head(&headRef, repo);
    if (error) {
      setError(GitError::ListCheckoutBranchesFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return {};
    }

    // git_repository_head resolves HEAD, so its type can't tell whether HEAD is
    // detached.
    if (git_repository_head_detached(repo) == 1) {
      // Detached HEAD — could be tag or raw commit
      if (tagRequested) {
        git_strarray tags = {nullptr};

        error = git_tag_list(&tags, repo);
        if (error) {
          setError(GitError::ListCheckoutBranchesFailedToListTags, error);
          git_reference_free(headRef);
          LOG_PERF(0, "Exit");
          return {};
        }

        for (size_t i = 0; i < tags.count; ++i) {
          git_object *tagObj = nullptr;

          // Peeled to the commit, an annotated tag resolves to a tag object.
          error = git_revparse_single(
              &tagObj, repo,
              (QStringLiteral("refs/tags/") + QString::fromUtf8(tags.strings[i]) +
               QStringLiteral("^{commit}"))
                  .toUtf8()
                  .constData());
          if (error) {
            setError(GitError::ListCheckoutBranchesFailedToResolveRevision,
                     error);
            continue;
          }

          if (git_oid_equal(git_reference_target(headRef),
                            git_object_id(tagObj))) {
            // HEAD is detached at tag
            activeBranches += tags.strings[i];
            git_object_free(tagObj);
            break;
          }

          git_object_free(tagObj);
        }

        git_strarray_dispose(&tags);
      } else {
        // Raw commit
        if (const QString referenceName = git_reference_name(headRef);
            referenceName == HEAD) {
          activeBranches += HEAD;
        }
      }
    }

    git_reference_free(headRef);
  }

  LOG_PERF(0, "Exit");

  return activeBranches;
}

static QStringList listBranches(git_repository *repo, const BranchType type,
                                const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return {};
  }

  git_branch_t branchType = GIT_BRANCH_ALL;
  switch (type) {
  case BranchType::Local: {
    branchType = GIT_BRANCH_LOCAL;
    break;
  }
  case BranchType::Remote: {
    branchType = GIT_BRANCH_REMOTE;
    break;
  }
  case BranchType::All:
  default: {
    break;
  }
  }

  if (branchType == GIT_BRANCH_ALL) {
    LOG_PERF(0, "Exit");
    return {};
  }

  QStringList activeBranches;

  if (branchType == GIT_BRANCH_LOCAL) {
    git_reference *headRef = nullptr;

    if (const int error = git_repository_head(&headRef, repo)) {
      setError(GitError::ListBranchesFailedToGetRepositoryHead, error);
      LOG_PERF(0, "Exit");
      return {};
    }

    if (git_reference_type(headRef) == GIT_REFERENCE_DIRECT) {
      // Detached HEAD — could be tag or raw commit
      if (const QString referenceName = git_reference_name(headRef);
          referenceName == HEAD) {
        activeBranches += HEAD;
      }
    }

    git_reference_free(headRef);
  }

  git_branch_iterator *it = nullptr;

  int error = git_branch_iterator_new(&it, repo, branchType);
  if (error) {
    setError(GitError::ListBranchesFailedToCreateBranchIterator, error);
    return {};
  }

  if (!it) {
    LOG_ERROR(Log::ErrorId::Git, "Invalid branch iterator.");
    return {};
  }

  git_reference *ref = nullptr;
  QStringList branches;

  while (!quitFlag) {
    error = git_branch_next(&ref, &branchType, it);
    if (error == GIT_ITEROVER)
      break;

    if (error) {
      setError(GitError::ListBranchesFailedToReadNextBranch, error);
      break;
    }

    if (!ref) {
      LOG_ERROR(Log::ErrorId::Git, "Invalid reference.");
      continue;
    }

    const char *branchName = nullptr;

    error = git_branch_name(&branchName, ref);
    if (error == GIT_EINVALID) {
      git_reference_free(ref);
      LOG_ERROR(Log::ErrorId::Git,
                "Reference is not a local or a remote branch.");
      continue;
    }

    if (error) {
      git_reference_free(ref);
      setError(GitError::ListBranchesFailedToReadBranchName, error);
      break;
    }

    if (!branchName) {
      git_reference_free(ref);
      LOG_ERROR(Log::ErrorId::Git, "Invalid branch name.");
      continue;
    }

    // Symbolic references such as origin/HEAD only point to a branch, like
    // `git branch -r` shows them they are not branches of their own.
    if (git_reference_type(ref) != GIT_REFERENCE_SYMBOLIC) {
      branches += branchName;
    }

    git_reference_free(ref);
  };

  git_branch_iterator_free(it);

  // libgit2 lists loose references before packed ones.
  branches.sort();
  activeBranches += branches;

  LOG_PERF(0, "Exit");

  return activeBranches;
}

QStringList listLocalBranches(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Local, quitFlag);
}

QStringList listRemoteBranches(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return listBranches(repo, BranchType::Remote, quitFlag);
}

QVector<BranchInfo> listRemoteBranchesWithStatus(git_repository *repo,
                                                 const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QVector<BranchInfo> branchesWithStatus;

  // First get all remote branches
  const QStringList remoteBranches = listRemoteBranches(repo, quitFlag);

  if (quitFlag) {
    LOG_PERF(0, "Exit");
    return {};
  }

  // Upstream reference name -> the local branch tracking it.
  QHash<QString, QString> trackingBranches;
  git_branch_iterator *localIterator = nullptr;
  if (git_branch_iterator_new(&localIterator, repo, GIT_BRANCH_LOCAL) == 0) {
    git_reference *localBranch = nullptr;
    git_branch_t localType = GIT_BRANCH_LOCAL;
    while (git_branch_next(&localBranch, &localType, localIterator) == 0) {
      git_buf upstreamName = GIT_BUF_INIT;
      if (git_branch_upstream_name(&upstreamName, repo,
                                   git_reference_name(localBranch)) == 0) {
        const char *shortName = nullptr;
        if (git_branch_name(&shortName, localBranch) == 0 && shortName) {
          trackingBranches.insert(QString::fromUtf8(upstreamName.ptr),
                                  QString::fromUtf8(shortName));
        }
      }
      git_buf_dispose(&upstreamName);
      git_reference_free(localBranch);
    }
    git_branch_iterator_free(localIterator);
  }

  // For each remote branch, calculate its status
  for (const QString &remoteBranchName : remoteBranches) {
    if (quitFlag) {
      break;
    }

    BranchInfo branchInfo;
    branchInfo.name = remoteBranchName;

    // Get the remote reference
    const QString fullRemoteRefName =
        QString("refs/remotes/%1").arg(remoteBranchName);
    git_reference *remoteRef = nullptr;
    int error = git_reference_lookup(&remoteRef, repo,
                                     fullRemoteRefName.toStdString().c_str());
    if (error) {
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      git_reference_free(remoteRef);
      branchesWithStatus.append(branchInfo);
      continue;
    }

    const git_oid *remoteRefTargetOid = git_reference_target(remoteRef);
    if (!remoteRefTargetOid) {
      // Remote reference does not point to a valid OID
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      git_reference_free(remoteRef);
      branchesWithStatus.append(branchInfo);
      continue;
    }

    // Get the remote OID
    git_oid remoteOid;
    git_oid_cpy(&remoteOid, remoteRefTargetOid);
    git_reference_free(remoteRef);

    // The local branch tracking this remote branch, or else the local branch
    // of the same name ("origin/feature/x" -> "feature/x").
    const QString localBranchName = trackingBranches.value(
        fullRemoteRefName, localNameOfRemoteBranch(repo, remoteBranchName));

    // Try to get the local branch that tracks this remote branch
    git_reference *localRef = nullptr;
    error = git_branch_lookup(&localRef, repo,
                              localBranchName.toStdString().c_str(),
                              GIT_BRANCH_LOCAL);

    if (error) {
      // No local tracking branch exists
      branchInfo.statusInfo.status = BranchStatusType::NoUpstream;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      continue;
    }

    const git_oid *localRefTargetOid = git_reference_target(localRef);
    if (!localRefTargetOid) {
      // Local reference does not point to a valid OID
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      git_reference_free(localRef);
      continue;
    }

    // Get the local OID
    git_oid localOid;
    git_oid_cpy(&localOid, localRefTargetOid);
    git_reference_free(localRef);

    // Calculate ahead/behind
    size_t ahead = 0, behind = 0;
    error =
        git_graph_ahead_behind(&ahead, &behind, repo, &localOid, &remoteOid);

    if (error) {
      branchInfo.statusInfo.status = BranchStatusType::Unknown;
      branchInfo.statusInfo.aheadCount = 0;
      branchInfo.statusInfo.behindCount = 0;
      branchesWithStatus.append(branchInfo);
      setError(
          GitError::ListRemoteBranchesWithStatusFailedToCalculateAheadBehind,
          error);
      continue;
    }

    // Determine the status type
    if (ahead == 0 && behind == 0) {
      branchInfo.statusInfo.status = BranchStatusType::UpToDate;
    } else if (ahead > 0 && behind == 0) {
      branchInfo.statusInfo.status = BranchStatusType::Ahead;
    } else if (ahead == 0 && behind > 0) {
      branchInfo.statusInfo.status = BranchStatusType::Behind;
    } else {
      branchInfo.statusInfo.status = BranchStatusType::Diverged;
    }

    branchInfo.statusInfo.aheadCount = ahead;
    branchInfo.statusInfo.behindCount = behind;

    branchesWithStatus.append(branchInfo);
  }

  LOG_PERF(0, "Exit");

  return branchesWithStatus;
}

QStringList listTags(git_repository *repo) {
  LOG_PERF(0, "Enter");

  git_strarray tagNames = {0};
  const int error = git_tag_list(&tagNames, repo);
  if (error) {
    setError(GitError::ListTagsFailedToListTags, error);
    return {};
  }

  QStringList tags;

  for (int i = 0; i < tagNames.count; ++i)
    tags += QString::fromUtf8(tagNames.strings[i]);

  git_strarray_dispose(&tagNames);

  // libgit2 lists loose tags before packed ones.
  tags.sort();

  LOG_PERF(0, "Exit");

  return tags;
}

bool createBranchFromCommit(git_repository *repo, const QString &commit,
                            const QString &branchName) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid oid;
  git_commit *commit_ = nullptr;

  int error = git_oid_fromstr(&oid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::CreateBranchFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&commit_, repo, &oid);
  if (error) {
    setError(GitError::CreateBranchFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;
  error = git_branch_create(&ref, repo, branchName.toStdString().c_str(),
                            commit_, 0);
  if (error) {
    setError(GitError::CreateBranchFromCommitFailedToCreateBranch, error);
    git_commit_free(commit_);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);
  git_commit_free(commit_);

  // Set upstream to default repository remote with same branch name
  const QString defaultRemote = getDefaultRemote(repo);
  if (!defaultRemote.isEmpty()) {
    LOG_DEBUG("Setting upstream for new branch '{}' to {}/{}",
              branchName.toStdString(), defaultRemote.toStdString(),
              branchName.toStdString());
    setUpstream(repo, defaultRemote, branchName, branchName);
  } else {
    LOG_DEBUG(
        "No default remote configured, skipping upstream setup for new branch "
        "'{}'",
        branchName.toStdString());
  }

  LOG_PERF(0, "Exit");

  return true;
}

QString getActiveBranch(git_repository *repo, const BranchType type,
                        const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  // Remote branches are never checked out. The active one is the upstream of
  // the current branch.
  if (type == BranchType::Remote) {
    QString upstreamName;
    git_reference *head = nullptr;
    if (repo && git_repository_head(&head, repo) == 0) {
      git_reference *upstream = nullptr;
      if (git_reference_is_branch(head) &&
          git_branch_upstream(&upstream, head) == 0) {
        const char *name = nullptr;
        if (git_branch_name(&name, upstream) == 0 && name) {
          upstreamName = QString::fromUtf8(name);
        }
        git_reference_free(upstream);
      }
      git_reference_free(head);
    }
    LOG_PERF(0, "Exit");
    return upstreamName;
  }

  const QStringList checkoutBranches =
      listCheckoutBranches(repo, type, quitFlag);

  LOG_PERF(0, "Exit");

  return !checkoutBranches.empty() ? checkoutBranches.first() : QString();
}

// Refuses to move a tag that exists on the remote with another target, like
// `git push` without --force.
static int refuseMovingRemoteTags(const git_push_update **updates,
                                  const size_t len, void *) {
  for (size_t i = 0; i < len; ++i) {
    const git_push_update *update = updates[i];
    if (QByteArray(update->dst_refname).startsWith("refs/tags/") &&
        // src is the tag's target on the remote, dst the one pushed.
        !git_oid_is_zero(&update->src) &&
        !git_oid_equal(&update->src, &update->dst)) {
      git_error_set_str(GIT_ERROR_REFERENCE,
                        QString("tag '%1' already exists on the remote")
                            .arg(QString::fromUtf8(update->dst_refname).mid(10))
                            .toUtf8()
                            .constData());
      return GIT_EEXISTS;
    }
  }

  return 0;
}

bool pushTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push tag operation for tag: '{}'", tagName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // Verify tag exists
  git_reference *tagRef = nullptr;
  const QString tagRefName = QString("refs/tags/%1").arg(tagName);
  int error =
      git_reference_lookup(&tagRef, repo, tagRefName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Tag '{}' not found: error={}",
              tagName.toStdString(), error);
    setError(GitError::PushTagFailedToLookupReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  git_reference_free(tagRef);
  LOG_DEBUG("Tag '{}' found", tagName.toStdString());

  git_remote *remote = nullptr;
  const QString defaultRemote = getDefaultRemote(repo);

  error = git_remote_lookup(&remote, repo, defaultRemote.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::PushTagFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = isSshOrGitUrl(url);
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::PushTagGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(
        Log::ErrorId::Git,
        "Pushing tags to git@ repositories is not available on the free plan.");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!GitRepo::Lfs::uploadLocalObjects(
          repo, repoPath, defaultRemote, url,
          QStringLiteral("refs/tags/%1").arg(tagName))) {
    setError(GitError::PushTagFailedToPushRemote, GIT_EUSER);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.sideband_progress = &pushSidebandProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.push_update_reference = &pushUpdateReferenceCallback;
  opts.callbacks.push_negotiation = &refuseMovingRemoteTags;
  opts.callbacks.payload = &callbackPayload;

  // Push tag with refspec: refs/tags/tagName:refs/tags/tagName
  const QString refspec = QString("refs/tags/%1:refs/tags/%1").arg(tagName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  gPushReferenceStatus = {};

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    error = git_remote_push(remote, &refspecs, &opts);
    if (error == GIT_EEXISTS) {
      setError(GitError::PushTagExistsOnRemote, error);
      break;
    }

    if (error) {
      setError(GitError::PushTagFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(url);

  finishFetchProgress(progressData.uuid, error != 0, false);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Push tag operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool pushAllTags(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting push all tags operation");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  // Get list of all tags
  const QStringList allTags = listTags(repo);
  if (allTags.isEmpty()) {
    LOG_INFO("No tags to push");
    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_DEBUG("Found {} tags to push", allTags.size());

  git_remote *remote = nullptr;
  const QString defaultRemote = getDefaultRemote(repo);

  int error =
      git_remote_lookup(&remote, repo, defaultRemote.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::PushAllTagsFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = isSshOrGitUrl(url);
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::PushAllTagsGitSshUrlRequiresCommercialPlan, GIT_EAUTH);
    LOG_ERROR(
        Log::ErrorId::Git,
        "Pushing tags to git@ repositories is not available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!GitRepo::Lfs::uploadLocalObjects(repo, repoPath, defaultRemote, url)) {
    setError(GitError::PushAllTagsFailedToPushRemote, GIT_EUSER);
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.sideband_progress = &pushSidebandProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.push_update_reference = &pushUpdateReferenceCallback;
  opts.callbacks.payload = &callbackPayload;

  // Create refspecs for all tags: refs/tags/tagName:refs/tags/tagName
  QStringList refspecList;
  for (const auto &tag : allTags) {
    refspecList += QString("refs/tags/%1:refs/tags/%1").arg(tag);
  }
  git_strarray refspecs = qstringlist_to_git_strarray(refspecList);

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    error = git_remote_push(remote, &refspecs, &opts);
    if (error) {
      setError(GitError::PushAllTagsFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(url);

  finishFetchProgress(progressData.uuid, error != 0, false);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Push all tags operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool renameBranch(git_repository *repo, const QString &oldName,
                  const QString &newName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;

  int error = git_branch_lookup(&ref, repo, oldName.toStdString().c_str(),
                                GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::RenameBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // git_branch_move only checks the reference name, not the rules for branch
  // names that git_branch_create applies (not HEAD, no leading dash).
  int valid = 0;
  error = git_branch_name_is_valid(&valid, newName.toStdString().c_str());
  if (error || !valid) {
    setError(GitError::RenameBranchInvalidName, error ? error : GIT_EINVALIDSPEC);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *renamed = nullptr;
  error = git_branch_move(&renamed, ref, newName.toStdString().c_str(), 0);
  git_reference_free(ref);
  ref = renamed;
  if (error) {
    setError(GitError::RenameBranchFailedToRenameBranch, error);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);

  LOG_PERF(0, "Exit");

  return true;
}

// True if the tip of `branch` is contained in its upstream or in HEAD, so
// deleting the branch loses no commits.
static bool branchIsMerged(git_repository *repo, git_reference *branch) {
  const git_oid *tip = git_reference_target(branch);
  if (!tip) {
    return true;
  }

  const auto contains = [repo, tip](git_reference *ref) {
    git_object *commit = nullptr;
    if (git_reference_peel(&commit, ref, GIT_OBJECT_COMMIT) != 0) {
      return false;
    }
    const git_oid *id = git_object_id(commit);
    const bool result = git_oid_equal(id, tip) ||
                        git_graph_descendant_of(repo, id, tip) == 1;
    git_object_free(commit);
    return result;
  };

  bool merged = false;
  git_reference *upstream = nullptr;
  if (git_branch_upstream(&upstream, branch) == 0) {
    merged = contains(upstream);
    git_reference_free(upstream);
  }

  git_reference *head = nullptr;
  if (!merged && git_repository_head(&head, repo) == 0) {
    merged = contains(head);
    git_reference_free(head);
  }

  return merged;
}

bool deleteBranch(git_repository *repo, const QString &branchName,
                  const bool force) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *ref = nullptr;

  int error = git_branch_lookup(&ref, repo, branchName.toStdString().c_str(),
                                GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::DeleteBranchFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!force && !branchIsMerged(repo, ref)) {
    setError(GitError::DeleteBranchNotFullyMerged, GIT_EUNMERGED);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_delete(ref);
  if (error) {
    setError(GitError::DeleteBranchFailedToDeleteBranch, error);
    git_reference_free(ref);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference_free(ref);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteRemoteBranch(git_repository *repo, const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting delete remote branch operation for branch: '{}'",
            branchName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_remote *remote = nullptr;

  // The branch is named as in the remote branches list, `<remote>/<branch>`,
  // or as a local branch, whose upstream is deleted.
  QString remoteName;
  QString remoteBranchName;
  git_reference *trackingRef = nullptr;
  if (git_branch_lookup(&trackingRef, repo, branchName.toUtf8().constData(),
                        GIT_BRANCH_REMOTE) == 0) {
    git_buf remoteBuf = GIT_BUF_INIT;
    if (git_branch_remote_name(&remoteBuf, repo,
                               git_reference_name(trackingRef)) == 0) {
      remoteName = QString::fromUtf8(remoteBuf.ptr);
      remoteBranchName = branchName.mid(remoteName.size() + 1);
    }
    git_buf_dispose(&remoteBuf);
    git_reference_free(trackingRef);
  }

  if (remoteName.isEmpty()) {
    if (const auto [ok, upstreamRemote, upstreamBranch] =
            getUpstream(repo, branchName);
        ok && !upstreamRemote.isEmpty()) {
      remoteName = upstreamRemote;
      remoteBranchName =
          upstreamBranch.isEmpty() ? branchName : upstreamBranch;
    }
  }

  if (remoteName.isEmpty()) {
    // `<remote>/<branch>` without a remote-tracking branch.
    git_strarray remotes = {};
    if (git_remote_list(&remotes, repo) == 0) {
      for (size_t i = 0; i < remotes.count; ++i) {
        const QString name = QString::fromUtf8(remotes.strings[i]);
        if (branchName.startsWith(name + '/') &&
            name.size() > remoteName.size()) {
          remoteName = name;
          remoteBranchName = branchName.mid(name.size() + 1);
        }
      }
      git_strarray_dispose(&remotes);
    }
  }

  if (remoteName.isEmpty()) {
    remoteName = getDefaultRemote(repo);
    remoteBranchName = branchName;
  }

  int error =
      git_remote_lookup(&remote, repo, remoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::DeleteRemoteBranchFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = isSshOrGitUrl(url);
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::DeleteRemoteBranchGitSshUrlRequiresCommercialPlan,
             GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git,
              "Deleting remote branches from git@ repositories is not "
              "available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Delete branch with refspec: :refs/heads/branchName (empty source = delete)
  const QString refspec = QString(":refs/heads/%1").arg(remoteBranchName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  LOG_DEBUG("Using refspec for delete: '{}' (original branch name: '{}')",
            refspec.toStdString(), branchName.toStdString());

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    LOG_DEBUG("Attempting deleteRemoteBranch push (attempt {}/{})", retries + 1,
              maxRetries);
    error = git_remote_push(remote, &refspecs, &opts);
    LOG_DEBUG("deleteRemoteBranch git_remote_push returned error code: {}",
              error);

    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "deleteRemoteBranch: git_remote_push failed with error: {}",
                error);
      setError(GitError::DeleteRemoteBranchFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    } else {
      LOG_DEBUG("deleteRemoteBranch: git_remote_push succeeded");
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(url);

  finishFetchProgress(progressData.uuid, error != 0, false);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Delete remote branch operation completed with error code: {}, "
            "returning {}",
            error, (error == 0 ? "success" : "failure"));
  LOG_PERF(0, "Exit");

  return error == 0;
}

bool createTagFromCommit(git_repository *repo, const QString &commit,
                         const QString &tagName, const QString &author,
                         const QString &email, const QString &message) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid targetOid;

  int error = git_oid_fromstr(&targetOid, commit.toStdString().c_str());
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToParseObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  error = git_commit_lookup(&targetCommit, repo, &targetOid);
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToLookupCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToCreateSignature, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid tagOid;

  error = git_tag_create(&tagOid, repo, tagName.toStdString().c_str(),
                         reinterpret_cast<git_object *>(targetCommit), sig,
                         message.toStdString().c_str(), 0);
  if (error) {
    setError(GitError::CreateTagFromCommitFailedToPeelCommitForTag, error);
    git_signature_free(sig);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

bool renameTag(git_repository *repo, const QString &oldName,
               const QString &newName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *tagRef = nullptr;
  git_object *target = nullptr;
  git_oid newOid;

  int error = git_reference_lookup(
      &tagRef, repo,
      QString("refs/tags/%1").arg(oldName).toStdString().c_str());
  if (error) {
    setError(GitError::RenameTagFailedToLookupReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reference_peel(&target, tagRef, GIT_OBJECT_TAG);
  if (error < 0) {
    // Might be a lightweight tag — try commit or tree
    error = git_reference_peel(&target, tagRef, GIT_OBJECT_ANY);
    if (error) {
      setError(GitError::RenameTagFailedToPeelReference, error);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  // If it's an annotated tag, get message and tagger
  if (git_object_type(target) == GIT_OBJECT_TAG) {
    git_tag *tag = nullptr;
    tag = reinterpret_cast<git_tag *>(target);
    const git_signature *tagger = git_tag_tagger(tag);
    if (!tagger) {
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    const char *message = git_tag_message(tag);

    git_object *tagTarget = nullptr;

    error = git_tag_target(&tagTarget, tag);
    if (error) {
      setError(GitError::RenameTagFailedToReadTagTarget, error);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_tag_create(&newOid, repo, newName.toStdString().c_str(),
                           tagTarget, tagger, message, 0);
    if (error) {
      setError(GitError::RenameTagFailedToCreateAnnotatedTag, error);
      git_object_free(tagTarget);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_object_free(tagTarget);
  } else {
    // Lightweight tag
    error = git_tag_create_lightweight(
        &newOid, repo, newName.toStdString().c_str(), target, 0);
    if (error) {
      setError(GitError::RenameTagFailedToCreateLightweightTag, error);
      git_object_free(target);
      git_reference_free(tagRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_tag_delete(repo, oldName.toStdString().c_str());
  if (error) {
    setError(GitError::RenameTagFailedToDeleteTag, error);
    git_object_free(target);
    git_reference_free(tagRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(target);
  git_reference_free(tagRef);

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (const int error = git_tag_delete(repo, tagName.toStdString().c_str())) {
    setError(GitError::DeleteTagFailedToDeleteTag, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteRemoteTag(git_repository *repo, const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Starting delete remote tag operation for tag: '{}'",
            tagName.toStdString());

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get repository path
  QString repoPath = QString::fromUtf8(git_repository_path(repo));
  if (repoPath.endsWith("/.git/")) {
    repoPath = repoPath.left(repoPath.length() - 6);
  } else if (repoPath.endsWith("/.git")) {
    repoPath = repoPath.left(repoPath.length() - 5);
  }
  LOG_DEBUG("Repository path: '{}'", repoPath.toStdString());

  git_remote *remote = nullptr;
  const QString defaultRemoteName = getDefaultRemote(repo);

  int error =
      git_remote_lookup(&remote, repo, defaultRemoteName.toStdString().c_str());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to lookup 'origin' remote: error={}",
              error);
    setError(GitError::DeleteRemoteTagFailedToLookupRemote, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Origin remote found");

  const QString url = git_remote_url(remote);
  LOG_DEBUG("Remote URL: '{}'", url);

  const bool gitSchema = isSshOrGitUrl(url);
  const bool commercial = isCommercialPlan();
  LOG_DEBUG("URL uses git schema: {}, Commercial plan: {}", gitSchema,
            commercial);

  if (!commercial && gitSchema) {
    setError(GitError::DeleteRemoteTagGitSshUrlRequiresCommercialPlan,
             GIT_EAUTH);
    LOG_ERROR(Log::ErrorId::Git, "Deleting remote tags from git@ repositories "
                                 "is not available on the free plan.");
    git_remote_free(remote);
    LOG_PERF(0, "Exit");
    return false;
  }

  CloneProgress progressData;
  progressData.uuid = Uuid::createUuid();
  progressData.cloning = false;

  GitCredentialPayload credPayload;
  credPayload.repoPath = repoPath;
  credPayload.cloning = false;

  std::tuple<CloneProgress, GitCredentialPayload> callbackPayload(progressData,
                                                                  credPayload);

  git_push_options opts;
  git_push_options_init(&opts, GIT_PUSH_OPTIONS_VERSION);
  opts.callbacks.credentials =
      commercial ? &credentialsAcquireCallback : nullptr;
  opts.callbacks.transfer_progress = &fetchProgressCallback;
  opts.callbacks.certificate_check = &certificateCheckCallback;
  opts.callbacks.payload = &callbackPayload;

  // Delete tag with refspec: :refs/tags/tagName (empty source = delete).
  // Tag names can contain slashes; only a full reference name is shortened.
  QString cleanTagName = tagName;
  if (cleanTagName.startsWith(QLatin1String("refs/tags/"))) {
    cleanTagName = cleanTagName.mid(10);
  }

  const QString refspec = QString(":refs/tags/%1").arg(cleanTagName);
  git_strarray refspecs = qstringlist_to_git_strarray(QStringList() << refspec);

  LOG_DEBUG("Using refspec for delete: '{}' (original tag name: '{}')",
            refspec.toStdString(), tagName.toStdString());

  int retries = 0;

  // Determine max retries: use 1 if SSH with configured key, otherwise
  // MAX_RETRIES
  int maxRetries = MAX_RETRIES;
  if (url.startsWith("git@")) {
    // Extract host from git@host:path format
    QRegularExpression hostRegex("^git@([^:]+):");
    QRegularExpressionMatch match = hostRegex.match(url);
    if (match.hasMatch()) {
      QString host = match.captured(1);
      GitKeyStorage &keyStorage = GitKeyStorage::getInstance();
      GitKeyPair keyPair;
      if (keyStorage.getKeyPairForHost(host, keyPair)) {
        maxRetries = 1; // Key is configured, only try once
        LOG_DEBUG("SSH key configured for host {}, setting max retries to 1",
                  host);
      }
    }
  }

  do {
    LOG_DEBUG("Attempting deleteRemoteTag push (attempt {}/{})", retries + 1,
              maxRetries);
    error = git_remote_push(remote, &refspecs, &opts);
    LOG_DEBUG("deleteRemoteTag git_remote_push returned: {}", error);

    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "deleteRemoteTag push failed: {}", error);
      setError(GitError::DeleteRemoteTagFailedToPushRemote, error);

      gGitCredentialsMutex.lock();
      gGitCredentials.remove(url);
      gGitCredentialsMutex.unlock();

      sendInvalidCredentialsSignal(url);
    }

    ++retries;
  } while (error != 0 && error != GIT_EUSER && retries < maxRetries);

  finishCredentialsAcquiring(url);

  finishFetchProgress(progressData.uuid, error != 0, false);

  git_strarray_destroy(refspecs);
  git_remote_free(remote);

  LOG_DEBUG("Delete remote tag operation completed with error code: {}", error);
  LOG_PERF(0, "Exit");

  return error == 0;
}

} // namespace GitRepo
