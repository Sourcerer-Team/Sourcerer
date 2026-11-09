// Branching workflows: classic Git Flow, GitLab Flow, GitHub Flow and
// git-flow-next, on named branches or bookmarks.

#include "HgRc.h"
#include "HgRepo.h"
#include "HgRepoPrivate.h"

#include <ConfigManager.h>
#include <GitFlowNextWorkflow.h>
#include <Log.h>

#include <QRegularExpression>

#define THREAD_ID "HgRepo"

namespace HgRepo {

using namespace Private;

namespace {

const QString kGitFlowSection = QStringLiteral("gitflow");
const QString kWorkflowKey = QStringLiteral("workflow");
const QString kBranchKindKey = QStringLiteral("flow-branch-kind");
const QString kGitFlowNextSection = QStringLiteral("sourcerer-gitflownext");

HgRc repositoryRc(const Repository *repo) {
  return HgRc(hgFile(repo, QStringLiteral("hgrc")));
}

bool loadRc(HgRc *rc) {
  if (!rc->load()) {
    setLastError(HgError::WorkflowFailed, -1, rc->errorMessage());
    return false;
  }
  return true;
}

bool saveRc(HgRc *rc) {
  if (!rc->save()) {
    setLastError(HgError::WorkflowFailed, -1, rc->errorMessage());
    return false;
  }
  return true;
}

bool workflowError(HgError error, const QString &message) {
  setLastError(error, -1, message);
  return false;
}

// Branch list name ("name" or "@name") of the bookmark or open named branch
// called name, empty when there is none. Bookmarks win, like in "hg update".
QString flowListName(Repository *repo, const QString &name) {
  const QString bare = name.trimmed();
  if (bare.isEmpty()) {
    return QString();
  }

  QString named;
  for (const HgBranchInfo &info :
       listLocalBranchesWithKind(repo, false, false)) {
    if (info.name != bare) {
      continue;
    }
    if (info.kind == HgBranchKind::Bookmark) {
      return '@' + bare;
    }
    if (!info.isNew) {
      named = bare;
    }
  }
  return named;
}

// A main branch that doesn't exist is Mercurial's "default".
QString mainListName(Repository *repo, const QString &name) {
  const QString listName = flowListName(repo, name);
  if (listName.isEmpty() && (name.trimmed() == QStringLiteral("main") ||
                             name.trimmed() == QStringLiteral("master"))) {
    return flowListName(repo, QStringLiteral("default"));
  }
  return listName;
}

QString withPrefix(const QString &name, const QString &prefix) {
  const QString trimmed = name.trimmed();
  return trimmed.startsWith(prefix) ? trimmed : prefix + trimmed;
}

bool updateTo(Repository *repo, const QString &revision) {
  return runSimple(
      repo, HgError::WorkflowFailed,
      {QStringLiteral("update"), QStringLiteral("--rev"), revision});
}

// Creates the workflow branch name at the tip of baseListName, a bookmark or
// a named branch as set for the repository. A named branch exists with its
// first commit, so it gets one that only opens the branch.
bool createFlowBranch(Repository *repo, const QString &baseListName,
                      const QString &name, bool checkout) {
  const QString base = branchRevset(baseListName, GitRepo::BranchType::Local);

  if (getFlowBranchKind(repo) == HgBranchKind::Bookmark) {
    return createBranchFromCommit(repo, base, name) &&
           (!checkout ||
            checkoutBranch(repo, '@' + name, GitRepo::BranchType::Local));
  }

  const QString previousBookmark = activeBookmark(repo);
  const QString previousNode = workingDirectoryNode(repo);

  QStringList commitArgs{
      QStringLiteral("commit"), QStringLiteral("--message"),
      QStringLiteral("Start branch %1").arg(name),
      // Only the branch name changes; the working directory stays as it is.
      QStringLiteral("--exclude"), QStringLiteral("glob:**")};
  commitArgs << userArgs(repo, QString(), QString());

  const bool created = updateTo(repo, base) &&
                       runSimple(repo, HgError::WorkflowFailed,
                                 {QStringLiteral("branch"), name}) &&
                       runSimple(repo, HgError::WorkflowFailed, commitArgs);

  if (!checkout || !created) {
    const HgErrorContext error = getLastError();
    updateTo(repo,
             previousBookmark.isEmpty() ? previousNode : previousBookmark);
    if (!created) {
      setLastError(error.error, error.errorCode, error.errorMessage);
    }
  }
  return created;
}

// Deletes a bookmark, or closes a named branch.
bool removeFlowBranch(Repository *repo, const QString &listName,
                      const QString &author, const QString &email) {
  if (listName.startsWith('@')) {
    return deleteBranch(repo, listName);
  }
  return closeNamedBranch(repo, listName, author, email);
}

QString developmentBranch(const QString &developmentBranchName) {
  const QString name = developmentBranchName.trimmed();
  if (!name.isEmpty()) {
    return name;
  }
  const QString configured =
      ConfigManager::ConfigManager::getInstance()
          .getString(
              ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName)
          .trimmed();
  return configured.isEmpty() ? QStringLiteral("develop") : configured;
}

QString mainBranch() {
  const QString configured =
      ConfigManager::ConfigManager::getInstance()
          .getString(ConfigManager::ConfigKey::GitRepoGitFlowMainBranchName)
          .trimmed();
  return configured.isEmpty() ? QStringLiteral("main") : configured;
}

QString gitFlowPrefix(GitRepo::GitFlowBranchType type) {
  switch (type) {
  case GitRepo::GitFlowBranchType::Feature:
    return QStringLiteral("feature/");
  case GitRepo::GitFlowBranchType::Release:
    return QStringLiteral("release/");
  case GitRepo::GitFlowBranchType::Hotfix:
    return QStringLiteral("hotfix/");
  default:
    return QString();
  }
}

// First existing branch of candidates, else the current branch.
QString firstExisting(Repository *repo, const QStringList &candidates) {
  for (const QString &candidate : candidates) {
    const QString listName = mainListName(repo, candidate);
    if (!listName.isEmpty()) {
      return listName;
    }
  }
  return getActiveBranch(repo, GitRepo::BranchType::Local, false);
}

// [gitflow] entries with their "gitflow." prefix, as parseGitFlowNextConfig
// expects them.
QList<QPair<QString, QString>> gitFlowEntries(const HgRc &rc) {
  QList<QPair<QString, QString>> entries;
  for (const auto &[key, value] : rc.entries(kGitFlowSection)) {
    entries.append({QStringLiteral("gitflow.") + key, value});
  }
  return entries;
}

void writeGitFlowEntries(HgRc *rc,
                         const QList<QPair<QString, QString>> &entries) {
  for (const auto &[key, value] : entries) {
    const QString name =
        key.startsWith(QStringLiteral("gitflow.")) ? key.mid(8) : key;
    rc->setValue(kGitFlowSection, name, value);
  }
}

bool loadGitFlowNext(Repository *repo, GitRepo::GitFlowNextConfig *config) {
  if (!readGitFlowNextConfig(repo, config)) {
    return false;
  }
  if (!config->initialized) {
    return workflowError(HgError::WorkflowNotInitialized,
                         QStringLiteral("git-flow-next is not initialized in "
                                        "this repository."));
  }
  return true;
}

const GitRepo::GitFlowNextBranch *
findBase(const GitRepo::GitFlowNextConfig &config, const QString &name) {
  for (const GitRepo::GitFlowNextBranch &branch : config.branches) {
    if (!branch.topic && branch.name == name) {
      return &branch;
    }
  }
  return nullptr;
}

// Brings target up to date with source using strategy ("merge" or
// "rebase"), ending on target.
bool integrateDownstream(Repository *repo, const QString &source,
                         const QString &target, const QString &strategy,
                         const QString &author, const QString &email) {
  if (strategy == QStringLiteral("rebase")) {
    return rebaseBranches(repo, target, source, author, email, false);
  }
  if (strategy == QStringLiteral("merge")) {
    return merge(repo, source, target, author, email);
  }
  return workflowError(
      HgError::WorkflowFailed,
      QStringLiteral("Unsupported strategy %1.").arg(strategy));
}

} // namespace

HgBranchKind getFlowBranchKind(Repository *repo) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !rc.load()) {
    return HgBranchKind::NamedBranch;
  }
  return rc.value(QString::fromLatin1(kSourcererSection), kBranchKindKey) ==
                 QStringLiteral("bookmark")
             ? HgBranchKind::Bookmark
             : HgBranchKind::NamedBranch;
}

