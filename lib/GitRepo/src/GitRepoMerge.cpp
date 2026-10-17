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

static bool file_has_conflict_markers(const QString &path) {
  return file_has_conflict_marker_triplet(path, "<<<<<<<",
                                          "=======", ">>>>>>>");
}

static bool collectConflictedFilePathsFromResultIndex(git_repository *repo,
                                                      git_index *index,
                                                      QStringList *filePaths) {
  LOG_PERF(0, "Enter");

  if (!repo || !index || !filePaths) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository, index pointer or file paths container is null when "
              "collecting conflicted files");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_reference *headRef = nullptr;
  git_commit *headCommit = nullptr;
  git_tree *headTree = nullptr;
  git_diff *candidateDiff = nullptr;
  git_index_conflict_iterator *conflictIterator = nullptr;
  QStringList candidatePaths;
  QSet<QString> uniqueCandidatePaths;

  int error = git_repository_head(&headRef, repo);
  if (error) {
    setError(
        GitError::
            CollectConflictedFilePathsFromResultIndexFailedToGetRepositoryHead,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_reference_peel(reinterpret_cast<git_object **>(&headCommit),
                             headRef, GIT_OBJECT_COMMIT);
  if (error) {
    setError(GitError::CollectConflictedFilePathsFailedToPeelHeadToCommit,
             error);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_tree(&headTree, headCommit);
  if (error) {
    setError(GitError::
                 CollectConflictedFilePathsFromResultIndexFailedToGetCommitTree,
             error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_diff_tree_to_index(&candidateDiff, repo, headTree, index, NULL);
  git_tree_free(headTree);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  if (error) {
    setError(
        GitError::
            CollectConflictedFilePathsFromResultIndexFailedToDiffTreeAgainstIndex,
        error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const auto appendCandidatePath = [&](const char *relativePathCstr) {
    if (!relativePathCstr) {
      return;
    }

    const QString relativePath = QString::fromUtf8(relativePathCstr);
    if (relativePath.isEmpty() || uniqueCandidatePaths.contains(relativePath)) {
      return;
    }

    uniqueCandidatePaths.insert(relativePath);
    candidatePaths.push_back(relativePath);
  };

  const size_t numDeltas = git_diff_num_deltas(candidateDiff);
  for (size_t i = 0; i < numDeltas; ++i) {
    const git_diff_delta *delta = git_diff_get_delta(candidateDiff, i);
    if (!delta) {
      continue;
    }

    appendCandidatePath(delta->new_file.path ? delta->new_file.path
                                             : delta->old_file.path);
  }

  if (candidatePaths.isEmpty()) {
    error = git_index_conflict_iterator_new(&conflictIterator, index);
    if (error) {
      setError(
          GitError::
              CollectConflictedFilePathsFromResultIndexFailedToCreateConflictIterator,
          error);
      git_diff_free(candidateDiff);
      LOG_PERF(0, "Exit");
      return false;
    }

    const git_index_entry *ancestor = nullptr;
    const git_index_entry *ours = nullptr;
    const git_index_entry *theirs = nullptr;

    while (
        !git_index_conflict_next(&ancestor, &ours, &theirs, conflictIterator)) {
      const git_index_entry *entry = ours ? ours : (theirs ? theirs : ancestor);
      if (!entry) {
        continue;
      }

      appendCandidatePath(entry->path);
    }
  }

  const char *repoWorkdir = git_repository_workdir(repo);
  QString workDir;
  if (repoWorkdir) {
    workDir = repoWorkdir;
  }

  QSet<QString> uniquePaths;
  for (const QString &relativePath : candidatePaths) {
    QString filePath = QDir(workDir).absoluteFilePath(relativePath);
    filePath = QDir::toNativeSeparators(filePath);

    if (!file_has_conflict_markers(filePath) ||
        uniquePaths.contains(relativePath)) {
      continue;
    }

    uniquePaths.insert(relativePath);
    filePaths->push_back(relativePath);
  }

  git_index_conflict_iterator_free(conflictIterator);
  git_diff_free(candidateDiff);

  LOG_DEBUG("Collected {} conflicted files", filePaths->size());
  LOG_PERF(0, "Exit");
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

  int error = git_revparse_single((git_object **)&targetCommit, repo,
                                  commit.toStdString().c_str());
  if (error) {
    setError(GitError::RevertCommitFailedToLookupTargetCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *headCommit = nullptr;
  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    setError(GitError::RevertCommitFailedToGetRepositoryHead, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&headCommit, repo, git_reference_target(headRef));
  if (error) {
    setError(GitError::RevertCommitFailedToGetReferenceTarget, error);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *revertIndex = nullptr;
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
  mergeOpts.file_flags |= GIT_MERGE_FILE_ACCEPT_CONFLICTS;

  error = git_revert_commit(&revertIndex, repo, targetCommit, headCommit,
                            0, // mainline = 0 for non-merge commits
                            &mergeOpts);
  if (error) {
    setError(GitError::RevertCommitFailedToPrepareRevertCommit, error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;

  error = git_checkout_index(repo, revertIndex, &checkoutOpts);
  if (error) {
    setError(GitError::RevertCommitFailedToCheckoutIndex, error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList conflictedFiles;
  if (!collectConflictedFilePathsFromResultIndex(repo, revertIndex,
                                                 &conflictedFiles)) {
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!conflictedFiles.isEmpty()) {
    git_index_free(revertIndex);

    if (!markConflictedFiles(repo, conflictedFiles)) {
      git_commit_free(headCommit);
      git_reference_free(headRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);

    LOG_DEBUG("Revert of commit '{}' completed with conflicts",
              commit.toStdString());
    LOG_PERF(0, "Exit");
    return true;
  }

  git_oid treeOid;

  error = git_index_write_tree_to(&treeOid, revertIndex, repo);
  if (error) {
    setError(GitError::RevertCommitFailedToWriteIndexTreeToRepository, error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree *revertTree = nullptr;

  error = git_tree_lookup(&revertTree, repo, &treeOid);
  if (error) {
    setError(GitError::RevertCommitFailedToLookupTree, error);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::RevertCommitFailedToCreateSignature, error);
    git_tree_free(revertTree);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid newCommitOid;

  error = git_commit_create_v(&newCommitOid, repo, "HEAD", sig, sig, nullptr,
                              message.toStdString().c_str(), revertTree, 1,
                              headCommit);
  if (error) {
    setError(GitError::RevertCommitFailedToCreateCommit, error);
    git_signature_free(sig);
    git_tree_free(revertTree);
    git_index_free(revertIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(revertTree);
  git_index_free(revertIndex);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
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

  int error = git_revparse_single((git_object **)&targetCommit, repo,
                                  commit.toStdString().c_str());
  if (error) {
    setError(GitError::CherryPickCommitFailedToLookupTargetCommit, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_commit *headCommit = nullptr;
  git_reference *headRef = nullptr;

  error = git_repository_head(&headRef, repo);
  if (error) {
    setError(GitError::CherryPickCommitFailedToGetRepositoryHead, error);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  error = git_commit_lookup(&headCommit, repo, git_reference_target(headRef));
  if (error) {
    setError(GitError::CherryPickCommitFailedToGetReferenceTarget, error);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_index *cherryPickIndex = nullptr;
  git_merge_options mergeOpts = GIT_MERGE_OPTIONS_INIT;
  mergeOpts.file_flags |= GIT_MERGE_FILE_ACCEPT_CONFLICTS;

  error = git_cherrypick_commit(&cherryPickIndex, repo, targetCommit,
                                headCommit, 0, &mergeOpts);
  if (error) {
    setError(GitError::CherryPickCommitFailedToPrepareCherryPickCommit, error);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_checkout_options checkoutOpts = GIT_CHECKOUT_OPTIONS_INIT;
  checkoutOpts.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;

  error = git_checkout_index(repo, cherryPickIndex, &checkoutOpts);
  if (error) {
    setError(GitError::CherryPickCommitFailedToCheckoutIndex, error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  QStringList conflictedFiles;
  if (!collectConflictedFilePathsFromResultIndex(repo, cherryPickIndex,
                                                 &conflictedFiles)) {
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!conflictedFiles.isEmpty()) {
    git_index_free(cherryPickIndex);

    if (!markConflictedFiles(repo, conflictedFiles)) {
      git_commit_free(headCommit);
      git_reference_free(headRef);
      git_commit_free(targetCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);

    LOG_DEBUG("Cherry-pick of commit '{}' completed with conflicts",
              commit.toStdString());
    LOG_PERF(0, "Exit");
    return true;
  }

  git_oid treeOid;

  error = git_index_write_tree_to(&treeOid, cherryPickIndex, repo);
  if (error) {
    setError(GitError::CherryPickCommitFailedToWriteIndexTreeToRepository,
             error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_tree *cherryPickTree = nullptr;

  error = git_tree_lookup(&cherryPickTree, repo, &treeOid);
  if (error) {
    setError(GitError::CherryPickCommitFailedToLookupTree, error);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature *sig = nullptr;

  error = git_signature_now(&sig, author.toStdString().c_str(),
                            email.toStdString().c_str());
  if (error) {
    setError(GitError::CherryPickCommitFailedToCreateSignature, error);
    git_tree_free(cherryPickTree);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid newCommitOid;
  const char *commitMessage = git_commit_message(targetCommit);

  error = git_commit_create_v(&newCommitOid, repo, "HEAD", sig, sig, nullptr,
                              commitMessage, cherryPickTree, 1, headCommit);
  if (error) {
    setError(GitError::CherryPickCommitFailedToCreateCommit, error);
    git_signature_free(sig);
    git_tree_free(cherryPickTree);
    git_index_free(cherryPickIndex);
    git_commit_free(headCommit);
    git_reference_free(headRef);
    git_commit_free(targetCommit);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(sig);
  git_tree_free(cherryPickTree);
  git_index_free(cherryPickIndex);
  git_commit_free(headCommit);
  git_reference_free(headRef);
  git_commit_free(targetCommit);

  LOG_PERF(0, "Exit");

  return true;
}

bool merge(git_repository *repo, const QString &sourceBranch,
           const QString &destinationBranch, const QString &author,
           const QString &email) {
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

    git_commit *newCommit = nullptr;

    error = git_commit_lookup(&newCommit, repo, git_commit_id(srcCommitObj));
    if (error) {
      setError(GitError::MergeFailedToReadSecondCommitId, error);
      git_reference_free(updatedRef);
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

    git_checkout_options ffOpts = GIT_CHECKOUT_OPTIONS_INIT;
    ffOpts.checkout_strategy =
        GIT_CHECKOUT_SAFE | GIT_CHECKOUT_RECREATE_MISSING;

    error = git_reset(repo, reinterpret_cast<git_object *>(newCommit),
                      GIT_RESET_HARD, &ffOpts);
    if (error) {
      setError(GitError::MergeFailedToCheckoutDestinationTree, error);
      git_commit_free(newCommit);
      LOG_PERF(0, "Exit");
      return false;
    }

    git_commit_free(newCommit);

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

  // GIT_EAPPLIED: the resolved step changes nothing, so it is skipped.
  error = git_rebase_commit(&oid, rebase, sig, sig, nullptr,
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
    error = git_rebase_commit(&oid, rebase, sig, sig, nullptr, nullptr);
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
