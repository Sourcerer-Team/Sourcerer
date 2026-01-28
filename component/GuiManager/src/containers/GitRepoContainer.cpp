#include "containers/GitRepoContainer.h"
#include "DarkThemeLexersHighlighter.h"
#include "MainWindow.h"
#include "QScintillaLexerFactory.h"
#include "containers/ui_GitRepoContainer.h"
#include "delegates/ElidePartialColorDelegate.h"
#include "delegates/HistorySliceTableColumnDelegate.h"
#include "delegates/HistorySummaryTableColumnDelegate.h"
#include "models/GitCommitListModel.h"
#include "models/GitCommitTreeModel.h"
#include "widgets/AddSubmoduleOverlayWidget.h"
#include "widgets/ClickableHeaderView.h"
#include "widgets/ConsoleWidget.h"
#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"
#include "widgets/ShortcutsToolbarWidget.h"
#include "widgets/SideBySideDiffView.h"
#include "widgets/StashesPage.h"
#include "widgets/TextInputOverlayWidget.h"
#include "widgets/ToastHostWidget.h"
#include "widgets/UpstreamInputOverlayWidget.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <QCheckBox>
#include <QDirIterator>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSvgRenderer>
#include <QTextStream>
#include <QToolTip>

#include <functional>
#include <utility>

#define THREAD_ID "GitRepoContainer"

#define ALL_BRANCHES "*"
#define HEAD "HEAD"

#define MAXIMUM_PARENTS_IN_LABEL 3
#define MAX_COMMIT_SHA_LENGTH 6

#define GIT_IGNORE_FILENAME ".gitignore"
#define GIT_SUBDIR ".git"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

namespace GuiManager::Containers {
enum class CommitAndMergeStackIndexes {
  Unknown = -1,

  CommitAndMerge = 0,
  Display = 1,

  last
};

enum class DiffStackIndexes {
  Unknown = -1,

  DiffUnified = 0,
  DiffSideBySide = 1,

  last
};

enum class MainViewPanes {
  Unknown = -1,

  History = 0,
  Diff = 1,
  ThreeWayDiff = 2,

  last
};

QStringList traverseModelIndexSubtree(const QModelIndex &parent,
                                      const QAbstractItemModel *model) {
  if (!parent.isValid()) {
    return {};
  }

  QStringList files;

  const int rowCount = model->rowCount(parent);

  if (rowCount == 0) {
    QModelIndex current = parent;
    QString fullPath = current.data().toString();
    while (current.parent().isValid()) {
      fullPath =
          current.parent().data().toString() + QDir::separator() + fullPath;
      current = current.parent();
    };

    files += fullPath;

    return files;
  }

  for (int i = 0; i < rowCount; ++i) {
    QModelIndex child = model->index(i, 0, parent);
    files += traverseModelIndexSubtree(child, model);
  }

  return files;
}

GitRepoContainer::GitRepoContainer(QString path, QWidget *parent)
    : ContainerWidget(Widgets::ContainerType::GitRepo, parent),
      ui(new Ui::GitRepoContainer), _uuid(Uuid::createUuid()),
      _path(std::move(path)), _lastDiffRepoStatusChanged(false),
      _activeBranchType(GitRepo::BranchType::All),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _userProfileManager(Common::UserProfileManager::instance()),
      _toastHost(ToastHostWidget::getInstance()),
      _historyModel(new Models::GitHistoryModel()), _dispatcher(1),
      _mousePos(-1, -1), _spinnerOverlay(new SpinnerOverlayWidget(this)),
      _spinnerOverlayTimer(new QTimer(this)), _quitGettingHistory(false),
      _resizedHeaderSection(-1), _resizedHeaderSectionPreviousWidth(-1),
      _displayTreeExpanded(false), _mergeInProgress(false),
      _commitListModel(nullptr), _commitTreeModel(nullptr),
      _repoDirWatcher(nullptr), _repoDirWatcherTimer(new QTimer(this)),
      _shortcutsWidget(new GuiManager::Widgets::ShortcutsToolbarWidget(this)),
      _selected(false) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->localPage->setContentsMargins(0, 0, 0, 0);
  ui->remotePage->setContentsMargins(0, 0, 0, 0);
  ui->tagsPage->setContentsMargins(0, 0, 0, 0);
  ui->toolBox->setItemText(0, tr("Local Branches"));
  ui->toolBox->setItemText(1, tr("Remote Branches"));
  ui->toolBox->setItemText(2, tr("Tags"));
  ui->toolBox->setItemText(3, tr("Submodules"));
  ui->toolBox->setItemText(4, tr("Stashes"));
  ui->toolBox->setAttribute(Qt::WA_StyledBackground, true);
  ui->splitter->setStretchFactor(0, 2);
  ui->splitter->setStretchFactor(1, 6);
  ui->splitter->setStretchFactor(2, 2);
  ui->submodulesPage->setContentsMargins(0, 0, 0, 0);
  ui->historyView->setItemDelegateForColumn(
      0, new Delegates::HistorySliceTableColumnDelegate(ui->historyView));
  ui->historyView->setItemDelegateForColumn(
      1, new Delegates::HistorySummaryTableColumnDelegate(ui->historyView));
  ui->historyView->setMouseTracking(true);
  ui->historyView->viewport()->setMouseTracking(true);
  ui->historyView->viewport()->installEventFilter(this);
  ui->historyView->setModel(_historyModel.get());
  ui->historyView->setSelectionMode(QAbstractItemView::SingleSelection);
  ui->historyView->setVisible(false);
  ui->spinner->setColor(Qt::lightGray);

  auto *header = new ClickableHeaderView(Qt::Horizontal, ui->historyView);
  ui->historyView->setHorizontalHeader(header);
  ui->historyView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
  ui->pushButtonAbort->setVisible(false);
  ui->pushButtonAbort->setEnabled(false);
  ui->pushButtonAbort->setMinimumHeight(32);
  ui->pushButtonAbort->setMaximumHeight(32);
  ui->pushButtonCommit->setEnabled(false);
  ui->pushButtonCommit->setMinimumHeight(32);
  ui->pushButtonCommit->setMaximumHeight(32);
  ui->pushButtonMerge->setVisible(false);
  ui->pushButtonMerge->setEnabled(false);
  ui->pushButtonMerge->setMinimumHeight(32);
  ui->pushButtonMerge->setMaximumHeight(32);

  _checkBoxAmend =
      new QCheckBox(tr("Amend previous commit"), ui->tabWidgetCommit);

  ui->tabWidgetCommit->setCornerWidget(_checkBoxAmend);
  ui->tabWidgetCommit->tabBar()->setObjectName("tabWidgetCommitTabBar");
  ui->tabWidgetIndex->tabBar()->setObjectName("tabWidgetIndexTabBar");
  ui->tabWidgetWorkingTree->tabBar()->setObjectName(
      "tabWidgetWorkingTreeTabBar");

  _commitAndMergeWorkingDirTreeStageAllButton =
      new QPushButton(tr("Stage all"), ui->tabWidgetWorkingTree);
  _commitAndMergeWorkingDirTreeStageAllButton->setObjectName(
      "commitAndMergeWorkingDirTreeStageAllButton");
  _commitAndMergeWorkingDirTreeStageAllButton->setFlat(false);
  _commitAndMergeWorkingDirTreeStageAllButton->setVisible(true);

  resetCommitAndMergeWorkingTreeTreeStageAllButton();

  ui->tabWidgetWorkingTree->setCornerWidget(
      _commitAndMergeWorkingDirTreeStageAllButton);

  ui->listViewTabWidgetWorkingTreeFlat->setItemDelegate(
      new Delegates::ElidePartialColorDelegate(
          Qt::ElideMiddle, false, ui->listViewTabWidgetWorkingTreeFlat));
  ui->treeViewTabWidgetWorkingTreeTree->setItemDelegate(
      new Delegates::ElidePartialColorDelegate(
          Qt::ElideMiddle, true, ui->treeViewTabWidgetWorkingTreeTree));

