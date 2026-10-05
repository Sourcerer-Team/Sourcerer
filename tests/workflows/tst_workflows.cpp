#include <WorkflowUtils.h>

#include <QtTest>

using namespace GitRepo;

class WorkflowsTest final : public QObject {
  Q_OBJECT

private slots:
  void workflowTypeRoundTrip() {
    QCOMPARE(workflowTypeToString(WorkflowType::GitFlow),
             QStringLiteral("gitflow"));
    QCOMPARE(workflowTypeToString(WorkflowType::GitLabFlow),
             QStringLiteral("gitlabflow"));
    QCOMPARE(workflowTypeFromString(QStringLiteral("gitflow")),
             WorkflowType::GitFlow);
    QCOMPARE(workflowTypeFromString(QStringLiteral("gitlabflow")),
             WorkflowType::GitLabFlow);
    QCOMPARE(workflowTypeFromString(QStringLiteral(" GitLabFlow ")),
             WorkflowType::GitLabFlow);
    QCOMPARE(workflowTypeToString(WorkflowType::GitHubFlow),
             QStringLiteral("githubflow"));
    QCOMPARE(workflowTypeFromString(QStringLiteral("githubflow")),
             WorkflowType::GitHubFlow);
  }

  void gitHubPullRequestUrl_data() {
    QTest::addColumn<QString>("remoteUrl");
    QTest::addColumn<QString>("expected");

    const QString expected = QStringLiteral(
        "https://github.com/owner/repo/compare/main...feature/login?expand=1");
    QTest::newRow("https") << QStringLiteral("https://github.com/owner/repo")
                           << expected;
    QTest::newRow("https .git")
        << QStringLiteral("https://github.com/owner/repo.git") << expected;
    QTest::newRow("scp") << QStringLiteral("git@github.com:owner/repo.git")
                         << expected;
    QTest::newRow("ssh") << QStringLiteral(
                                "ssh://git@github.com/owner/repo.git")
                         << expected;
    QTest::newRow("gitlab")
        << QStringLiteral("https://gitlab.com/owner/repo.git") << QString();
    QTest::newRow("no repo")
        << QStringLiteral("https://github.com/owner") << QString();
  }

  void gitHubPullRequestUrl() {
    QFETCH(QString, remoteUrl);
    QFETCH(QString, expected);
    QCOMPARE(GitRepo::gitHubPullRequestUrl(remoteUrl, QStringLiteral("main"),
                                           QStringLiteral("feature/login")),
             expected);
  }

  void gitHubRepositoryName() {
    QCOMPARE(GitRepo::gitHubRepositoryName(
                 QStringLiteral("git@github.com:owner/repo.git")),
             QStringLiteral("owner/repo"));
    QCOMPARE(GitRepo::gitHubRepositoryName(
                 QStringLiteral("https://github.com/owner/repo/")),
             QStringLiteral("owner/repo"));
    QVERIFY(GitRepo::gitHubRepositoryName(
                QStringLiteral("https://example.com/owner/repo.git"))
                .isEmpty());
  }

  void gitHubPullRequestUrlEncodesBranches() {
    QCOMPARE(GitRepo::gitHubPullRequestUrl(
                 QStringLiteral("https://github.com/owner/repo"),
                 QStringLiteral("main"), QStringLiteral("fix #12")),
             QStringLiteral("https://github.com/owner/repo/compare/"
                            "main...fix%20%2312?expand=1"));
  }

  void workflowTypeDefaultsToGitFlow() {
    QCOMPARE(workflowTypeFromString(QString()), WorkflowType::GitFlow);
    QCOMPARE(workflowTypeFromString(QStringLiteral("trunk")),
             WorkflowType::GitFlow);
  }

  void environmentChain() {
    QCOMPARE(
        gitLabFlowEnvironmentChain(QStringLiteral("main"),
                                   QStringLiteral("pre-production,"
                                                  "production")),
        QStringList({QStringLiteral("main"), QStringLiteral("pre-production"),
                     QStringLiteral("production")}));
  }

