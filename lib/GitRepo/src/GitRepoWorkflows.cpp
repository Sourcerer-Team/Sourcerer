#include "GitRepo.h"
#include "GitRepoPrivate.h"

#include <ConfigManager.h>
#include <Log.h>

#include <QVector>

#include <algorithm>

#define THREAD_ID "GitRepo"

// Branching workflows: Git Flow, GitLab Flow, GitHub Flow and git-flow-next.
namespace GitRepo {

using namespace Private;

static QString gitFlowBranchPrefix(GitFlowBranchType branchType) {
  switch (branchType) {
  case GitFlowBranchType::Feature:
    return QStringLiteral("feature");
  case GitFlowBranchType::Release:
    return QStringLiteral("release");
  case GitFlowBranchType::Hotfix:
    return QStringLiteral("hotfix");
  case GitFlowBranchType::Unknown:
  default:
    return QString();
  }
}

static QString gitFlowBranchName(GitFlowBranchType branchType,
                                 const QString &branchName) {
  const QString trimmedBranchName = branchName.trimmed();
  if (trimmedBranchName.isEmpty()) {
    return {};
  }

  const QString prefix = gitFlowBranchPrefix(branchType);
  if (prefix.isEmpty() ||
      trimmedBranchName.startsWith(prefix + QStringLiteral("/"))) {
    return trimmedBranchName;
  }

  return prefix + QStringLiteral("/") + trimmedBranchName;
}

static bool localBranchExists(git_repository *repo, const QString &branchName) {
  git_reference *ref = nullptr;
  const int error = git_branch_lookup(
      &ref, repo, branchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (!error) {
    git_reference_free(ref);
    return true;
  }
  return false;
}

static QString gitFlowConfiguredBranchName(ConfigManager::ConfigKey key) {
  return ConfigManager::ConfigManager::getInstance().getString(key).trimmed();
}

static QString
defaultGitFlowBaseBranchName(git_repository *repo, GitFlowBranchType branchType,
                             const QString &developmentBranchName) {
  const auto branchExists = [repo](const QString &branchName) {
    return localBranchExists(repo, branchName);
  };

  const QString configuredDevelopmentBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName);
  const QString configuredMainBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName);
  const QString configuredMasterBranchName = QStringLiteral("master");
  const QString normalizedDevelopmentBranchName =
      developmentBranchName.trimmed().isEmpty()
          ? configuredDevelopmentBranchName
          : developmentBranchName.trimmed();

  switch (branchType) {
  case GitFlowBranchType::Feature:
  case GitFlowBranchType::Release:
    if (branchExists(normalizedDevelopmentBranchName)) {
      return normalizedDevelopmentBranchName;
    }
    break;
  case GitFlowBranchType::Hotfix:
    if (branchExists(configuredMainBranchName)) {
      return configuredMainBranchName;
    }
    if (branchExists(configuredMasterBranchName)) {
      return configuredMasterBranchName;
    }
    break;
  case GitFlowBranchType::Unknown:
  default:
    break;
  }

  if (branchExists(normalizedDevelopmentBranchName)) {
    return normalizedDevelopmentBranchName;
  }

  if (branchExists(configuredMainBranchName)) {
    return configuredMainBranchName;
  }

  if (branchExists(configuredMasterBranchName)) {
    return configuredMasterBranchName;
  }

  return getActiveBranch(repo, BranchType::Local, false);
}

static QString
defaultGitFlowPrimaryBranchName(git_repository *repo,
                                const QString &developmentBranchName) {
  const auto branchExists = [repo](const QString &branchName) {
    return localBranchExists(repo, branchName);
  };

  const QString configuredMainBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName);
  const QString configuredMasterBranchName = QStringLiteral("master");
  const QString configuredDevelopmentBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName);
  const QString normalizedDevelopmentBranchName =
      developmentBranchName.trimmed().isEmpty()
          ? configuredDevelopmentBranchName
          : developmentBranchName.trimmed();

  if (branchExists(configuredMainBranchName)) {
    return configuredMainBranchName;
  }

  if (branchExists(configuredMasterBranchName)) {
    return configuredMasterBranchName;
  }

  if (branchExists(normalizedDevelopmentBranchName)) {
    return normalizedDevelopmentBranchName;
  }

  return getActiveBranch(repo, BranchType::Local, false);
}