  ui->listViewTabWidgetWorkingTreeFlat->setHorizontalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);

  ui->listViewTabWidgetWorkingTreeFlat->setContextMenuPolicy(
      Qt::CustomContextMenu);
  ui->treeViewTabWidgetWorkingTreeTree->setContextMenuPolicy(
      Qt::CustomContextMenu);

  _commitAndMergeIndexTreeUnstageAllButton =
      new QPushButton(tr("Unstage all"), ui->tabWidgetIndex);
  _commitAndMergeIndexTreeUnstageAllButton->setObjectName(
      "commitAndMergeIndexTreeUnstageAllButton");
  _commitAndMergeIndexTreeUnstageAllButton->setFlat(false);
  _commitAndMergeIndexTreeUnstageAllButton->setVisible(true);

  resetCommitAndMergeIndexTreeUnstageAllButton();

  ui->tabWidgetIndex->setCornerWidget(_commitAndMergeIndexTreeUnstageAllButton);

  ui->listViewTabWidgetIndexFlat->setItemDelegate(
      new Delegates::ElidePartialColorDelegate(Qt::ElideMiddle, false,
                                               ui->listViewTabWidgetIndexFlat));
  ui->treeViewTabWidgetIndexTree->setItemDelegate(
      new Delegates::ElidePartialColorDelegate(Qt::ElideMiddle, true,
                                               ui->treeViewTabWidgetIndexTree));

  ui->listViewTabWidgetIndexFlat->setHorizontalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);

  ui->listViewTabWidgetIndexFlat->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->treeViewTabWidgetIndexTree->setContextMenuPolicy(Qt::CustomContextMenu);

  _displayTreeExpandAllButton =
      new QPushButton(tr("Expand all"), ui->tabWidgetPageDisplayFiles);
  _displayTreeExpandAllButton->setObjectName("displayTreeExpandAllButton");
  _displayTreeExpandAllButton->setFlat(false);
  _displayTreeExpandAllButton->setVisible(
      ui->tabWidgetPageDisplayFiles->currentIndex() != 0);

  resetDisplayTreeExpandedButton();

  ui->tabWidgetPageDisplayFiles->setCornerWidget(_displayTreeExpandAllButton);
  ui->tabWidgetPageDisplayFiles->tabBar()->setObjectName(
      "tabWidgetPageDisplayFilesTabBar");

  ui->listViewPageDisplayFilesFlat->setItemDelegate(
      new Delegates::ElidePartialColorDelegate(
          Qt::ElideMiddle, false, ui->listViewPageDisplayFilesFlat));
  ui->treeViewPageDisplayFilesTree->setItemDelegate(
      new Delegates::ElidePartialColorDelegate(
          Qt::ElideMiddle, true, ui->treeViewPageDisplayFilesTree));

  ui->listViewPageDisplayFilesFlat->setHorizontalScrollBarPolicy(
      Qt::ScrollBarAlwaysOff);

  // Set placeholder text color via palette
  QPalette palette = ui->textEditDescription->palette();
  palette.setColor(QPalette::PlaceholderText, QColor(0x808080)); // Bright blue
  ui->textEditDescription->setPalette(palette);

  // Set editing cursor for commit panel summary and description widgets
  ui->lineEditSummary->setCursor(Qt::IBeamCursor);
  ui->textEditDescription->setCursor(Qt::IBeamCursor);

  _errorPixmap = new QPixmap(QSize(128, 128));
  _errorPixmap->fill(Qt::transparent);

  QSvgRenderer svgRenderer(QStringLiteral(":/icons/error.svg"));
  QPainter painter(_errorPixmap);
  svgRenderer.render(&painter);

  ui->labelErrorImage->setPixmap(*_errorPixmap);

  _historyModel->setSelectionModel(ui->historyView->selectionModel());

  _spinnerOverlayTimer->setSingleShot(true);
  _repoDirWatcherTimer->setSingleShot(true);

  const QIcon pullIcon(QPixmap(":icons/pull"));
  const QIcon pushIcon(QPixmap(":icons/push"));
  const QIcon popIcon(QPixmap(":icons/pop"));
  const QIcon stashIcon(QPixmap(":icons/stash"));

  auto *pushButtonPull = new QPushButton();
  pushButtonPull->setObjectName("pushButtonPull");
  pushButtonPull->setIcon(pullIcon);
  pushButtonPull->setToolTip(tr("Pull"));
  pushButtonPull->setMinimumSize(42, 42);
  pushButtonPull->setMaximumSize(42, 42);

  auto *pushButtonPush = new QPushButton();
  pushButtonPush->setObjectName("pushButtonPush");
  pushButtonPush->setIcon(pushIcon);
  pushButtonPush->setToolTip(tr("Push"));
  pushButtonPush->setMinimumSize(42, 42);
  pushButtonPush->setMaximumSize(42, 42);

  auto *pushButtonPop = new QPushButton();
  pushButtonPop->setObjectName("pushButtonPop");
  pushButtonPop->setIcon(popIcon);
  pushButtonPop->setToolTip(tr("Pop"));
  pushButtonPop->setMinimumSize(42, 42);
  pushButtonPop->setMaximumSize(42, 42);

  auto *pushButtonStash = new QPushButton();
  pushButtonStash->setObjectName("pushButtonStash");
  pushButtonStash->setIcon(stashIcon);
  pushButtonStash->setToolTip(tr("Stash"));
  pushButtonStash->setMinimumSize(42, 42);
  pushButtonStash->setMaximumSize(42, 42);

  _shortcutsWidget->addShortcutWidget(pushButtonPull);
  _shortcutsWidget->addShortcutWidget(pushButtonPush);
  _shortcutsWidget->addShortcutWidget(pushButtonStash);
  _shortcutsWidget->addShortcutWidget(pushButtonPop);

  // Explicitly show buttons to ensure they're interactive
  pushButtonPull->show();
  pushButtonPush->show();
  pushButtonStash->show();
  pushButtonPop->show();

  ui->leftVerticalLayout->insertWidget(0, _shortcutsWidget);

  ui->pushButtonDiffClose->setIcon(QPixmap(":/icons/close_button.png"));
  ui->pushButtonDiffClose->setMinimumSize(32, 32);
  ui->pushButtonDiffClose->setMaximumSize(32, 32);

  ui->pushButtonDiffUnified->setIcon(QPixmap(":/icons/diff-unified.png"));
  ui->pushButtonDiffUnified->setMinimumSize(32, 32);
  ui->pushButtonDiffUnified->setMaximumSize(32, 32);
  ui->pushButtonDiffUnified->setChecked(true);

  ui->pushButtonDiffSideBySide->setIcon(
      QPixmap(":/icons/diff-side-by-side.png"));
  ui->pushButtonDiffSideBySide->setMinimumSize(32, 32);
  ui->pushButtonDiffSideBySide->setMaximumSize(32, 32);

  ui->pushButtonMergeDiffClose->setIcon(QPixmap(":/icons/close_button.png"));
  ui->pushButtonMergeDiffClose->setMinimumSize(32, 32);
  ui->pushButtonMergeDiffClose->setMaximumSize(32, 32);

  ui->pushButtonMergeDiffDown->setIcon(QPixmap(":/icons/down_arrow.png"));
  ui->pushButtonMergeDiffDown->setMinimumSize(32, 32);
  ui->pushButtonMergeDiffDown->setMaximumSize(32, 32);
  ui->pushButtonMergeDiffDown->setChecked(true);

  ui->pushButtonMergeDiffUp->setIcon(QPixmap(":/icons/up_arrow.png"));
  ui->pushButtonMergeDiffUp->setMinimumSize(32, 32);
  ui->pushButtonMergeDiffUp->setMaximumSize(32, 32);

  ui->pushButtonMergeDiffSave->setMinimumHeight(32);
  ui->pushButtonMergeDiffSave->setMaximumHeight(32);

  const QScintillaLexerFactory factory;

  QsciLexer *lexerDiffUnified =
      factory.createLexerByName("Diff", ui->scintillaTextEditorUnified);

  ui->scintillaTextEditorUnified->setSizePolicy(QSizePolicy::Expanding,
                                                QSizePolicy::Expanding);
  ui->scintillaTextEditorUnified->setWrapMode(QsciScintilla::WrapWord);
  ui->scintillaTextEditorUnified->setLexer(lexerDiffUnified);
  ui->scintillaTextEditorUnified->setMarginsBackgroundColor(QColor(0x384D59));
  ui->scintillaTextEditorUnified->setMarginType(0, QsciScintilla::NumberMargin);
  ui->scintillaTextEditorUnified->setMarginType(1, QsciScintilla::SymbolMargin);
  ui->scintillaTextEditorUnified->setMarginWidth(1, 2);
  ui->scintillaTextEditorUnified->markerDefine(QsciScintilla::Background, 1);
  ui->scintillaTextEditorUnified->setMarkerBackgroundColor(Qt::darkYellow, 1);
  ui->scintillaTextEditorUnified->setReadOnly(true);

  DarkThemeLexersHighlighter highlighter;
  highlighter.applyDarkTheme(ui->scintillaTextEditorUnified);

  // Create scrollbar marks widget for unified diff
  _unifiedDiffMarks = new Widgets::ScrollbarMarksWidget(
      ui->scintillaTextEditorUnified->verticalScrollBar(),
      ui->scintillaTextEditorUnified, ui->scintillaTextEditorUnified);
  _unifiedDiffMarks->hide();

  showMainSpinnerPage(false);

  const QByteArray splitterState = QByteArray::fromHex(
      _configManager.getString(ConfigManager::ConfigKey::GitRepoSplitterPos)
          .toUtf8());
  ui->splitter->restoreState(splitterState);

  connect(ui->historyView, &QAbstractItemView::entered, this,
          &GitRepoContainer::onHistoryViewItemEntered);

  connect(ui->splitter, &QSplitter::splitterMoved, this,
          &GitRepoContainer::onSplitterMoved);

  connect(ui->localPage, &LocalGitBranchesPage::branchSelected, this,
          &GitRepoContainer::onLocalBranchSelected);
  connect(ui->localPage, &LocalGitBranchesPage::branchRightClicked, this,
          &GitRepoContainer::onLocalBranchRightClicked);
  connect(ui->localPage, &LocalGitBranchesPage::branchesMoved, this,
          &GitRepoContainer::onLocalBranchMoved);

  connect(ui->remotePage, &RemoteGitBranchesPage::branchSelected, this,
          &GitRepoContainer::onRemoteBranchSelected);
  connect(ui->remotePage, &RemoteGitBranchesPage::branchRightClicked, this,
          &GitRepoContainer::onRemoteBranchRightClicked);

  connect(ui->tagsPage, &GitTagsPage::tagSelected, this,
          &GitRepoContainer::onTagSelected);
  connect(ui->tagsPage, &GitTagsPage::tagRightClicked, this,
          &GitRepoContainer::onTagRightClicked);

  connect(ui->stashesPage, &StashesPage::stashSelected, this,
          &GitRepoContainer::onStashSelected);
  connect(ui->stashesPage, &StashesPage::applyStash, this,
          &GitRepoContainer::onApplyStash);
  connect(ui->stashesPage, &StashesPage::popStash, this,
          &GitRepoContainer::onPopStash);
  connect(ui->stashesPage, &StashesPage::deleteStash, this,
          &GitRepoContainer::onDeleteStash);

  connect(ui->submodulesPage, &GitSubmodulesPage::rightClicked, this,
          &GitRepoContainer::onSubmodulePanelRightClicked);
  connect(ui->submodulesPage, &GitSubmodulesPage::submoduleSelected, this,
          &GitRepoContainer::onSubmoduleSelected);
  connect(ui->submodulesPage, &GitSubmodulesPage::submoduleRightClicked, this,
          &GitRepoContainer::onSubmoduleRightClicked);

  connect(_spinnerOverlayTimer, &QTimer::timeout, this,
          &GitRepoContainer::onShowSpinnerOverlay);

  connect(header, &ClickableHeaderView::headerDoubleClicked, this,
          &GitRepoContainer::onGraphColumnExpand);

  connect(ui->historyView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &GitRepoContainer::onHistorySelectionChanged);

  connect(ui->tabWidgetPageDisplayFiles, &QTabWidget::currentChanged, this,
          [=](const int index) {
            _displayTreeExpandAllButton->setVisible(index != 0);
          });

  connect(_displayTreeExpandAllButton, &QPushButton::clicked, this,
          &GitRepoContainer::onDisplayTreeExpandAllButtonClicked);

  connect(
      _commitAndMergeWorkingDirTreeStageAllButton, &QPushButton::clicked, this,
      &GitRepoContainer::onCommitAndMergeWorkingTreeTreeStageAllButtonClicked);

  connect(_commitAndMergeIndexTreeUnstageAllButton, &QPushButton::clicked, this,
          &GitRepoContainer::onCommitAndMergeIndexTreeUnstageAllButtonClicked);

  connect(ui->lineEditSummary, &QLineEdit::textChanged, this,
          [=](const QString &text) { resetCommitButton(); });

  connect(ui->lineEditSummary, &QLineEdit::editingFinished, this,
          [=]() { resetCommitButton(); });

  connect(ui->textEditDescription, &QTextEdit::textChanged, this,
          [=]() { resetCommitButton(); });

  connect(ui->pushButtonAbort, &QPushButton::clicked, this,
          &GitRepoContainer::onAbortButtonClicked);
  connect(ui->pushButtonCommit, &QPushButton::clicked, this,
          &GitRepoContainer::onCommitButtonClicked);
  connect(ui->pushButtonMerge, &QPushButton::clicked, this,
          &GitRepoContainer::onMergeButtonClicked);

  connect(ui->listViewTabWidgetWorkingTreeFlat,
          &QListView::customContextMenuRequested, this,
          &GitRepoContainer::onWorkingTreeFlatListViewContextMenuRequested);

  connect(ui->actionWorkingTreeFlatStage, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeFlatListViewStageSelected);
  connect(ui->actionWorkingTreeFlatIgnore, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeFlatListViewIgnoreSelected);
  connect(ui->actionWorkingTreeFlatCreateFilePatch, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeFlatListViewCreateFilePatchSelected);
  connect(ui->actionWorkingTreeFlatDelete, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeFlatListViewDeleteSelected);
  connect(ui->actionWorkingTreeFlatDiscard, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeFlatListViewDiscardSelected);

  connect(ui->treeViewTabWidgetWorkingTreeTree,
          &QTreeView::customContextMenuRequested, this,
          &GitRepoContainer::onWorkingTreeTreeTreeViewContextMenuRequested);

  connect(ui->actionWorkingTreeTreeStage, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeTreeTreeViewStageSelected);
  connect(ui->actionWorkingTreeTreeIgnore, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected);
  connect(ui->actionWorkingTreeTreeCreateFilePatch, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeTreeTreeViewCreateFilePatchSelected);
  connect(ui->actionWorkingTreeTreeDelete, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeTreeTreeViewDeleteSelected);
  connect(ui->actionWorkingTreeTreeDiscard, &QAction::triggered, this,
          &GitRepoContainer::onWorkingTreeTreeTreeViewDiscardSelected);

  connect(ui->listViewTabWidgetIndexFlat,
          &QListView::customContextMenuRequested, this,
          &GitRepoContainer::onIndexFlatListViewContextMenuRequested);

  connect(ui->actionIndexFlatUnstage, &QAction::triggered, this,
          &GitRepoContainer::onIndexFlatListViewUnstageSelected);
  connect(ui->actionIndexFlatIgnore, &QAction::triggered, this,
          &GitRepoContainer::onIndexFlatListViewIgnoreSelected);
  connect(ui->actionIndexFlatCreateFilePatch, &QAction::triggered, this,
          &GitRepoContainer::onIndexFlatListViewCreateFilePatchSelected);
  connect(ui->actionIndexFlatDelete, &QAction::triggered, this,
          &GitRepoContainer::onIndexFlatListViewDeleteSelected);
  connect(ui->actionIndexFlatDiscard, &QAction::triggered, this,
          &GitRepoContainer::onIndexFlatListViewDiscardSelected);

  connect(ui->treeViewTabWidgetIndexTree,
          &QTreeView::customContextMenuRequested, this,
          &GitRepoContainer::onIndexTreeTreeViewContextMenuRequested);

  connect(ui->actionIndexTreeUnstage, &QAction::triggered, this,
          &GitRepoContainer::onIndexTreeTreeViewUnstageSelected);
  connect(ui->actionIndexTreeIgnore, &QAction::triggered, this,
          &GitRepoContainer::onIndexTreeTreeViewIgnoreSelected);
  connect(ui->actionIndexTreeCreateFilePatch, &QAction::triggered, this,
          &GitRepoContainer::onIndexTreeTreeViewCreateFilePatchSelected);
  connect(ui->actionIndexTreeDelete, &QAction::triggered, this,
          &GitRepoContainer::onIndexTreeTreeViewDeleteSelected);
  connect(ui->actionIndexTreeDiscard, &QAction::triggered, this,
          &GitRepoContainer::onIndexTreeTreeViewDiscardSelected);

  connect(_repoDirWatcherTimer, &QTimer::timeout, this,
          &GitRepoContainer::onRepoDirChangedTimeout);

  connect(pushButtonPull, &QPushButton::clicked, this,
          &GitRepoContainer::onPull);
  connect(pushButtonPush, &QPushButton::clicked, this,
          &GitRepoContainer::onPush);
  connect(pushButtonStash, &QPushButton::clicked, this,
          &GitRepoContainer::onStash);
  connect(pushButtonPop, &QPushButton::clicked, this, &GitRepoContainer::onPop);

  connect(this, &GitRepoContainer::historyViewRightClicked, this,
          &GitRepoContainer::onHistoryViewRightClicked);

  connect(ui->pushButtonDiffUnified, &QPushButton::clicked, this,
          &GitRepoContainer::onDiffUnifiedClicked);
  connect(ui->pushButtonDiffSideBySide, &QPushButton::clicked, this,
          &GitRepoContainer::onDiffSideBySideClicked);
  connect(ui->pushButtonDiffClose, &QPushButton::clicked, this,
          &GitRepoContainer::onDiffCloseClicked);

  connect(ui->pushButtonMergeDiffDown, &QPushButton::clicked, this,
          &GitRepoContainer::onMergeDiffDownClicked);
  connect(ui->pushButtonMergeDiffUp, &QPushButton::clicked, this,
          &GitRepoContainer::onMergeDiffUpClicked);
  connect(ui->pushButtonMergeDiffSave, &QPushButton::clicked, this,
          &GitRepoContainer::onMergeDiffSaveClicked);
  connect(ui->pushButtonMergeDiffClose, &QPushButton::clicked, this,
          &GitRepoContainer::onMergeDiffCloseClicked);

  qApp->setStyleSheet(qApp->styleSheet());

  LOG_PERF(0, "Exit")
}

GitRepoContainer::~GitRepoContainer() {
  LOG_PERF(0, "Enter");

  // Also quit GitRepo jobs
  closeRepo();

  _historyModel->quitUpdate();

  delete _errorPixmap;
  delete _repoDirWatcher;
  delete _repoDirWatcherTimer;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::init() {
  LOG_PERF(0, "Enter");

  ContainerWidget::init();

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    openRepo(guard);
    prepareWatcher(guard);
  });

  LOG_PERF(0, "Exit")
}