  void environmentChainIsNormalized() {
    QCOMPARE(gitLabFlowEnvironmentChain(
                 QStringLiteral(" main "),
                 QStringLiteral(" staging , ,main,production,staging,")),
             QStringList({QStringLiteral("main"), QStringLiteral("staging"),
                          QStringLiteral("production")}));
    QCOMPARE(gitLabFlowEnvironmentChain(QStringLiteral("main"), QString()),
             QStringList({QStringLiteral("main")}));
  }

  void nextEnvironment() {
    const QStringList chain = {QStringLiteral("main"),
                               QStringLiteral("pre-production"),
                               QStringLiteral("production")};
    QCOMPARE(gitLabFlowNextEnvironment(chain, QStringLiteral("main")),
             QStringLiteral("pre-production"));
    QCOMPARE(gitLabFlowNextEnvironment(chain, QStringLiteral("pre-production")),
             QStringLiteral("production"));
    QVERIFY(gitLabFlowNextEnvironment(chain, QStringLiteral("production"))
                .isEmpty());
    QVERIFY(gitLabFlowNextEnvironment(chain, QStringLiteral("feature/x"))
                .isEmpty());
  }

  void releaseBranchName() {
    const QString suffix = QStringLiteral("-stable");
    QCOMPARE(gitLabFlowReleaseBranchName(QStringLiteral("1.2"), suffix),
             QStringLiteral("1-2-stable"));
    QCOMPARE(gitLabFlowReleaseBranchName(QStringLiteral(" 2.10.1 "), suffix),
             QStringLiteral("2-10-1-stable"));
    QCOMPARE(gitLabFlowReleaseBranchName(QStringLiteral("1-2-stable"), suffix),
             QStringLiteral("1-2-stable"));
    QVERIFY(
        gitLabFlowReleaseBranchName(QStringLiteral("  "), suffix).isEmpty());
  }

  void releaseBranchDetection() {
    const QString suffix = QStringLiteral("-stable");
    QVERIFY(isGitLabFlowReleaseBranch(QStringLiteral("1-2-stable"), suffix));
    QVERIFY(!isGitLabFlowReleaseBranch(QStringLiteral("-stable"), suffix));
    QVERIFY(!isGitLabFlowReleaseBranch(QStringLiteral("main"), suffix));
    QVERIFY(
        !isGitLabFlowReleaseBranch(QStringLiteral("1-2-stable"), QString()));
  }
  void gitFlowNextWorkflowType() {
    QCOMPARE(workflowTypeToString(WorkflowType::GitFlowNext),
             QStringLiteral("gitflownext"));
    QCOMPARE(workflowTypeFromString(QStringLiteral("gitflownext")),
             WorkflowType::GitFlowNext);
  }

  void gitFlowNextClassicPresetRoundTrip() {
    const GitFlowNextConfig config =
        parseGitFlowNextConfig(gitFlowNextClassicPreset(
            QStringLiteral("main"), QStringLiteral("develop")));
    QVERIFY(config.initialized);
    QCOMPARE(config.branches.size(), 5);

    const QVector<GitFlowNextBranch> types = config.topicTypes();
    QCOMPARE(types.size(), 3);
    QCOMPARE(types.at(0).name, QStringLiteral("feature"));
    QCOMPARE(types.at(0).parent, QStringLiteral("develop"));
    QCOMPARE(types.at(0).prefix, QStringLiteral("feature/"));
    QCOMPARE(types.at(0).downstreamStrategy, QStringLiteral("rebase"));
    QVERIFY(!types.at(0).tag);
    QCOMPARE(types.at(1).name, QStringLiteral("release"));
    QCOMPARE(types.at(1).parent, QStringLiteral("main"));
    QCOMPARE(gitFlowNextStartPoint(types.at(1)), QStringLiteral("develop"));
    QVERIFY(types.at(1).tag);
    QCOMPARE(gitFlowNextStartPoint(types.at(2)), QStringLiteral("main"));

    QCOMPARE(gitFlowNextAutoUpdateBranches(config, QStringLiteral("main")),
             QStringList{QStringLiteral("develop")});
    QVERIFY(gitFlowNextAutoUpdateBranches(config, QStringLiteral("develop"))
                .isEmpty());
  }

