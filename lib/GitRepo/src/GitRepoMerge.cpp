// MIT License
// Copyright (c) 2026 Sourcerer

#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <Log.h>

#include <QDir>
#include <QSet>

#include <memory>

#define THREAD_ID "GitRepo"

// Merging, rebasing, cherry-picking and reverting.
namespace GitRepo {

using namespace Private;

// Resolves a revision (id, branch, tag...) to the commit it points to.
static int lookupCommitRevision(git_commit **out, git_repository *repo,
                                const QString &revision) {
  git_object *object = nullptr;
  int error =
      git_revparse_single(&object, repo, revision.toStdString().c_str());
  if (error) {
    return error;
  }

  error = git_object_peel(reinterpret_cast<git_object **>(out), object,
                          GIT_OBJECT_COMMIT);
  git_object_free(object);
  return error;
}

// libgit2 keeps the index it read before. git may have changed it since, and
// libgit2's own change check misses a rewrite of the same size within the file
// time resolution. Every change made here is written to disk already.
static void reloadIndex(git_repository *repo) {
  git_index *index = nullptr;
  if (git_repository_index(&index, repo) == 0) {
    git_index_read(index, true);
    git_index_free(index);
  }
}

namespace {

struct PickErrors {
  GitError conflicts;
  GitError empty;
  GitError getHead;
  GitError writeTree;
  GitError lookupTree;
  GitError createCommit;
};

} // namespace

// Commits what git_cherrypick or git_revert left in the index and ends the
// operation. With conflicts the operation stays in progress (CHERRY_PICK_HEAD
// or REVERT_HEAD) for the user to resolve, commit or abort, like git does.
static bool commitPickResult(git_repository *repo, const git_signature *author,
                             const git_signature *committer,
                             const char *message, const PickErrors &errors) {
  git_index *index = nullptr;
  int error = git_repository_index(&index, repo);
  if (error) {
    setError(errors.writeTree, error);
    return false;
  }

  if (git_index_has_conflicts(index)) {
    git_index_free(index);
    setError(errors.conflicts, GIT_EUNMERGED);
    return false;
  }

  git_oid treeId;
  error = git_index_write_tree(&treeId, index);
  git_index_free(index);
  if (error) {
    setError(errors.writeTree, error);
    git_repository_state_cleanup(repo);
    return false;
  }

  git_reference *headRef = nullptr;
  git_commit *headCommit = nullptr;
  error = git_repository_head(&headRef, repo);
  if (!error) {
    error = git_reference_peel(reinterpret_cast<git_object **>(&headCommit),
                               headRef, GIT_OBJECT_COMMIT);
    git_reference_free(headRef);
  }
  if (error) {
    setError(errors.getHead, error);
    git_repository_state_cleanup(repo);
    return false;
  }

  // Nothing changes: git stops here as well instead of making an empty commit.
  if (git_oid_equal(&treeId, git_commit_tree_id(headCommit))) {
    git_commit_free(headCommit);
    git_repository_state_cleanup(repo);
    setError(errors.empty, GIT_EAPPLIED);
    return false;
  }

  git_tree *tree = nullptr;
  error = git_tree_lookup(&tree, repo, &treeId);
  if (error) {
    setError(errors.lookupTree, error);
    git_commit_free(headCommit);
    git_repository_state_cleanup(repo);
    return false;
  }

  git_oid commitId;
  const git_commit *parents[1] = {headCommit};
  error = git_commit_create(&commitId, repo, "HEAD", author, committer, nullptr,
                            message, tree, 1, parents);
  git_tree_free(tree);
  git_commit_free(headCommit);
  if (error) {
    setError(errors.createCommit, error);
    git_repository_state_cleanup(repo);
    return false;
  }

  git_repository_state_cleanup(repo);
  return true;
}

bool revertCommit(git_repository *repo, const QString &commit,
                  const QString &author, const QString &email,
                  const QString &message) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  int error = lookupCommitRevision(&targetCommit, repo, commit);
  if (error) {
    setError(GitError::RevertCommitFailedToLookupTargetCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::RevertCommitFailedToCreateSignature, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Merges the inverse of the commit into the index and working copy and
  // writes REVERT_HEAD. Local changes in the way stop it before anything is
  // touched. A merge commit needs a mainline, which isn't supported.
  git_revert_options opts = GIT_REVERT_OPTIONS_INIT;
  reloadIndex(repo);
  error = git_revert(repo, targetCommit, &opts);
  if (error) {
    setError(GitError::RevertCommitFailedToPrepareRevertCommit, error);
    git_signature_free(sig);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  const PickErrors errors{GitError::RevertCommitFailedWithConflicts,
                          GitError::RevertCommitIsEmpty,
                          GitError::RevertCommitFailedToGetRepositoryHead,
                          GitError::RevertCommitFailedToWriteIndexTreeToRepository,
                          GitError::RevertCommitFailedToLookupTree,
                          GitError::RevertCommitFailedToCreateCommit};
  const bool committed = commitPickResult(
      repo, sig, sig, message.toStdString().c_str(), errors);

  git_signature_free(sig);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");
  return committed;
}

bool cherryPickCommit(git_repository *repo, const QString &commit,
                      const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *targetCommit = nullptr;
  int error = lookupCommitRevision(&targetCommit, repo, commit);
  if (error) {
    setError(GitError::CherryPickCommitFailedToLookupTargetCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CherryPickCommitFailedToCreateSignature, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Merges the commit into the index and working copy and writes
  // CHERRY_PICK_HEAD and MERGE_MSG. Local changes in the way stop it before
  // anything is touched. A merge commit needs a mainline, which isn't
  // supported.
  git_cherrypick_options opts = GIT_CHERRYPICK_OPTIONS_INIT;
  reloadIndex(repo);
  error = git_cherrypick(repo, targetCommit, &opts);
  if (error) {
    setError(GitError::CherryPickCommitFailedToPrepareCherryPickCommit, error);
    git_signature_free(sig);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Like `git cherry-pick`, the picked commit keeps its author and message,
  // the committer is the one passed in.
  const PickErrors errors{
      GitError::CherryPickCommitFailedWithConflicts,
      GitError::CherryPickCommitIsEmpty,
      GitError::CherryPickCommitFailedToGetRepositoryHead,
      GitError::CherryPickCommitFailedToWriteIndexTreeToRepository,
      GitError::CherryPickCommitFailedToLookupTree,
      GitError::CherryPickCommitFailedToCreateCommit};
  const bool committed =
      commitPickResult(repo, git_commit_author(targetCommit), sig,
                       git_commit_message(targetCommit), errors);

  git_signature_free(sig);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");
  return committed;
}

bool merge(git_repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email, const bool allowUnrelatedHistories) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Starting merge operation: source='{}', destination='{}', "
            "author='{}', email='{}'",
            sourceBranch.toStdString(), destinationBranch.toStdString(),
            author.toStdString(), email.toStdString());

  git_reference *srcRef = nullptr;
  git_reference *dstRef = nullptr;

  // Lookup branches
  int error = git_branch_lookup(
      &srcRef, repo, sourceBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to lookup source branch '{}': error={}",
              sourceBranch.toStdString(), error);
    setError(GitError::MergeFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Source branch '{}' lookup successful", sourceBranch.toStdString());

  error = git_branch_lookup(
      &dstRef, repo, destinationBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to lookup destination branch '{}': error={}",
              destinationBranch.toStdString(), error);
    setError(GitError::MergeFailedToLookupDestinationBranch, error);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }
  LOG_DEBUG("Destination branch '{}' lookup successful",
            destinationBranch.toStdString());

  // Like `git merge` without --allow-unrelated-histories.
  if (!allowUnrelatedHistories) {
    git_oid base;
    const int baseError =
        git_merge_base(&base, repo, git_reference_target(srcRef),
                       git_reference_target(dstRef));
    if (baseError == GIT_ENOTFOUND) {
      setError(GitError::MergeUnrelatedHistories, baseError);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_object *dstObj = nullptr;

  error = git_reference_peel(&dstObj, dstRef, GIT_OBJECT_COMMIT);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to peel destination reference: error={}", error);
    setError(GitError::MergeFailedToPeelReference, error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Checking out destination branch");
  error = git_checkout_tree(repo, dstObj, nullptr);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to checkout tree for destination branch: error={}",
              error);
    setError(GitError::MergeFailedToCheckoutTree, error);
    git_object_free(dstObj);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_object_free(dstObj);

  error = git_repository_set_head(repo, git_reference_name(dstRef));
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to set repository HEAD: error={}",
              error);
    setError(GitError::MergeFailedToGetReferenceName, error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Repository HEAD set, preparing to merge");
  // Get annotated commits
  git_annotated_commit *srcCommit = nullptr;

  error = git_annotated_commit_from_ref(&srcCommit, repo, srcRef);
  if (error) {
    setError(GitError::MergeFailedToCreateAnnotatedCommitFromReference, error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *dstCommit = nullptr;

  error = git_annotated_commit_from_ref(&dstCommit, repo, dstRef);
  if (error) {
    setError(GitError::MergeFailedToCreateSecondAnnotatedCommitFromReference,
             error);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_merge_analysis_t analysis;
  git_merge_preference_t preference;

  const git_annotated_commit *commits[1] = {srcCommit};

  // Merge analysis
  error = git_merge_analysis(&analysis, &preference, repo, commits, 1);
  if (error) {
    setError(GitError::MergeFailedToAnalyzeMerge, error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
    LOG_DEBUG("Destination branch is already up to date.");
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return true;
  } else if (analysis & GIT_MERGE_ANALYSIS_FASTFORWARD) {
    LOG_DEBUG("Performing fast-forward merge...");

    git_commit *srcCommitObj = nullptr;
    error = git_commit_lookup(&srcCommitObj, repo,
                              git_annotated_commit_id(srcCommit));
    if (error) {
      setError(GitError::MergeFailedToReadAnnotatedCommitId, error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Update the working copy first, like `git merge`: local changes to files
    // the merge doesn't touch are kept, conflicting ones stop the merge.
    git_checkout_options ffOpts = GIT_CHECKOUT_OPTIONS_INIT;
    ffOpts.checkout_strategy = GIT_CHECKOUT_SAFE;

    error = git_checkout_tree(
        repo, reinterpret_cast<git_object *>(srcCommitObj), &ffOpts);
    if (error) {
      setError(GitError::MergeFailedToCheckoutDestinationTree, error);
      git_commit_free(srcCommitObj);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference *updatedRef = nullptr;

    // Update destination branch to point to source commit
    error = git_reference_set_target(
        &updatedRef, dstRef, git_commit_id(srcCommitObj), "Fast-forward merge");
    if (error) {
      setError(GitError::MergeFailedToReadCommitId, error);
      git_commit_free(srcCommitObj);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_reference_free(updatedRef);
    git_commit_free(srcCommitObj);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);

    LOG_PERF(0, "Exit");
    return true;
  } else if (analysis & GIT_MERGE_ANALYSIS_NORMAL) {
    LOG_DEBUG("Performing normal merge...");

    git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
    git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;

    error = git_merge(repo, commits, 1, &mergeOpts, &checkoutOpts);
    if (error) {
      setError(GitError::MergeFailedToMerge, error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_index *index = NULL;
    error = git_repository_index(&index, repo);
    if (error) {
      setError(GitError::MergeFailedToGetRepositoryIndex, error);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    if (git_index_has_conflicts(index)) {
      LOG_DEBUG("Merge has conflicts. Resolve them before committing.");
      setError(GitError::MergeFailedWithConflicts, GIT_EUNMERGED);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Write tree
    git_oid treeOid;
    git_tree *tree = nullptr;

    error = git_index_write_tree(&treeOid, index);
    if (error) {
      setError(GitError::MergeFailedToWriteIndexTree, error);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error = git_tree_lookup(&tree, repo, &treeOid);
    if (error) {
      setError(GitError::MergeFailedToLookupTree, error);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    // Create commit
    git_signature *sig = nullptr;

    error = git_signature_now(&sig, author.toStdString().c_str(),
                              email.toStdString().c_str());
    if (error) {
      setError(GitError::MergeFailedToCreateSignature, error);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_oid commitOid;
    git_commit *dstCommitObj = nullptr;
    git_commit *srcCommitObj = nullptr;

    error =
        git_commit_lookup(&dstCommitObj, repo, git_reference_target(dstRef));
    if (error) {
      setError(GitError::MergeFailedToGetReferenceTarget, error);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    error =
        git_commit_lookup(&srcCommitObj, repo, git_reference_target(srcRef));
    if (error) {
      setError(GitError::MergeFailedToGetSecondReferenceTarget, error);
      git_commit_free(dstCommitObj);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    const git_commit *parents[2] = {dstCommitObj, srcCommitObj};

    const QString comment = QString("Merge branch '%1' into '%2'")
                                .arg(sourceBranch, destinationBranch);

    error = git_commit_create(&commitOid, repo, "HEAD", sig, sig, NULL,
                              comment.toStdString().c_str(), tree, 2, parents);
    if (error) {
      setError(GitError::MergeFailedToCreateCommit, error);
      git_commit_free(srcCommitObj);
      git_commit_free(dstCommitObj);
      git_signature_free(sig);
      git_tree_free(tree);
      git_index_free(index);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(srcCommitObj);
    git_commit_free(dstCommitObj);
    git_signature_free(sig);
    git_tree_free(tree);
    git_index_free(index);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);

    // The merge is committed, remove MERGE_HEAD and MERGE_MSG.
    error = git_repository_state_cleanup(repo);
    if (error) {
      setError(GitError::CommitStagedFailedToCleanupRepositoryState, error);
      LOG_PERF(0, "Exit");
      return false;
    }

    LOG_PERF(0, "Exit");
    return true;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool rebaseBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (sourceBranch.isEmpty() || destinationBranch.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "Source or destination branch is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  reloadIndex(repo);

  git_reference *srcRef = nullptr;
  git_reference *dstRef = nullptr;
  int error = git_branch_lookup(
      &srcRef, repo, sourceBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::RebaseBranchesFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_branch_lookup(
      &dstRef, repo, destinationBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::RebaseBranchesFailedToLookupDestinationBranch, error);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *srcCommit = nullptr;
  error = git_annotated_commit_from_ref(&srcCommit, repo, srcRef);
  if (error) {
    setError(GitError::RebaseBranchesFailedToCreateAnnotatedCommitFromReference,
             error);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *dstCommit = nullptr;
  error = git_annotated_commit_from_ref(&dstCommit, repo, dstRef);
  if (error) {
    setError(GitError::
                 RebaseBranchesFailedToCreateSecondAnnotatedCommitFromReference,
             error);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid baseOid;
  error = git_merge_base(&baseOid, repo, git_annotated_commit_id(srcCommit),
                         git_annotated_commit_id(dstCommit));
  if (error) {
    setError(GitError::RebaseBranchesFailedToReadAnnotatedCommitId, error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *baseCommit = nullptr;
  error = git_annotated_commit_lookup(&baseCommit, repo, &baseOid);
  if (error) {
    setError(GitError::RebaseBranchesFailedToLookupAnnotatedCommit, error);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *signature = nullptr;
  error = git_signature_now(&signature, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::RebaseBranchesFailedToCreateSignature, error);
    git_annotated_commit_free(baseCommit);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  CommitSigningConfig signingConfig;
  QString signingError;
  bool signingConfigLoaded = true;
  if (signCommit) {
    signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      git_signature_free(signature);
      git_annotated_commit_free(baseCommit);
      git_annotated_commit_free(dstCommit);
      git_annotated_commit_free(srcCommit);
      git_reference_free(dstRef);
      git_reference_free(srcRef);
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::CommitStagedFailedToLoadSigningConfiguration, -1);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_rebase *rebaseSession = nullptr;

  CommitSigningPayload signingPayload;
  signingPayload.config = signingConfig;
  signingPayload.errorMessage = &signingError;
  signingPayload.repo = repo;

  git_rebase_options rebaseOpts = GIT_REBASE_OPTIONS_INIT;
  rebaseOpts.checkout_options.checkout_strategy = GIT_CHECKOUT_SAFE;
  rebaseOpts.commit_create_cb = commitRebaseSigningCallback;
  rebaseOpts.payload = &signingPayload;

  error = git_rebase_init(&rebaseSession, repo, srcCommit, baseCommit,
                          dstCommit, &rebaseOpts);
  if (error) {
    setError(GitError::RebaseBranchesFailedToStartRebase, error);
    git_signature_free(signature);
    git_annotated_commit_free(baseCommit);
    git_annotated_commit_free(dstCommit);
    git_annotated_commit_free(srcCommit);
    git_reference_free(dstRef);
    git_reference_free(srcRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  bool rebaseSuccess = true;
  git_rebase_operation *operation = nullptr;
  git_oid commitId{};

  while ((error = git_rebase_next(&operation, rebaseSession)) == 0) {
    error = git_rebase_commit(&commitId, rebaseSession, nullptr, signature,
                              nullptr, nullptr);
    if (error == 0) {
      continue;
    }

    if (error == GIT_EUNMERGED) {
      LOG_ERROR(Log::ErrorId::Git,
                "Rebase stopped because conflicts must be resolved manually");
      setError(GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts,
               error);
    } else {
      setError(GitError::RebaseBranchesFailedToCommitRebaseStep, error);
      git_rebase_abort(rebaseSession);
    }
    rebaseSuccess = false;
    break;
  }

  if (rebaseSuccess && error == GIT_ITEROVER) {
    error = git_rebase_finish(rebaseSession, signature);
    if (error) {
      setError(GitError::RebaseBranchesFailedToFinishRebase, error);
      git_rebase_abort(rebaseSession);
      rebaseSuccess = false;
    }
  } else if (rebaseSuccess) {
    setError(GitError::RebaseBranchesFailedToAbortRebase, error);
    rebaseSuccess = false;
  }

  git_rebase_free(rebaseSession);
  git_signature_free(signature);
  git_annotated_commit_free(baseCommit);
  git_annotated_commit_free(dstCommit);
  git_annotated_commit_free(srcCommit);
  git_reference_free(dstRef);
  git_reference_free(srcRef);

  LOG_PERF(0, "Exit");
  return rebaseSuccess;
}

bool squashBranches(git_repository *repo, const QString &sourceBranch,
                    const QString &destinationBranch, const QString &author,
                    const QString &email, const bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (sourceBranch.isEmpty() || destinationBranch.isEmpty() ||
      sourceBranch == destinationBranch) {
    LOG_ERROR(Log::ErrorId::Git,
              "Invalid squash arguments: source='{}', destination='{}'",
              sourceBranch.toStdString(), destinationBranch.toStdString());
    setError(GitError::SquashBranchesInvalidArguments, -1);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (git_repository_state(repo) != GIT_REPOSITORY_STATE_NONE) {
    LOG_ERROR(Log::ErrorId::Git,
              "Cannot squash branches while another operation is in progress");
    setError(GitError::SquashBranchesRepositoryBusy, -1);
    LOG_PERF(0, "Exit");
    return false;
  }

  using RefPtr = std::unique_ptr<git_reference, decltype(&git_reference_free)>;
  using ObjectPtr = std::unique_ptr<git_object, decltype(&git_object_free)>;
  using AnnotatedPtr = std::unique_ptr<git_annotated_commit,
                                       decltype(&git_annotated_commit_free)>;
  using IndexPtr = std::unique_ptr<git_index, decltype(&git_index_free)>;
  using CommitPtr = std::unique_ptr<git_commit, decltype(&git_commit_free)>;
  using RevwalkPtr = std::unique_ptr<git_revwalk, decltype(&git_revwalk_free)>;

  LOG_DEBUG("Starting squash operation: source='{}', destination='{}'",
            sourceBranch.toStdString(), destinationBranch.toStdString());

  git_reference *rawSrcRef = nullptr;
  int error = git_branch_lookup(
      &rawSrcRef, repo, sourceBranch.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::SquashBranchesFailedToLookupBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  const RefPtr srcRef(rawSrcRef, git_reference_free);

  git_reference *rawDstRef = nullptr;
  error = git_branch_lookup(&rawDstRef, repo,
                            destinationBranch.toStdString().c_str(),
                            GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::SquashBranchesFailedToLookupDestinationBranch, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  const RefPtr dstRef(rawDstRef, git_reference_free);

  // Move HEAD to the destination branch, the squash commit is created there.
  git_object *rawDstObj = nullptr;
  error = git_reference_peel(&rawDstObj, dstRef.get(), GIT_OBJECT_COMMIT);
  if (error) {
    setError(GitError::SquashBranchesFailedToPeelReference, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  const ObjectPtr dstObj(rawDstObj, git_object_free);

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy = GIT_CHECKOUT_SAFE;
  error = git_checkout_tree(repo, dstObj.get(), &checkoutOpts);
  if (error) {
    setError(GitError::SquashBranchesFailedToCheckoutTree, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_repository_set_head(repo, git_reference_name(dstRef.get()));
  if (error) {
    setError(GitError::SquashBranchesFailedToSetHead, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_annotated_commit *rawSrcCommit = nullptr;
  error = git_annotated_commit_from_ref(&rawSrcCommit, repo, srcRef.get());
  if (error) {
    setError(GitError::SquashBranchesFailedToCreateAnnotatedCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }
  const AnnotatedPtr srcCommit(rawSrcCommit, git_annotated_commit_free);

  const git_annotated_commit *commits[1] = {srcCommit.get()};

  git_merge_analysis_t analysis;
  git_merge_preference_t preference;
  error = git_merge_analysis(&analysis, &preference, repo, commits, 1);
  if (error) {
    setError(GitError::SquashBranchesFailedToAnalyzeMerge, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (analysis & GIT_MERGE_ANALYSIS_UP_TO_DATE) {
    LOG_DEBUG("Destination branch already contains all source changes.");
    LOG_PERF(0, "Exit");
    return true;
  }

  // Apply the source changes to the index and working tree. The merge state it
  // leaves behind (MERGE_HEAD) is removed below so that the resulting commit
  // has a single parent.
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
  git_checkout_options mergeCheckoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  mergeCheckoutOpts.checkout_strategy =
      GIT_CHECKOUT_SAFE | GIT_CHECKOUT_ALLOW_CONFLICTS;

  error = git_merge(repo, commits, 1, &mergeOpts, &mergeCheckoutOpts);
  if (error) {
    setError(GitError::SquashBranchesFailedToMerge, error);
    git_repository_state_cleanup(repo);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *rawIndex = nullptr;
  error = git_repository_index(&rawIndex, repo);
  if (error) {
    setError(GitError::SquashBranchesFailedToGetRepositoryIndex, error);
    git_repository_state_cleanup(repo);
    LOG_PERF(0, "Exit");
    return false;
  }
  const IndexPtr index(rawIndex, git_index_free);

  if (git_index_has_conflicts(index.get())) {
    LOG_DEBUG("Squash has conflicts. Resolve them before committing.");
    // Like `git merge --squash`: keep the conflicts in the working tree but
    // drop the merge state so the final commit is a regular one.
    git_repository_state_cleanup(repo);
    setError(GitError::SquashBranchesFailedWithConflicts, GIT_EUNMERGED);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid treeOid;
  error = git_index_write_tree(&treeOid, index.get());
  if (error) {
    setError(GitError::SquashBranchesFailedToWriteIndexTree, error);
    git_repository_state_cleanup(repo);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *rawHeadCommit = nullptr;
  error = git_commit_lookup(&rawHeadCommit, repo,
                            git_reference_target(dstRef.get()));
  if (error) {
    setError(GitError::SquashBranchesFailedToLookupHeadCommit, error);
    git_repository_state_cleanup(repo);
    LOG_PERF(0, "Exit");
    return false;
  }
  const CommitPtr headCommit(rawHeadCommit, git_commit_free);

  if (git_oid_equal(&treeOid, git_commit_tree_id(headCommit.get()))) {
    LOG_DEBUG("Squash produced no changes, nothing to commit.");
    git_repository_state_cleanup(repo);
    LOG_PERF(0, "Exit");
    return true;
  }

  // List the squashed commits in the message body.
  QString body;
  git_revwalk *rawWalker = nullptr;
  if (git_revwalk_new(&rawWalker, repo) == 0) {
    const RevwalkPtr walker(rawWalker, git_revwalk_free);
    git_revwalk_sorting(walker.get(), GIT_SORT_TOPOLOGICAL | GIT_SORT_TIME);

    if (git_revwalk_push(walker.get(), git_reference_target(srcRef.get())) ==
            0 &&
        git_revwalk_hide(walker.get(), git_reference_target(dstRef.get())) ==
            0) {
      git_oid oid;
      while (git_revwalk_next(&oid, walker.get()) == 0) {
        git_commit *rawCommit = nullptr;
        if (git_commit_lookup(&rawCommit, repo, &oid) != 0) {
          continue;
        }
        const CommitPtr commit(rawCommit, git_commit_free);

        const char *commitSummary = git_commit_summary(commit.get());
        body += QString("* %1\n").arg(
            QString::fromUtf8(commitSummary ? commitSummary : ""));
      }
    }
  }
  body = body.trimmed();

  error = git_repository_state_cleanup(repo);
  if (error) {
    setError(GitError::SquashBranchesFailedToCleanupRepositoryState, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString summary = QString("Squash merge branch '%1' into '%2'")
                              .arg(sourceBranch, destinationBranch);

  const bool committed = commitStaged(repo, summary, body, author, email, false,
                                      false, signCommit);

  LOG_PERF(0, "Exit");
  return committed;
}

bool getIsRebaseInProgress(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const int state = git_repository_state(repo);

  LOG_PERF(0, "Exit");

  return state == GIT_REPOSITORY_STATE_REBASE ||
         state == GIT_REPOSITORY_STATE_REBASE_INTERACTIVE ||
         state == GIT_REPOSITORY_STATE_REBASE_MERGE;
  ;
}

bool commitRebase(git_repository *repo, const QString &summary,
                  const QString &message, const QString &author,
                  const QString &email, bool cleanup, bool signoff,
                  bool signCommit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Creating commit: summary='{}', author='{}', email='{}'",
            summary.toStdString(), author.toStdString(), email.toStdString());
  LOG_DEBUG("Message length: {}, cleanup: {}, signoff: {}", message.length(),
            cleanup, signoff);

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  CommitSigningConfig signingConfig;
  QString signingError;
  bool signingConfigLoaded = true;
  if (signCommit) {
    signingConfigLoaded =
        loadCommitSigningConfig(&signingConfig, &signingError);
    if (!signingConfigLoaded) {
      LOG_ERROR(Log::ErrorId::Git, "{}", signingError.toStdString());
      setError(GitError::CommitStagedFailedToLoadSigningConfiguration, -1);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  CommitSigningPayload signingPayload;
  signingPayload.config = signingConfig;
  signingPayload.errorMessage = &signingError;
  signingPayload.repo = repo;

  git_rebase *rebase = nullptr;
  git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;
  rebase_opts.commit_create_cb = commitRebaseSigningCallback;
  rebase_opts.payload = &signingPayload;

  int error = git_rebase_open(&rebase, repo, &rebase_opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open rebase session: error={}",
              error);
    setError(GitError::CommitStagedFailedToOpenRebaseSession, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Preparing commit: loading index and building tree");
  git_index *index = nullptr;
  git_oid treeId;
  git_signature *sig = nullptr;

  // Load index
  error = git_repository_index(&index, repo);
  if (error) {
    git_rebase_free(rebase);
    LOG_ERROR(Log::ErrorId::Git, "Failed to get repository index: error={}",
              error);
    setError(GitError::CommitStagedFailedToGetRepositoryIndex, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Write index to tree
  error = git_index_write_tree(&treeId, index);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to write index to tree: error={}",
              error);
    setError(GitError::CommitStagedFailedToWriteIndexTree, error);
    git_rebase_free(rebase);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Tree prepared successfully");

  // Create signature
  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CommitStagedFailedToCreateSignature, error);
    git_rebase_free(rebase);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString fullMessage =
      buildCommitMessage(summary, message, author, email, signoff);

  git_oid oid;

  // GIT_EAPPLIED: the resolved step changes nothing, so it is skipped. Like
  // `git rebase --continue`, every step keeps the author of its original
  // commit (a null author), the committer is the one passed in.
  error = git_rebase_commit(&oid, rebase, nullptr, sig, nullptr,
                            fullMessage.toStdString().c_str());
  if (error && error != GIT_EAPPLIED) {
    setError(GitError::CommitStagedFailedToCreateCommit, error);
    git_rebase_free(rebase);
    git_signature_free(sig);
    git_index_free(index);
    LOG_PERF(0, "Exit");
    return false;
  }

  // The remaining steps. One with conflicts stops the rebase, which stays in
  // progress so the user resolves it and commits again.
  git_rebase_operation *operation;
  bool stepFailed = false;
  while ((error = git_rebase_next(&operation, rebase)) == 0) {
    error = git_rebase_commit(&oid, rebase, nullptr, sig, nullptr, nullptr);
    if (error == GIT_EAPPLIED) {
      continue;
    }
    if (error) {
      setError(
          error == GIT_EUNMERGED
              ? GitError::RebaseBranchesFailedToCommitRebaseStepWithConflicts
              : GitError::CommitStagedFailedToCreateCommit,
          error);
      stepFailed = true;
      break;
    }
  }

  if (error == GIT_ITEROVER) {
    // Every step is committed.
    error = git_rebase_finish(rebase, sig);
    if (error) {
      setError(GitError::RebaseBranchesFailedToFinishRebase, error);
    }
  } else if (!stepFailed) {
    // git_rebase_next failed to apply the next step.
    setError(GitError::CommitStagedFailedToCreateCommit, error);
  }

  git_rebase_free(rebase);
  git_signature_free(sig);
  git_index_free(index);

  if (cleanup && error == 0) {
    error = git_repository_state_cleanup(repo);
    if (error) {
      setError(GitError::CommitStagedFailedToCleanupRepositoryState, error);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");

  return error == 0;
}

bool abortRebase(git_repository *repo) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const int state = git_repository_state(repo);

  if (state != GIT_REPOSITORY_STATE_REBASE &&
      state != GIT_REPOSITORY_STATE_REBASE_INTERACTIVE &&
      state != GIT_REPOSITORY_STATE_REBASE_MERGE) {
    setOwnError(GitError::AbortRebaseNothingToAbort, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_rebase *rebase = nullptr;
  git_rebase_options rebase_opts = GIT_REBASE_OPTIONS_INIT;

  int error = git_rebase_open(&rebase, repo, &rebase_opts);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to open rebase session: error={}",
              error);
    setError(GitError::CommitStagedFailedToOpenRebaseSession, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_rebase_abort(rebase);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to abort rebase session: error={}",
              error);
    setError(GitError::CommitStagedFailedToAbortRebaseSession, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return error == 0;
}

} // namespace GitRepo
