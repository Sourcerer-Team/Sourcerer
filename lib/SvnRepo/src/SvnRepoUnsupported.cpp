// Git operations without a Subversion counterpart. Queries return what a
// repository that never used the feature would return, actions fail with
// SvnError::UnsupportedOperation.

#include "SvnRepo.h"
#include "SvnRepoPrivate.h"

namespace SvnRepo {

using namespace Private;

GitRepo::Lfs::StorageStatus getLfsStorageStatus(const QString &path) {
  GitRepo::Lfs::StorageStatus status;
  status.repositoryPath = normalizedPath(path);
  return status;
}

bool rebaseBranches(const QString &, const QString &, const QString &,
                    const QString &, const QString &, const bool) {
  return setUnsupported("Rebase");
}

bool squashBranches(const QString &, const QString &, const QString &,
                    const QString &, const QString &, const bool) {
  return setUnsupported("Squash");
}

bool getIsRebaseInProgress(const QString &) {
  return false;
}

bool abortRebase(const QString &) {
  return setUnsupported("Aborting a rebase");
}

bool commitRebase(const QString &, const QString &, const QString &,
                  const QString &, const QString &, bool, bool, bool) {
  return setUnsupported("Committing during a rebase");
}

bool bisect(const QString &, GitRepo::BisectAction, const QString &,
            GitRepo::BisectStatus &status) {
  status = GitRepo::BisectStatus();
  return setUnsupported("Bisect");
}

QHash<QString, int> getBisectMarks(const QString &) {
  return {};
}

QVector<GitRepo::WorktreeInfo> listWorktrees(const QString &) {
  return {};
}

bool addWorktree(const QString &, const QString &, const QString &,
                 const QString &, bool, const QString &) {
  return setUnsupported("Adding a worktree");
}

bool removeWorktree(const QString &, const QString &, bool) {
  return setUnsupported("Removing a worktree");
}

bool lockWorktree(const QString &, const QString &, const QString &) {
  return setUnsupported("Locking a worktree");
}

bool unlockWorktree(const QString &, const QString &) {
  return setUnsupported("Unlocking a worktree");
}

bool startGitLabFlowFeature(const QString &, const QString &, const QString &) {
  return setUnsupported("GitLab Flow");
}

bool finishGitLabFlowFeature(const QString &, const QString &, const QString &,
                             const QString &, const QString &) {
  return setUnsupported("GitLab Flow");
}

bool promoteGitLabFlowEnvironment(const QString &, const QStringList &,
                                  const QString &, const QString &,
                                  const QString &) {
  return setUnsupported("GitLab Flow");
}

bool startGitLabFlowRelease(const QString &, const QString &, const QString &) {
  return setUnsupported("GitLab Flow");
}

bool cherryPickToGitLabFlowRelease(const QString &, const QString &,
                                   const QString &, const QString &,
                                   const QString &, bool *conflicted) {
  if (conflicted) {
    *conflicted = false;
  }
  return setUnsupported("GitLab Flow");
}

bool startGitHubFlowBranch(const QString &, const QString &, const QString &) {
  return setUnsupported("GitHub Flow");
}

bool updateGitHubFlowBranch(const QString &, const QString &, const QString &,
                            const QString &, const QString &) {
  return setUnsupported("GitHub Flow");
}

bool finishGitHubFlowBranch(const QString &, const QString &, const QString &,
                            const QString &, const QString &) {
  return setUnsupported("GitHub Flow");
}

bool readGitFlowNextConfig(const QString &,
                           GitRepo::GitFlowNextConfig *config) {
  if (config) {
    *config = GitRepo::GitFlowNextConfig();
  }
  return false;
}

bool initGitFlowNext(const QString &, const QString &, const QString &) {
  return setUnsupported("GitFlow-Next");
}

bool startGitFlowNextBranch(const QString &, const QString &, const QString &) {
  return setUnsupported("GitFlow-Next");
}

bool finishGitFlowNextBranch(const QString &, const QString &, const QString &,
                             const QString &) {
  return setUnsupported("GitFlow-Next");
}

bool updateGitFlowNextBranch(const QString &, const QString &, const QString &,
                             const QString &) {
  return setUnsupported("GitFlow-Next");
}

GitRepo::WorkflowType getWorkflowType(const QString &) {
  return GitRepo::WorkflowType::GitFlow;
}

bool setWorkflowType(const QString &, GitRepo::WorkflowType) {
  return setUnsupported("Changing the workflow");
}

bool applyGitFlowNextWorkflow(const QString &,
                              const GitRepo::GitFlowNextWorkflow &) {
  return setUnsupported("GitFlow-Next");
}

bool getGitFlowNextWorkflowReference(const QString &, QString *id,
                                     int *revision) {
  if (id) {
    id->clear();
  }
  if (revision) {
    *revision = 0;
  }
  return false;
}

} // namespace SvnRepo