bool setFlowBranchKind(Repository *repo, HgBranchKind kind) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !loadRc(&rc)) {
    return false;
  }
  rc.setValue(QString::fromLatin1(kSourcererSection), kBranchKindKey,
              kind == HgBranchKind::Bookmark ? QStringLiteral("bookmark")
                                             : QStringLiteral("named"));
  return saveRc(&rc);
}

bool startGitFlowBranch(Repository *repo, GitRepo::GitFlowBranchType branchType,
                        const QString &branchName,
                        const QString &developmentBranchName) {
  if (branchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("A branch name is required."));
  }

  const QString name = withPrefix(branchName, gitFlowPrefix(branchType));
  const QString develop = developmentBranch(developmentBranchName);
  const QString main = mainBranch();

  const QString base =
      branchType == GitRepo::GitFlowBranchType::Hotfix
          ? firstExisting(repo, {main, QStringLiteral("default"), develop})
          : firstExisting(repo, {develop, main, QStringLiteral("default")});
  if (base.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("No branch to start %1 from.").arg(name));
  }

  return createFlowBranch(repo, base, name, true);
}

bool finishGitFlowBranch(Repository *repo,
                         GitRepo::GitFlowBranchType branchType,
                         const QString &branchName, const QString &author,
                         const QString &email,
                         const QString &developmentBranchName) {
  if (branchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("A branch name is required."));
  }

  const QString branch =
      flowListName(repo, withPrefix(branchName, gitFlowPrefix(branchType)));
  if (branch.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Branch %1 does not exist.").arg(branchName));
  }

  const QString develop = developmentBranch(developmentBranchName);
  const QString main = mainBranch();
  // The same fallbacks as when the branch was started.
  const QString developList =
      firstExisting(repo, {develop, main, QStringLiteral("default")});
  const QString primary =
      firstExisting(repo, {main, QStringLiteral("default"), develop});

  if (branchType == GitRepo::GitFlowBranchType::Feature) {
    if (!merge(repo, branch, developList, author, email)) {
      return false;
    }
  } else {
    if (!merge(repo, branch, primary, author, email)) {
      return false;
    }
    if (developList != primary &&
        !merge(repo, branch, developList, author, email)) {
      return false;
    }
  }

  return removeFlowBranch(repo, branch, author, email);
}