bool GitRepoContainer::eventFilter(QObject *watched, QEvent *event) {
  if (watched == ui->historyView->viewport()) {
    if (event->type() == QEvent::MouseMove) {
      LOCKGUARD(_tooltipsMutex);
      const QMouseEvent *const mouseEvent = static_cast<QMouseEvent *>(event);
      _mousePos = mouseEvent->globalPos();
    } else if (event->type() == QEvent::Leave) {
      LOCKGUARD(_tooltipsMutex);
      _mousePos = QPoint(-1, -1);
    } else if (event->type() == QEvent::MouseButtonPress) {
      if (const auto mouseEvent = static_cast<QMouseEvent *>(event);
          mouseEvent->button() == Qt::RightButton) {
        if (const QModelIndex index =
                ui->historyView->indexAt(mouseEvent->pos());
            index.isValid()) {
          emit historyViewRightClicked(index, mouseEvent->globalPos());
        }
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

void GitRepoContainer::deselected() {
  LOG_PERF(0, "Enter");

  _selected = false;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::selected() {
  LOG_PERF(0, "Enter");

  _selected = true;

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);
  _toastHost.raise();

  // Send commands to console when this container is selected
  // Find MainWindow by walking up the parent chain
  QWidget *widget = this;
  Widgets::ConsoleWidget *consoleWidget = nullptr;
  while (widget) {
    auto *mainWindow = qobject_cast<GuiManager::MainWindow *>(widget);
    if (mainWindow) {
      consoleWidget = mainWindow->findChild<Widgets::ConsoleWidget *>();
      if (consoleWidget) {
        break;
      }
    }

    widget = widget->parentWidget();
  }

  Q_ASSERT(consoleWidget);

  if (!consoleWidget) {
    LOG_PERF(0, "Exit")
    return;
  }

  // Send Ctrl+C to interrupt any running command
  consoleWidget->executeCommand("\x03");

  // Change directory to container's path
  consoleWidget->executeCommand(
      QString("cd \"%1\"").arg(QDir::toNativeSeparators(_path)));

  // Clear the console
  consoleWidget->executeCommand("clear");

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::hideEvent(QHideEvent *event) {
  LOG_PERF(0, "Enter");

  ContainerWidget::hideEvent(event);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");

  ContainerWidget::resizeEvent(event);
  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showEvent(QShowEvent *event) {
  LOG_PERF(0, "Enter");

  ContainerWidget::showEvent(event);

  raise();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onAddSubmodule() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  const auto overlayWidget = new AddSubmoduleOverlayWidget(this);

  connect(overlayWidget, &AddSubmoduleOverlayWidget::canceled, this, [=]() {
    overlayWidget->hide();
    delete overlayWidget;
  });

  connect(overlayWidget, &AddSubmoduleOverlayWidget::submitted, this,
          [=](const QString &url, const QString &name) {
            overlayWidget->hide();
            delete overlayWidget;

            // startOverlayTimer();

            _dispatcher.dispatch([=]() { addSubmodule(guard, url, name); });
          });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onApplyPatch() {
  LOG_PERF(0, "Enter");

  const QString filePath = QFileDialog::getOpenFileName(
      this, tr("Select a patch file to open"), QDir::homePath(),
      tr("Text Files (*.patch);;All Files (*)"));

  if (filePath.isEmpty()) {
    LOG_PERF(0, "Exit")
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { applyPatch(guard, filePath); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onApplyStash(const int stashIndex) {
  LOG_PERF(0, "Enter");

  const QMessageBox::StandardButton reply =
      QMessageBox::question(this,                               // parent widget
                            "Confirm Action",                   // dialog title
                            "Do you want to apply this stash?", // message text
                            QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_PERF(0, "Exit")
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { applyStash(guard, stashIndex); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onDeleteStash(const int stashIndex) {
  LOG_PERF(0, "Enter");

  const QMessageBox::StandardButton reply =
      QMessageBox::question(this,             // parent widget
                            "Confirm Action", // dialog title
                            "Do you want to delete this stash?", // message text
                            QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_PERF(0, "Exit")
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteStash(guard, stashIndex); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onPopStash(const int stashIndex) {
  LOG_PERF(0, "Enter");

  const QMessageBox::StandardButton reply =
      QMessageBox::question(this,                             // parent widget
                            "Confirm Action",                 // dialog title
                            "Do you want to pop this stash?", // message text
                            QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_PERF(0, "Exit")
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { popStash(guard, stashIndex); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::onSplitterMoved(const int pos, const int index) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("pos = {}, index = {}", pos, index);

  const QString splitterState = ui->splitter->saveState().toHex();
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSplitterPos,
                           splitterState);
  _configManager.save();

  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::showTooltipForColumnSlice(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  /*
  const QModelIndex shaIndex =
      _historyModel->index(index.row(), COLUMN_COMMIT_FULL_SHA, QModelIndex());
  if (!shaIndex.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString commit = shaIndex.data(Qt::DisplayRole).toString();

  {
    LOCKGUARD(_tooltipsMutex);
    _tooltipsToShow.clear();
    _tooltipsToShow += commit;
  }

  const auto guard = getSelf().toWeakRef();

  _dispatcher.dispatch([&, commit, index, guard]() {
    LOG_PERF(0, "Enter");

    const QStringList commitBranches = getCommitBranches(guard, commit);

    if (commitBranches.isEmpty()) {
      LOG_PERF(0, "Exit");
      return;
    }

    {
      LOCKGUARD(_tooltipsMutex);

      if (!_tooltipsToShow.contains(commit)) {
        LOG_PERF(0, "Exit");
        return;
      }
    }

    if (const auto locked = guard.lock()) {
      const QPoint viewportPos =
          ui->historyView->viewport()->mapFromGlobal(_mousePos);

      if (const QModelIndex idx = ui->historyView->indexAt(viewportPos);
          idx != index) {
        LOG_PERF(0, "Exit");
        return;
      }
    }

    const qlonglong tooltipDisplayTime =
        _configManager.getLong(ConfigManager::ConfigKey::TooltipDisplayTime);

    const QString tooltipText =
        QString("On branches:\n\n%1").arg(commitBranches.join("\n"));

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      QToolTip::showText(_mousePos, tooltipText, ui->historyView, {},
                         static_cast<int>(tooltipDisplayTime));
    });

    LOG_PERF(0, "Exit");
  });
*/

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::showTooltipForSummaryColumn(
    const QModelIndex &index) const {
  LOG_PERF(0, "Enter");

  QString tooltipText = tr("Repository status:\n\n");
  const auto statusFieldsMap =
      index.data(StatusRole).value<Models::StatusFieldsMap>();
  tooltipText += "New " +
                 QString::number(statusFieldsMap[Models::StatusFields::New]) +
                 "\n";
  tooltipText +=
      "Modified " +
      QString::number(statusFieldsMap[Models::StatusFields::Modified]) + "\n";
  tooltipText +=
      "Deleted " +
      QString::number(statusFieldsMap[Models::StatusFields::Deleted]) + "\n";
  tooltipText +=
      "Renamed " +
      QString::number(statusFieldsMap[Models::StatusFields::Renamed]) + "\n";
  tooltipText +=
      "Staged new " +
      QString::number(statusFieldsMap[Models::StatusFields::NewStaged]) + "\n";
  tooltipText +=
      "Staged  modified " +
      QString::number(statusFieldsMap[Models::StatusFields::ModifiedStaged]) +
      "\n";
  tooltipText +=
      "Staged deleted " +
      QString::number(statusFieldsMap[Models::StatusFields::DeletedStaged]) +
      "\n";
  tooltipText +=
      "Staged renamed " +
      QString::number(statusFieldsMap[Models::StatusFields::RenamedStaged]) +
      "\n";
  tooltipText +=
      "Ignored " +
      QString::number(statusFieldsMap[Models::StatusFields::Ignored]) + "\n";
  tooltipText +=
      "Conflicted " +
      QString::number(statusFieldsMap[Models::StatusFields::Conflicted]) + "\n";

  const qlonglong tooltipDisplayTime =
      _configManager.getLong(ConfigManager::ConfigKey::TooltipDisplayTime);

  QToolTip::showText(_mousePos, tooltipText, nullptr, {}, tooltipDisplayTime);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCheckoutCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { checkoutCommit(guard, commit); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onAbortButtonClicked() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  startOverlayTimer();

  _dispatcher.dispatch([=]() { abortMerge(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCommitButtonClicked() {
  LOG_PERF(0, "Enter");

  const Common::UserProfile &currentProfile =
      _userProfileManager.currentProfile();
  const QString summary = ui->lineEditSummary->text().trimmed();
  const QString description = ui->textEditDescription->toPlainText().trimmed();
  const bool toAmend = _checkBoxAmend->isChecked();

  const WeakPtr guard = getSelf().toWeakRef();

  startOverlayTimer();

  _dispatcher.dispatch([=]() {
    if (toAmend) {
      amend(guard, summary, description, currentProfile.author,
            currentProfile.email);
    } else {
      commit(guard, summary, description, currentProfile.author,
             currentProfile.email, false);
    }
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onMergeButtonClicked() {
  LOG_PERF(0, "Enter");

  const Common::UserProfile &currentProfile =
      _userProfileManager.currentProfile();
  const QString summary = ui->lineEditSummary->text().trimmed();
  const QString description = ui->textEditDescription->toPlainText().trimmed();

  const WeakPtr guard = getSelf().toWeakRef();

  startOverlayTimer();

  _dispatcher.dispatch([=]() {
    commit(guard, summary, description, currentProfile.author,
           currentProfile.email, true);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCommitAndMergeIndexTreeUnstageAllButtonClicked() {
  LOG_PERF(0, "Enter");

  _commitAndMergeIndexTreeUnstageAllButton->setVisible(false);

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { unstageAll(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCommitAndMergeWorkingTreeTreeStageAllButtonClicked() {
  LOG_PERF(0, "Enter");

  _commitAndMergeWorkingDirTreeStageAllButton->setVisible(false);

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stageAll(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCreateBranchFromCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  const auto overlayWidget = new TextInputOverlayWidget(this);
  overlayWidget->setPlaceholderText(tr("Branch name here"));

  connect(overlayWidget, &TextInputOverlayWidget::canceled, this, [=]() {
    overlayWidget->hide();
    delete overlayWidget;
  });

  connect(overlayWidget, &TextInputOverlayWidget::submitted, this,
          [=](const QString &branchName) {
            overlayWidget->hide();
            delete overlayWidget;

            startOverlayTimer();

            _dispatcher.dispatch([=]() {
              createBranchFromCommit(
                  guard, commit, branchName,
                  [=](const WeakPtr &guard, const QString &branchName) {
                    checkoutBranch(
                        guard, branchName, GitRepo::BranchType::Local,
                        [=](const WeakPtr &guard) {
                          Util::Dispatcher::dispatchToMainThread(guard, [=]() {
                            showSpinnerOverlay(false);
                            showToast(tr("Branch created sucesfully."),
                                      tr("Info"));
                          });
                        });
                  });
            });
          });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCreatePatchFromCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { createPatchFromCommit(guard, commit); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCreateTagFromCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  const auto overlayWidget = new TextInputOverlayWidget(this);
  overlayWidget->setPlaceholderText(tr("Tag name here"));

  connect(overlayWidget, &TextInputOverlayWidget::canceled, this, [=]() {
    overlayWidget->hide();
    delete overlayWidget;
  });

  connect(overlayWidget, &TextInputOverlayWidget::submitted, this,
          [=](const QString &tagName) {
            overlayWidget->hide();
            delete overlayWidget;

            startOverlayTimer();

            _dispatcher.dispatch(
                [=]() { createTagFromCommit(guard, commit, tagName); });
          });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDeleteBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString question =
      QString("Are you sure that you want to delete branch '%1'?")
          .arg(branchName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteBranch(guard, branchName); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDeleteTag(const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString question =
      QString("Are you sure that you want to delete tag '%1'?").arg(tagName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteTag(guard, tagName); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDiffCloseClicked() {
  LOG_PERF(0, "Enter");

  closeDiffViewerIfOpen();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::closeDiffViewerIfOpen() {
  LOG_PERF(0, "Enter");

  // Check if the diff viewer is currently displayed
  if (ui->stackedWidgetMiddle->currentIndex() !=
          static_cast<int>(MainViewPanes::Diff) &&
      ui->stackedWidgetMiddle->currentIndex() !=
          static_cast<int>(MainViewPanes::ThreeWayDiff)) {
    LOG_PERF(0, "Exit");
    return;
  }

  _lastDiffedCommit.clear();
  _lastDiffedFilePath.clear();
  _lastDiffRepoStatusChanged = false;

  // Hide scrollbar marks to prevent them from appearing in history view
  _unifiedDiffMarks->hide();

  ui->stackedWidgetMiddle->setCurrentIndex(
      static_cast<int>(MainViewPanes::History));

  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDiffUnifiedClicked() {
  LOG_PERF(0, "Enter");

  ui->pushButtonDiffSideBySide->setChecked(false);

  // Show unified diff marks when switching to unified view
  _unifiedDiffMarks->show();

  ui->stackedWidgetDiff->setCurrentIndex(
      static_cast<int>(DiffStackIndexes::DiffUnified));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDiffSideBySideClicked() {
  LOG_PERF(0, "Enter");

  ui->pushButtonDiffUnified->setChecked(false);

  // Hide unified diff marks when switching to side-by-side view
  _unifiedDiffMarks->hide();

  ui->stackedWidgetDiff->setCurrentIndex(
      static_cast<int>(DiffStackIndexes::DiffSideBySide));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDisplayTreeExpandAllButtonClicked() {
  LOG_PERF(0, "Enter");

  if (_displayTreeExpanded) {
    ui->treeViewPageDisplayFilesTree->collapseAll();
    _displayTreeExpandAllButton->setText(tr("Expand all"));
    _displayTreeExpanded = false;
    return;
  }

  ui->treeViewPageDisplayFilesTree->expandAll();
  _displayTreeExpandAllButton->setText(tr("Collapse all"));
  _displayTreeExpanded = true;

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onHardResetToCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { reset(guard, commit, GitRepo::ResetType::Hard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onHistorySelectionChanged(
    const QItemSelection &selected, const QItemSelection &deselected) {
  LOG_PERF(0, "Enter");

  const QModelIndexList indexes = selected.indexes();
  if (indexes.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  resetCommitAndMergeWorkingTreeTreeStageAllButton();
  resetDisplayTreeExpandedButton();

  const WeakPtr guard = getSelf().toWeakRef();

  const QModelIndex index = indexes.first();

  const auto statusFieldsMap =
      index.data(StatusRole).value<Models::StatusFieldsMap>();
  if (statusFieldsMap.isEmpty()) {
    const QModelIndex shaColumnModelIndex = _historyModel->index(
        index.row(), COLUMN_COMMIT_FULL_SHA, QModelIndex());
    if (shaColumnModelIndex.isValid()) {
      ui->stackedWidgetCommitAndMerge->setCurrentIndex(
          static_cast<int>(CommitAndMergeStackIndexes::Display));

      _dispatcher.clearPool();
      _dispatcher.dispatch([=]() {
        showCommit(guard, shaColumnModelIndex.data(Qt::DisplayRole).toString());
      });
    }
  } else {
    ui->stackedWidgetCommitAndMerge->setCurrentIndex(
        static_cast<int>(CommitAndMergeStackIndexes::CommitAndMerge));

    _dispatcher.clearPool();
    _dispatcher.dispatch([=]() { showCommitAndMerge(guard); });
  }

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onHistoryViewRightClicked(const QModelIndex &index,
                                                 const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex commitIndex =
      _historyModel->index(index.row(), COLUMN_COMMIT_FULL_SHA, QModelIndex());
  Q_ASSERT(commitIndex.isValid());
  if (!commitIndex.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString commit = commitIndex.data(Qt::DisplayRole).toString();

  QAction actionPull(tr("Pull"), this);
  QAction actionPush(tr("Push"), this);
  QAction actionSetUpstream(tr("Set Upstream"), this);
  QAction actionCheckoutCommit(tr("Checkout commit"), this);
  QAction actionCreateBranchFromCommit(tr("Create branch from commit"), this);
  QAction actionSoftResetToCommit(tr("Soft reset"), this);
  QAction actionMixedResetToCommit(tr("Mixed reset"), this);
  QAction actionHardResetToCommit(tr("Hard reset"), this);
  QAction actionRevertCommit(tr("Revert"), this);
  QAction actionApplyPatch(tr("Apply patch"), this);
  QAction actionRenameBranch(QString(tr("Rename %1")).arg(_activeBranch), this);
  QAction actionCreateTagFromCommit(tr("Create tag from commit"), this);
  QAction actionCreatePatchFromCommit(tr("Create patch from commit"), this);

  connect(&actionPull, &QAction::triggered, this, &GitRepoContainer::onPull);
  connect(&actionPush, &QAction::triggered, this, &GitRepoContainer::onPush);
  connect(&actionSetUpstream, &QAction::triggered, this,
          &GitRepoContainer::onSetUpstream);

  connect(&actionCheckoutCommit, &QAction::triggered, this,
          [=]() { onCheckoutCommit(commit); });
  connect(&actionCreateBranchFromCommit, &QAction::triggered, this,
          [=]() { onCreateBranchFromCommit(commit); });
  connect(&actionSoftResetToCommit, &QAction::triggered, this,
          [=]() { onSoftResetToCommit(commit); });
  connect(&actionMixedResetToCommit, &QAction::triggered, this,
          [=]() { onMixedResetToCommit(commit); });
  connect(&actionHardResetToCommit, &QAction::triggered, this,
          [=]() { onHardResetToCommit(commit); });
  connect(&actionRevertCommit, &QAction::triggered, this,
          [=]() { onRevertCommit(commit); });
  connect(&actionApplyPatch, &QAction::triggered, this,
          &GitRepoContainer::onApplyPatch);
  connect(&actionRenameBranch, &QAction::triggered, this,
          [=]() { onRenameBranch(_activeBranch); });
  connect(&actionCreateTagFromCommit, &QAction::triggered, this,
          [=]() { onCreateTagFromCommit(commit); });
  connect(&actionCreatePatchFromCommit, &QAction::triggered, this,
          [=]() { onCreatePatchFromCommit(commit); });

  QMenu resetMenu(QString(tr("Reset %1 to commit")).arg(_activeBranch), this);
  resetMenu.addAction(&actionSoftResetToCommit);
  resetMenu.addAction(&actionMixedResetToCommit);
  resetMenu.addAction(&actionHardResetToCommit);

  QMenu menu(this);
  menu.addAction(&actionPull);
  menu.addAction(&actionPush);
  menu.addAction(&actionSetUpstream);
  menu.addSeparator();

  if (!commit.isEmpty()) {
    menu.addAction(&actionCheckoutCommit);
    menu.addSeparator();
    menu.addAction(&actionCreateBranchFromCommit);
    menu.addMenu(&resetMenu);
    menu.addAction(&actionRevertCommit);
    menu.addSeparator();
  }

  menu.addAction(&actionApplyPatch);
  menu.addAction(&actionRenameBranch);

  if (!commit.isEmpty()) {
    menu.addSeparator();
    menu.addAction(&actionCreateTagFromCommit);
    menu.addSeparator();
    menu.addAction(&actionCreatePatchFromCommit);
  }

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onHistoryViewItemEntered(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.column() == COLUMN_HISTORY_SLICE) {
    showTooltipForColumnSlice(index);
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.column() == COLUMN_SUMMARY) {
    const auto statusFieldsMap =
        index.data(StatusRole).value<Models::StatusFieldsMap>();
    if (index.row() == 0 && !statusFieldsMap.isEmpty()) {
      showTooltipForSummaryColumn(index);
      LOG_PERF(0, "Exit");
      return;
    }
  }

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onListViewTabWidgetFlatSelected(
    const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString filePath = index.data(Qt::DisplayRole).toString();

  filePath = QDir::fromNativeSeparators(filePath);

  const GuiManager::Models::GitCommitListModificationType modificationType =
      static_cast<GuiManager::Models::GitCommitListModificationType>(
          index.data(Qt::UserRole).toInt());

  // Check file size for large files
  const QString fullPath = QDir(_path).filePath(filePath);
  const QFileInfo fileInfo(fullPath);
  const qint64 maxDiffFileSize =
      _configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  if (fileInfo.exists() && fileInfo.size() > maxDiffFileSize) {
    showErrorToast(tr("File is too large to be displayed."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    const int blobSize = getFileSizeFromCommit(filePath, QString());
    if (blobSize > maxDiffFileSize) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        showSpinnerOverlay(false);
        showErrorToast(tr("File is too large to be displayed."), tr("Error"));
        return;
      });

      return;
    }

    const bool conflicted =
        modificationType ==
        GuiManager::Models::GitCommitListModificationType::Conflicted;
    if (!conflicted) {
      diff(guard, filePath);
    } else {
      diffThreeWay(guard, filePath);
    }
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onListViewTabWidgetFlatSelectedFromCommit(
    const QModelIndex &index, const QString &commit) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString filePath = index.data(Qt::DisplayRole).toString();

  filePath = QDir::fromNativeSeparators(filePath);

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    // Check file size from commit before generating diff
    const qint64 maxDiffFileSize = _configManager.getLong(
        ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
    const qint64 fileSize = getFileSizeFromCommit(filePath, commit);

    if (fileSize > 0 && fileSize > maxDiffFileSize) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        showErrorToast(tr("File is too large to be displayed."), tr("Error"));
        return;
      });

      return;
    }

    Util::Dispatcher::dispatchToMainThread(guard,
                                           [=]() { startOverlayTimer(); });

    diffFromCommit(guard, filePath, commit);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onMergeDiffCloseClicked() {
  LOG_PERF(0, "Enter");

  _lastDiffedCommit.clear();
  _lastDiffedFilePath.clear();
  _lastDiffRepoStatusChanged = false;

  ui->stackedWidgetMiddle->setCurrentIndex(
      static_cast<int>(MainViewPanes::History));

  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onMergeDiffDownClicked() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onMergeDiffUpClicked() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onMergeDiffSaveClicked() {
  LOG_PERF(0, "Enter");

  if (!ui->mergeDiffViewer->areConflictsResolved()) {
    const QString question =
        QString("All conflicts in the file are not resolved.\nAre you sure "
                "that you want to save this file?");

    const auto reply =
        QMessageBox::question(this, tr("Confirm action"), question,
                              QMessageBox::Yes, QMessageBox::No);
    if (reply == QMessageBox::No) {
      LOG_PERF(0, "Exit");
      return;
    }
  }

  QString mergedContent = ui->mergeDiffViewer->mergedContent();

  if (ui->mergeDiffViewer->shouldConvertToWindowsFile()) {
    mergedContent = mergedContent.replace("\n", "\r\n");
  }

  const QString filePath = ui->mergeDiffViewer->filePath();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { saveThreeWayMergeResultFile(guard, filePath, mergedContent); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onTreeViewTabWidgetTreeSelected(
    const QModelIndex &index, const Models::GitCommitTreeModel &model) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Check if it is leaf node
  if (model.rowCount(index) > 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  GuiManager::Models::GitCommitListModificationType modificationType =
      GuiManager::Models::GitCommitListModificationType::Unknown;

  QStringList path;
  QModelIndex current = index;

  while (current.isValid()) {
    QStandardItem *item = model.itemFromIndex(current);
    Q_ASSERT(item);

    if (!item) {
      continue;
    }

    path.prepend(item->text());
    current = current.parent();

    modificationType =
        static_cast<GuiManager::Models::GitCommitListModificationType>(
            index.data(Qt::UserRole).toInt());
  }

  QString filePath = path.join("/");

  filePath = QDir::fromNativeSeparators(filePath);

  // Check file size for large files
  const QString fullPath = QDir(_path).filePath(filePath);
  const QFileInfo fileInfo(fullPath);
  const qint64 maxDiffFileSize =
      _configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  if (fileInfo.exists() && fileInfo.size() > maxDiffFileSize) {
    showErrorToast(tr("File is too large to be displayed."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    const int blobSize = getFileSizeFromCommit(filePath, QString());
    if (blobSize > maxDiffFileSize) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        showSpinnerOverlay(false);
        showErrorToast(tr("File is too large to be displayed."), tr("Error"));
        return;
      });

      return;
    }

    const bool conflicted =
        modificationType ==
        GuiManager::Models::GitCommitListModificationType::Conflicted;
    if (!conflicted) {
      diff(guard, filePath);
    } else {
      diffThreeWay(guard, filePath);
    }
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onTreeViewTabWidgetTreeSelectedFromCommit(
    const QModelIndex &index, const Models::GitCommitTreeModel &model,
    const QString &commit) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Check if it is leaf node
  if (model.rowCount(index) > 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  GuiManager::Models::GitCommitListModificationType modificationType =
      GuiManager::Models::GitCommitListModificationType::Unknown;

  QStringList path;
  QModelIndex current = index;

  while (current.isValid()) {
    QStandardItem *item = model.itemFromIndex(current);
    Q_ASSERT(item);

    if (!item) {
      continue;
    }

    path.prepend(item->text());
    current = current.parent();
  }

  QString filePath = path.join("/");

  filePath = QDir::fromNativeSeparators(filePath);

  // Check file size from commit before generating diff
  const qint64 maxDiffFileSize =
      _configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
  const qint64 fileSize = getFileSizeFromCommit(filePath, commit);

  if (fileSize > 0 && fileSize > maxDiffFileSize) {
    showErrorToast(tr("File is too large to be displayed."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { diffFromCommit(guard, filePath, commit); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onLocalBranchMoved(const QString &sourceBranch,
                                          const QString &destBranch) {
  LOG_PERF(0, "Enter");

  const QString question =
      QString("Are you sure that you want to merge branch '%1' to branch '%2'?")
          .arg(sourceBranch, destBranch);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { merge(guard, sourceBranch, destBranch); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onLocalBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { checkoutBranch(guard, branchName, GitRepo::BranchType::Local); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onLocalBranchRightClicked(const QString &branchName,
                                                 const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction actionCheckoutBranch(tr("Checkout branch"), this);
  QAction actionRanameBranch(tr("Rename branch"), this);
  QAction actionDeleteBranch(tr("Delete branch"), this);

  connect(&actionCheckoutBranch, &QAction::triggered, this, [=]() {
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() {
      checkoutBranch(guard, branchName, GitRepo::BranchType::Local);
    });
  });

  connect(&actionRanameBranch, &QAction::triggered, this,
          [=]() { onRenameBranch(branchName); });

  connect(&actionDeleteBranch, &QAction::triggered, this,
          [=]() { onDeleteBranch(branchName); });

  QMenu menu;
  menu.addAction(&actionCheckoutBranch);
  menu.addAction(&actionRanameBranch);
  menu.addSeparator();
  menu.addAction(&actionDeleteBranch);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onMixedResetToCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { reset(guard, commit, GitRepo::ResetType::Mixed); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onPop() {
  LOG_PERF(0, "Enter");

  const int stashIndex = ui->stashesPage->lastStash();
  if (stashIndex == -1) {
    LOG_PERF(0, "Exit");
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    popStash(guard, stashIndex);
    getStashes(guard);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onPull() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { pull(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onPush() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { push(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onStash() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stash(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onStashSelected(int stashIndex) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { applyStash(guard, stashIndex); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSubmitRenameBranch(const QString &oldName,
                                            const QString &newName) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { renameBranch(guard, oldName, newName); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSubmitRenameTag(const QString &oldName,
                                         const QString &newName) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { renameTag(guard, oldName, newName); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSubmoduleRightClicked(const QString &submoduleName,
                                               const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction actionOpenSubmodule(tr("Open"), this);
  QAction actionUpdateSubmodule(tr("Update"), this);
  QAction actionDeleteSubmodule(tr("Delete"), this);

  connect(&actionOpenSubmodule, &QAction::triggered, this, [=]() {
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() { openSubmodule(guard, submoduleName); });
  });

  connect(&actionUpdateSubmodule, &QAction::triggered, this, [=]() {
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() { updateSubmodule(guard, submoduleName); });
  });

  connect(&actionDeleteSubmodule, &QAction::triggered, this,
          [=]() { onDeleteSubmodule(submoduleName); });

  QMenu menu;
  menu.addAction(&actionOpenSubmodule);
  menu.addAction(&actionUpdateSubmodule);
  menu.addSeparator();
  menu.addAction(&actionDeleteSubmodule);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSubmodulePanelRightClicked(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction actionAddSubmodule(tr("Add"), this);

  connect(&actionAddSubmodule, &QAction::triggered, this,
          &GitRepoContainer::onAddSubmodule);

  QMenu menu;
  menu.addAction(&actionAddSubmodule);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSubmoduleSelected(const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { openSubmodule(guard, submoduleName); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onTagRightClicked(const QString &tagName,
                                         const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction actionCheckoutTag(tr("Checkout tag"), this);
  QAction actionRanameTag(tr("Rename tag"), this);
  QAction actionDeleteTag(tr("Delete tag"), this);

  connect(&actionCheckoutTag, &QAction::triggered, this, [=]() {
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch(
        [=]() { checkoutBranch(guard, tagName, GitRepo::BranchType::Tag); });
  });

  connect(&actionRanameTag, &QAction::triggered, this,
          [=]() { onRenameTag(tagName); });

  connect(&actionDeleteTag, &QAction::triggered, this,
          [=]() { onDeleteTag(tagName); });

  QMenu menu;
  menu.addAction(&actionCheckoutTag);
  menu.addAction(&actionRanameTag);
  menu.addSeparator();
  menu.addAction(&actionDeleteTag);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRemoteBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    checkoutBranch(guard, branchName, GitRepo::BranchType::Remote);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRemoteBranchRightClicked(const QString &branchName,
                                                  const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QAction actionCheckoutBranch(tr("Checkout branch"), this);

  connect(&actionCheckoutBranch, &QAction::triggered, this, [=]() {
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() {
      checkoutBranch(guard, branchName, GitRepo::BranchType::Remote);
    });
  });

  QMenu menu;
  menu.addAction(&actionCheckoutBranch);

  menu.exec(pos);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRenameBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");

  const auto inputOverlay = new TextInputOverlayWidget(this);
  inputOverlay->setPlaceholderText(tr("New branch name"));

  connect(inputOverlay, &TextInputOverlayWidget::canceled, this, [=]() {
    inputOverlay->hide();
    delete inputOverlay;
  });
  connect(inputOverlay, &TextInputOverlayWidget::submitted, this,
          [=](const QString &newName) {
            inputOverlay->hide();
            delete inputOverlay;
            onSubmitRenameBranch(branchName, newName);
          });

  inputOverlay->show();
  inputOverlay->raise();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRenameTag(const QString &tagName) {
  LOG_PERF(0, "Enter");

  const auto inputOverlay = new TextInputOverlayWidget(this);
  inputOverlay->setPlaceholderText(tr("New tag name"));

  connect(inputOverlay, &TextInputOverlayWidget::canceled, this, [=]() {
    inputOverlay->hide();
    delete inputOverlay;
  });
  connect(inputOverlay, &TextInputOverlayWidget::submitted, this,
          [=](const QString &newName) {
            inputOverlay->hide();
            delete inputOverlay;
            onSubmitRenameTag(tagName, newName);
          });

  inputOverlay->show();
  inputOverlay->raise();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRepoDirChanged(const QString &path) const {
  LOG_PERF(0, "Enter");

  const qlonglong holdbackTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoDirWatcherHoldbackTimeout);

  _repoDirWatcherTimer->stop();
  _repoDirWatcherTimer->start(holdbackTimeout);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRepoDirChangedTimeout() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    prepareWatcher(guard);
    refreshStatus(guard);

    _lastDiffRepoStatusChanged = true;
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRevertCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { revert(guard, commit); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSetUpstream() {
  LOG_PERF(0, "Enter");

  const auto upstreamOverlay = new UpstreamInputOverlayWidget(this);
  upstreamOverlay->setRemotePlaceholderText(tr("Remote name"));
  upstreamOverlay->setBranchPlaceholderText(tr("Remote branch name"));

  connect(upstreamOverlay, &UpstreamInputOverlayWidget::canceled, this, [=]() {
    upstreamOverlay->hide();
    delete upstreamOverlay;
  });

  const WeakPtr guard = getSelf().toWeakRef();

  connect(upstreamOverlay, &UpstreamInputOverlayWidget::submitted, this,
          [&, upstreamOverlay, guard](const QString &remote,
                                      const QString &branch) {
            upstreamOverlay->hide();
            delete upstreamOverlay;

            _dispatcher.dispatch([=]() { setUpstream(guard, remote, branch); });
          });

  _dispatcher.dispatch([=]() {
    const auto &[remoteName, remoteBranchName] = getUpstream(guard);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      upstreamOverlay->setRemoteName(remoteName);
      upstreamOverlay->setRemoteBranchName(remoteBranchName);
      upstreamOverlay->selectAllRemoteText();
      upstreamOverlay->show();
      upstreamOverlay->raise();
    });
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onSoftResetToCommit(const QString &commit) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { reset(guard, commit, GitRepo::ResetType::Soft); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onTagSelected(const QString &tagName) {
  LOG_PERF(0, "Enter");

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { checkoutBranch(guard, tagName, GitRepo::BranchType::Tag); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onShowSpinnerOverlay() {
  LOG_PERF(0, "Enter");

  showSpinnerOverlay();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onDeleteSubmodule(const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString question =
      QString("Are you sure that you want to delete submodule '%1'?")
          .arg(submoduleName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteSubmodule(guard, submoduleName); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::resizeSectionToViewport(const int section) {
  LOG_PERF(0, "Enter");

  _resizedHeaderSectionPreviousWidth =
      ui->historyView->horizontalHeader()->sectionSize(section);
  _resizedHeaderSection = section;

  ui->historyView->setColumnWidth(section,
                                  ui->historyView->viewport()->width());

  // Ensure layout is stable
  QTimer::singleShot(0, ui->historyView, [=]() {
    QModelIndex index =
        ui->historyView->model()->index(0, section); // Row 0, column N
    ui->historyView->scrollTo(index, QAbstractItemView::PositionAtCenter);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::restorePreviousSectionWidth() {
  LOG_PERF(0, "Enter");

  ui->historyView->setColumnWidth(_resizedHeaderSection,
                                  _resizedHeaderSectionPreviousWidth);
  _resizedHeaderSection = -1;

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::saveRepoInRecentList() const {
  LOG_PERF(0, "Enter");

  QString recentRepos =
      _configManager.getString(ConfigManager::ConfigKey::RecentlyOpenedRepos);

  QStringList recentReposList = recentRepos.split(";", Qt::SkipEmptyParts);
  if (recentReposList.contains(QDir::fromNativeSeparators(_path))) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (recentReposList.length() > 9) {
    recentReposList = recentReposList.mid(1, 9);
  }

  recentReposList += QDir::fromNativeSeparators(_path);
  recentRepos = recentReposList.join(";");

  _configManager.setString(ConfigManager::ConfigKey::RecentlyOpenedRepos,
                           recentRepos);
  _configManager.save();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::saveThreeWayMergeResultFile(const WeakPtr &guard,
                                                   const QString &filePath,
                                                   const QString &content) {
  LOG_PERF(0, "Enter");

  QFile file(QDir(_path).absoluteFilePath(filePath));
  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << content;
    file.close();
  }

  stageFiles(guard, QStringList() << filePath);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    ui->pushButtonMergeDiffSave->setEnabled(false);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::setUpstream(const WeakPtr &guard,
                                   const QString &remoteName,
                                   const QString &remoteBranchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoSetUpstreamCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.remoteBranchName = remoteBranchName;
  command.localBranchName = _activeBranch;

  EventManager::Event setUpstreamStashEvent(
      EventManager::EventType::GitRepoManagerSetUpstream, eventUuid);
  setUpstreamStashEvent.addData(command);
  _eventManager.send(setUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerSetUpstreamError,
      EventManager::EventType::GitRepoManagerSetUpstreamResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to set upstream.")), tr("Apply"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerSetUpstreamResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString(QString("%1\n%2").arg(
                         tr("Failed to set upstream."), error.message)),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool setUpstreamSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!setUpstreamSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to set upstream."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    showToast(tr("Successfully set upstream."), tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::startOverlayTimer() const {
  LOG_PERF(0, "Enter");

  const qlonglong timerTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);

  _spinnerOverlayTimer->start(static_cast<int>(timerTimeout));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::stash(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoStashCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event stageFilesEvent(
      EventManager::EventType::GitRepoManagerStash, eventUuid);
  stageFilesEvent.addData(command);
  _eventManager.send(stageFilesEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerStashError,
                           EventManager::EventType::GitRepoManagerStashResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to stash changes."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerStashResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to stash changes."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool stashSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!stashSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to stash changes."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::stopOverlayTimer() const {
  LOG_PERF(0, "Enter");

  _spinnerOverlayTimer->stop();

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::stopWatcher(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
#ifdef Q_OS_WIN32
    if (_repoDirWatcher) {
      _repoDirWatcher->stopWatching();
    }
#endif
    delete _repoDirWatcher;
    _repoDirWatcher = nullptr;
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onGraphColumnExpand(int section) {
  LOG_PERF(0, "Enter");

  if (_resizedHeaderSection != -1 && _resizedHeaderSection != section) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (_resizedHeaderSection == -1) {
    resizeSectionToViewport(section);
  } else {
    restorePreviousSectionWidth();
  }

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewContextMenuRequested(
    const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listViewTabWidgetWorkingTreeFlat->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const GuiManager::Models::GitCommitListModificationType modificationType =
      static_cast<GuiManager::Models::GitCommitListModificationType>(
          index.data(Qt::UserRole).toInt());

  const bool conflicted =
      modificationType ==
      GuiManager::Models::GitCommitListModificationType::Conflicted;

  const QStringList files =
      getSelectedFilesWhichExist(ui->listViewTabWidgetWorkingTreeFlat);

  QMenu contextMenu;

  if (!conflicted) {
    contextMenu.addAction(ui->actionWorkingTreeFlatStage);
    contextMenu.addAction(ui->actionWorkingTreeFlatDiscard);
  }

  if (!files.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &GitRepoContainer::onWorkingTreeFlatListViewIgnoreSelected);
    connect(
        ignoreRecursiveAction, &QAction::triggered, this,
        &GitRepoContainer::onWorkingTreeFlatListViewIgnoreRecursiveSelected);

    contextMenu.addSeparator();

    if (!conflicted) {
      contextMenu.addAction(ui->actionWorkingTreeFlatCreateFilePatch);
      contextMenu.addSeparator();
    }

    contextMenu.addAction(ui->actionWorkingTreeFlatDelete);
  }

  contextMenu.exec(ui->listViewTabWidgetWorkingTreeFlat->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewStageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewIgnoreRecursiveSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFilesRecursive(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewCreateFilePatchSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewDeleteSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFilesWhichExist(ui->listViewTabWidgetWorkingTreeFlat);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to delete the selected file?");
  } else {
    question = tr("Are you sure you want to delete the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewDiscardSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to discard the selected file?");
  } else {
    question = tr("Are you sure you want to discard the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewContextMenuRequested(
    const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->treeViewTabWidgetWorkingTreeTree->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const GuiManager::Models::GitCommitListModificationType modificationType =
      static_cast<GuiManager::Models::GitCommitListModificationType>(
          index.data(Qt::UserRole).toInt());

  const bool conflicted =
      modificationType ==
      GuiManager::Models::GitCommitListModificationType::Conflicted;

  const QStringList files =
      getSelectedFilesWhichExist(ui->treeViewTabWidgetWorkingTreeTree);

  QMenu contextMenu;

  if (!conflicted) {
    contextMenu.addAction(ui->actionWorkingTreeTreeStage);
    contextMenu.addAction(ui->actionWorkingTreeTreeDiscard);
  }

  if (!files.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &GitRepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected);
    connect(
        ignoreRecursiveAction, &QAction::triggered, this,
        &GitRepoContainer::onWorkingTreeTreeTreeViewIgnoreRecursiveSelected);

    contextMenu.addSeparator();

    if (!conflicted) {
      contextMenu.addAction(ui->actionWorkingTreeTreeCreateFilePatch);
      contextMenu.addSeparator();
    }

    contextMenu.addAction(ui->actionWorkingTreeTreeDelete);
  }

  contextMenu.exec(ui->treeViewTabWidgetWorkingTreeTree->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewStageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  const WeakPtr &guard = getSelf().toWeakRef();

  closeDiffViewerIfOpen();

  startOverlayTimer();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewIgnoreRecursiveSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  const WeakPtr &guard = getSelf().toWeakRef();

  closeDiffViewerIfOpen();

  startOverlayTimer();

  _dispatcher.dispatch([=]() { ignoreFilesRecursive(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewCreateFilePatchSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewDeleteSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFilesWhichExist(ui->treeViewTabWidgetWorkingTreeTree);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to delete the selected file?");
  } else {
    question = tr("Are you sure you want to delete the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewDiscardSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to discard the selected file?");
  } else {
    question = tr("Are you sure you want to discard the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewContextMenuRequested(
    const QPoint &pos) {
  LOG_PERF(0, "Enter");

  if (const QModelIndex index = ui->listViewTabWidgetIndexFlat->indexAt(pos);
      !index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files =
      getSelectedFilesWhichExist(ui->listViewTabWidgetIndexFlat);

  QMenu contextMenu;
  contextMenu.addAction(ui->actionIndexFlatUnstage);
  contextMenu.addAction(ui->actionIndexFlatDiscard);

  if (!files.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &GitRepoContainer::onIndexFlatListViewIgnoreSelected);
    connect(ignoreRecursiveAction, &QAction::triggered, this,
            &GitRepoContainer::onIndexFlatListViewIgnoreRecursiveSelected);

    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexFlatCreateFilePatch);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexFlatDelete);
  }

  contextMenu.exec(ui->listViewTabWidgetIndexFlat->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewUnstageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { unstageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewIgnoreRecursiveSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFilesRecursive(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewCreateFilePatchSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewDeleteSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFilesWhichExist(ui->listViewTabWidgetIndexFlat);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to delete the selected file?");
  } else {
    question = tr("Are you sure you want to delete the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewDiscardSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to discard the selected file?");
  } else {
    question = tr("Are you sure you want to discard the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewContextMenuRequested(
    const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->treeViewTabWidgetIndexTree->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files =
      getSelectedFilesWhichExist(ui->treeViewTabWidgetIndexTree);

  QMenu contextMenu;
  contextMenu.addAction(ui->actionIndexTreeUnstage);
  contextMenu.addAction(ui->actionIndexTreeDiscard);

  if (!files.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &GitRepoContainer::onIndexTreeTreeViewIgnoreSelected);
    connect(ignoreRecursiveAction, &QAction::triggered, this,
            &GitRepoContainer::onIndexTreeTreeViewIgnoreRecursiveSelected);

    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexTreeCreateFilePatch);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexTreeDelete);
  }

  contextMenu.exec(ui->treeViewTabWidgetIndexTree->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewUnstageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { unstageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewIgnoreRecursiveSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFilesRecursive(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewCreateFilePatchSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewDeleteSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFilesWhichExist(ui->treeViewTabWidgetIndexTree);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to delete the selected file?");
  } else {
    question = tr("Are you sure you want to delete the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { deleteFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewDiscardSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QString question;

  if (files.length() == 1) {
    question = tr("Are you sure you want to discard the selected file?");
  } else {
    question = tr("Are you sure you want to discard the %1 selected files?")
                   .arg(files.length());
  }

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_PERF(0, "Exit");
    return;
  }

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::updateModelActiveBranch(const WeakPtr &guard,
                                               const QString &branchName,
                                               GitRepo::BranchType branchType) {
  LOG_PERF(0, "Enter");

  switch (branchType) {
  case GitRepo::BranchType::Local: {
    updateLocalBranchesModelActiveBranch(guard, branchName);
    break;
  }
  case GitRepo::BranchType::Remote: {
    updateRemoteBranchesModelActiveBranch(guard, branchName);
    break;
  }
  case GitRepo::BranchType::Tag: {
    updateTagsModelActiveTag(guard, branchName);
    break;
  }
  case GitRepo::BranchType::All:
  default: {
    LOG_ERROR(Log::ErrorId::Logic, "Unsupported branch type {}",
              static_cast<int>(branchType));
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::abortMerge(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoAbortMergeCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event abortMergeEvent(
      EventManager::EventType::GitRepoManagerAbortMerge, eventUuid);
  abortMergeEvent.addData(command);
  _eventManager.send(abortMergeEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerAbortMergeError,
      EventManager::EventType::GitRepoManagerAbortMergeResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to abort merge.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerAbortMergeResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to abort merge."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool abortSuccess = commandResult.value.toBool();

  refreshHistory(guard);

  if (!abortSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to abort merge."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::addSubmodule(const WeakPtr &guard, const QString &url,
                                    const QString &name) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoAddSubmoduleCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.url = url;
  command.name = name;

  EventManager::Event addSubmoduleEvent(
      EventManager::EventType::GitRepoManagerAddSubmodule, eventUuid);
  addSubmoduleEvent.addData(command);
  _eventManager.send(addSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerAddSubmoduleError,
      EventManager::EventType::GitRepoManagerAddSubmoduleResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to add submodule.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerAddSubmoduleResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to add submodule."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool addSuccess = commandResult.value.toBool();

  refreshHistory(guard);

  if (!addSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to add submodule."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::amend(const WeakPtr &guard, const QString &summary,
                             const QString &description, const QString &author,
                             const QString &email) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoAmendCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;

  EventManager::Event amendEvent(EventManager::EventType::GitRepoManagerAmend,
                                 eventUuid);
  amendEvent.addData(command);
  _eventManager.send(amendEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerAmendError,
                           EventManager::EventType::GitRepoManagerAmendResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to amend changes."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerAmendResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to amend changes."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to amend changes."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    if (commitSuccess) {
      clearSummaryAndDescription();
    }
    showSpinnerOverlay(false);
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::applyPatch(const WeakPtr &guard,
                                  const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Can't open file \"%1\" for reading").arg(filePath),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  QTextStream in(&file);
  const QString content = in.readAll();

  file.close();

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoApplyPatchCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.patchContent = content;

  EventManager::Event applyPatchEvent(
      EventManager::EventType::GitRepoManagerApplyPatch, eventUuid);
  applyPatchEvent.addData(command);
  _eventManager.send(applyPatchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerApplyPatchError,
      EventManager::EventType::GitRepoManagerApplyPatchResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to apply patch.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerApplyPatchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to apply patch."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool applySuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!applySuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to apply patch."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::applyStash(
    const WeakPtr &guard, const int stashIndex,
    const std::function<void(const WeakPtr &, int)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoApplyStashCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event applyStashEvent(
      EventManager::EventType::GitRepoManagerApplyStash, eventUuid);
  applyStashEvent.addData(command);
  _eventManager.send(applyStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerApplyStashError,
      EventManager::EventType::GitRepoManagerApplyStashResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to apply stash.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerApplyStashResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to apply stash."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool applySuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!applySuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to apply stash."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (onSuccess) {
    onSuccess(guard, stashIndex);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::checkoutBranch(
    const WeakPtr &guard, const QString &branchName,
    const GitRepo::BranchType branchType,
    const std::function<void(const WeakPtr &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  if (branchName == ALL_BRANCHES) {
    updateModelActiveBranch(guard, branchName, branchType);
  } else {
    stopWatcher(guard);

    const QString eventUuid = Uuid::createUuid();

    const int token = _eventManager.startWait();

    GitRepoCheckoutBranchCommand command;
    command.uuid = _uuid;
    command.syncUuid = eventUuid;
    command.branchName = branchName;
    command.branchType = branchType;

    EventManager::Event checkoutBranchEvent(
        EventManager::EventType::GitRepoManagerCheckoutBranch, eventUuid);
    checkoutBranchEvent.addData(command);
    _eventManager.send(checkoutBranchEvent);

    static std::set types = {
        EventManager::EventType::GitRepoManagerCheckoutBranchError,
        EventManager::EventType::GitRepoManagerCheckoutBranchResult};

    const auto &[success, event] =
        _eventManager.waitForAnswer(token, eventUuid, types);

    QString additionalClarificationMessage;

    if (_historyModel->hasStatus()) {
      additionalClarificationMessage =
          tr("\nPlease "
             "commit your changes or stash them before you switch branches.");
    }

    if (!success) {
      closeRepo();

      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        showSpinnerOverlay(false);
        showErrorPage();
        showErrorToast(QString(tr("Failed to checkout branch. %1"))
                           .arg(additionalClarificationMessage),
                       tr("Checkout"));
      });

      LOG_PERF(0, "Exit")
      return;
    }

    if (event.type() !=
        EventManager::EventType::GitRepoManagerCheckoutBranchResult) {
      const auto error = event.data<GitRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        showSpinnerOverlay(false);
        showErrorToast(QString("%1 %2\n%3")
                           .arg(tr("Failed to checkout branch."),
                                additionalClarificationMessage, error.message),
                       tr("Error"));
      });

      prepareWatcher(guard);

      LOG_PERF(0, "Exit")
      return;
    }

    (void)getLocalBranches();

    const auto commandResult = event.data<GitRepoResult>();

    const auto [branchName, branchType] =
        commandResult.value.value<GitRepoCheckoutBranchCommandResult>();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      updateModelActiveBranch(guard, branchName, branchType);
    });

    prepareWatcher(guard);

    if (onSuccess) {
      onSuccess(guard);
    }
  }

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::checkoutCommit(const WeakPtr &guard,
                                      const QString &commit) {
  LOG_PERF(0, "Enter");

  stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoCheckoutCommitCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event checkoutCommitEvent(
      EventManager::EventType::GitRepoManagerCheckoutCommit, eventUuid);
  checkoutCommitEvent.addData(command);
  _eventManager.send(checkoutCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCheckoutCommitError,
      EventManager::EventType::GitRepoManagerCheckoutCommitResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  QString additionalClarificationMessage;

  if (_historyModel->hasStatus()) {
    additionalClarificationMessage =
        tr("\nPlease "
           "commit your changes or stash them before you checkout.");
  }

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to checkout commit. %1"))
                         .arg(additionalClarificationMessage),
                     tr("Checkout"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerCheckoutCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1 %2\n%3")
                         .arg(tr("Failed to checkout commit."),
                              additionalClarificationMessage, error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  const auto commandResult = event.data<GitRepoResult>();
  const bool checkoutSuccess = commandResult.value.toBool();

  if (!checkoutSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString(tr("Failed to checkout commit. %1"))
                         .arg(additionalClarificationMessage),
                     tr("Checkout"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [=]() { showSpinnerOverlay(false); });

  prepareWatcher(guard);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::clearSummaryAndDescription() {
  LOG_PERF(0, "Enter");

  ui->lineEditSummary->setText(QString());
  ui->textEditDescription->setText(QString());

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::closeRepo() const {
  LOG_PERF(0, "Enter");

  GitRepoCloseCommand closeCommand;
  closeCommand.uuid = _uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::GitRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::commit(const WeakPtr &guard, const QString &summary,
                              const QString &description, const QString &author,
                              const QString &email, bool cleanup) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoCommitStagedCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;
  command.cleanup = cleanup;

  EventManager::Event commitStagedEvent(
      EventManager::EventType::GitRepoManagerCommitStaged, eventUuid);
  commitStagedEvent.addData(command);
  _eventManager.send(commitStagedEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCommitStagedError,
      EventManager::EventType::GitRepoManagerCommitStagedResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to commit staged changes."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerCommitStagedResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(
                         tr("Failed to commit staged changes."), error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to commit staged changes."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    if (commitSuccess) {
      clearSummaryAndDescription();
    }
    showSpinnerOverlay(false);
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::createBranchFromCommit(
    const WeakPtr &guard, const QString &commit, const QString &branchName,
    const std::function<void(const WeakPtr &, const QString &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoCreateBranchFromCommitCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.branchName = branchName;

  EventManager::Event createBranchFromCommitEvent(
      EventManager::EventType::GitRepoManagerCreateBranchFromCommit, eventUuid);
  createBranchFromCommitEvent.addData(command);
  _eventManager.send(createBranchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitError,
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to create branch from commit."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to create branch from commit."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  refreshHistory(guard);

  if (onSuccess) {
    onSuccess(guard, branchName);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::createPatchFromCommit(const WeakPtr &guard,
                                             const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoCreatePatchFromCommitCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event createPatchFromCommitEvent(
      EventManager::EventType::GitRepoManagerCreatePatchFromCommit, eventUuid);
  createPatchFromCommitEvent.addData(command);
  _eventManager.send(createPatchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitError,
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to create patch from commit."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to create patch from commit."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto result =
      commandResult.value.value<GitRepoCreatePatchFromCommitCommandResult>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);

    if (result.patchContent.isEmpty()) {
      showErrorToast(tr("Failed to create patch from commit."), tr("Error"));
      return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr,                               // Parent widget
        "Save Patch File",                     // Dialog title
        QDir::homePath(),                      // Default directory
        "Patch Files (*.patch);;All Files (*)" // File filters
    );

    if (filePath.isEmpty()) {
      return;
    }

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
      QTextStream out(&file);
      out << result.patchContent;

      file.close();
    }
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::createTagFromCommit(const WeakPtr &guard,
                                           const QString &commit,
                                           const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString author =
      Common::UserProfileManager::instance().currentProfile().author;
  const QString email =
      Common::UserProfileManager::instance().currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoCreateTagFromCommitCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.tagName = tagName;
  command.author = author;
  command.email = email;
  command.message = QString();

  EventManager::Event createTagFromCommitEvent(
      EventManager::EventType::GitRepoManagerCreateTagFromCommit, eventUuid);
  createTagFromCommitEvent.addData(command);
  _eventManager.send(createTagFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
      EventManager::EventType::GitRepoManagerCreateTagFromCommitResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to create tag from commit."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerCreateTagFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to create tag from commit."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [=]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::deleteBranch(const WeakPtr &guard,
                                    const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDeleteBranchCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteBranchEvent(
      EventManager::EventType::GitRepoManagerDeleteBranch, eventUuid);
  deleteBranchEvent.addData(command);
  _eventManager.send(deleteBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteBranchError,
      EventManager::EventType::GitRepoManagerDeleteBranchResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to delete branch.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerDeleteBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to delete branch."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to delete branch."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::deleteFiles(const WeakPtr &guard,
                                   const QStringList &files) {
  LOG_PERF(0, "Enter");

  bool failed = false;

  for (const auto &file : files) {
    const QString fullPath = QDir(_path).filePath(file);
    if (QFileInfo(fullPath).isDir()) {
      QDir dir(fullPath);
      if (dir.exists()) {
        if (!dir.removeRecursively()) {
          failed = true;
        }
      }

      continue;
    }

    if (QFile::exists(fullPath)) {
      if (!QFile::remove(fullPath)) {
        failed = true;
      }
    }
  }

  if (failed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to delete some files."), tr("Error"));
    });
  }

  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::deleteStash(
    const WeakPtr &guard, int stashIndex,
    const std::function<void(const WeakPtr &, int)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDeleteStashCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event deleteStashEvent(
      EventManager::EventType::GitRepoManagerDeleteStash, eventUuid);
  deleteStashEvent.addData(command);
  _eventManager.send(deleteStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteStashError,
      EventManager::EventType::GitRepoManagerDeleteStashResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to delete stash.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerDeleteStashResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to delete stash."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to delete stash."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (onSuccess) {
    onSuccess(guard, stashIndex);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::deleteTag(const WeakPtr &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDeleteTagCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::GitRepoManagerDeleteTag, eventUuid);
  deleteTagEvent.addData(command);
  _eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteTagError,
      EventManager::EventType::GitRepoManagerDeleteTagResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to delete tag.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerDeleteTagResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to delete tag."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to delete tag."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::openSubmodule(const WeakPtr &guard,
                                     const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetSubmodulePathCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::GitRepoManagerGetSubmodulePath, eventUuid);
  deleteTagEvent.addData(command);
  _eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetSubmodulePathError,
      EventManager::EventType::GitRepoManagerGetSubmodulePathResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to get submodule path.")), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetSubmodulePathResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to get submodule path."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto [submodulePath] =
      commandResult.value.value<GitRepoGetSubmodulePathCommandResult>();

  if (submodulePath.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to get submodule path."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);

    const QString fullPath = QDir(_path).filePath(submodulePath);
    emit openRepoRequested(fullPath);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::popStash(const WeakPtr &guard, int stashIndex) {
  LOG_PERF(0, "Enter");

  applyStash(guard, stashIndex, [=](const WeakPtr &guard, const int index) {
    deleteStash(guard, index, [=](const WeakPtr &guard, const int) {
      Util::Dispatcher::dispatchToMainThread(
          guard, [&]() { showSpinnerOverlay(false); });
    });
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::deleteSubmodule(const WeakPtr &guard,
                                       const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  const QString fullPath = QDir(_path).filePath(submoduleName);

  GitRepoDeleteSubmoduleCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;
  command.submodulePath = fullPath;

  EventManager::Event deleteSubmoduleEvent(
      EventManager::EventType::GitRepoManagerDeleteSubmodule, eventUuid);
  deleteSubmoduleEvent.addData(command);
  _eventManager.send(deleteSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteSubmoduleError,
      EventManager::EventType::GitRepoManagerDeleteSubmoduleResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to delete submodule."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerDeleteSubmoduleResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to delete submodule."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool commandSuccess = commandResult.value.toBool();

  if (!commandSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to delete submodule."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [=]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

inline void GitRepoContainer::diff(const WeakPtr &guard,
                                   const QString &filePath) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_lastDiffRepoStatusChanged && _lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::Diff));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  _lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDiffFilesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::GitRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiffFilesError,
      EventManager::EventType::GitRepoManagerDiffFilesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to create file diff."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerDiffFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to create file diff."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto filesDiff =
      commandResult.value.value<GitRepoDiffFilesCommandResult>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showToast(tr("No difference compared to the HEAD commit."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const QString normalized =
        QString(filesDiff.diff).replace("\r\n", "\n").replace('\r', '\n');
    const int lineCount = normalized.count('\n') + 1;
    const int width = QString::number(lineCount).length();
    const QString mask = QString::number(0).rightJustified(width + 1, '0');

    ui->scintillaTextEditorUnified->setMarginWidth(0, mask);
    ui->scintillaTextEditorUnified->setText(filesDiff.diff);

    // Collect added and deleted lines for scrollbar marks
    QVector<int> deletedLines;
    QVector<int> addedLines;

    // Mark hunk headers
    for (int i = 0; i < ui->scintillaTextEditorUnified->lines(); ++i) {
      const QString lineText = ui->scintillaTextEditorUnified->text(i);
      if (lineText.startsWith("@@ ")) {
        ui->scintillaTextEditorUnified->markerAdd(i, 1);
      } else if (lineText.startsWith("-") && !lineText.startsWith("---")) {
        deletedLines.append(i);
      } else if (lineText.startsWith("+") && !lineText.startsWith("+++")) {
        addedLines.append(i);
      }
    }

    // Update scrollbar marks
    _unifiedDiffMarks->setMarks(deletedLines, addedLines);

    // Only show marks if unified view is selected
    if (ui->pushButtonDiffUnified->isChecked()) {
      _unifiedDiffMarks->show();
    } else {
      _unifiedDiffMarks->hide();
    }

    if (!filesDiff.deltas.isEmpty() && !filesDiff.repoContent.isEmpty()) {
      const QFileInfo fileInfo(filesDiff.repoContent.firstKey());
      ui->sideBySideDiffViewer->loadAndHighlight(filesDiff.repoContent.first(),
                                                 filesDiff.deltas.first(),
                                                 fileInfo.suffix());
    }

    showSpinnerOverlay(false);
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Diff));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::diffThreeWay(const WeakPtr &guard,
                                    const QString &filePath) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_lastDiffRepoStatusChanged && _lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::Diff));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  _lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoListConflictsCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;

  EventManager::Event listConflictsEvent(
      EventManager::EventType::GitRepoManagerListConflicts, eventUuid);
  listConflictsEvent.addData(command);
  _eventManager.send(listConflictsEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListConflictsError,
      EventManager::EventType::GitRepoManagerListConflictsResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to create file diff."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListConflictsResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to create file diff."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto listConflictsResult =
      commandResult.value.value<GitRepoListConflictsCommandResult>();

  if (listConflictsResult.fileContentWithMarkers.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showToast(tr("No difference compared to the HEAD commit."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    // Hide unified diff marks when switching to three-way diff
    _unifiedDiffMarks->hide();

    ui->pushButtonMergeDiffSave->setEnabled(true);
    ui->mergeDiffViewer->loadAndHighlight(
        listConflictsResult.fileContentWithMarkers, filePath);

    showSpinnerOverlay(false);
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::ThreeWayDiff));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::diffFromCommit(const WeakPtr &guard,
                                      const QString &filePath,
                                      const QString &commit) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_lastDiffRepoStatusChanged && _lastDiffedCommit == commit &&
      _lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::Diff));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  _lastDiffedCommit = commit;
  _lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDiffFilesFromCommitCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;
  command.commit = commit;

  EventManager::Event diffFilesFromCommitEvent(
      EventManager::EventType::GitRepoManagerDiffFilesFromCommit, eventUuid);
  diffFilesFromCommitEvent.addData(command);
  _eventManager.send(diffFilesFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiffFilesFromCommitError,
      EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to create file diff."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to create file diff."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto filesDiff =
      commandResult.value.value<GitRepoDiffFilesFromCommitCommandResult>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showToast(tr("No difference compared to the HEAD commit."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const QString normalized =
        QString(filesDiff.diff).replace("\r\n", "\n").replace('\r', '\n');
    const int lineCount = normalized.count('\n') + 1;
    const int width = QString::number(lineCount).length();
    const QString mask = QString::number(0).rightJustified(width + 1, '0');

    ui->scintillaTextEditorUnified->setMarginWidth(0, mask);
    ui->scintillaTextEditorUnified->setText(filesDiff.diff);

    // Collect added and deleted lines for scrollbar marks
    QVector<int> deletedLines;
    QVector<int> addedLines;

    // Mark hunk headers
    for (int i = 0; i < ui->scintillaTextEditorUnified->lines(); ++i) {
      const QString lineText = ui->scintillaTextEditorUnified->text(i);
      if (lineText.startsWith("@@ ")) {
        ui->scintillaTextEditorUnified->markerAdd(i, 1);
      } else if (lineText.startsWith("-") && !lineText.startsWith("---")) {
        deletedLines.append(i);
      } else if (lineText.startsWith("+") && !lineText.startsWith("+++")) {
        addedLines.append(i);
      }
    }

    // Update scrollbar marks
    _unifiedDiffMarks->setMarks(deletedLines, addedLines);

    // Only show marks if unified view is selected
    if (ui->pushButtonDiffUnified->isChecked()) {
      _unifiedDiffMarks->show();
    } else {
      _unifiedDiffMarks->hide();
    }

    if (!filesDiff.deltas.isEmpty() && !filesDiff.repoContent.isEmpty()) {
      const QFileInfo fileInfo(filesDiff.repoContent.firstKey());
      ui->sideBySideDiffViewer->loadAndHighlight(filesDiff.repoContent.first(),
                                                 filesDiff.deltas.first(),
                                                 fileInfo.suffix());
    }

    showSpinnerOverlay(false);
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Diff));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::diffFiles(const WeakPtr &guard,
                                 const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDiffFilesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::GitRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiffFilesError,
      EventManager::EventType::GitRepoManagerDiffFilesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to create diff file."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerDiffFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to create diff file."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto filesDiff =
      commandResult.value.value<GitRepoDiffFilesCommandResult>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showToast(tr("No difference compared to the HEAD commit."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr, tr("Save Diff File"), QString(), tr("Patch Files (*.patch)"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                   QIODevice::Text)) {
      showErrorToast(tr("Failed to create diff file %1.").arg(filePath),
                     tr("Error"));
      LOG_PERF(0, "Exit")
      return;
    }

    QTextStream out(&file);
    out << filesDiff.diff;
    out.flush();

    file.close();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::discardFiles(const WeakPtr &guard,
                                    const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoDiscardFilesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event discardFilesEvent(
      EventManager::EventType::GitRepoManagerDiscardFiles, eventUuid);
  discardFilesEvent.addData(command);
  _eventManager.send(discardFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiscardFilesError,
      EventManager::EventType::GitRepoManagerDiscardFilesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to discard file(s)."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerDiscardFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to discard file(s)."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool discarded = commandResult.value.toBool();

  if (!discarded) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showToast(tr("Failed to discard file(s)."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
    ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

bool GitRepoContainer::getActiveBranchOrTag(
    const WeakPtr &guard, const GitRepo::BranchType branchType) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetActiveBranchCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.branchType = branchType;

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::GitRepoManagerGetActiveBranch, eventUuid);
  getActiveBranchEvent.addData(command);
  _eventManager.send(getActiveBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      EventManager::EventType::GitRepoManagerGetActiveBranchResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the active local branch."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetActiveBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(
          QString("%1\n%2").arg(
              tr("Failed to retrieve the active local branch."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto [branchName, _] =
      commandResult.value.value<GitRepoGetActiveBranchCommandResult>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { updateModelActiveBranch(guard, branchName, branchType); });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoContainer::getActiveLocalBranch() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit")

  return getActiveBranchOrTag(getSelf().toWeakRef(),
                              GitRepo::BranchType::Local);
}

bool GitRepoContainer::getActiveRemoteBranch() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit")

  return getActiveBranchOrTag(getSelf().toWeakRef(),
                              GitRepo::BranchType::Remote);
}

bool GitRepoContainer::getActiveTag() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit")

  return getActiveBranchOrTag(getSelf().toWeakRef(), GitRepo::BranchType::Tag);
}

bool GitRepoContainer::getBranchHistory(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString historyEventUuid = Uuid::createUuid();

  const int historyToken = _eventManager.startWait();

  GitRepoGetBranchHistoryCommand historyCommand;
  historyCommand.uuid = _uuid;
  historyCommand.branchName = _activeBranch;
  historyCommand.branchType = _activeBranchType;
  historyCommand.syncUuid = historyEventUuid;

  EventManager::Event historyEvent(
      EventManager::EventType::GitRepoManagerGetBranchHistory,
      historyEventUuid);
  historyEvent.addData(historyCommand);
  _eventManager.send(historyEvent);

  static std::set historyTypes = {
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult};

  const auto &[historySuccess, historyResultEvent] =
      _eventManager.waitForAnswer(historyToken, historyEventUuid, historyTypes);

  if (!historySuccess) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to retrieve the history of branch %1."))
                         .arg(_activeBranch),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (historyResultEvent.type() !=
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult) {
    const auto error = historyResultEvent.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1 %2\n%3")
              .arg(tr("Failed to retrieve the history of branch %1."),
                   _activeBranch, error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto historyCommandResult = historyResultEvent.data<GitRepoResult>();

  auto history = historyCommandResult.value.value<GitRepo::GitHistory>();

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _eventManager.startWait();

  GitRepoStatusCommand statusCommand;
  statusCommand.uuid = _uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::GitRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::GitRepoManagerStatusError,
      EventManager::EventType::GitRepoManagerStatusResult};

  const auto &[statusSuccess, statusResultEvent] =
      _eventManager.waitForAnswer(statusToken, statusEventUuid, statusTypes);

  if (!statusSuccess) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the repository status."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::GitRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to retrieve the repository status."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto statusCommandResult = statusResultEvent.data<GitRepoResult>();

  const auto status = statusCommandResult.value.value<GitRepo::GitStatus>();

  GitRepo::GitHistoryWithStatus historyWithStatus;
  historyWithStatus.history = history;
  historyWithStatus.status = status;

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { ui->labelProgress->setText("0%"); });

  updateBranchHistory(guard, historyWithStatus);

  LOG_PERF(0, "Exit")

  return true;
}

QStringList GitRepoContainer::getCommitBranches(const WeakPtr &guard,
                                                const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetCommitBranchesCommand command;
  command.uuid = _uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event branchesEvent(
      EventManager::EventType::GitRepoManagerGetCommitBranches, eventUuid);
  branchesEvent.addData(command);
  _eventManager.send(branchesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetCommitBranchesError,
      EventManager::EventType::GitRepoManagerGetCommitBranchesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() { showErrorPage(); });
    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetCommitBranchesResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto branches = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return branches;
}

GitRepo::GitCommitStatus
GitRepoContainer::getCommitStatus(const WeakPtr &guard,
                                  const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetCommitStatusCommand command;
  command.uuid = _uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event commitStatusEvent(
      EventManager::EventType::GitRepoManagerGetCommitStatus, eventUuid);
  commitStatusEvent.addData(command);
  _eventManager.send(commitStatusEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetCommitStatusError,
      EventManager::EventType::GitRepoManagerGetCommitStatusResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    ::Util::Dispatcher::dispatchToMainThread(guard, [&]() { showErrorPage(); });

    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetCommitStatusResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto commitStatus =
      commandResult.value.value<GitRepo::GitCommitStatus>();

  LOG_PERF(0, "Exit");

  return commitStatus;
}

QStringList GitRepoContainer::getSelectedFiles(QListView *listView) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(listView != nullptr);

  if (!listView) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QModelIndexList listIndexes =
      listView->selectionModel()->selectedIndexes();

  QStringList files;

  for (const auto &index : listIndexes) {
    const GuiManager::Models::GitCommitListModificationType modificationType =
        static_cast<GuiManager::Models::GitCommitListModificationType>(
            index.data(Qt::UserRole).toInt());

    if (modificationType ==
        GuiManager::Models::GitCommitListModificationType::Conflicted) {
      continue;
    }

    files += QDir::fromNativeSeparators(index.data().toString());
  }

  LOG_PERF(0, "Exit");

  return files;
}

QStringList GitRepoContainer::getSelectedFilesWhichExist(QListView *listView) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(listView != nullptr);

  if (!listView) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QStringList files = getSelectedFiles(listView);

  QStringList filesThatExist;

  for (const auto &file : files) {
    const QString fullPath = QDir(_path).filePath(file);
    if (QFileInfo::exists(fullPath)) {
      filesThatExist += fullPath;
    }
  }

  LOG_PERF(0, "Exit");

  return filesThatExist;
}

QStringList GitRepoContainer::getSelectedFiles(QTreeView *treeView) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(treeView != nullptr);

  if (!treeView) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QModelIndexList listIndexes =
      treeView->selectionModel()->selectedIndexes();

  QStringList files;

  for (const auto &index : listIndexes) {
    const GuiManager::Models::GitCommitListModificationType modificationType =
        static_cast<GuiManager::Models::GitCommitListModificationType>(
            index.data(Qt::UserRole).toInt());

    if (modificationType ==
        GuiManager::Models::GitCommitListModificationType::Conflicted) {
      continue;
    }

    const QStringList traversedFiles =
        traverseModelIndexSubtree(index, treeView->model());

    for (const auto &traversedFile : traversedFiles) {
      files += QDir::fromNativeSeparators(traversedFile);
    }
  }

  LOG_PERF(0, "Exit");

  return files;
}

QStringList GitRepoContainer::getSelectedFilesWhichExist(QTreeView *treeView) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(treeView != nullptr);

  if (!treeView) {
    LOG_PERF(0, "Exit");
    return {};
  }

  const QStringList files = getSelectedFiles(treeView);

  QStringList filesThatExist;

  for (const auto &file : files) {
    const QString fullPath = QDir(_path).filePath(file);
    if (QFileInfo::exists(fullPath)) {
      filesThatExist += fullPath;
    }
  }

  LOG_PERF(0, "Exit");

  return filesThatExist;
}

bool GitRepoContainer::getLocalBranches() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoListLocalBranchesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listLocalEvent(
      EventManager::EventType::GitRepoManagerListLocalBranches, eventUuid);
  listLocalEvent.addData(command);
  _eventManager.send(listLocalEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      EventManager::EventType::GitRepoManagerListLocalBranchesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  const WeakPtr guard = getSelf().toWeakRef();

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the list of local branches."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListLocalBranchesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(QString("%1\n%2").arg(
                         tr("Failed to retrieve the list of local branches."),
                         error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto localBranches = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { updateLocalBranchesModel(guard, localBranches); });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoContainer::getRemoteBranches() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoListRemoteBranchesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemoteEvent(
      EventManager::EventType::GitRepoManagerListRemoteBranches, eventUuid);
  listRemoteEvent.addData(command);
  _eventManager.send(listRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the list of remote branches."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(QString("%1\n%2").arg(
                         tr("Failed to retrieve the list of remote branches."),
                         error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto localBranches = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { updateRemoteBranchesModel(guard, localBranches); });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoContainer::getStashes(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoListStashesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listStashesEvent(
      EventManager::EventType::GitRepoManagerListStashes, eventUuid);
  listStashesEvent.addData(command);
  _eventManager.send(listStashesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListStashesError,
      EventManager::EventType::GitRepoManagerListStashesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the list of stashes."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListStashesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to retrieve the list of stashes."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto stashes = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { updateStashesModel(guard, stashes); });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoContainer::getSubmodules() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoListSubmodulesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listSubmodulesEvent(
      EventManager::EventType::GitRepoManagerListSubmodules, eventUuid);
  listSubmodulesEvent.addData(command);
  _eventManager.send(listSubmodulesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      EventManager::EventType::GitRepoManagerListSubmodulesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  const WeakPtr guard = getSelf().toWeakRef();

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the list of submodules."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListSubmodulesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(
          QString("%1\n%2").arg(
              tr("Failed to retrieve the list of submodules."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto submodules = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { updateSubmodulesModel(guard, submodules); });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoContainer::getTags() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoListTagsCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listTagsEvent(
      EventManager::EventType::GitRepoManagerListTags, eventUuid);
  listTagsEvent.addData(command);
  _eventManager.send(listTagsEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListTagsError,
      EventManager::EventType::GitRepoManagerListTagsResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  const WeakPtr guard = getSelf().toWeakRef();

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the list of tags."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerListTagsResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to retrieve the list of tags."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto tags = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { updateTagsModel(guard, tags); });

  LOG_PERF(0, "Exit")

  return true;
}

QStringList
GitRepoContainer::getSubdirectories(const QString &path,
                                    const QStringList &excluded) const {
  LOG_PERF(0, "Enter");

  QStringList paths;

  paths += path;

  QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    const QString filePath = it.next();
    const QString relativePath = QDir(_path).relativeFilePath(filePath);
    if (excluded.contains(relativePath)) {
      continue;
    }

    paths += filePath;
  }

  LOG_PERF(0, "Exit")

  return paths;
}

QStringList GitRepoContainer::getAllFiles(const QString &path,
                                          const QStringList &excluded) const {
  LOG_PERF(0, "Enter");

  QStringList files;

  QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::NoSymLinks,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    const QString filePath = it.next();
    const QString relativePath = QDir(_path).relativeFilePath(filePath);

    // Check if file is in excluded directory
    bool shouldExclude = false;
    for (const QString &excludedDir : excluded) {
      if (relativePath.startsWith(excludedDir + "/") ||
          relativePath == excludedDir) {
        shouldExclude = true;
        break;
      }
    }

    if (!shouldExclude) {
      files += filePath;
    }
  }

  LOG_PERF(0, "Exit");

  return files;
}

QPair<QString, QString> GitRepoContainer::getUpstream(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetUpstreamCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = _activeBranch;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::GitRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetUpstreamError,
      EventManager::EventType::GitRepoManagerGetUpstreamResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(QString(tr("Failed to get upstream.")), tr("Apply"));
    });

    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetUpstreamResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to get upstream."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto [remoteName, remoteBranchName] =
      commandResult.value.value<GitRepoGetUpstreamCommandResult>();

  refreshStatus(guard);

  LOG_PERF(0, "Exit")

  return qMakePair(remoteName, remoteBranchName);
}

void GitRepoContainer::hideAllWidgets() const {
  LOG_PERF(0, "Enter");

  ui->splitter->hide();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::ignoreFiles(const WeakPtr &guard,
                                   const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString ignoreFilePath =
      QDir::toNativeSeparators(_path + QDir::separator() + GIT_IGNORE_FILENAME);

  QFile file(ignoreFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString(tr("Failed to open %1 for writing."))
                         .arg(GIT_IGNORE_FILENAME),
                     tr("Error"));
    });
    LOG_PERF(0, "Exit");
    return;
  };

  QTextStream out(&file);

  out << Qt::endl;

  for (const QString &file : files) {
    QString ignoreFile = file;
    const QString fullPath =
        QDir::toNativeSeparators(_path + QDir::separator() + ignoreFile);
    if (QFileInfo(fullPath).isDir()) {
      ignoreFile += "/";
    }

    out << QDir::fromNativeSeparators(ignoreFile) << Qt::endl;
  }

  out.flush();

  file.close();

  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::ignoreFilesRecursive(const WeakPtr &guard,
                                            const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString ignoreFilePath =
      QDir::toNativeSeparators(_path + QDir::separator() + GIT_IGNORE_FILENAME);

  QFile file(ignoreFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString(tr("Failed to open %1 for writing."))
                         .arg(GIT_IGNORE_FILENAME),
                     tr("Error"));
    });
    LOG_PERF(0, "Exit");
    return;
  };

  QTextStream out(&file);

  out << Qt::endl;

  // Get unique parent folders
  QSet<QString> parentFolders;
  for (const QString &filePath : files) {
    QFileInfo fileInfo(
        QDir::toNativeSeparators(_path + QDir::separator() + filePath));
    QString parentDir = fileInfo.dir().path();

    // Make path relative to repo root
    QString relativePath = QDir(_path).relativeFilePath(parentDir);
    if (relativePath != ".") {
      parentFolders.insert(relativePath);
    }
  }

  // Add ignore patterns for each parent folder
  for (const QString &folder : parentFolders) {
    QString ignorePattern = QDir::fromNativeSeparators(folder) + "/**";
    out << ignorePattern << Qt::endl;
  }

  out.flush();

  file.close();

  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::merge(const WeakPtr &guard, const QString &sourceBranch,
                             const QString &destBranch) {
  LOG_PERF(0, "Enter");

  const QString author = _userProfileManager.currentProfile().author;
  const QString email = _userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoMergeCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;

  EventManager::Event mergeEvent(EventManager::EventType::GitRepoManagerMerge,
                                 eventUuid);
  mergeEvent.addData(command);
  _eventManager.send(mergeEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerMergeError,
                           EventManager::EventType::GitRepoManagerMergeResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to merge branches."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerMergeResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to merge branches."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  const auto commandResult = event.data<GitRepoResult>();
  const auto merged = commandResult.value.toBool();

  if (!merged) {
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { showToast(tr("Merge has failed."), tr("Info")); });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit")
}

bool GitRepoContainer::openRepo(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoOpenCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.path = _path;

  EventManager::Event openRepoEvent(
      EventManager::EventType::GitRepoManagerOpenRepo, eventUuid);
  openRepoEvent.addData(command);
  _eventManager.send(openRepoEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerOpenRepoError,
      EventManager::EventType::GitRepoManagerOpenRepoResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerOpenRepoResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const bool refreshHistorySuccess = refreshHistory(guard);

  if (refreshHistorySuccess) {
    saveRepoInRecentList();
  }

  LOG_PERF(0, "Exit")

  return refreshHistorySuccess;
}

void GitRepoContainer::pull(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoPullCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event pullEvent(EventManager::EventType::GitRepoManagerPull,
                                eventUuid);
  pullEvent.addData(command);
  _eventManager.send(pullEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerPullError,
                           EventManager::EventType::GitRepoManagerPullResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerPullResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString(tr("Failed to pull from remote.\n%1")).arg(error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pulled = commandResult.value.toBool(); !pulled) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to pull from remote."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Git repository is up to-date."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::push(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoPushCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event pushEvent(EventManager::EventType::GitRepoManagerPush,
                                eventUuid);
  pushEvent.addData(command);
  _eventManager.send(pushEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerPushError,
                           EventManager::EventType::GitRepoManagerPushResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerPushResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString(tr("Failed to push to remote.\n%1")).arg(error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to push to remote."), tr("Error"));
    });

    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString(tr("Failed to pull from remote.\n%1")).arg(error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Successfully pushed to remote."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::renameBranch(const WeakPtr &guard,
                                    const QString &oldName,
                                    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoRenameBranchCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.oldBranchName = oldName;
  command.newBranchName = newName;

  EventManager::Event renameBranchEvent(
      EventManager::EventType::GitRepoManagerRenameBranch, eventUuid);
  renameBranchEvent.addData(command);
  _eventManager.send(renameBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerRenameBranchError,
      EventManager::EventType::GitRepoManagerRenameBranchResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to rename branch"), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerRenameBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to rename branch"), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to rename branch"), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Successfully renamed branch."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::renameTag(const WeakPtr &guard, const QString &oldName,
                                 const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoRenameTagCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.oldTagName = oldName;
  command.newTagName = newName;

  EventManager::Event renameTagEvent(
      EventManager::EventType::GitRepoManagerRenameTag, eventUuid);
  renameTagEvent.addData(command);
  _eventManager.send(renameTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerRenameTagError,
      EventManager::EventType::GitRepoManagerRenameTagResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to rename tag"), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerRenameTagResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to rename tag"), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to rename tag"), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Successfully renamed tag."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::reset(const WeakPtr &guard, const QString &commit,
                             GitRepo::ResetType type) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoResetCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.resetType = type;

  EventManager::Event resetEvent(EventManager::EventType::GitRepoManagerReset,
                                 eventUuid);
  resetEvent.addData(command);
  _eventManager.send(resetEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerResetError,
                           EventManager::EventType::GitRepoManagerResetResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerResetResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to reset to commit."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Successfully reset to commit."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::revert(const WeakPtr &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  QString summaryToRevert;

  for (int i = 0; i < _historyModel->rowCount(QModelIndex()); ++i) {
    const QString rowFullSHA =
        _historyModel
            ->data(
                _historyModel->index(i, COLUMN_COMMIT_FULL_SHA, QModelIndex()),
                Qt::DisplayRole)
            .toString();
    if (rowFullSHA == commit) {
      summaryToRevert =
          _historyModel
              ->data(_historyModel->index(i, COLUMN_SUMMARY, QModelIndex()),
                     Qt::DisplayRole)
              .toString();
      break;
    }
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoRevertCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;
  command.message = QString("[%1] %2").arg(
      tr("Revert"), summaryToRevert.isEmpty() ? commit : summaryToRevert);

  EventManager::Event revertEvent(EventManager::EventType::GitRepoManagerRevert,
                                  eventUuid);
  revertEvent.addData(command);
  _eventManager.send(revertEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerRevertError,
                           EventManager::EventType::GitRepoManagerRevertResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerRevertResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to revert commit."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Successfully reverted commit."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::prepareWatcher(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    delete _repoDirWatcher;
#ifdef Q_OS_WIN32
    _repoDirWatcher = new Util::DirectoryWatcher(_path, this);
    connect(_repoDirWatcher, &Util::DirectoryWatcher::directoryChanged, this,
            &GitRepoContainer::onRepoDirChanged, Qt::QueuedConnection);
    _repoDirWatcher->startWatching();
#else
    _repoDirWatcher = new QFileSystemWatcher(this);
    connect(_repoDirWatcher, &QFileSystemWatcher::directoryChanged, this,
            &GitRepoContainer::onRepoDirChanged);
    connect(_repoDirWatcher, &QFileSystemWatcher::fileChanged, this,
            &GitRepoContainer::onRepoDirChanged);
    
    // Watch all subdirectories except .git to detect file additions/deletions
    const QStringList directories =
        getSubdirectories(_path, QStringList() << GIT_SUBDIR);
    _repoDirWatcher->addPaths(directories);
    
    // Watch all files recursively (excluding .git directory) to detect modifications
    const QStringList files = getAllFiles(_path, QStringList() << GIT_SUBDIR);
    _repoDirWatcher->addPaths(files);
#endif
  });

  LOG_PERF(0, "Exit");
}

bool GitRepoContainer::refreshHistory(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  bool ok = true;

  ok &= getLocalBranches();
  ok &= ok && getActiveLocalBranch();
  ok &= ok && getRemoteBranches();
  ok &= ok && getActiveRemoteBranch();
  ok &= ok && getTags();
  ok &= ok && getActiveTag();
  ok &= ok && getSubmodules();
  ok &= ok && getStashes(guard);

  if (!ok) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });
  }

  LOG_PERF(0, "Exit")

  return ok;
}

void GitRepoContainer::refreshStatus(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _eventManager.startWait();

  GitRepoStatusCommand statusCommand;
  statusCommand.uuid = _uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::GitRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::GitRepoManagerStatusError,
      EventManager::EventType::GitRepoManagerStatusResult};

  const auto &[statusSuccess, statusResultEvent] =
      _eventManager.waitForAnswer(statusToken, statusEventUuid, statusTypes);

  if (!statusSuccess) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the repository status."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::GitRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to retrieve the repository status."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto statusCommandResult = statusResultEvent.data<GitRepoResult>();

  const auto status = statusCommandResult.value.value<GitRepo::GitStatus>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const QModelIndex currentIndex = ui->historyView->currentIndex();
    const int currentRow = currentIndex.row();

    int offset = 0;

    if (!_historyModel->hasStatus() && status.hasChanges()) {
      ++offset;
    } else if (_historyModel->hasStatus() && !status.hasChanges()) {
      --offset;
    }

    int newRow = currentRow + offset;
    if (newRow < 0) {
      newRow = 0;
    }

    _historyModel->updateStatus(status);

    simulateClickOnHistoryRow(newRow);
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetCommitAndMergeIndexTreeUnstageAllButton() const {
  LOG_PERF(0, "Enter");

  int rowCount = 0;

  if (_commitStagedListModel) {
    for (int i = 0; i < _commitStagedListModel->rowCount(); ++i) {
      const GuiManager::Models::GitCommitListModificationType modificationType =
          static_cast<GuiManager::Models::GitCommitListModificationType>(
              _commitStagedListModel->index(i, 0).data(Qt::UserRole).toInt());
      if (modificationType ==
          GuiManager::Models::GitCommitListModificationType::Conflicted) {
        continue;
      }

      ++rowCount;
    }
  }

  _commitAndMergeIndexTreeUnstageAllButton->setText(tr("Unstage all"));
  _commitAndMergeIndexTreeUnstageAllButton->setVisible(rowCount > 0);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetCommitAndMergeWorkingTreeTreeStageAllButton() {
  LOG_PERF(0, "Enter");

  _commitAndMergeWorkingDirTreeStageAllButton->setText(tr("Stage all"));

  int rowCount = 0;

  if (_commitListModel) {
    for (int i = 0; i < _commitListModel->rowCount(); ++i) {
      const GuiManager::Models::GitCommitListModificationType modificationType =
          static_cast<GuiManager::Models::GitCommitListModificationType>(
              _commitListModel->index(i, 0).data(Qt::UserRole).toInt());
      if (modificationType ==
          GuiManager::Models::GitCommitListModificationType::Conflicted) {
        continue;
      }

      ++rowCount;
    }
  }

  _commitAndMergeWorkingDirTreeStageAllButton->setVisible(rowCount > 0);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetCommitButton() const {
  LOG_PERF(0, "Enter");

  ui->pushButtonCommit->setEnabled(
      !_mergeInProgress && _commitStagedListModel &&
      _commitStagedListModel->rowCount() > 0 &&
      (!ui->lineEditSummary->text().isEmpty() ||
       !ui->textEditDescription->toPlainText().isEmpty()));
  ui->pushButtonCommit->setVisible(!_mergeInProgress);

  ui->pushButtonAbort->setVisible(_mergeInProgress);
  ui->pushButtonAbort->setEnabled(_mergeInProgress);

  ui->pushButtonMerge->setVisible(_mergeInProgress);
  ui->pushButtonMerge->setEnabled(
      _mergeInProgress && _commitStagedListModel &&
      _commitStagedListModel->rowCount() > 0 &&
      (!ui->lineEditSummary->text().isEmpty() ||
       !ui->textEditDescription->toPlainText().isEmpty()));
  _checkBoxAmend->setVisible(!_mergeInProgress);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetDisplayTreeExpandedButton() {
  LOG_PERF(0, "Enter");

  _displayTreeExpandAllButton->setText(tr("Expand all"));
  _displayTreeExpanded = false;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::stretchBranchHistoryTableColumns() const {
  LOG_PERF(0, "Enter");

  ui->historyView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);

  const QFont font = ui->historyView->font();
  const QFontMetrics fontMetrics(font);
  const int charWidth = fontMetrics.horizontalAdvance('0');

  const int totalWidth = ui->historyView->viewport()->width();
  int colSummary = totalWidth / 2;
  if (constexpr int maxSummaryWidth = 600; colSummary > maxSummaryWidth)
    colSummary = maxSummaryWidth;

  const int remaining = totalWidth - colSummary;
  const int colSha = charWidth * 7; // Short SHA length
  const int remainingAfterSha = remaining - colSha;
  const int colOther = remainingAfterSha / 4;

  ui->historyView->setColumnWidth(0, colOther);
  ui->historyView->setColumnWidth(1, colSummary);
  ui->historyView->setColumnWidth(2, colOther);
  ui->historyView->setColumnWidth(3, colOther);
  ui->historyView->setColumnWidth(4, colOther);
  ui->historyView->setColumnWidth(5, colSha);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showAllWidgets() const {
  LOG_PERF(0, "Enter");

  ui->splitter->show();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showCommit(const WeakPtr &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus &historyAndStatus =
      _historyModel->historyWithStatus();

  Q_ASSERT(historyAndStatus.history.commits.contains(commit));
  Q_ASSERT(historyAndStatus.history.commitInfos.contains(commit));

  if (!historyAndStatus.history.commitInfos.contains(commit)) {
    LOG_PERF(0, "Exit");
    return;
  }

  const GitRepo::GitCommitStatus commitStatus = getCommitStatus(guard, commit);

  const GitRepo::GitCommitInfo &commitInfo =
      historyAndStatus.history.commitInfos[commit];

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    ui->labelPageDiaplayShaValue->setText(commit.left(MAX_COMMIT_SHA_LENGTH));
    ui->textEditPageDisplayDescription->setText(commitInfo.message);
    ui->labelPageDiaplayAuthorValue->setText(commitInfo.authorName);
    ui->labelPageDiaplayParentsValue->setText(
        commitInfo.parents.isEmpty()
            ? tr("None")
            : commitInfo.parents
                      .mid(0, MAXIMUM_PARENTS_IN_LABEL) // Show up to 3 parents
                      .join(", ")
                      .left(MAX_COMMIT_SHA_LENGTH) +
                  (commitInfo.parents.size() > MAXIMUM_PARENTS_IN_LABEL ? "..."
                                                                        : ""));

    QTextCursor cursor = ui->textEditPageDisplayDescription->textCursor();
    QTextBlockFormat format = cursor.blockFormat();
    format.setAlignment(Qt::AlignJustify); // or AlignCenter, AlignJustify, etc.
    cursor.mergeBlockFormat(format);
    ui->textEditPageDisplayDescription->setTextCursor(cursor);

    _commitListModel.reset(Models::GitCommitListModel::from(commitStatus));
    if (_commitListModel) {
      ui->listViewPageDisplayFilesFlat->setModel(_commitListModel.get());

      disconnect(ui->listViewPageDisplayFilesFlat, &QListView::pressed, this,
                 nullptr);

      connect(ui->listViewPageDisplayFilesFlat, &QListView::pressed, this,
              [=](const QModelIndex &index) {
                onListViewTabWidgetFlatSelectedFromCommit(index, commit);
              });
    }

    _commitTreeModel.reset(Models::GitCommitTreeModel::from(commitStatus));
    if (_commitTreeModel) {
      ui->treeViewPageDisplayFilesTree->setModel(_commitTreeModel.get());

      disconnect(ui->treeViewPageDisplayFilesTree, &QTreeView::pressed, this,
                 nullptr);

      connect(ui->treeViewPageDisplayFilesTree, &QTreeView::pressed, this,
              [=](const QModelIndex &index) {
                onTreeViewTabWidgetTreeSelectedFromCommit(
                    index, *_commitTreeModel, commit);
              });
    }
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showCommitAndMerge(WeakPtr guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetIsMergeInProgressCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;

  EventManager::Event getIsMergeInProgressEvent(
      EventManager::EventType::GitRepoManagerGetIsMergeInProgress, eventUuid);
  getIsMergeInProgressEvent.addData(command);
  _eventManager.send(getIsMergeInProgressEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressError,
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to get merge progress status."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to get merge progress status."),
                                error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  _mergeInProgress = commandResult.value.toBool();

  const GitRepo::GitHistoryWithStatus &historyAndStatus =
      _historyModel->historyWithStatus();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _commitListModel.reset(
        Models::GitCommitListModel::from(historyAndStatus.status));
    if (_commitListModel) {
      ui->listViewTabWidgetWorkingTreeFlat->setModel(_commitListModel.get());

      disconnect(ui->listViewTabWidgetWorkingTreeFlat,
                 &GuiManager::Widgets::RightClickSelectListView::pressed, this,
                 nullptr);

      connect(ui->listViewTabWidgetWorkingTreeFlat,
              &GuiManager::Widgets::RightClickSelectListView::pressed, this,
              [=](const QModelIndex &index) {
                onListViewTabWidgetFlatSelected(index);
              });
    }

    _commitTreeModel.reset(
        Models::GitCommitTreeModel::from(historyAndStatus.status));
    if (_commitTreeModel) {
      ui->treeViewTabWidgetWorkingTreeTree->setModel(_commitTreeModel.get());

      disconnect(ui->treeViewTabWidgetWorkingTreeTree,
                 &GuiManager::Widgets::RightClickSelectTreeView::pressed, this,
                 nullptr);

      connect(ui->treeViewTabWidgetWorkingTreeTree,
              &GuiManager::Widgets::RightClickSelectTreeView::pressed, this,
              [=](const QModelIndex &index) {
                onTreeViewTabWidgetTreeSelected(index, *_commitTreeModel);
              });
    }

    _commitStagedListModel.reset(
        Models::GitCommitListModel::fromStaged(historyAndStatus.status));
    if (_commitStagedListModel) {
      ui->listViewTabWidgetIndexFlat->setModel(_commitStagedListModel.get());

      disconnect(ui->listViewTabWidgetIndexFlat,
                 &GuiManager::Widgets::RightClickSelectListView::pressed, this,
                 nullptr);

      connect(ui->listViewTabWidgetIndexFlat,
              &GuiManager::Widgets::RightClickSelectListView::pressed, this,
              [=](const QModelIndex &index) {
                onListViewTabWidgetFlatSelected(index);
              });
    }

    _commitStagedTreeModel.reset(
        Models::GitCommitTreeModel::fromStaged(historyAndStatus.status));
    if (_commitStagedTreeModel) {
      ui->treeViewTabWidgetIndexTree->setModel(_commitStagedTreeModel.get());

      disconnect(ui->treeViewTabWidgetIndexTree,
                 &GuiManager::Widgets::RightClickSelectTreeView::pressed, this,
                 nullptr);

      connect(ui->treeViewTabWidgetIndexTree,
              &GuiManager::Widgets::RightClickSelectTreeView::pressed, this,
              [=](const QModelIndex &index) {
                onTreeViewTabWidgetTreeSelected(index, *_commitStagedTreeModel);
              });
    }

    resetCommitButton();
    resetCommitAndMergeIndexTreeUnstageAllButton();
    resetCommitAndMergeWorkingTreeTreeStageAllButton();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showErrorPage(const bool error) const {
  LOG_PERF(0, "Enter");

  ui->stackedWidget1->setCurrentIndex(error ? 2 : 1);

  ui->spinner->stop();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showMainSpinnerPage(const bool spinnerEnabled) const {
  LOG_PERF(0, "Enter");

  ui->stackedWidget1->setCurrentIndex(spinnerEnabled ? 0 : 1);

  if (spinnerEnabled) {
    ui->spinner->start();
  } else {
    ui->spinner->stop();
    ui->labelProgress->setText("");
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showSpinnerOverlay(bool spinnerEnabled) const {
  LOG_PERF(0, "Enter");

  if (spinnerEnabled) {
    hideAllWidgets();
    _spinnerOverlay->startSpinning();
    _spinnerOverlay->setText("");
    _spinnerOverlay->raise();
    _spinnerOverlay->show();
  } else {
    stopOverlayTimer();
    _spinnerOverlay->hide();
    _spinnerOverlay->stopSpinning();
    _spinnerOverlay->setText("");
    showAllWidgets();
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showToast(const QString &message, const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new InfoToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showErrorToast(const QString &message,
                                      const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new ErrorToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::simulateClickOnHistoryRow(int row) const {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->historyView->model()->index(row, 0);

  if (!index.isValid()) {
    LOG_PERF(0, "Exit")
    return;
  }

  ui->historyView->selectRow(index.row());
  ui->historyView->setCurrentIndex(index);
  ui->historyView->scrollTo(index, QAbstractItemView::PositionAtCenter);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::simulateClickOnFirstHistoryRow() const {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->historyView->model()->index(0, 0);

  ui->historyView->selectRow(0);
  ui->historyView->setCurrentIndex(index);
  ui->historyView->scrollToTop();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::stageAll(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus &historyAndStatus =
      _historyModel->historyWithStatus();

  const QStringList modifiedFiles = historyAndStatus.status.newFiles +
                                    historyAndStatus.status.modifiedFiles +
                                    historyAndStatus.status.deletedFiles +
                                    historyAndStatus.status.renamedFiles;

  if (!modifiedFiles.isEmpty()) {
    stageFiles(guard, modifiedFiles);
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::stageFiles(const WeakPtr &guard,
                                  const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    showSpinnerOverlay(false);
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoStageFilesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event stageFilesEvent(
      EventManager::EventType::GitRepoManagerStageFiles, eventUuid);
  stageFilesEvent.addData(command);
  _eventManager.send(stageFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerStageFilesError,
      EventManager::EventType::GitRepoManagerStageFilesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to stage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerStageFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to stage files."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool stageAllSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!stageAllSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to stage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
    resetCommitAndMergeWorkingTreeTreeStageAllButton();
  });

  LOG_PERF(0, "Enter");
}

void GitRepoContainer::unstageAll(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus &historyAndStatus =
      _historyModel->historyWithStatus();

  const QStringList stagedFiles = historyAndStatus.status.newStagedFiles +
                                  historyAndStatus.status.modifiedStagedFiles +
                                  historyAndStatus.status.deletedStagedFiles +
                                  historyAndStatus.status.renamedStagedFiles;

  if (!stagedFiles.isEmpty()) {
    unstageFiles(guard, stagedFiles);
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::unstageFiles(const WeakPtr &guard,
                                    const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    showSpinnerOverlay(false);
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoUnstageFilesCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event unstageFilesEvent(
      EventManager::EventType::GitRepoManagerUnstageFiles, eventUuid);
  unstageFilesEvent.addData(command);
  _eventManager.send(unstageFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerUnstageFilesError,
      EventManager::EventType::GitRepoManagerUnstageFilesResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to unstage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerUnstageFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(
          QString("%1\n%2").arg(tr("Failed to unstage files."), error.message),
          tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool unstageAllSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!unstageAllSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to unstage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
    resetCommitAndMergeIndexTreeUnstageAllButton();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateBranchHistory(
    const WeakPtr &guard,
    const GitRepo::GitHistoryWithStatus &historyWithStatus) {
  LOG_PERF(0, "Enter");

  _historyModel->updateHistoryAndStatus(
      historyWithStatus, [=](const int progress) {
        Util::Dispatcher::dispatchToMainThread(guard, [=]() {
          const QString progressStr = QString("%1%").arg(progress);
          if (_spinnerOverlay) {
            _spinnerOverlay->setText(progressStr);
          }
        });
      });

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    ui->historyView->setVisible(true);
    showSpinnerOverlay(false);
    stretchBranchHistoryTableColumns();
    simulateClickOnFirstHistoryRow();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModel(
    const WeakPtr &guard, const QStringList &localBranches) const {
  LOG_PERF(0, "Enter");

  QStringList finalBranches;

  if (!localBranches.isEmpty()) {
    finalBranches += ALL_BRANCHES;
  }

  finalBranches += localBranches;

  if (const auto locked = guard.lock()) {
    ui->localPage->updateModel(finalBranches);
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModelActiveBranch(
    const WeakPtr &guard, const QString &activeBranch, const bool updatePages) {
  LOG_PERF(0, "Enter");

  if (activeBranch.isEmpty()) {
    LOG_PERF(0, "Exit")
    return;
  }

  _activeBranch = activeBranch;
  _activeBranchType = GitRepo::BranchType::Local;

  if (const auto locked = guard.lock(); locked && updatePages) {
    ui->localPage->updateActiveBranch(activeBranch);
    if (_activeBranch != ALL_BRANCHES) {
      ui->remotePage->updateActiveBranch(
          QString());                           // deselect remote branch if any
      ui->tagsPage->updateActiveTag(QString()); // deselect remote branch if any
    } else {
      ui->remotePage->clearCached();
      ui->tagsPage->clearCached();
    }
  }

  _dispatcher.dispatch([=]() { (void)getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModel(
    const WeakPtr &guard, const QStringList &remoteBranches) const {
  LOG_PERF(0, "Enter");

  QStringList finalBranches;

  if (!remoteBranches.isEmpty()) {
    finalBranches += ALL_BRANCHES;
  }

  finalBranches += remoteBranches;

  if (const auto locked = guard.lock()) {
    ui->remotePage->updateModel(finalBranches);
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModelActiveBranch(
    const WeakPtr &guard, const QString &activeBranch, const bool updatePages) {
  LOG_PERF(0, "Enter");

  if (activeBranch.isEmpty()) {
    LOG_PERF(0, "Exit")
    return;
  }

  _activeBranch = activeBranch;
  _activeBranchType = GitRepo::BranchType::Remote;

  if (const auto locked = guard.lock(); locked && updatePages) {
    ui->remotePage->updateActiveBranch(activeBranch);
    if (_activeBranch != ALL_BRANCHES) {
      ui->localPage->updateActiveBranch(
          QString());                           // deselect remote branch if any
      ui->tagsPage->updateActiveTag(QString()); // deselect remote branch if any
    } else {
      ui->localPage->clearCached();
      ui->tagsPage->clearCached();
    }
  }

  _dispatcher.dispatch([=]() { (void)getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateStashesModel(const WeakPtr &guard,
                                          const QStringList &stashes) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->stashesPage->updateModel(stashes);
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateTagsModelActiveTag(const WeakPtr &guard,
                                                const QString &activeTag,
                                                const bool updatePages) {
  LOG_PERF(0, "Enter");

  if (activeTag.isEmpty()) {
    LOG_PERF(0, "Exit")
    return;
  }

  _activeBranch = activeTag;
  _activeBranchType = GitRepo::BranchType::Tag;

  if (const auto locked = guard.lock(); locked && updatePages) {
    ui->localPage->clearCached();
    ui->localPage->updateActiveBranch(
        QString()); // deselect local branch if any
    ui->remotePage->clearCached();
    ui->remotePage->updateActiveBranch(QString());
    ui->tagsPage->updateActiveTag(activeTag); // deselect local branch if any
  }

  _dispatcher.dispatch([=]() { (void)getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateSubmodule(const WeakPtr &guard,
                                       const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoSubmoduleUpdateCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event submoduleUpdateEvent(
      EventManager::EventType::GitRepoManagerSubmoduleUpdate, eventUuid);
  submoduleUpdateEvent.addData(command);
  _eventManager.send(submoduleUpdateEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerSubmoduleUpdateError,
      EventManager::EventType::GitRepoManagerSubmoduleUpdateResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorPage();
      showErrorToast(tr("Failed to update submodule."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerSubmoduleUpdateResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorToast(QString("%1\n%2").arg(tr("Failed to update submodule."),
                                           error.message),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool commandSuccess = commandResult.value.toBool();
      !commandSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showSpinnerOverlay(false);
      showErrorToast(tr("Failed to update submodule."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    showSpinnerOverlay(false);
    showToast(tr("Submodule updated successfully."), tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateSubmodulesModel(const WeakPtr &guard,
                                             const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->submodulesPage->updateModel(tags);
  }

  LOG_PERF(0, "Exit")
}

qint64 GitRepoContainer::getFileSizeFromCommit(const QString &filePath,
                                               const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _eventManager.startWait();

  GitRepoGetFileSizeFromCommitCommand command;
  command.uuid = _uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;
  command.commit = commit;

  EventManager::Event getFileSizeEvent(
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommit, eventUuid);
  getFileSizeEvent.addData(command);
  _eventManager.send(getFileSizeEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommitError,
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommitResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success ||
      event.type() !=
          EventManager::EventType::GitRepoManagerGetFileSizeFromCommitResult) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto fileSizeResult =
      commandResult.value.value<GitRepoGetFileSizeFromCommitCommandResult>();

  LOG_PERF(0, "Exit");
  return fileSizeResult.fileSize;
}

void GitRepoContainer::updateTagsModel(const WeakPtr &guard,
                                       const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->tagsPage->updateModel(tags);
  }

  LOG_PERF(0, "Exit")
}
} // namespace GuiManager::Containers
