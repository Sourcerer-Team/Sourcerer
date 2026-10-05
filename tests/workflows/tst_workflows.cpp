#include <GitFlowNextWorkflow.h>
#include <GraphiteUtils.h>
#include <WorkflowUtils.h>

#include <QtTest>

using namespace GitRepo;

namespace {

// JSON written with single quotes, which keeps the test readable.
QByteArray json(const char *text) {
  return QByteArray(text).replace('\'', '"');
}

// gt 1.8.6: main <- base <- {left <- left-top, right}, base moved on.
QByteArray graphiteStateJson() {
  return json("{"
              "'main': {'trunk': true},"
              "'base': {'trunk': false, 'needs_restack': false,"
              "         'parents': [{'ref': 'main', 'sha': '92aade5'}]},"
              "'left': {'trunk': false, 'needs_restack': true,"
              "         'parents': [{'ref': 'base', 'sha': '92aade5'}]},"
              "'left-top': {'trunk': false, 'needs_restack': false,"
              "             'parents': [{'ref': 'left', 'sha': '1111111'}]},"
              "'right': {'trunk': false, 'needs_restack': true,"
              "          'parents': [{'ref': 'base', 'sha': '92aade5'}]}"
              "}");
}

GitFlowNextWorkflow releaseTrain() {
  GitFlowNextWorkflow workflow;
  workflow.id = QStringLiteral("0b1c");
  workflow.name = QStringLiteral("Release train");
  workflow.revision = 3;
  GitFlowNextBranch main;
  main.name = QStringLiteral("main");
  GitFlowNextBranch develop;
  develop.name = QStringLiteral("develop");
  develop.parent = QStringLiteral("main");
  develop.autoUpdate = true;
  GitFlowNextBranch staging;
  staging.name = QStringLiteral("staging");
  staging.parent = QStringLiteral("develop");
  GitFlowNextBranch feature;
  feature.topic = true;
  feature.name = QStringLiteral("feature");
  feature.prefix = QStringLiteral("feature/");
  feature.parent = QStringLiteral("develop");
  feature.downstreamStrategy = QStringLiteral("rebase");
  GitFlowNextBranch release;
  release.topic = true;
  release.name = QStringLiteral("release");
  release.prefix = QStringLiteral("release/");
  release.parent = QStringLiteral("main");
  release.startPoint = QStringLiteral("staging");
  release.upstreamStrategy = QStringLiteral("squash");
  release.tag = true;
  release.tagPrefix = QStringLiteral("v");
  release.keepOnFinish = true;
  workflow.branches = {main, develop, staging, feature, release};
  return workflow;
}

} // namespace

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

  void graphiteWorkflowType() {
    QCOMPARE(workflowTypeToString(WorkflowType::Graphite),
             QStringLiteral("graphite"));
    QCOMPARE(workflowTypeFromString(QStringLiteral("graphite")),
             WorkflowType::Graphite);
  }

  void graphiteBranchNames_data() {
    QTest::addColumn<QString>("name");
    QTest::addColumn<bool>("safe");

    QTest::newRow("simple") << QStringLiteral("add-search") << true;
    QTest::newRow("nested") << QStringLiteral("feature/x.y-z_1") << true;
    QTest::newRow("empty") << QString() << false;
    QTest::newRow("space") << QStringLiteral("add search") << false;
    QTest::newRow("ampersand") << QStringLiteral("a&calc") << false;
    QTest::newRow("percent") << QStringLiteral("a%PATH%") << false;
    QTest::newRow("caret") << QStringLiteral("a^b") << false;
    QTest::newRow("quote") << QStringLiteral("a\"b") << false;
    QTest::newRow("pipe") << QStringLiteral("a|b") << false;
    QTest::newRow("option") << QStringLiteral("--force") << false;
    QTest::newRow("dots") << QStringLiteral("a..b") << false;
    QTest::newRow("lock") << QStringLiteral("a.lock") << false;
    QTest::newRow("trailing slash") << QStringLiteral("a/") << false;
    QTest::newRow("hidden component") << QStringLiteral("a/.b") << false;
    QTest::newRow("non ascii") << QStringLiteral("caf\u00e9") << false;
  }

  void graphiteBranchNames() {
    QFETCH(QString, name);
    QFETCH(bool, safe);
    QCOMPARE(isSafeGraphiteBranchName(name), safe);
  }

  // Output of gt 1.8.6 on a forked stack.
  void graphiteAmbiguousBranches() {
    QCOMPARE(
        GitRepo::graphiteAmbiguousBranches(QStringLiteral(
            "base\r\nERROR: Cannot get upstack branch in non-interactive "
            "mode; multiple choices available:\r\nleft-top\r\nright \r\n")),
        QStringList({QStringLiteral("left-top"), QStringLiteral("right")}));
    QCOMPARE(GitRepo::graphiteAmbiguousBranches(QStringLiteral(
                 "base\nERROR: Cannot get upstack branch in non-interactive "
                 "mode; multiple choices available:\nleft\nright\n\ntip: Run "
                 "gt checkout to pick a branch.")),
             QStringList({QStringLiteral("left"), QStringLiteral("right")}));
    QVERIFY(GitRepo::graphiteAmbiguousBranches(
                QStringLiteral("ERROR: Already at the top of the stack."))
                .isEmpty());
    QVERIFY(GitRepo::graphiteAmbiguousBranches(QString()).isEmpty());
  }

  void graphiteStateParsing() {
    const GraphiteStacks stacks = parseGraphiteState(
        QByteArrayLiteral("notice: something\n") + graphiteStateJson());
    QVERIFY(stacks.valid);
    QCOMPARE(stacks.branches.size(), 5);
    QCOMPARE(stacks.trunks(), QStringList{QStringLiteral("main")});
    QCOMPARE(stacks.branches.value("base").children,
             QStringList({QStringLiteral("left"), QStringLiteral("right")}));
    QCOMPARE(stacks.branches.value("left").parent, QStringLiteral("base"));
    QVERIFY(stacks.branches.value("left").needsRestack);
    QVERIFY(!stacks.branches.value("base").needsRestack);
    QVERIFY(!parseGraphiteState("ERROR: not initialized").valid);
  }

  void graphitePrInfo() {
    GraphiteStacks stacks = parseGraphiteState(graphiteStateJson());
    applyGraphitePrInfo(
        stacks,
        json("{'prInfos': ["
             "      {'headRefName': 'base', 'prNumber': 12, 'state': 'OPEN',"
             "       'isDraft': false, 'title': 'Add base',"
             "       'url': 'https://github.com/owner/repo/pull/12'},"
             "      {'headRefName': 'left', 'prNumber': 13, 'state': 'open',"
             "       'isDraft': true},"
             "      {'headRefName': 'gone', 'prNumber': 7, 'state': 'MERGED'}"
             "    ], 'mergeabilityStatuses': []}"));
    const GraphitePullRequest base = stacks.branches.value("base").pullRequest;
    QCOMPARE(base.number, 12);
    QCOMPARE(base.state, QStringLiteral("OPEN"));
    QCOMPARE(base.title, QStringLiteral("Add base"));
    QVERIFY(stacks.branches.value("left").pullRequest.draft);
    QCOMPARE(stacks.branches.value("left").pullRequest.state,
             QStringLiteral("OPEN"));
    QCOMPARE(stacks.branches.value("right").pullRequest.number, 0);
    QCOMPARE(gitHubRepositoryFromPullRequestUrl(base.url),
             QStringLiteral("owner/repo"));
    QVERIFY(gitHubRepositoryFromPullRequestUrl(
                QStringLiteral("https://gitlab.com/owner/repo/-/1"))
                .isEmpty());
  }

  void graphiteRows() {
    const GraphiteStacks stacks = parseGraphiteState(graphiteStateJson());
    const QVector<GraphiteBranchRow> rows = graphiteBranchRows(
        stacks, {QStringLiteral("base"), QStringLiteral("experiment"),
                 QStringLiteral("left"), QStringLiteral("left-top"),
                 QStringLiteral("main"), QStringLiteral("right")});

    QStringList order;
    for (const GraphiteBranchRow &row : rows) {
      order.append(row.kind == GraphiteBranchRow::Kind::UntrackedHeader
                       ? QStringLiteral("--")
                       : row.branch);
    }
    QCOMPARE(order, QStringList({"main", "base", "left", "left-top", "right",
                                 "--", "experiment"}));

    // main: root with children
    QCOMPARE(rows.at(0).depth, 0);
    QVERIFY(rows.at(0).hasChildren);
    // base: only child of main
    QCOMPARE(rows.at(1).depth, 1);
    QVERIFY(rows.at(1).lastChild);
    QVERIFY(rows.at(1).throughLines.isEmpty());
    // left: first of two children, a line continues to right
    QCOMPARE(rows.at(2).depth, 2);
    QVERIFY(!rows.at(2).lastChild);
    QCOMPARE(rows.at(2).throughLines, QVector<bool>{false});
    // left-top: under left, base's column has no sibling below
    QCOMPARE(rows.at(3).depth, 3);
    QCOMPARE(rows.at(3).throughLines, QVector<bool>({false, true}));
    // right: last child of base
    QCOMPARE(rows.at(4).depth, 2);
    QVERIFY(rows.at(4).lastChild);
    QVERIFY(!rows.at(4).hasChildren);
    // untracked
    QVERIFY(rows.at(5).kind == GraphiteBranchRow::Kind::UntrackedHeader);
    QVERIFY(!rows.at(6).tracked);
    QCOMPARE(rows.at(6).depth, 0);
  }

  // Tracked branches whose parent is not local become roots, and no header
  // is added when every branch is tracked.
  void graphiteRowsWithoutLocalParent() {
    const GraphiteStacks stacks = parseGraphiteState(graphiteStateJson());
    const QVector<GraphiteBranchRow> rows = graphiteBranchRows(
        stacks, {QStringLiteral("main"), QStringLiteral("left"),
                 QStringLiteral("left-top")});
    QStringList order;
    for (const GraphiteBranchRow &row : rows) {
      order.append(row.branch);
    }
    QCOMPARE(order, QStringList({"main", "left", "left-top"}));
    QCOMPARE(rows.at(1).depth, 0);
    QCOMPARE(rows.at(2).depth, 1);
    QVERIFY(!rows.at(0).hasChildren);
  }

  void customWorkflowJsonRoundTrip() {
    const GitFlowNextWorkflow workflow = releaseTrain();
    const QVector<GitFlowNextWorkflow> parsed =
        parseGitFlowNextWorkflows(serializeGitFlowNextWorkflows({workflow}));
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed.first().id, workflow.id);
    QCOMPARE(parsed.first().name, workflow.name);
    QCOMPARE(parsed.first().revision, 3);
    QCOMPARE(parsed.first().branches.size(), 5);
    const GitFlowNextBranch release = parsed.first().branches.at(4);
    QVERIFY(release.topic && release.tag && release.keepOnFinish);
    QCOMPARE(release.startPoint, QStringLiteral("staging"));
    QCOMPARE(release.upstreamStrategy, QStringLiteral("squash"));
    QCOMPARE(release.tagPrefix, QStringLiteral("v"));
    QVERIFY(parseGitFlowNextWorkflows("not json").isEmpty());
  }

  // What is written into .git/config reads back as the same model.
  void customWorkflowConfigEntries() {
    const GitFlowNextWorkflow workflow = releaseTrain();
    QList<QPair<QString, QString>> entries;
    for (const auto &[key, value] : gitFlowNextConfigEntries(workflow)) {
      // libgit2 reports variable names in lower case.
      const qsizetype dot = key.lastIndexOf(QLatin1Char('.'));
      entries.append({key.left(dot + 1) + key.mid(dot + 1).toLower(), value});
    }
    const GitFlowNextConfig config = parseGitFlowNextConfig(entries);
    QVERIFY(config.initialized);
    QCOMPARE(config.branches.size(), 5);
    const GitFlowNextWorkflow back = gitFlowNextWorkflowFromConfig(config);
    for (int i = 0; i < 5; ++i) {
      const GitFlowNextBranch &a = workflow.branches.at(i);
      const GitFlowNextBranch &b = back.branches.at(i);
      QCOMPARE(b.name, a.name);
      QCOMPARE(b.topic, a.topic);
      QCOMPARE(b.parent, a.parent);
      QCOMPARE(b.startPoint, a.startPoint);
      QCOMPARE(b.upstreamStrategy, a.upstreamStrategy);
      QCOMPARE(b.downstreamStrategy, a.downstreamStrategy);
      QCOMPARE(b.tag, a.tag);
      QCOMPARE(b.tagPrefix, a.tagPrefix);
      QCOMPARE(b.keepOnFinish, a.keepOnFinish);
      QCOMPARE(b.autoUpdate, a.autoUpdate);
      if (a.topic) {
        QCOMPARE(b.prefix, a.prefix);
      }
    }
  }

  void customWorkflowValid() {
    QVERIFY(validateGitFlowNextWorkflow(releaseTrain()).isEmpty());
  }

  void customWorkflowValidation_data() {
    QTest::addColumn<int>("change");
    for (int i = 0; i < 12; ++i) {
      QTest::newRow(QByteArray::number(i).constData()) << i;
    }
  }

  void customWorkflowValidation() {
    QFETCH(int, change);
    GitFlowNextWorkflow workflow = releaseTrain();
    auto &b = workflow.branches;
    switch (change) {
    case 0:
      workflow.name = QStringLiteral("  ");
      break; // name
    case 1:
      b[1].parent.clear();
      break; // 2 roots
    case 2:
      b[0].parent = QStringLiteral("staging");
      break; // cycle
    case 3:
      b[2].name = QStringLiteral("develop");
      break; // twice
    case 4:
      b[1].name = QStringLiteral("dev elop");
      break; // ref
    case 5:
      b[3].name = QStringLiteral("fea.ture");
      break; // type
    case 6:
      b[3].prefix = QStringLiteral("feature");
      break; // no /
    case 7:
      b[4].prefix = QStringLiteral("feature/");
      break; // dup
    case 8:
      b[3].parent = QStringLiteral("feature");
      break; // parent
    case 9:
      b[4].startPoint = QStringLiteral("nowhere");
      break; // start
    case 10:
      b[3].upstreamStrategy = QStringLiteral("cherry");
      break; // strat
    case 11:
      b.remove(0, 3);
      break; // no base
    }
    QVERIFY2(!validateGitFlowNextWorkflow(workflow).isEmpty(),
             qPrintable(QString::number(change)));
  }

  void customWorkflowBranchesToCreate() {
    const GitFlowNextWorkflow workflow = releaseTrain();
    QCOMPARE(gitFlowNextBaseBranchesToCreate(workflow, {"main"}),
             QStringList({"develop", "staging"}));
    QCOMPARE(gitFlowNextBaseBranchesToCreate(workflow, {"main", "develop"}),
             QStringList{"staging"});
    QVERIFY(gitFlowNextBaseBranchesToCreate(workflow,
                                            {"main", "develop", "staging"})
                .isEmpty());
  }
};

QTEST_APPLESS_MAIN(WorkflowsTest)

#include "tst_workflows.moc"