bool startGitLabFlowFeature(Repository *repo, const QString &branchName,
                            const QString &mainBranchName) {
  if (branchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("A branch name is required."));
  }
  const QString main = mainListName(repo, mainBranchName);
  if (main.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Branch %1 does not exist.").arg(mainBranchName));
  }
  return createFlowBranch(
      repo, main, withPrefix(branchName, QStringLiteral("feature/")), true);
}

bool finishGitLabFlowFeature(Repository *repo, const QString &branchName,
                             const QString &mainBranchName,
                             const QString &author, const QString &email) {
  if (branchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("A branch name is required."));
  }
  const QString main = mainListName(repo, mainBranchName);
  const QString feature =
      flowListName(repo, withPrefix(branchName, QStringLiteral("feature/")));
  if (main.isEmpty() || feature.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Branch %1 or %2 does not exist.")
                             .arg(branchName, mainBranchName));
  }
  return merge(repo, feature, main, author, email) &&
         removeFlowBranch(repo, feature, author, email);
}

bool promoteGitLabFlowEnvironment(Repository *repo,
                                  const QStringList &environmentChain,
                                  const QString &sourceBranchName,
                                  const QString &author, const QString &email) {
  const QString next = GitRepo::gitLabFlowNextEnvironment(
      environmentChain, sourceBranchName.trimmed());
  if (next.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("%1 has no downstream environment.")
                             .arg(sourceBranchName));
  }

  const QString source = mainListName(repo, sourceBranchName);
  if (source.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Branch %1 does not exist.").arg(sourceBranchName));
  }

  const QString destination = flowListName(repo, next);
  if (destination.isEmpty()) {
    return createFlowBranch(repo, source, next, true);
  }
  return merge(repo, source, destination, author, email);
}