  // libgit2 reports section and variable names in lower case.
  void gitFlowNextParsesNormalizedKeys() {
    const GitFlowNextConfig config = parseGitFlowNextConfig({
        {QStringLiteral("gitflow.branch.trunk.type"), QStringLiteral("base")},
        {QStringLiteral("gitflow.branch.task.type"), QStringLiteral("topic")},
        {QStringLiteral("gitflow.branch.task.parent"), QStringLiteral("trunk")},
        {QStringLiteral("gitflow.branch.task.upstreamstrategy"),
         QStringLiteral("Squash")},
        {QStringLiteral("gitflow.branch.task.tag"), QStringLiteral("yes")},
        {QStringLiteral("gitflow.branch.task.tagprefix"), QStringLiteral("v")},
        {QStringLiteral("gitflow.task.finish.keep"), QStringLiteral("true")},
        {QStringLiteral("gitflow.branch.orphan.parent"),
         QStringLiteral("trunk")},
    });

    QVERIFY(config.initialized);
    QCOMPARE(config.branches.size(), 2); // orphan has no type
    const GitFlowNextBranch task = config.topicTypes().first();
    QCOMPARE(task.prefix, QStringLiteral("task/"));
    QCOMPARE(task.upstreamStrategy, QStringLiteral("squash"));
    QCOMPARE(task.downstreamStrategy, QStringLiteral("merge"));
    QVERIFY(task.tag);
    QVERIFY(task.keepOnFinish);
    QCOMPARE(gitFlowNextStartPoint(task), QStringLiteral("trunk"));
    QCOMPARE(gitFlowNextTagName(task, QStringLiteral("task/1.2")),
             QStringLiteral("v1.2"));
  }

  void gitFlowNextNoTagOverridesTag() {
    const GitFlowNextConfig config = parseGitFlowNextConfig({
        {QStringLiteral("gitflow.branch.release.type"),
         QStringLiteral("topic")},
        {QStringLiteral("gitflow.branch.release.tag"), QStringLiteral("true")},
        {QStringLiteral("gitflow.release.finish.notag"),
         QStringLiteral("true")},
    });
    QVERIFY(!config.topicTypes().first().tag);
  }

  void gitFlowNextUninitialized() {
    const GitFlowNextConfig config = parseGitFlowNextConfig(
        {{QStringLiteral("gitflow.version"), QStringLiteral("1.0")}});
    QVERIFY(!config.initialized);
    QVERIFY(config.branches.isEmpty());
  }

  void gitFlowNextTopicTypeMatching() {
    const GitFlowNextConfig config = parseGitFlowNextConfig({
        {QStringLiteral("gitflow.branch.main.type"), QStringLiteral("base")},
        {QStringLiteral("gitflow.branch.feature.type"),
         QStringLiteral("topic")},
        {QStringLiteral("gitflow.branch.ui.type"), QStringLiteral("topic")},
        {QStringLiteral("gitflow.branch.ui.prefix"),
         QStringLiteral("feature/ui/")},
    });

    const auto typeOf = [&](const char *branch) {
      const GitFlowNextBranch *type =
          gitFlowNextTopicTypeForBranch(config, QString::fromLatin1(branch));
      return type ? type->name : QString();
    };
    QCOMPARE(typeOf("feature/login"), QStringLiteral("feature"));
    QCOMPARE(typeOf("feature/ui/dark-mode"), QStringLiteral("ui"));
    QCOMPARE(typeOf("feature/"), QString());
    QCOMPARE(typeOf("main"), QString());
    QCOMPARE(typeOf("bugfix/x"), QString());
  }

  void gitFlowNextBranchNameFromInput() {
    GitFlowNextBranch type;
    type.prefix = QStringLiteral("feature/");
    QCOMPARE(gitFlowNextBranchNameForInput(type, QStringLiteral(" login ")),
             QStringLiteral("feature/login"));
    QCOMPARE(
        gitFlowNextBranchNameForInput(type, QStringLiteral("feature/login")),
        QStringLiteral("feature/login"));
    QVERIFY(gitFlowNextBranchNameForInput(type, QStringLiteral(" ")).isEmpty());
  }
};

QTEST_APPLESS_MAIN(WorkflowsTest)

#include "tst_workflows.moc"
