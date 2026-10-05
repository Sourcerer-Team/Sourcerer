#include <GitLabFlowUtils.h>

#include <QtTest>

using namespace GitRepo;

class GitLabFlowTest final : public QObject {
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

QTEST_APPLESS_MAIN(GitLabFlowTest)

#include "tst_gitlab_flow.moc"