bool startGitLabFlowRelease(Repository *repo, const QString &releaseBranchName,
                            const QString &mainBranchName) {
  if (releaseBranchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("A branch name is required."));
  }
  const QString main = mainListName(repo, mainBranchName);
  if (main.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Branch %1 does not exist.").arg(mainBranchName));
  }
  return createFlowBranch(repo, main, releaseBranchName.trimmed(), true);
}

bool cherryPickToGitLabFlowRelease(Repository *repo, const QString &commit,
                                   const QString &releaseBranchName,
                                   const QString &author, const QString &email,
                                   bool *conflicted) {
  if (conflicted) {
    *conflicted = false;
  }

  const QString release = flowListName(repo, releaseBranchName);
  if (release.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Release branch %1 does not exist.")
                             .arg(releaseBranchName));
  }

  const QString original =
      getActiveBranch(repo, GitRepo::BranchType::Local, false);
  if (original != release &&
      !checkoutBranch(repo, release, GitRepo::BranchType::Local)) {
    return false;
  }

  const bool picked = cherryPickCommit(repo, commit, author, email);
  if (picked && getIsMergeInProgress(repo)) {
    // The release branch stays checked out with the conflicts.
    if (conflicted) {
      *conflicted = true;
    }
    return true;
  }

  // Back to where it started, also when the graft failed.
  const HgErrorContext error = getLastError();
  if (original != release && !original.isEmpty()) {
    checkoutBranch(repo, original, GitRepo::BranchType::Local);
  }
  if (!picked) {
    setLastError(error.error, error.errorCode, error.errorMessage);
  }
  return picked;
}

bool startGitHubFlowBranch(Repository *repo, const QString &branchName,
                           const QString &mainBranchName) {
  if (branchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("A branch name is required."));
  }
  const QString main = mainListName(repo, mainBranchName);
  if (main.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Branch %1 does not exist.").arg(mainBranchName));
  }
  return createFlowBranch(repo, main, branchName.trimmed(), true);
}

bool updateGitHubFlowBranch(Repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email) {
  const QString main = mainListName(repo, mainBranchName);
  const QString branch = flowListName(repo, branchName);
  if (branch.isEmpty() || main.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Branch %1 or %2 does not exist.")
                             .arg(branchName, mainBranchName));
  }
  if (branch == main) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("The main branch can't be updated "
                                        "from itself."));
  }
  return merge(repo, main, branch, author, email);
}

bool finishGitHubFlowBranch(Repository *repo, const QString &branchName,
                            const QString &mainBranchName,
                            const QString &author, const QString &email) {
  const QString main = mainListName(repo, mainBranchName);
  const QString branch = flowListName(repo, branchName);
  if (branch.isEmpty() || main.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Branch %1 or %2 does not exist.")
                             .arg(branchName, mainBranchName));
  }
  if (branch == main) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("The main branch can't be finished."));
  }
  return merge(repo, branch, main, author, email) &&
         removeFlowBranch(repo, branch, author, email);
}

bool readGitFlowNextConfig(Repository *repo,
                           GitRepo::GitFlowNextConfig *config) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !loadRc(&rc)) {
    return false;
  }
  *config = GitRepo::parseGitFlowNextConfig(gitFlowEntries(rc));
  return true;
}

bool initGitFlowNext(Repository *repo, const QString &mainBranchName,
                     const QString &developBranchName) {
  if (mainBranchName.trimmed().isEmpty() ||
      developBranchName.trimmed().isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("The main and develop branch names are "
                                        "required."));
  }

  const QString main = flowListName(repo, mainBranchName);
  if (main.isEmpty()) {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Branch %1 does not exist.").arg(mainBranchName));
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(&rc)) {
    return false;
  }
  writeGitFlowEntries(&rc, GitRepo::gitFlowNextClassicPreset(
                               mainBranchName, developBranchName));
  if (!saveRc(&rc)) {
    return false;
  }

  if (flowListName(repo, developBranchName).isEmpty()) {
    return createFlowBranch(repo, main, developBranchName.trimmed(), true);
  }
  return true;
}