// Creates branchName at the tip of the local baseBranchName and checks it out.
static bool createAndCheckoutBranch(git_repository *repo,
                                    const QString &baseBranchName,
                                    const QString &branchName) {
  git_reference *baseRef = nullptr;
  const int error = git_branch_lookup(
      &baseRef, repo, baseBranchName.toStdString().c_str(), GIT_BRANCH_LOCAL);
  if (error) {
    setError(GitError::CheckoutBranchFailedToLookupBranch, error);
    return false;
  }

  const git_oid *baseOid = git_reference_target(baseRef);
  if (!baseOid) {
    git_reference_free(baseRef);
    setError(GitError::CheckoutBranchFailedToLookupBranch, GIT_ENOTFOUND);
    return false;
  }

  char oidBuffer[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(oidBuffer, sizeof(oidBuffer), baseOid);
  git_reference_free(baseRef);

  if (!createBranchFromCommit(repo, QString::fromLatin1(oidBuffer),
                              branchName)) {
    return false;
  }

  return checkoutBranch(repo, branchName, BranchType::Local);
}

bool startGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                        const QString &branchName,
                        const QString &developmentBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (gitFlowBranchPrefix(branchType).isEmpty()) {
    setErrorContext(GitError::GitFlowUnknownBranchType,
                    gitErrorMessage(GitError::GitFlowUnknownBranchType), -1);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName =
      gitFlowBranchName(branchType, branchName);
  if (normalizedBranchName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "GitFlow branch name is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString baseBranchName =
      defaultGitFlowBaseBranchName(repo, branchType, developmentBranchName);
  if (baseBranchName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git,
              "Unable to determine the base branch for GitFlow branch '{}'",
              normalizedBranchName.toStdString());
    setError(GitError::CheckoutBranchFailedToLookupBranch, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created =
      createAndCheckoutBranch(repo, baseBranchName, normalizedBranchName);

  LOG_PERF(0, "Exit");
  return created;
}

bool finishGitFlowBranch(git_repository *repo, GitFlowBranchType branchType,
                         const QString &branchName, const QString &author,
                         const QString &email,
                         const QString &developmentBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString prefix = gitFlowBranchPrefix(branchType);
  if (prefix.isEmpty()) {
    setErrorContext(GitError::GitFlowUnknownBranchType,
                    gitErrorMessage(GitError::GitFlowUnknownBranchType), -1);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName =
      gitFlowBranchName(branchType, branchName);
  if (normalizedBranchName.isEmpty()) {
    LOG_ERROR(Log::ErrorId::Git, "GitFlow branch name is empty");
    LOG_PERF(0, "Exit");
    return false;
  }

  // Like git flow, a release or hotfix is tagged on the primary branch with
  // its name, refused before anything is merged if the tag exists.
  const bool tagged = branchType == GitFlowBranchType::Release ||
                      branchType == GitFlowBranchType::Hotfix;
  const QString tagName = normalizedBranchName.mid(prefix.size() + 1);
  if (tagged) {
    git_oid tagOid;
    if (git_reference_name_to_id(
            &tagOid, repo,
            (QStringLiteral("refs/tags/") + tagName).toUtf8().constData()) ==
        0) {
      setErrorContext(GitError::GitFlowTagExists,
                      gitErrorMessage(GitError::GitFlowTagExists), -1);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const QString primaryBranchName =
      defaultGitFlowPrimaryBranchName(repo, developmentBranchName);
  if (primaryBranchName.isEmpty()) {
    LOG_ERROR(
        Log::ErrorId::Git,
        "Unable to determine the destination branch for GitFlow branch '{}'",
        normalizedBranchName.toStdString());
    setError(GitError::MergeFailedToLookupDestinationBranch, GIT_ENOTFOUND);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString configuredDevelopmentBranchName = gitFlowConfiguredBranchName(
      ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName);
  const QString normalizedDevelopmentBranchName =
      developmentBranchName.trimmed().isEmpty()
          ? configuredDevelopmentBranchName
          : developmentBranchName.trimmed();

  const QString destinationBranchName = branchType == GitFlowBranchType::Feature
                                            ? normalizedDevelopmentBranchName
                                            : primaryBranchName;

  if (!merge(repo, normalizedBranchName, destinationBranchName, author,
             email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (tagged) {
    git_oid primaryOid;
    char primaryId[GIT_OID_HEXSZ + 1] = {0};
    if (git_reference_name_to_id(
            &primaryOid, repo,
            (QStringLiteral("refs/heads/") + destinationBranchName)
                .toUtf8()
                .constData()) == 0) {
      git_oid_tostr(primaryId, sizeof(primaryId), &primaryOid);
    }
    if (!createTagFromCommit(repo, QString::fromLatin1(primaryId), tagName,
                             author, email, tagName)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (branchType == GitFlowBranchType::Release ||
      branchType == GitFlowBranchType::Hotfix) {
    if (normalizedDevelopmentBranchName != destinationBranchName &&
        !merge(repo, normalizedBranchName, normalizedDevelopmentBranchName,
               author, email)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (!deleteBranch(repo, normalizedBranchName, /*force=*/true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

static void setWorkflowError(const GitError errorType) {
  setErrorContext(errorType, gitErrorMessage(errorType), -1);
  LOG_ERROR(Log::ErrorId::Git, "Workflow error: {}",
            gitErrorMessage(errorType).toStdString());
}

static QString gitLabFlowFeatureBranchName(const QString &branchName) {
  const QString trimmedBranchName = branchName.trimmed();
  if (trimmedBranchName.isEmpty() ||
      trimmedBranchName.startsWith(QStringLiteral("feature/"))) {
    return trimmedBranchName;
  }

  return QStringLiteral("feature/") + trimmedBranchName;
}

bool startGitLabFlowFeature(git_repository *repo, const QString &branchName,
                            const QString &mainBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = gitLabFlowFeatureBranchName(branchName);
  if (normalizedBranchName.isEmpty()) {
    setWorkflowError(GitError::GitLabFlowBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString baseBranchName = mainBranchName.trimmed();
  if (!localBranchExists(repo, baseBranchName)) {
    setWorkflowError(GitError::GitLabFlowFailedToLookupBaseBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created =
      createAndCheckoutBranch(repo, baseBranchName, normalizedBranchName);

  LOG_PERF(0, "Exit");
  return created;
}

bool finishGitLabFlowFeature(git_repository *repo, const QString &branchName,
                             const QString &mainBranchName,
                             const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = gitLabFlowFeatureBranchName(branchName);
  if (normalizedBranchName.isEmpty()) {
    setWorkflowError(GitError::GitLabFlowBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString destinationBranchName = mainBranchName.trimmed();
  if (!localBranchExists(repo, destinationBranchName)) {
    setWorkflowError(GitError::GitLabFlowFailedToLookupBaseBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!merge(repo, normalizedBranchName, destinationBranchName, author,
             email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!deleteBranch(repo, normalizedBranchName, /*force=*/true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool promoteGitLabFlowEnvironment(git_repository *repo,
                                  const QStringList &environmentChain,
                                  const QString &sourceBranchName,
                                  const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedSourceBranchName = sourceBranchName.trimmed();
  const QString destinationBranchName =
      gitLabFlowNextEnvironment(environmentChain, normalizedSourceBranchName);
  if (destinationBranchName.isEmpty()) {
    setWorkflowError(GitError::GitLabFlowPromoteNotDownstream);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Promoting '{}' to '{}'", normalizedSourceBranchName.toStdString(),
            destinationBranchName.toStdString());

  if (!localBranchExists(repo, destinationBranchName)) {
    const bool created = createAndCheckoutBranch(
        repo, normalizedSourceBranchName, destinationBranchName);
    LOG_PERF(0, "Exit");
    return created;
  }

  const bool merged = merge(repo, normalizedSourceBranchName,
                            destinationBranchName, author, email);

  LOG_PERF(0, "Exit");
  return merged;
}

bool startGitLabFlowRelease(git_repository *repo,
                            const QString &releaseBranchName,
                            const QString &mainBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = releaseBranchName.trimmed();
  if (normalizedBranchName.isEmpty()) {
    setWorkflowError(GitError::GitLabFlowBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString baseBranchName = mainBranchName.trimmed();
  if (!localBranchExists(repo, baseBranchName)) {
    setWorkflowError(GitError::GitLabFlowFailedToLookupBaseBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created =
      createAndCheckoutBranch(repo, baseBranchName, normalizedBranchName);

  LOG_PERF(0, "Exit");
  return created;
}

bool cherryPickToGitLabFlowRelease(git_repository *repo, const QString &commit,
                                   const QString &releaseBranchName,
                                   const QString &author, const QString &email,
                                   bool *conflicted) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (conflicted) {
    *conflicted = false;
  }

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = releaseBranchName.trimmed();
  if (!localBranchExists(repo, normalizedBranchName)) {
    setWorkflowError(GitError::GitLabFlowFailedToLookupReleaseBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString originalBranchName =
      getActiveBranch(repo, BranchType::Local, false);

  if (originalBranchName != normalizedBranchName &&
      !checkoutBranch(repo, normalizedBranchName, BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  // A cherry-pick that stops with conflicts fails, the release branch stays
  // checked out with the cherry-pick in progress.
  if (!cherryPickCommit(repo, commit, author, email)) {
    if (getLastError().error == GitError::CherryPickCommitFailedWithConflicts) {
      LOG_DEBUG("Cherry-pick of '{}' onto '{}' has conflicts",
                commit.toStdString(), normalizedBranchName.toStdString());
      if (conflicted) {
        *conflicted = true;
      }
      LOG_PERF(0, "Exit");
      return true;
    }
    LOG_PERF(0, "Exit");
    return false;
  }

  if (hasConflicts(repo)) {
    LOG_DEBUG("Cherry-pick of '{}' onto '{}' has conflicts",
              commit.toStdString(), normalizedBranchName.toStdString());
    if (conflicted) {
      *conflicted = true;
    }
    LOG_PERF(0, "Exit");
    return true;
  }

  if (!originalBranchName.isEmpty() &&
      originalBranchName != normalizedBranchName &&
      !checkoutBranch(repo, originalBranchName, BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool startGitHubFlowBranch(git_repository *repo, const QString &branchName,
                           const QString &mainBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = branchName.trimmed();
  if (normalizedBranchName.isEmpty()) {
    setWorkflowError(GitError::GitHubFlowBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString baseBranchName = mainBranchName.trimmed();
  if (!localBranchExists(repo, baseBranchName)) {
    setWorkflowError(GitError::GitHubFlowFailedToLookupMainBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool created =
      createAndCheckoutBranch(repo, baseBranchName, normalizedBranchName);

  LOG_PERF(0, "Exit");
  return created;
}

bool updateGitHubFlowBranch(git_repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = branchName.trimmed();
  const QString normalizedMainBranchName = mainBranchName.trimmed();
  if (normalizedBranchName.isEmpty()) {
    setWorkflowError(GitError::GitHubFlowBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (normalizedBranchName == normalizedMainBranchName) {
    setWorkflowError(GitError::GitHubFlowMainBranchNotAllowed);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!localBranchExists(repo, normalizedMainBranchName)) {
    setWorkflowError(GitError::GitHubFlowFailedToLookupMainBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const bool merged = merge(repo, normalizedMainBranchName,
                            normalizedBranchName, author, email);

  LOG_PERF(0, "Exit");
  return merged;
}

bool finishGitHubFlowBranch(git_repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName = branchName.trimmed();
  const QString normalizedMainBranchName = mainBranchName.trimmed();
  if (normalizedBranchName.isEmpty()) {
    setWorkflowError(GitError::GitHubFlowBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (normalizedBranchName == normalizedMainBranchName) {
    setWorkflowError(GitError::GitHubFlowMainBranchNotAllowed);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!localBranchExists(repo, normalizedMainBranchName)) {
    setWorkflowError(GitError::GitHubFlowFailedToLookupMainBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!merge(repo, normalizedBranchName, normalizedMainBranchName, author,
             email)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!deleteBranch(repo, normalizedBranchName, /*force=*/true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool readGitFlowNextConfig(git_repository *repo, GitFlowNextConfig *config) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);
  Q_ASSERT(config != nullptr);

  if (!repo || !config) {
    LOG_ERROR(Log::ErrorId::Git, "Repository or config pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *repoConfig = nullptr;
  int error = git_repository_config(&repoConfig, repo);
  if (error) {
    setError(GitError::GitFlowNextFailedToReadConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *snapshot = nullptr;
  error = git_config_snapshot(&snapshot, repoConfig);
  git_config_free(repoConfig);
  if (error) {
    setError(GitError::GitFlowNextFailedToReadConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  QList<QPair<QString, QString>> entries;
  error = git_config_foreach_match(
      snapshot, "^gitflow\\.",
      [](const git_config_entry *entry, void *payload) -> int {
        static_cast<QList<QPair<QString, QString>> *>(payload)->append(
            {QString::fromUtf8(entry->name), QString::fromUtf8(entry->value)});
        return 0;
      },
      &entries);
  git_config_free(snapshot);
  if (error) {
    setError(GitError::GitFlowNextFailedToReadConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  *config = parseGitFlowNextConfig(entries);
  LOG_DEBUG("git-flow-next config: initialized={}, {} branch definitions",
            config->initialized, config->branches.size());

  LOG_PERF(0, "Exit");
  return true;
}

bool initGitFlowNext(git_repository *repo, const QString &mainBranchName,
                     const QString &developBranchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString main = mainBranchName.trimmed();
  const QString develop = developBranchName.trimmed();
  if (main.isEmpty() || develop.isEmpty()) {
    setWorkflowError(GitError::GitFlowNextBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!localBranchExists(repo, main)) {
    setWorkflowError(GitError::GitFlowNextFailedToLookupBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    setError(GitError::GitFlowNextFailedToWriteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *snapshot = nullptr;
  error = git_config_snapshot(&snapshot, config);
  if (error) {
    setError(GitError::GitFlowNextFailedToReadConfig, error);
    git_config_free(config);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Initializing again only adds what is missing, a branch model the user
  // changed is kept.
  const auto entries = gitFlowNextClassicPreset(main, develop);
  for (const auto &[key, value] : entries) {
    git_config_entry *existing = nullptr;
    if (git_config_get_entry(&existing, snapshot, key.toUtf8().constData()) ==
        0) {
      git_config_entry_free(existing);
      continue;
    }

    error = git_config_set_string(config, key.toUtf8().constData(),
                                  value.toUtf8().constData());
    if (error) {
      setError(GitError::GitFlowNextFailedToWriteConfig, error);
      git_config_free(snapshot);
      git_config_free(config);
      LOG_PERF(0, "Exit");
      return false;
    }
  }
  git_config_free(snapshot);
  git_config_free(config);

  if (!localBranchExists(repo, develop) &&
      !createAndCheckoutBranch(repo, main, develop)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

static bool loadInitializedGitFlowNextConfig(git_repository *repo,
                                             GitFlowNextConfig &config) {
  if (!readGitFlowNextConfig(repo, &config)) {
    return false;
  }

  if (!config.initialized) {
    setWorkflowError(GitError::GitFlowNextNotInitialized);
    return false;
  }

  return true;
}

static QString branchTipId(git_repository *repo, const QString &branchName) {
  git_oid oid;
  const QString refName = QStringLiteral("refs/heads/") + branchName;
  if (git_reference_name_to_id(&oid, repo, refName.toUtf8().constData())) {
    return {};
  }

  char buffer[GIT_OID_HEXSZ + 1] = {0};
  git_oid_tostr(buffer, sizeof(buffer), &oid);
  return QString::fromLatin1(buffer);
}

static bool tagExists(git_repository *repo, const QString &tagName) {
  git_oid oid;
  const QString refName = QStringLiteral("refs/tags/") + tagName;
  return git_reference_name_to_id(&oid, repo, refName.toUtf8().constData()) ==
         0;
}

// Brings branchName up to date with parentBranch, see downstreamStrategy.
static bool updateFromParent(git_repository *repo, const QString &branchName,
                             const QString &parentBranch,
                             const QString &strategy, const QString &author,
                             const QString &email) {
  if (strategy == QStringLiteral("rebase")) {
    return rebaseBranches(repo, branchName, parentBranch, author, email, false);
  }

  if (strategy == QStringLiteral("merge")) {
    return merge(repo, parentBranch, branchName, author, email);
  }

  setWorkflowError(GitError::GitFlowNextUnsupportedStrategy);
  return false;
}

bool startGitFlowNextBranch(git_repository *repo, const QString &branchType,
                            const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  GitFlowNextConfig config;
  if (!loadInitializedGitFlowNextConfig(repo, config)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QVector<GitFlowNextBranch> types = config.topicTypes();
  const auto type = std::find_if(
      types.cbegin(), types.cend(), [&](const GitFlowNextBranch &candidate) {
        return candidate.name == branchType.trimmed();
      });
  if (type == types.cend()) {
    setWorkflowError(GitError::GitFlowNextUnknownBranchType);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString normalizedBranchName =
      gitFlowNextBranchNameForInput(*type, branchName);
  if (normalizedBranchName.isEmpty() || normalizedBranchName == type->prefix) {
    setWorkflowError(GitError::GitFlowNextBranchNameEmpty);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString startPoint = gitFlowNextStartPoint(*type);
  if (!localBranchExists(repo, startPoint)) {
    setWorkflowError(GitError::GitFlowNextFailedToLookupBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Starting '{}' from '{}'", normalizedBranchName.toStdString(),
            startPoint.toStdString());
  const bool created =
      createAndCheckoutBranch(repo, startPoint, normalizedBranchName);

  LOG_PERF(0, "Exit");
  return created;
}

bool finishGitFlowNextBranch(git_repository *repo, const QString &branchName,
                             const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  GitFlowNextConfig config;
  if (!loadInitializedGitFlowNextConfig(repo, config)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString branch = branchName.trimmed();
  const GitFlowNextBranch *type = gitFlowNextTopicTypeForBranch(config, branch);
  if (!type) {
    setWorkflowError(GitError::GitFlowNextNotATopicBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString parent = type->parent;
  if (!localBranchExists(repo, branch) || !localBranchExists(repo, parent)) {
    setWorkflowError(GitError::GitFlowNextFailedToLookupBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString tagName =
      type->tag ? gitFlowNextTagName(*type, branch) : QString();
  if (!tagName.isEmpty() && tagExists(repo, tagName)) {
    setWorkflowError(GitError::GitFlowNextTagExists);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Finishing '{}' into '{}' with strategy '{}'", branch.toStdString(),
            parent.toStdString(), type->upstreamStrategy.toStdString());

  bool integrated = false;
  if (type->upstreamStrategy == QStringLiteral("merge")) {
    integrated = merge(repo, branch, parent, author, email);
  } else if (type->upstreamStrategy == QStringLiteral("rebase")) {
    // Replay the branch onto its parent, then fast-forward the parent.
    integrated = rebaseBranches(repo, branch, parent, author, email, false) &&
                 merge(repo, branch, parent, author, email);
  } else if (type->upstreamStrategy == QStringLiteral("squash")) {
    integrated = squashBranches(repo, branch, parent, author, email, false);
  } else {
    setWorkflowError(GitError::GitFlowNextUnsupportedStrategy);
  }

  if (!integrated) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!tagName.isEmpty() &&
      !createTagFromCommit(repo, branchTipId(repo, parent), tagName, author,
                           email, tagName)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QStringList autoUpdateBranches =
      gitFlowNextAutoUpdateBranches(config, parent);
  for (const QString &baseBranch : autoUpdateBranches) {
    if (!localBranchExists(repo, baseBranch)) {
      continue;
    }

    const auto base =
        std::find_if(config.branches.cbegin(), config.branches.cend(),
                     [&](const GitFlowNextBranch &candidate) {
                       return !candidate.topic && candidate.name == baseBranch;
                     });
    LOG_DEBUG("Updating '{}' from '{}'", baseBranch.toStdString(),
              parent.toStdString());
    if (!updateFromParent(repo, baseBranch, parent, base->downstreamStrategy,
                          author, email)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  if (!checkoutBranch(repo, parent, BranchType::Local)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!type->keepOnFinish && !deleteBranch(repo, branch, /*force=*/true)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool updateGitFlowNextBranch(git_repository *repo, const QString &branchName,
                             const QString &author, const QString &email) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  GitFlowNextConfig config;
  if (!loadInitializedGitFlowNextConfig(repo, config)) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const QString branch = branchName.trimmed();
  const GitFlowNextBranch *type = gitFlowNextTopicTypeForBranch(config, branch);
  if (!type) {
    setWorkflowError(GitError::GitFlowNextNotATopicBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!localBranchExists(repo, branch) ||
      !localBranchExists(repo, type->parent)) {
    setWorkflowError(GitError::GitFlowNextFailedToLookupBranch);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_DEBUG("Updating '{}' from '{}' with strategy '{}'", branch.toStdString(),
            type->parent.toStdString(), type->downstreamStrategy.toStdString());
  const bool updated = updateFromParent(
      repo, branch, type->parent, type->downstreamStrategy, author, email);

  LOG_PERF(0, "Exit");
  return updated;
}

static const char *kWorkflowIdKey = "sourcerer.gitflownext.workflow";

static const char *kWorkflowRevisionKey = "sourcerer.gitflownext.revision";

// Removes the reference to a custom GitFlow-Next workflow, if there is one.
static void clearGitFlowNextWorkflowReference(git_config *localConfig) {
  for (const char *key : {kWorkflowIdKey, kWorkflowRevisionKey}) {
    const int error = git_config_delete_entry(localConfig, key);
    if (error && error != GIT_ENOTFOUND) {
      LOG_WARN("Failed to remove '{}' from the repository config: error={}",
               key, error);
    }
  }
}

bool applyGitFlowNextWorkflow(git_repository *repo,
                              const GitFlowNextWorkflow &workflow) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  if (!validateGitFlowNextWorkflow(workflow).isEmpty()) {
    setWorkflowError(GitError::GitFlowNextInvalidWorkflow);
    LOG_PERF(0, "Exit");
    return false;
  }

  // The root base branch is where the missing base branches start from.
  QStringList existingBaseBranches;
  for (const GitFlowNextBranch &branch : workflow.baseBranches()) {
    if (localBranchExists(repo, branch.name)) {
      existingBaseBranches.append(branch.name);
    } else if (branch.parent.isEmpty()) {
      setWorkflowError(GitError::GitFlowNextFailedToLookupBranch);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    setError(GitError::GitFlowNextFailedToWriteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Only the repository's own configuration is changed, never the global one.
  git_config *localConfig = nullptr;
  error = git_config_open_level(&localConfig, config, GIT_CONFIG_LEVEL_LOCAL);
  git_config_free(config);
  if (error) {
    setError(GitError::GitFlowNextFailedToWriteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  // Replace the previous branch model, including branch types that the new
  // workflow doesn't have.
  QStringList oldKeys;
  git_config_foreach_match(
      localConfig, "^gitflow\\.(branch\\..+|[^.]+\\.finish\\..+)$",
      [](const git_config_entry *entry, void *payload) -> int {
        static_cast<QStringList *>(payload)->append(
            QString::fromUtf8(entry->name));
        return 0;
      },
      &oldKeys);
  for (const QString &key : std::as_const(oldKeys)) {
    error = git_config_delete_entry(localConfig, key.toUtf8().constData());
    if (error && error != GIT_ENOTFOUND) {
      setError(GitError::GitFlowNextFailedToWriteConfig, error);
      git_config_free(localConfig);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  const auto entries = gitFlowNextConfigEntries(workflow);
  for (const auto &[key, value] : entries) {
    error = git_config_set_string(localConfig, key.toUtf8().constData(),
                                  value.toUtf8().constData());
    if (error) {
      setError(GitError::GitFlowNextFailedToWriteConfig, error);
      git_config_free(localConfig);
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  error = git_config_set_string(
      localConfig, "sourcerer.workflow",
      workflowTypeToString(WorkflowType::GitFlowNext).toUtf8().constData());
  if (!error) {
    error = git_config_set_string(localConfig, kWorkflowIdKey,
                                  workflow.id.toUtf8().constData());
  }
  if (!error) {
    error = git_config_set_int32(localConfig, kWorkflowRevisionKey,
                                 workflow.revision);
  }
  git_config_free(localConfig);
  if (error) {
    setError(GitError::GitFlowNextFailedToWriteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  const QStringList toCreate =
      gitFlowNextBaseBranchesToCreate(workflow, existingBaseBranches);
  const QVector<GitFlowNextBranch> baseBranches = workflow.baseBranches();
  for (const QString &name : toCreate) {
    const auto branch = std::find_if(baseBranches.cbegin(), baseBranches.cend(),
                                     [&](const GitFlowNextBranch &candidate) {
                                       return candidate.name == name;
                                     });
    LOG_DEBUG("Creating base branch '{}' from '{}'", name.toStdString(),
              branch->parent.toStdString());
    if (!createBranchFromCommit(repo, branchTipId(repo, branch->parent),
                                name)) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");
  return true;
}

bool getGitFlowNextWorkflowReference(git_repository *repo, QString *id,
                                     int *revision) {
  if (!repo) {
    return false;
  }

  git_config *config = nullptr;
  if (git_repository_config(&config, repo)) {
    return false;
  }
  git_config *snapshot = nullptr;
  const int snapshotError = git_config_snapshot(&snapshot, config);
  git_config_free(config);
  if (snapshotError) {
    return false;
  }

  git_buf buf = GIT_BUF_INIT;
  int32_t storedRevision = 0;
  const bool found =
      git_config_get_string_buf(&buf, snapshot, kWorkflowIdKey) == 0 &&
      buf.ptr && buf.ptr[0] != '\0';
  if (found) {
    if (id) {
      *id = QString::fromUtf8(buf.ptr);
    }
    git_config_get_int32(&storedRevision, snapshot, kWorkflowRevisionKey);
    if (revision) {
      *revision = storedRevision;
    }
  }
  git_buf_dispose(&buf);
  git_config_free(snapshot);
  return found;
}

WorkflowType getWorkflowType(git_repository *repo) {
  LOG_PERF(0, "Enter");

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return WorkflowType::GitFlow;
  }

  git_config *config = nullptr;
  if (git_repository_config(&config, repo)) {
    LOG_DEBUG("Failed to get repository config, using Git Flow workflow");
    LOG_PERF(0, "Exit");
    return WorkflowType::GitFlow;
  }

  git_buf buf = GIT_BUF_INIT;
  const int error =
      git_config_get_string_buf(&buf, config, "sourcerer.workflow");
  const WorkflowType type =
      error ? WorkflowType::GitFlow
            : workflowTypeFromString(QString::fromUtf8(buf.ptr));
  git_buf_dispose(&buf);
  git_config_free(config);

  LOG_DEBUG("Repository workflow is '{}'",
            workflowTypeToString(type).toStdString());
  LOG_PERF(0, "Exit");
  return type;
}

bool setWorkflowType(git_repository *repo, WorkflowType type) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(repo != nullptr);

  if (!repo) {
    LOG_ERROR(Log::ErrorId::Git, "Repository pointer is null");
    LOG_PERF(0, "Exit");
    return false;
  }

  git_config *config = nullptr;
  int error = git_repository_config(&config, repo);
  if (error) {
    setError(GitError::SetWorkflowTypeFailedToGetRepositoryConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  error =
      git_config_set_string(config, "sourcerer.workflow",
                            workflowTypeToString(type).toUtf8().constData());

  // Choosing a workflow from the menu ends the use of a custom GitFlow-Next
  // workflow; applyGitFlowNextWorkflow() sets it again.
  git_config *localConfig = nullptr;
  if (!error &&
      !git_config_open_level(&localConfig, config, GIT_CONFIG_LEVEL_LOCAL)) {
    clearGitFlowNextWorkflowReference(localConfig);
    git_config_free(localConfig);
  }
  git_config_free(config);

  if (error) {
    setError(GitError::SetWorkflowTypeFailedToWriteConfig, error);
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit");
  return true;
}

} // namespace GitRepo
