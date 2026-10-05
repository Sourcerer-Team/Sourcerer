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
};

QTEST_APPLESS_MAIN(WorkflowsTest)

#include "tst_workflows.moc"