bool startGitFlowNextBranch(Repository *repo, const QString &branchType,
                            const QString &branchName) {
  GitRepo::GitFlowNextConfig config;
  if (!loadGitFlowNext(repo, &config)) {
    return false;
  }

  for (const GitRepo::GitFlowNextBranch &type : config.topicTypes()) {
    if (type.name != branchType.trimmed()) {
      continue;
    }

    const QString name =
        GitRepo::gitFlowNextBranchNameForInput(type, branchName);
    if (name.isEmpty() || name == type.prefix) {
      return workflowError(HgError::InvalidArgument,
                           QStringLiteral("A branch name is required."));
    }

    const QString startPoint =
        flowListName(repo, GitRepo::gitFlowNextStartPoint(type));
    if (startPoint.isEmpty()) {
      return workflowError(HgError::WorkflowFailed,
                           QStringLiteral("Branch %1 does not exist.")
                               .arg(GitRepo::gitFlowNextStartPoint(type)));
    }
    return createFlowBranch(repo, startPoint, name, true);
  }

  return workflowError(
      HgError::WorkflowFailed,
      QStringLiteral("Unknown branch type %1.").arg(branchType));
}

bool finishGitFlowNextBranch(Repository *repo, const QString &branchName,
                             const QString &author, const QString &email) {
  GitRepo::GitFlowNextConfig config;
  if (!loadGitFlowNext(repo, &config)) {
    return false;
  }

  const QString bare =
      branchName.startsWith('@') ? branchName.mid(1) : branchName.trimmed();
  const GitRepo::GitFlowNextBranch *type =
      GitRepo::gitFlowNextTopicTypeForBranch(config, bare);
  if (!type) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("%1 is not a topic branch.").arg(bare));
  }

  const QString branch = flowListName(repo, bare);
  const QString parent = flowListName(repo, type->parent);
  if (branch.isEmpty() || parent.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Branch %1 or %2 does not exist.")
                             .arg(bare, type->parent));
  }

  const QString tagName =
      type->tag ? GitRepo::gitFlowNextTagName(*type, bare) : QString();
  if (!tagName.isEmpty() && listTags(repo).contains(tagName)) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Tag %1 already exists.").arg(tagName));
  }

  bool integrated = false;
  if (type->upstreamStrategy == QStringLiteral("merge")) {
    integrated = merge(repo, branch, parent, author, email);
  } else if (type->upstreamStrategy == QStringLiteral("rebase")) {
    integrated = rebaseBranches(repo, branch, parent, author, email, false) &&
                 merge(repo, branch, parent, author, email);
  } else if (type->upstreamStrategy == QStringLiteral("squash")) {
    integrated = squashBranches(repo, branch, parent, author, email, false);
  } else {
    return workflowError(
        HgError::WorkflowFailed,
        QStringLiteral("Unsupported strategy %1.").arg(type->upstreamStrategy));
  }
  if (!integrated) {
    return false;
  }

  if (!tagName.isEmpty()) {
    QString parentTip;
    if (!resolveNode(repo, HgError::WorkflowFailed,
                     branchRevset(parent, GitRepo::BranchType::Local),
                     &parentTip) ||
        !createTagFromCommit(repo, parentTip, tagName, author, email,
                             tagName)) {
      return false;
    }
  }

  for (const QString &baseName :
       GitRepo::gitFlowNextAutoUpdateBranches(config, type->parent)) {
    const GitRepo::GitFlowNextBranch *base = findBase(config, baseName);
    const QString baseList = flowListName(repo, baseName);
    if (!base || baseList.isEmpty()) {
      continue;
    }
    if (!integrateDownstream(repo, parent, baseList, base->downstreamStrategy,
                             author, email)) {
      return false;
    }
  }

  if (!checkoutBranch(repo, parent, GitRepo::BranchType::Local)) {
    return false;
  }
  return type->keepOnFinish || removeFlowBranch(repo, branch, author, email);
}

bool updateGitFlowNextBranch(Repository *repo, const QString &branchName,
                             const QString &author, const QString &email) {
  GitRepo::GitFlowNextConfig config;
  if (!loadGitFlowNext(repo, &config)) {
    return false;
  }

  const QString bare =
      branchName.startsWith('@') ? branchName.mid(1) : branchName.trimmed();
  const GitRepo::GitFlowNextBranch *type =
      GitRepo::gitFlowNextTopicTypeForBranch(config, bare);
  if (!type) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("%1 is not a topic branch.").arg(bare));
  }

  const QString branch = flowListName(repo, bare);
  const QString parent = flowListName(repo, type->parent);
  if (branch.isEmpty() || parent.isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Branch %1 or %2 does not exist.")
                             .arg(bare, type->parent));
  }
  return integrateDownstream(repo, parent, branch, type->downstreamStrategy,
                             author, email);
}

