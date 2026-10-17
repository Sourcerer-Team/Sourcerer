#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <Log.h>

#include <QByteArray>
#include <QDir>
#include <QFile>

#define THREAD_ID "GitRepo"

// Stashes.
namespace GitRepo {

using namespace Private;

static void collectCommitStatusFromDiff(git_diff *diff,
                                        GitCommitStatus *status) {
  if (!diff || !status) {
    return;
  }

  git_diff_foreach(
      diff,
      [](const git_diff_delta *delta, float, void *payload) {
        auto *status = static_cast<GitCommitStatus *>(payload);

        switch (delta->status) {
        case GIT_DELTA_ADDED:
        case GIT_DELTA_UNTRACKED:
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
        case GIT_DELTA_UNMODIFIED:
        case GIT_DELTA_IGNORED:
        case GIT_DELTA_CONFLICTED:
          break;
        }

        return 0;
      },
      nullptr, // binary_cb
      nullptr, // hunk_cb
      nullptr, // line_cb
      status);
}

struct CollectTreePathsPayload {
  GitCommitStatus *status = nullptr;
  QStringList *paths = nullptr;
};

static int collectTreePathsCallback(const char *root,
                                    const git_tree_entry *entry,
                                    void *payload) {
  auto *data = static_cast<CollectTreePathsPayload *>(payload);
  if (!data || !data->status || !data->paths || !entry) {
    return -1;
  }

  if (git_tree_entry_type(entry) == GIT_OBJECT_TREE) {
    return 0;
  }

  const QString path = QString::fromUtf8(root ? root : "") +
                       QString::fromUtf8(git_tree_entry_name(entry));
  data->paths->append(path);

  return 0;
}

static bool file_has_conflict_markers_for_apply_stash(const QString &path) {
  return file_has_conflict_marker_triplet(path, "<<<<<<< Updated upstream",
                                          "=======", ">>>>>>> Stashed changes");
}

bool applyStash(git_repository *repo, int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Applying stash at index: {}", index);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_stash_apply_options options;
  git_stash_apply_options_init(&options, GIT_STASH_APPLY_OPTIONS_VERSION);

  // Optional: customize behavior
  options.flags = GIT_STASH_APPLY_REINSTATE_INDEX; // restores index state
  options.checkout_options.checkout_strategy |= GIT_CHECKOUT_ALLOW_CONFLICTS;

  int error = 0;

  error = git_stash_apply(repo, index, &options);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to apply stash at index {}: error={}",
              index, error);
    setError(GitError::ApplyStashFailedToApplyStash, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Detect conflicts by diffing index vs workdir
  git_diff *workdir_diff = NULL;
  git_diff_options diff_opts = GIT_DIFF_OPTIONS_INIT;

  error = git_diff_index_to_workdir(&workdir_diff, repo, NULL, &diff_opts);
  if (error) {
    setError(GitError::ApplyStashFailedToDiffIndexAgainstWorkdir, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const size_t num_deltas = git_diff_num_deltas(workdir_diff);

  const char *repo_workdir = git_repository_workdir(repo);

  QString workDir;
  if (repo_workdir) {
    workDir = repo_workdir;
  }

  QStringList conflictedFiles;

  for (size_t i = 0; i < num_deltas; i++) {
    const git_diff_delta *delta = git_diff_get_delta(workdir_diff, i);

    if (!delta->new_file.path) {
      LOG_WARN("New file path is not defined for delta {}", i);
      continue;
    }

    QString filePath = QDir(workDir).absoluteFilePath(delta->new_file.path);
    filePath = QDir::toNativeSeparators(filePath);

    if (file_has_conflict_markers_for_apply_stash(filePath)) {
      conflictedFiles += filePath;
    }
  }

  markConflictedFiles(repo, conflictedFiles);

  git_diff_free(workdir_diff);

  LOG_DEBUG("Stash at index {} applied successfully", index);
  LOG_PERF(0, "Exit");
  return true;
}

QStringList listStashes(git_repository *repo, const bool &quitFlag) {
  LOG_PERF(0, "Enter");

  QStringList stashes;

  const auto callback = [](size_t index, const char *message,
                           const git_oid *stash_id, void *payload) {
    if (!payload) {
      return -1;
    }

    const auto payloadStashes = static_cast<QStringList *>(payload);
    if (message) {
      *payloadStashes += QString::fromUtf8(message);
    } else {
      *payloadStashes += QString("stash@{%1}").arg(index);
    }

    return 0;
  };

  if (const int error = git_stash_foreach(repo, callback, &stashes)) {
    setError(GitError::ListStashesFailedToListStashes, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  LOG_PERF(0, "Exit");

  return stashes;
}

GitCommitStatus getStashStatus(git_repository *repo, int stashIndex) {
  LOG_PERF(0, "Enter");

  GitCommitStatus status;

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return {};
  }

  // Get stash commit object
  git_object *stash_obj = nullptr;
  git_commit *stash_commit = nullptr;
  git_tree *stash_tree = nullptr;
  git_tree *parent_tree = nullptr;
  git_commit *parent_commit = nullptr;
  git_tree *parent1_tree = nullptr;
  git_tree *parent2_tree = nullptr;
  git_commit *parent1_commit = nullptr;
  git_commit *parent2_commit = nullptr;
  git_diff *diff = nullptr;

  // Use git_revparse_single to get the stash commit
  QString stashRef = QString("stash@{%1}").arg(stashIndex);
  int error =
      git_revparse_single(&stash_obj, repo, stashRef.toUtf8().constData());
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get stash ref at index {}: error={}", stashIndex,
              error);
    setError(GitError::GetStashStatusFailedToResolveRevision, error);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Convert to commit
  git_object *peeled_obj = nullptr;
  error = git_object_peel(&peeled_obj, stash_obj, GIT_OBJ_COMMIT);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to peel stash object to commit at index {}: error={}",
              stashIndex, error);
    setError(GitError::GetStashStatusFailedToPeelObjectToCommit, error);
    git_object_free(stash_obj);
    LOG_PERF(0, "Exit");
    return {};
  }

  stash_commit = (git_commit *)peeled_obj;
  git_object_free(stash_obj);

  // Get stash tree
  error = git_commit_tree(&stash_tree, stash_commit);
  if (error) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get stash tree: error={}", error);
    setError(GitError::GetStashStatusFailedToGetCommitTree, error);
    git_commit_free(stash_commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  // Check if stash tree is empty (common for stash commits which are merge
  // commits). If empty, try to use parent 1 (index state) for tracked changes.
  // Parent 2 contains untracked files and is handled separately below.
  git_tree *tracked_working_tree = stash_tree;

  if (git_tree_entrycount(stash_tree) == 0) {
    LOG_DEBUG("Stash tree is empty, trying parent 1 (index state)");

    // Try to get parent 1 commit (the index state in the stash)
    error = git_commit_parent(&parent1_commit, stash_commit, 1);
    if (error == 0) {
      error = git_commit_tree(&parent1_tree, parent1_commit);
      if (error == 0 && git_tree_entrycount(parent1_tree) > 0) {
        LOG_DEBUG("Using parent 1 tree (index state) with {} entries",
                  git_tree_entrycount(parent1_tree));
        tracked_working_tree = parent1_tree;
      } else if (error == 0) {
        LOG_DEBUG("Parent 1 tree is also empty");
        git_tree_free(parent1_tree);
        parent1_tree = nullptr;
        git_commit_free(parent1_commit);
        parent1_commit = nullptr;
        tracked_working_tree = nullptr;
      } else {
        tracked_working_tree = nullptr;
      }
    } else {
      tracked_working_tree = nullptr;
    }
  }

  // Get parent commit (the commit the stash was based on)
  error = git_commit_parent(&parent_commit, stash_commit, 0);
  if (error && error != GIT_ENOTFOUND) {
    LOG_ERROR(Log::ErrorId::Git, "Failed to get stash parent: error={}", error);
    setError(GitError::GetStashStatusFailedToGetBaseParentCommit, error);
    git_tree_free(stash_tree);
    if (parent1_tree) {
      git_tree_free(parent1_tree);
    }
    if (parent2_tree) {
      git_tree_free(parent2_tree);
    }
    if (parent1_commit) {
      git_commit_free(parent1_commit);
    }
    if (parent2_commit) {
      git_commit_free(parent2_commit);
    }
    git_commit_free(stash_commit);
    LOG_PERF(0, "Exit");
    return {};
  }

  if (error == GIT_ENOTFOUND) {
    parent_tree = nullptr; // Initial stash
  } else {
    error = git_commit_tree(&parent_tree, parent_commit);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to get parent tree: error={}",
                error);
      setError(GitError::GetStashStatusFailedToGetSecondCommitTree, error);
      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      git_commit_free(stash_commit);
      git_commit_free(parent_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }
  }

  // Load the untracked-files commit if it exists. It is separate from the
  // tracked-worktree diff and should be shown in the stash viewer as added
  // files.
  error = git_commit_parent(&parent2_commit, stash_commit, 2);
  if (!error) {
    error = git_commit_tree(&parent2_tree, parent2_commit);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to get untracked stash tree: error={}", error);
      setError(GitError::GetStashStatusFailedToGetThirdCommitTree, error);
      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      if (parent_tree) {
        git_tree_free(parent_tree);
      }
      git_commit_free(stash_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      if (parent_commit) {
        git_commit_free(parent_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }
  } else if (error != GIT_ENOTFOUND) {
    LOG_ERROR(Log::ErrorId::Git,
              "Failed to get untracked stash parent: error={}", error);
    setError(GitError::GetStashStatusFailedToGetFourthCommitTree, error);
    git_tree_free(stash_tree);
    if (parent1_tree) {
      git_tree_free(parent1_tree);
    }
    if (parent_tree) {
      git_tree_free(parent_tree);
    }
    git_commit_free(stash_commit);
    if (parent1_commit) {
      git_commit_free(parent1_commit);
    }
    if (parent_commit) {
      git_commit_free(parent_commit);
    }
    LOG_PERF(0, "Exit");
    return {};
  }

  // Generate diff for tracked changes.
  git_diff_options opts = GIT_DIFF_OPTIONS_INIT;
  opts.flags |= GIT_DIFF_INCLUDE_TYPECHANGE | GIT_DIFF_RECURSE_UNTRACKED_DIRS |
                GIT_DIFF_INCLUDE_UNTRACKED | GIT_DIFF_IGNORE_CASE |
                GIT_DIFF_INCLUDE_CASECHANGE | GIT_DIFF_SHOW_UNTRACKED_CONTENT;

  if (tracked_working_tree) {
    error = git_diff_tree_to_tree(&diff, repo, parent_tree,
                                  tracked_working_tree, &opts);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git, "Failed to create diff for stash: error={}",
                error);
      setError(GitError::GetStashStatusFailedToCreateStashDiff, error);

      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      if (parent_tree) {
        git_tree_free(parent_tree);
      }
      git_commit_free(stash_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      if (parent_commit) {
        git_commit_free(parent_commit);
      }
      LOG_PERF(0, "Exit");
      return {};
    }

    collectCommitStatusFromDiff(diff, &status);
    git_diff_free(diff);
    diff = nullptr;
  }

  // Untracked files are stored in parent 2. Walk that tree directly so the
  // stash viewer shows them as files in the stash content list.
  if (parent2_tree) {
    QStringList untrackedFiles;
    CollectTreePathsPayload treePayload{&status, &untrackedFiles};
    error = git_tree_walk(parent2_tree, GIT_TREEWALK_PRE,
                          collectTreePathsCallback, &treePayload);
    if (error) {
      LOG_ERROR(Log::ErrorId::Git,
                "Failed to walk stash untracked tree: error={}", error);
      setError(GitError::GetStashStatusFailedToWalkUntrackedStashTree, error);

      git_tree_free(stash_tree);
      if (parent1_tree) {
        git_tree_free(parent1_tree);
      }
      if (parent2_tree) {
        git_tree_free(parent2_tree);
      }
      if (parent_tree) {
        git_tree_free(parent_tree);
      }
      git_commit_free(stash_commit);
      if (parent1_commit) {
        git_commit_free(parent1_commit);
      }
      if (parent2_commit) {
        git_commit_free(parent2_commit);
      }
      if (parent_commit) {
        git_commit_free(parent_commit);
      }
      if (diff) {
        git_diff_free(diff);
      }
      LOG_PERF(0, "Exit");
      return {};
    }

    status.untrackedFiles += untrackedFiles;
  }

  // Get stash commit OID
  const git_oid *oid = git_commit_id(stash_commit);
  char oid_str[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oid_str, GIT_OID_HEXSZ + 1, oid);
  status.commit = QString::fromUtf8(oid_str);

  if (const char *summary = git_commit_summary(stash_commit)) {
    status.summary = QString::fromUtf8(summary);
  } else if (const char *message = git_commit_message(stash_commit)) {
    status.summary = QString::fromUtf8(message).section('\n', 0, 0);
  }

  const int parentCount = git_commit_parentcount(stash_commit);
  status.parentShas.reserve(parentCount);
  for (int i = 0; i < parentCount; ++i) {
    const git_oid *parentOid = git_commit_parent_id(stash_commit, i);
    if (!parentOid) {
      continue;
    }

    char parentOidStr[GIT_OID_HEXSZ + 1] = {0};
    git_oid_tostr(parentOidStr, GIT_OID_HEXSZ + 1, parentOid);
    status.parentShas += QString::fromUtf8(parentOidStr);
  }

  if (const git_signature *author = git_commit_author(stash_commit)) {
    if (author->name) {
      status.authorName = QString::fromUtf8(author->name);
    }
    if (author->email) {
      status.authorEmail = QString::fromUtf8(author->email);
    }
  }

  status.committerDate =
      commitSignatureDateToString(git_commit_committer(stash_commit));

  // Cleanup
  git_tree_free(stash_tree);

  if (parent1_tree) {
    git_tree_free(parent1_tree);
  }
  if (parent2_tree) {
    git_tree_free(parent2_tree);
  }
  if (parent_tree) {
    git_tree_free(parent_tree);
  }

  if (parent1_commit) {
    git_commit_free(parent1_commit);
  }
  if (parent2_commit) {
    git_commit_free(parent2_commit);
  }
  if (parent_commit) {
    git_commit_free(parent_commit);
  }

  git_commit_free(stash_commit);

  LOG_DEBUG(
      "Stash status retrieved for index {}: {} new, {} modified, {} deleted",
      stashIndex, status.newFiles.size(), status.modifiedFiles.size(),
      status.deletedFiles.size());
  LOG_PERF(0, "Exit");
  return status;
}

bool deleteStash(git_repository *repo, int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to delete stash at index: {}", stashIndex);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when deleting stash at index {}",
              stashIndex);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (const int error = git_stash_drop(repo, stashIndex)) {
    setError(GitError::DeleteStashFailedToDropStash, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");

  return true;
}

bool deleteAllStashes(git_repository *repo) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Attempting to delete all stashes");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer is null when deleting all stashes");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Get the count of stashes first
  size_t stashCount = 0;
  if (const int error = git_stash_foreach(
          repo,
          [](size_t index, const char *name, const git_oid *oid,
             void *payload) -> int {
            (*static_cast<size_t *>(payload))++;
            return 0;
          },
          &stashCount)) {
    setError(GitError::DeleteAllStashesFailedToReadObjectId, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Found {} stashes to delete", stashCount);

  // Delete stashes from highest index to lowest to avoid index shifting issues
  for (int i = static_cast<int>(stashCount) - 1; i >= 0; --i) {
    if (const int error = git_stash_drop(repo, i)) {
      LOG_WARN("Failed to delete stash at index {}: error code {}", i, error);
      setError(GitError::DeleteAllStashesFailedToDropStash, error);
      // Continue trying to delete other stashes even if one fails
    } else {
      LOG_DEBUG("Deleted stash at index {}", i);
    }
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool stash(git_repository *repo, const QString &note) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_oid stashId;
  git_signature *signature = nullptr;

  // Get default signature (author info)
  int error = git_signature_default(&signature, repo);
  if (error) {
    setError(GitError::StashFailedToGetDefaultSignature, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QByteArray noteUtf8 = note.toUtf8();
  error = git_stash_save(
      &stashId,  // Optional: receives the stash commit ID
      repo,      // Your git_repository pointer
      signature, // Signature of the stasher
      noteUtf8.isEmpty() ? nullptr : noteUtf8.constData(),
      GIT_STASH_DEFAULT | GIT_STASH_INCLUDE_UNTRACKED // Flags (can be
      // GIT_STASH_INCLUDE_UNTRACKED, etc.)
  );
  if (error) {
    setError(GitError::StashFailedToCreateStash, error);
    git_signature_free(signature);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_signature_free(signature);

  LOG_PERF(0, "Exit");

  return true;
}

QString listConflictsForStashFile(git_repository *repo, int stashIndex,
                                  const QString &filePath) {
  LOG_PERF(0, "Enter");

  (void)stashIndex;

  if (!repo || filePath.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository pointer or file path is null when reading stash "
              "conflict markers");
    LOG_PERF(0, "Exit");
    return {};
  }

  const char *workdir = git_repository_workdir(repo);
  if (!workdir) {
    LOG_ERROR(Log::ErrorId::Git,
              "Repository working directory is unavailable when reading stash "
              "conflict markers");
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString fullPath = QDir::fromNativeSeparators(
      QString::fromUtf8(workdir) + QDir::separator() + filePath);
  QFile file(fullPath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    LOG_DEBUG("Could not read stash conflict file '{}': '{}'",
              filePath.toStdString(), fullPath.toStdString());
    LOG_PERF(0, "Exit");
    return {};
  }

  const QString content = QString::fromUtf8(file.readAll());
  file.close();

  LOG_PERF(0, "Exit");
  return content;
}

} // namespace GitRepo