GitRepo::WorkflowType getWorkflowType(Repository *repo) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !rc.load()) {
    return GitRepo::WorkflowType::GitFlow;
  }
  return GitRepo::workflowTypeFromString(
      rc.value(QString::fromLatin1(kSourcererSection), kWorkflowKey));
}

bool setWorkflowType(Repository *repo, GitRepo::WorkflowType type) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !loadRc(&rc)) {
    return false;
  }
  rc.setValue(QString::fromLatin1(kSourcererSection), kWorkflowKey,
              GitRepo::workflowTypeToString(type));
  // Like GitRepo, choosing a workflow forgets the applied custom one.
  rc.removeSection(kGitFlowNextSection);
  return saveRc(&rc);
}

bool applyGitFlowNextWorkflow(Repository *repo,
                              const GitRepo::GitFlowNextWorkflow &workflow) {
  const QStringList problems = GitRepo::validateGitFlowNextWorkflow(workflow);
  if (!problems.isEmpty()) {
    return workflowError(HgError::WorkflowFailed, problems.join('\n'));
  }
  if (workflow.id.isEmpty()) {
    return workflowError(HgError::InvalidArgument,
                         QStringLiteral("The workflow has no id."));
  }

  QString root;
  for (const GitRepo::GitFlowNextBranch &branch : workflow.baseBranches()) {
    if (branch.parent.isEmpty()) {
      root = branch.name;
    }
  }
  if (flowListName(repo, root).isEmpty()) {
    return workflowError(HgError::WorkflowFailed,
                         QStringLiteral("Branch %1 does not exist.").arg(root));
  }

  HgRc rc = repositoryRc(repo);
  if (!loadRc(&rc)) {
    return false;
  }

  // The branch model is replaced; gitflow.version and initialized stay.
  static const QRegularExpression replaced(
      QStringLiteral("^(branch\\..+|[^.]+\\.finish\\..+)$"));
  for (const auto &[key, value] : rc.entries(kGitFlowSection)) {
    Q_UNUSED(value);
    if (replaced.match(key).hasMatch()) {
      rc.remove(kGitFlowSection, key);
    }
  }
  writeGitFlowEntries(&rc, GitRepo::gitFlowNextConfigEntries(workflow));
  rc.setValue(
      QString::fromLatin1(kSourcererSection), kWorkflowKey,
      GitRepo::workflowTypeToString(GitRepo::WorkflowType::GitFlowNext));
  rc.setValue(kGitFlowNextSection, QStringLiteral("workflow"), workflow.id);
  rc.setValue(kGitFlowNextSection, QStringLiteral("revision"),
              QString::number(workflow.revision));
  if (!saveRc(&rc)) {
    return false;
  }

  QStringList localBranches;
  for (const HgBranchInfo &info :
       listLocalBranchesWithKind(repo, false, false)) {
    localBranches << info.name;
  }

  for (const QString &name :
       GitRepo::gitFlowNextBaseBranchesToCreate(workflow, localBranches)) {
    for (const GitRepo::GitFlowNextBranch &branch : workflow.baseBranches()) {
      if (branch.name != name) {
        continue;
      }
      const QString parent = flowListName(repo, branch.parent);
      if (parent.isEmpty() || !createFlowBranch(repo, parent, name, false)) {
        return false;
      }
    }
  }
  return true;
}

bool getGitFlowNextWorkflowReference(Repository *repo, QString *id,
                                     int *revision) {
  HgRc rc = repositoryRc(repo);
  if (!repo || !rc.load()) {
    return false;
  }
  const QString workflow =
      rc.value(kGitFlowNextSection, QStringLiteral("workflow"));
  if (workflow.isEmpty()) {
    return false;
  }
  if (id) {
    *id = workflow;
  }
  if (revision) {
    *revision =
        rc.value(kGitFlowNextSection, QStringLiteral("revision")).toInt();
  }
  return true;
}

} // namespace HgRepo
