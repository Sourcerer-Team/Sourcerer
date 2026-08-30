#include "containers/RepoContainer.h"
#include "DarkThemeLexersHighlighter.h"
#include "MainWindow.h"
#include "MousePositionCorrection.h"
#include "QScintillaLexerFactory.h"
#include "containers/GitRepoManagerBridgeImpl.h"
#include "containers/ui_RepoContainer.h"
#include "delegates/ElidePartialColorDelegate.h"
#include "delegates/HistorySliceTableColumnDelegate.h"
#include "delegates/HistorySummaryTableColumnDelegate.h"
#include "models/GitCommitListModel.h"
#include "models/GitCommitTreeModel.h"
#include "widgets/AddRemoteOverlayWidget.h"
#include "widgets/AddSubmoduleOverlayWidget.h"
#include "widgets/AddTargetOverlayWidget.h"
#include "widgets/AnnotatedTagOverlayWidget.h"
#include "widgets/ClickableHeaderView.h"
#include "widgets/ConsoleWidget.h"
#include "widgets/EditRemoteOverlayWidget.h"
#include "widgets/EditTargetOverlayWidget.h"
#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"
#include "widgets/RemotesPage.h"
#include "widgets/ShortcutsToolbarWidget.h"
#include "widgets/SideBySideDiffView.h"
#include "widgets/StashesPage.h"
#include "widgets/TextInputOverlayWidget.h"
#include "widgets/ToastHostWidget.h"
#include "widgets/UpstreamInputOverlayWidget.h"

#include <Dispatcher.h>
#include <FileManager.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <StylesheetManager.h>
#include <Uuid.h>

#include <QAbstractItemDelegate>
#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QSize>
#include <QStyleOptionViewItem>
#include <QSvgRenderer>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <Qsci/qsciscintilla.h>

#include <algorithm>
#include <functional>
#include <utility>

#define THREAD_ID "RepoContainer"

#define ALL_BRANCHES "*"
#define HEAD "HEAD"

#define MAXIMUM_PARENTS_IN_LABEL 3
#define MAX_COMMIT_SHA_LENGTH 7

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
  StashContent = 3,

  last
};

constexpr QSize kCommitSignatureIconSize(16, 16);

QPixmap themedIcon(const QString &resourcePath) {
  return StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(resourcePath),
      StylesheetManager::StylesheetManager::menuBarButtonIconColor());
}

void setIconLabel(QLabel *label, const QPixmap &icon,
                  const QString &accessibleText) {
  if (!label)
    return;

  label->setPixmap(icon.scaled(kCommitSignatureIconSize, Qt::KeepAspectRatio,
                               Qt::SmoothTransformation));
  label->setText(QString());
  label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  label->setToolTip(accessibleText);
  label->setStatusTip(accessibleText);
  label->setAccessibleName(accessibleText);
}

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

RepoContainer::RepoContainer(QString path, QWidget *parent)
    : ContainerWidget(Widgets::ContainerType::GitRepo, parent),
      ui(new Ui::RepoContainer), _uuid(Uuid::createUuid()),
      _path(std::move(path)), _lastDiffRepoStatusChanged(false),
      _activeBranchType(GitRepo::BranchType::All),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _bridge(std::make_unique<GitRepoManagerBridgeImpl>(*this)),
      _userProfileManager(Common::UserProfileManager::instance()),
      _toastHost(ToastHostWidget::getInstance()),
      _historyModel(new Models::GitHistoryModel()), _dispatcher(1),
      _mousePos(-1, -1), _spinnerOverlay(new SpinnerOverlayWidget(this)),
      _spinnerOverlayTimer(new QTimer(this)), _quitGettingHistory(false),
      _resizedHeaderSection(-1), _resizedHeaderSectionPreviousWidth(-1),
      _applyingHistoryColumnWidths(false), _displayTreeExpanded(false),
      _mergeInProgress(false), _commitListModel(nullptr),
      _commitTreeModel(nullptr), _repoDirWatcher(nullptr),
      _repoDirWatcherTimer(new QTimer(this)),
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
  ui->historyView->setAttribute(Qt::WA_Hover, true);
  ui->historyView->viewport()->setAttribute(Qt::WA_Hover, true);

  ui->historyView->setModel(_historyModel.get());
  ui->historyView->setSelectionMode(QAbstractItemView::SingleSelection);

  connect(ui->toolButtonPageDisplaySignature, &QToolButton::toggled, this,
          [this](bool checked) {
            ui->widgetPageDisplaySignatureContent->setVisible(checked);
            ui->toolButtonPageDisplaySignature->setArrowType(
                checked ? Qt::DownArrow : Qt::RightArrow);
          });
  connect(ui->pushButtonPageDisplaySignatureCopy, &QPushButton::clicked, this,
          [this]() {
            QGuiApplication::clipboard()->setText(_currentSignature);
            const qlonglong tooltipDisplayTime = _configManager.getLong(
                ConfigManager::ConfigKey::TooltipDisplayTime);
            QToolTip::showText(
                ui->pushButtonPageDisplaySignatureCopy->mapToGlobal(QPoint(
                    0, ui->pushButtonPageDisplaySignatureCopy->height())),
                tr("Signature copied to clipboard."),
                ui->pushButtonPageDisplaySignatureCopy,
                ui->pushButtonPageDisplaySignatureCopy->rect(),
                tooltipDisplayTime);
          });

  ui->historyView->setVisible(false);
  ui->spinner->setColor(Qt::lightGray);
  _spinnerOverlay->setParent(ui->pageMain);

  auto *header = new ClickableHeaderView(Qt::Horizontal, ui->historyView);
  ui->historyView->setHorizontalHeader(header);
  ui->historyView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Interactive);

  ui->historyView->horizontalHeader()->setContextMenuPolicy(
      Qt::CustomContextMenu);
  connect(ui->historyView->horizontalHeader(),
          &QWidget::customContextMenuRequested, this,
          &RepoContainer::onHistoryHeaderContextMenuRequested);
  connect(header, &QHeaderView::sectionResized, this,
          &RepoContainer::onHistoryHeaderSectionResized);

  applyHistoryColumnVisibility();

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

  QWidget *commitButtonContainer = new QWidget(ui->tabWidgetCommit);
  QHBoxLayout *commitButtonLayout = new QHBoxLayout(commitButtonContainer);
  commitButtonLayout->setContentsMargins(0, 0, 0, 0);
  commitButtonLayout->setSpacing(8);

  _commitAmendSignButton = new QToolButton(commitButtonContainer);
  // Make some space between label text and dropdown arrow
  _commitAmendSignButton->setText(tr("Amend and Sign") + QString(" "));
  _commitAmendSignButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  _commitAmendSignButton->setPopupMode(QToolButton::InstantPopup);

  auto *commitAmendSignMenu = new QMenu(_commitAmendSignButton);
  _actionCommitAmend =
      commitAmendSignMenu->addAction(tr("Amend previous commit"));
  _actionCommitAmend->setCheckable(true);
  _actionCommitSignOff = commitAmendSignMenu->addAction(tr("Sign-off commit"));
  _actionCommitSignOff->setCheckable(true);
  _actionCommitSign = commitAmendSignMenu->addAction(tr("Sign commit"));
  _actionCommitSign->setCheckable(true);
  _commitAmendSignButton->setMenu(commitAmendSignMenu);

  commitButtonLayout->addWidget(_commitAmendSignButton);
  commitButtonLayout->addStretch();

  ui->tabWidgetCommit->setCornerWidget(commitButtonContainer,
                                       Qt::TopRightCorner);
  ui->tabWidgetCommit->tabBar()->setObjectName("tabWidgetCommitTabBar");
  refreshSigningControls();
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

  ui->listViewStashFilesFlat->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->treeViewStashFilesTree->setContextMenuPolicy(Qt::CustomContextMenu);

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

  ui->listViewPageDisplayFilesFlat->setContextMenuPolicy(Qt::CustomContextMenu);
  ui->treeViewPageDisplayFilesTree->setContextMenuPolicy(Qt::CustomContextMenu);

  connect(ui->listViewPageDisplayFilesFlat,
          &QListView::customContextMenuRequested, this,
          &RepoContainer::onPageDisplayFilesFlatListViewContextMenuRequested);
  connect(ui->treeViewPageDisplayFilesTree,
          &QTreeView::customContextMenuRequested, this,
          &RepoContainer::onPageDisplayFilesTreeViewContextMenuRequested);

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

  const QColor shortcutIconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();

  // Create dropdown button for Pull/Fetch All
  auto *pullFetchDropdown = new QToolButton();
  pullFetchDropdown->setObjectName("pullFetchDropdown");
  pullFetchDropdown->setProperty("iconSource", QStringLiteral(":icons/pull"));
  pullFetchDropdown->setIcon(
      StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":icons/pull"), shortcutIconColor));
  pullFetchDropdown->setToolButtonStyle(Qt::ToolButtonIconOnly);
  pullFetchDropdown->setIconSize(QSize(18, 18));
  pullFetchDropdown->setAttribute(Qt::WA_StyledBackground, true);
  pullFetchDropdown->setToolTip(tr("Pull"));
  pullFetchDropdown->setMinimumSize(42, 42);
  pullFetchDropdown->setMaximumSize(42, 42);
  pullFetchDropdown->setPopupMode(QToolButton::MenuButtonPopup);

  auto *pullFetchMenu = new QMenu();
  auto *actionPullMenu = pullFetchMenu->addAction(tr("Pull"));
  actionPullMenu->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":icons/pull"), shortcutIconColor));
  auto *actionPullWithRebaseMenu =
      pullFetchMenu->addAction(tr("Pull with rebase"));
  actionPullWithRebaseMenu->setIcon(
      StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":icons/pull"), shortcutIconColor));
  auto *actionFetchAllMenu = pullFetchMenu->addAction(tr("Fetch All"));
  actionFetchAllMenu->setIcon(
      QIcon(StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":icons/pull"), shortcutIconColor)));

  pullFetchDropdown->setMenu(pullFetchMenu);

  auto *pushButtonPull = pullFetchDropdown;
  auto *pushButtonFetchAll = pullFetchDropdown;

  // Create dropdown button for Push/Force Push
  auto *pushButtonPush = new QToolButton();
  pushButtonPush->setObjectName("pushButtonPush");
  pushButtonPush->setProperty("iconSource", QStringLiteral(":icons/push"));
  pushButtonPush->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":icons/push"), shortcutIconColor));
  pushButtonPush->setToolButtonStyle(Qt::ToolButtonIconOnly);
  pushButtonPush->setIconSize(QSize(18, 18));
  pushButtonPush->setAttribute(Qt::WA_StyledBackground, true);
  pushButtonPush->setToolTip(tr("Push"));
  pushButtonPush->setMinimumSize(42, 42);
  pushButtonPush->setMaximumSize(42, 42);
  pushButtonPush->setPopupMode(QToolButton::MenuButtonPopup);

  auto *pushMenu = new QMenu();
  auto *actionPushMenu = pushMenu->addAction(tr("Push"));
  actionPushMenu->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":icons/push"), shortcutIconColor));
  auto *actionForcePushMenu = pushMenu->addAction(tr("Force push"));
  actionForcePushMenu->setIcon(
      StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":icons/push"), shortcutIconColor));
  pushButtonPush->setMenu(pushMenu);

  auto *pushButtonPop = new QPushButton();
  pushButtonPop->setObjectName("pushButtonPop");
  pushButtonPop->setProperty("iconSource", QStringLiteral(":icons/pop"));
  pushButtonPop->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":icons/pop"), shortcutIconColor));
  pushButtonPop->setToolTip(tr("Pop"));
  pushButtonPop->setMinimumSize(42, 42);
  pushButtonPop->setMaximumSize(42, 42);
  pushButtonPop->setAttribute(Qt::WA_StyledBackground, true);

  auto *pushButtonStash = new QPushButton();
  pushButtonStash->setObjectName("pushButtonStash");
  pushButtonStash->setProperty("iconSource", QStringLiteral(":icons/stash"));
  pushButtonStash->setIcon(StylesheetManager::StylesheetManager::recolorPixmap(
      QPixmap(":icons/stash"), shortcutIconColor));
  pushButtonStash->setToolTip(tr("Stash"));
  pushButtonStash->setMinimumSize(42, 42);
  pushButtonStash->setMaximumSize(42, 42);
  pushButtonStash->setAttribute(Qt::WA_StyledBackground, true);

  _shortcutsWidget->addShortcutWidget(pullFetchDropdown);
  _shortcutsWidget->addShortcutWidget(pushButtonPush);
  _shortcutsWidget->addShortcutWidget(pushButtonStash);
  _shortcutsWidget->addShortcutWidget(pushButtonPop);

  // Explicitly show buttons to ensure they're interactive
  pullFetchDropdown->show();
  pushButtonPush->show();
  pushButtonStash->show();
  pushButtonPop->show();

  ui->leftVerticalLayout->insertWidget(0, _shortcutsWidget);

  const QColor closeButtonIconColor = QColor(Qt::white);
  ui->pushButtonDiffClose->setIcon(
      StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":/icons/close_button.png"), closeButtonIconColor));
  ui->pushButtonDiffClose->setMinimumSize(32, 32);
  ui->pushButtonDiffClose->setMaximumSize(32, 32);

  ui->pushButtonDiffUnified->setIcon(QPixmap(":/icons/diff-unified.png"));
  ui->pushButtonDiffUnified->setMinimumSize(32, 32);
  ui->pushButtonDiffUnified->setMaximumSize(32, 32);

  ui->pushButtonDiffSideBySide->setIcon(
      QPixmap(":/icons/diff-side-by-side.png"));
  ui->pushButtonDiffSideBySide->setMinimumSize(32, 32);
  ui->pushButtonDiffSideBySide->setMaximumSize(32, 32);

  // Load the saved diff view preference from ConfigManager
  // false = unified (default), true = side-by-side
  const bool useSideBySideDiff =
      _configManager.getBoolean(ConfigManager::ConfigKey::GitRepoDiffViewType);
  ui->pushButtonDiffSideBySide->setChecked(useSideBySideDiff);
  ui->pushButtonDiffUnified->setChecked(!useSideBySideDiff);

  ui->stackedWidgetDiff->setCurrentIndex(
      static_cast<int>(useSideBySideDiff ? DiffStackIndexes::DiffSideBySide
                                         : DiffStackIndexes::DiffUnified));

  ui->pushButtonMergeDiffClose->setIcon(
      StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":/icons/close_button.png"), closeButtonIconColor));

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

  ui->pushButtonStashClose->setIcon(
      StylesheetManager::StylesheetManager::recolorPixmap(
          QPixmap(":/icons/close_button.png"), closeButtonIconColor));

  ui->pushButtonStashClose->setMinimumSize(32, 32);
  ui->pushButtonStashClose->setMaximumSize(32, 32);

  ui->pushButtonStashDiffUnified->setIcon(QPixmap(":/icons/diff-unified.png"));
  ui->pushButtonStashDiffUnified->setMinimumSize(32, 32);
  ui->pushButtonStashDiffUnified->setMaximumSize(32, 32);

  ui->pushButtonStashDiffSideBySide->setIcon(
      QPixmap(":/icons/diff-side-by-side.png"));
  ui->pushButtonStashDiffSideBySide->setMinimumSize(32, 32);
  ui->pushButtonStashDiffSideBySide->setMaximumSize(32, 32);

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
  const bool lightTheme =
      StylesheetManager::StylesheetManager::currentStylesheet() ==
      StylesheetManager::StylesheetKey::SourcererLight;
  const QColor hunkMarkerColor =
      lightTheme ? QColor(255, 241, 201) : QColor(Qt::darkYellow);
  ui->scintillaTextEditorUnified->setMarkerBackgroundColor(hunkMarkerColor, 1);
  ui->scintillaTextEditorUnified->setReadOnly(true);

  DarkThemeLexersHighlighter highlighter;
  highlighter.applyDarkTheme(ui->scintillaTextEditorUnified);

  ui->scintillaTextEditorUnified->setFolding(QsciScintilla::NoFoldStyle);

  // Setup stash unified diff viewer with same styling
  QsciLexer *lexerStashDiffUnified =
      factory.createLexerByName("Diff", ui->scintillaStashDiffUnified);

  ui->scintillaStashDiffUnified->setSizePolicy(QSizePolicy::Expanding,
                                               QSizePolicy::Expanding);
  ui->scintillaStashDiffUnified->setWrapMode(QsciScintilla::WrapWord);
  ui->scintillaStashDiffUnified->setLexer(lexerStashDiffUnified);
  ui->scintillaStashDiffUnified->setMarginsBackgroundColor(QColor(0x384D59));
  ui->scintillaStashDiffUnified->setMarginType(0, QsciScintilla::NumberMargin);
  ui->scintillaStashDiffUnified->setMarginType(1, QsciScintilla::SymbolMargin);
  ui->scintillaStashDiffUnified->setMarginWidth(1, 2);
  ui->scintillaStashDiffUnified->markerDefine(QsciScintilla::Background, 1);
  ui->scintillaStashDiffUnified->setMarkerBackgroundColor(hunkMarkerColor, 1);
  ui->scintillaStashDiffUnified->setReadOnly(true);

  DarkThemeLexersHighlighter stashHighlighter;
  stashHighlighter.applyDarkTheme(ui->scintillaStashDiffUnified);

  ui->scintillaStashDiffUnified->setFolding(QsciScintilla::NoFoldStyle);

  // Setup stash apply and pop buttons as corner widgets
  _stashApplyButton = new QPushButton(tr("Apply"), ui->tabWidgetStashFiles);
  _stashApplyButton->setObjectName("stashApplyButton");
  _stashApplyButton->setFlat(false);
  _stashApplyButton->setVisible(true);
  _stashApplyButton->setFixedWidth(65);

  _stashPopButton = new QPushButton(tr("Pop"), ui->tabWidgetStashFiles);
  _stashPopButton->setObjectName("stashPopButton");
  _stashPopButton->setFlat(false);
  _stashPopButton->setVisible(true);
  _stashPopButton->setFixedWidth(65);

  // Create a container widget for the two buttons
  QWidget *stashButtonContainer = new QWidget(ui->tabWidgetStashFiles);
  QHBoxLayout *stashButtonLayout = new QHBoxLayout(stashButtonContainer);
  stashButtonLayout->setContentsMargins(0, 0, 0, 0);
  stashButtonLayout->setSpacing(5);
  stashButtonLayout->addWidget(_stashApplyButton);
  stashButtonLayout->addWidget(_stashPopButton);

  // Remove close button from toolbar layout and add it to button container
  ui->horizontalLayoutStashContentButtons->removeWidget(
      ui->pushButtonStashClose);
  stashButtonLayout->addSpacing(2);
  stashButtonLayout->addWidget(ui->pushButtonStashClose);

  ui->tabWidgetStashFiles->setCornerWidget(stashButtonContainer,
                                           Qt::TopRightCorner);

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
          &RepoContainer::onHistoryViewItemEntered);

  connect(ui->splitter, &QSplitter::splitterMoved, this,
          &RepoContainer::onSplitterMoved);

  connect(ui->localPage, &LocalGitBranchesPage::branchSelected, this,
          &RepoContainer::onLocalBranchSelected);
  connect(ui->localPage, &LocalGitBranchesPage::branchRightClicked, this,
          &RepoContainer::onLocalBranchRightClicked);
  connect(ui->localPage, &LocalGitBranchesPage::branchesMoved, this,
          &RepoContainer::onLocalBranchMoved);

  connect(ui->remotePage, &RemoteGitBranchesPage::branchSelected, this,
          &RepoContainer::onRemoteBranchSelected);
  connect(ui->remotePage, &RemoteGitBranchesPage::branchRightClicked, this,
          &RepoContainer::onRemoteBranchRightClicked);
  connect(ui->remotesPage, &Widgets::RemotesPage::fetchRemote, this,
          &RepoContainer::onFetchRemote);

  connect(ui->tagsPage, &GitTagsPage::tagSelected, this,
          &RepoContainer::onTagSelected);
  connect(ui->tagsPage, &GitTagsPage::tagRightClicked, this,
          &RepoContainer::onTagRightClicked);
  connect(ui->tagsPage, &GitTagsPage::emptyAreaRightClicked, this,
          &RepoContainer::onTagsEmptyAreaRightClicked);

  connect(ui->stashesPage, &StashesPage::stashSelected, this,
          &RepoContainer::onStashSelected);
  connect(ui->stashesPage, &StashesPage::applyStash, this,
          &RepoContainer::onApplyStash);
  connect(ui->stashesPage, &StashesPage::popStash, this,
          &RepoContainer::onPopStash);
  connect(ui->stashesPage, &StashesPage::deleteStash, this,
          &RepoContainer::onDeleteStash);
  connect(ui->stashesPage, &StashesPage::clearAllStashes, this,
          &RepoContainer::onClearAllStashes);

  connect(ui->remotesPage, &RemotesPage::addRemote, this,
          &RepoContainer::onAddRemote);
  connect(ui->remotesPage, &RemotesPage::editRemote, this,
          &RepoContainer::onEditRemote);
  connect(ui->remotesPage, &RemotesPage::deleteRemote, this,
          &RepoContainer::onDeleteRemote);
  connect(ui->remotesPage, &RemotesPage::setDefaultRemote, this,
          &RepoContainer::onSetDefaultRemote);
  connect(ui->remotesPage, &RemotesPage::addTarget, this,
          &RepoContainer::onAddTarget);
  connect(ui->remotesPage, &RemotesPage::editTarget, this,
          &RepoContainer::onEditTarget);
  connect(ui->remotesPage, &RemotesPage::deleteTarget, this,
          &RepoContainer::onDeleteTarget);
  connect(ui->remotesPage, &RemotesPage::setDefaultTarget, this,
          &RepoContainer::onSetDefaultTarget);

  connect(ui->submodulesPage, &GitSubmodulesPage::rightClicked, this,
          &RepoContainer::onSubmodulePanelRightClicked);
  connect(ui->submodulesPage, &GitSubmodulesPage::submoduleSelected, this,
          &RepoContainer::onSubmoduleSelected);
  connect(ui->submodulesPage, &GitSubmodulesPage::submoduleRightClicked, this,
          &RepoContainer::onSubmoduleRightClicked);

  connect(_spinnerOverlayTimer, &QTimer::timeout, this,
          &RepoContainer::onShowSpinnerOverlay);

  connect(header, &ClickableHeaderView::headerDoubleClicked, this,
          &RepoContainer::onGraphColumnExpand);

  connect(ui->historyView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &RepoContainer::onHistorySelectionChanged);

  connect(ui->tabWidgetPageDisplayFiles, &QTabWidget::currentChanged, this,
          [=](const int index) {
            _displayTreeExpandAllButton->setVisible(index != 0);
          });

  connect(_displayTreeExpandAllButton, &QPushButton::clicked, this,
          &RepoContainer::onDisplayTreeExpandAllButtonClicked);

  connect(_commitAndMergeWorkingDirTreeStageAllButton, &QPushButton::clicked,
          this,
          &RepoContainer::onCommitAndMergeWorkingTreeTreeStageAllButtonClicked);

  connect(_commitAndMergeIndexTreeUnstageAllButton, &QPushButton::clicked, this,
          &RepoContainer::onCommitAndMergeIndexTreeUnstageAllButtonClicked);

  connect(ui->lineEditSummary, &QLineEdit::textChanged, this,
          [=](const QString &text) { resetCommitButton(); });

  connect(ui->lineEditSummary, &QLineEdit::editingFinished, this,
          [=]() { resetCommitButton(); });

  connect(ui->textEditDescription, &QTextEdit::textChanged, this,
          [=]() { resetCommitButton(); });

  connect(ui->pushButtonAbort, &QPushButton::clicked, this,
          &RepoContainer::onAbortButtonClicked);
  connect(ui->pushButtonCommit, &QPushButton::clicked, this,
          &RepoContainer::onCommitButtonClicked);
  connect(ui->pushButtonMerge, &QPushButton::clicked, this,
          &RepoContainer::onMergeButtonClicked);

  connect(ui->listViewTabWidgetWorkingTreeFlat,
          &QListView::customContextMenuRequested, this,
          &RepoContainer::onWorkingTreeFlatListViewContextMenuRequested);

  connect(ui->actionWorkingTreeFlatStage, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeFlatListViewStageSelected);
  connect(ui->actionWorkingTreeFlatIgnore, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeFlatListViewIgnoreSelected);
  connect(ui->actionWorkingTreeFlatCreateFilePatch, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeFlatListViewCreateFilePatchSelected);
  connect(ui->actionWorkingTreeFlatDelete, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeFlatListViewDeleteSelected);
  connect(ui->actionWorkingTreeFlatDiscard, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeFlatListViewDiscardSelected);
  connect(ui->actionWorkingTreeFlatShowInFileManager, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeFlatListViewShowInFileManagerSelected);

  connect(ui->treeViewTabWidgetWorkingTreeTree,
          &QTreeView::customContextMenuRequested, this,
          &RepoContainer::onWorkingTreeTreeTreeViewContextMenuRequested);

  connect(ui->actionWorkingTreeTreeStage, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeTreeTreeViewStageSelected);
  connect(ui->actionWorkingTreeTreeIgnore, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected);
  connect(ui->actionWorkingTreeTreeCreateFilePatch, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeTreeTreeViewCreateFilePatchSelected);
  connect(ui->actionWorkingTreeTreeDelete, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeTreeTreeViewDeleteSelected);
  connect(ui->actionWorkingTreeTreeDiscard, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeTreeTreeViewDiscardSelected);
  connect(ui->actionWorkingTreeTreeShowInFileManager, &QAction::triggered, this,
          &RepoContainer::onWorkingTreeTreeTreeViewShowInFileManagerSelected);

  connect(ui->listViewTabWidgetIndexFlat,
          &QListView::customContextMenuRequested, this,
          &RepoContainer::onIndexFlatListViewContextMenuRequested);

  connect(ui->actionIndexFlatUnstage, &QAction::triggered, this,
          &RepoContainer::onIndexFlatListViewUnstageSelected);
  connect(ui->actionIndexFlatIgnore, &QAction::triggered, this,
          &RepoContainer::onIndexFlatListViewIgnoreSelected);
  connect(ui->actionIndexFlatCreateFilePatch, &QAction::triggered, this,
          &RepoContainer::onIndexFlatListViewCreateFilePatchSelected);
  connect(ui->actionIndexFlatDelete, &QAction::triggered, this,
          &RepoContainer::onIndexFlatListViewDeleteSelected);
  connect(ui->actionIndexFlatDiscard, &QAction::triggered, this,
          &RepoContainer::onIndexFlatListViewDiscardSelected);
  connect(ui->actionIndexFlatShowInFileManager, &QAction::triggered, this,
          &RepoContainer::onIndexFlatListViewShowInFileManagerSelected);

  connect(ui->treeViewTabWidgetIndexTree,
          &QTreeView::customContextMenuRequested, this,
          &RepoContainer::onIndexTreeTreeViewContextMenuRequested);

  connect(ui->actionIndexTreeUnstage, &QAction::triggered, this,
          &RepoContainer::onIndexTreeTreeViewUnstageSelected);
  connect(ui->actionIndexTreeIgnore, &QAction::triggered, this,
          &RepoContainer::onIndexTreeTreeViewIgnoreSelected);
  connect(ui->actionIndexTreeCreateFilePatch, &QAction::triggered, this,
          &RepoContainer::onIndexTreeTreeViewCreateFilePatchSelected);
  connect(ui->actionIndexTreeDelete, &QAction::triggered, this,
          &RepoContainer::onIndexTreeTreeViewDeleteSelected);
  connect(ui->actionIndexTreeDiscard, &QAction::triggered, this,
          &RepoContainer::onIndexTreeTreeViewDiscardSelected);
  connect(ui->actionIndexTreeShowInFileManager, &QAction::triggered, this,
          &RepoContainer::onIndexTreeTreeViewShowInFileManagerSelected);

  connect(_repoDirWatcherTimer, &QTimer::timeout, this,
          &RepoContainer::onRepoDirChangedTimeout);

  connect(pullFetchDropdown, &QToolButton::clicked, this,
          &RepoContainer::onPull);
  connect(actionPullMenu, &QAction::triggered, this, &RepoContainer::onPull);
  connect(actionPullWithRebaseMenu, &QAction::triggered, this,
          [=](bool) { onPull(true); });
  connect(actionFetchAllMenu, &QAction::triggered, this,
          &RepoContainer::onFetchAll);
  connect(pushButtonPush, &QToolButton::clicked, this, &RepoContainer::onPush);
  connect(actionPushMenu, &QAction::triggered, this, &RepoContainer::onPush);
  connect(actionForcePushMenu, &QAction::triggered, this,
          [=](bool) { onPush(true); });
  connect(pushButtonStash, &QPushButton::clicked, this,
          &RepoContainer::onStash);
  connect(pushButtonPop, &QPushButton::clicked, this, &RepoContainer::onPop);

  connect(this, &RepoContainer::historyViewRightClicked, this,
          &RepoContainer::onHistoryViewRightClicked);

  connect(ui->pushButtonDiffUnified, &QPushButton::clicked, this,
          &RepoContainer::onDiffUnifiedClicked);
  connect(ui->pushButtonDiffSideBySide, &QPushButton::clicked, this,
          &RepoContainer::onDiffSideBySideClicked);
  connect(ui->pushButtonDiffClose, &QPushButton::clicked, this,
          &RepoContainer::onDiffCloseClicked);

  connect(ui->pushButtonMergeDiffDown, &QPushButton::clicked, this,
          &RepoContainer::onMergeDiffDownClicked);
  connect(ui->pushButtonMergeDiffUp, &QPushButton::clicked, this,
          &RepoContainer::onMergeDiffUpClicked);
  connect(ui->pushButtonMergeDiffSave, &QPushButton::clicked, this,
          &RepoContainer::onMergeDiffSaveClicked);
  connect(ui->pushButtonMergeDiffClose, &QPushButton::clicked, this,
          &RepoContainer::onMergeDiffCloseClicked);

  connect(_stashApplyButton, &QPushButton::clicked, this,
          &RepoContainer::onStashApplyClicked);
  connect(_stashPopButton, &QPushButton::clicked, this,
          &RepoContainer::onStashPopClicked);
  connect(ui->pushButtonStashClose, &QPushButton::clicked, this,
          &RepoContainer::onStashContentCloseClicked);
  connect(ui->pushButtonStashDiffUnified, &QPushButton::clicked, this,
          &RepoContainer::onStashDiffUnifiedClicked);
  connect(ui->pushButtonStashDiffSideBySide, &QPushButton::clicked, this,
          &RepoContainer::onStashDiffSideBySideClicked);

  qApp->setStyleSheet(qApp->styleSheet());

  LOG_PERF(0, "Exit")
}

RepoContainer::~RepoContainer() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Destroying RepoContainer for path: '{}'", _path.toStdString());

  // Also quit GitRepo jobs
  _bridge->closeRepo();
  LOG_DEBUG("Repository closed");

  _historyModel->quitUpdate();
  LOG_DEBUG("History model quit signal sent");

  delete _errorPixmap;
  delete _repoDirWatcher;
  delete _repoDirWatcherTimer;
  LOG_DEBUG("UI resources cleaned up");

  LOG_INFO("RepoContainer destroyed for path: '{}'", _path.toStdString());
  LOG_PERF(0, "Exit");
}

void RepoContainer::init() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Initializing RepoContainer for repository: '{}'",
            _path.toStdString());

  ContainerWidget::init();
  LOG_DEBUG("Container widget initialized");

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Dispatching openRepo and prepareWatcher tasks");
    if (_bridge->openRepo(guard)) {
      prepareWatcher(guard);
    }
  });

  LOG_INFO("RepoContainer initialization completed for: '{}'",
           _path.toStdString());
  LOG_PERF(0, "Exit");
}

bool RepoContainer::eventFilter(QObject *watched, QEvent *event) {
  if (watched == ui->historyView->viewport()) {
    if (event->type() == QEvent::MouseMove) {
      const QPoint globalPos =
          MousePositionCorrection::instance().correctedGlobalPosition(
              static_cast<QMouseEvent *>(event));
      const QModelIndex index = ui->historyView->indexAt(
          ui->historyView->viewport()->mapFromGlobal(globalPos));
      if (index.isValid() && index.column() == COLUMN_HISTORY_SLICE) {
        const auto guard = getSelf().toWeakRef();
        const QModelIndex fullShaIndex = index.model()->index(
            index.row(), COLUMN_COMMIT_FULL_SHA, index.parent());
        const QString commitOid = fullShaIndex.data(Qt::DisplayRole).toString();
        QHash<QString, QStringList> commitRefs;
        QStringList branches = _bridge->getCommitBranches(guard, commitOid);
        branches += _bridge->getCommitTags(guard, commitOid);
        if (!branches.isEmpty()) {
          commitRefs.insert(commitOid, branches);
        }

        _historyModel->updateCommitRefs(commitRefs);
        _historyModel->dataChanged(index, index);

        const QString tooltipText = branches.join(", ");
        const qlonglong tooltipDisplayTime = _configManager.getLong(
            ConfigManager::ConfigKey::TooltipDisplayTime);

        _mousePos = globalPos;
        QToolTip::showText(_mousePos, tooltipText, ui->historyView, {},
                           tooltipDisplayTime);
      }
      if (index.isValid() && index.column() != COLUMN_HISTORY_SLICE) {
        const qlonglong tooltipDisplayTime = _configManager.getLong(
            ConfigManager::ConfigKey::TooltipDisplayTime);

        _mousePos = globalPos;
        QToolTip::showText(_mousePos, index.data(Qt::DisplayRole).toString(),
                           ui->historyView, {}, tooltipDisplayTime);
      } else {
        QToolTip::hideText();
      }
    } else if (event->type() == QEvent::MouseButtonPress) {
      if (const auto mouseEvent = static_cast<QMouseEvent *>(event);
          mouseEvent->button() == Qt::RightButton) {
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        if (const QModelIndex index = ui->historyView->indexAt(
                ui->historyView->viewport()->mapFromGlobal(globalPos));
            index.isValid()) {
          emit historyViewRightClicked(index, globalPos);
        }
      }
    }
  }

  return QObject::eventFilter(watched, event);
}

void RepoContainer::deselected() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Container deselected: '{}'", _path.toStdString());

  _selected = false;

  LOG_DEBUG("Container marked as not selected");
  LOG_PERF(0, "Exit");
}

void RepoContainer::selected() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Container selected: '{}'", _path.toStdString());

  refreshSigningControls();

  _selected = true;
  LOG_DEBUG("Container marked as selected");

  const qlonglong titleBarHeight =
      _configManager.getLong(ConfigManager::ConfigKey::TitleBarHeight);
  LOG_DEBUG("Title bar height retrieved: {} px", titleBarHeight);

  _toastHost.setParent(this);
  _toastHost.setYOffset(titleBarHeight);
  _toastHost.raise();
  LOG_DEBUG("Toast host configured and raised");

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
    LOG_ERROR(Log::ErrorId::UI, "Console widget not found in main window");
    LOG_PERF(0, "Exit");
    return;
  }
  LOG_DEBUG("Console widget found and configured");

  // Send Ctrl+C to interrupt any running command
  LOG_DEBUG("Sending interrupt command to console");
  consoleWidget->executeCommand("\x03");

  // Change directory to container's path
  LOG_DEBUG("Changing console directory to: '{}'", _path.toStdString());
  consoleWidget->executeCommand(
      QString("cd \"%1\"").arg(QDir::toNativeSeparators(_path)));

  // Clear the console
  LOG_DEBUG("Clearing console window");
  consoleWidget->executeCommand("clear");

  LOG_DEBUG("Console configured and ready");
  LOG_PERF(0, "Exit");
}

void RepoContainer::hideEvent(QHideEvent *event) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Container hide event triggered for: '{}'", _path.toStdString());

  ContainerWidget::hideEvent(event);

  LOG_DEBUG("Container hidden");
  LOG_PERF(0, "Exit");
}

void RepoContainer::resizeEvent(QResizeEvent *event) {
  LOG_PERF(0, "Enter");

  ContainerWidget::resizeEvent(event);
  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit")
}

void RepoContainer::changeEvent(QEvent *event) {
  QWidget::changeEvent(event);

  if (!event)
    return;

  switch (event->type()) {
  case QEvent::PaletteChange:
  case QEvent::ApplicationPaletteChange:
  case QEvent::StyleChange:
    refreshScintillaTheme();
    break;
  default:
    break;
  }
}

void RepoContainer::refreshScintillaTheme() {
  DarkThemeLexersHighlighter highlighter;
  const auto editors = findChildren<QsciScintilla *>();

  for (auto *editor : editors) {
    highlighter.applyDarkTheme(editor);
  }
}

void RepoContainer::showEvent(QShowEvent *event) {
  LOG_PERF(0, "Enter");

  ContainerWidget::showEvent(event);

  raise();

  LOG_PERF(0, "Exit")
}

void RepoContainer::onAddSubmodule() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Add submodule dialog requested for repository: '{}'",
            _path.toStdString());

  const WeakPtr guard = getSelf().toWeakRef();

  const auto overlayWidget = new AddSubmoduleOverlayWidget(this);

  connect(overlayWidget, &AddSubmoduleOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Add submodule dialog cancelled");
    overlayWidget->hide();
    delete overlayWidget;
  });

  connect(overlayWidget, &AddSubmoduleOverlayWidget::submitted, this,
          [=](const QString &url, const QString &name, const QString &branch) {
            LOG_DEBUG("Adding submodule - URL: '{}', Name: '{}', Branch: '{}'",
                      url.toStdString(), name.toStdString(),
                      branch.toStdString());
            overlayWidget->hide();
            delete overlayWidget;

            // startOverlayTimer();

            _dispatcher.dispatch(
                [=]() { _bridge->addSubmodule(guard, url, name, branch); });
          });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_DEBUG("Add submodule dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onApplyPatch() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Apply patch dialog requested for repository: '{}'",
            _path.toStdString());

  const QString filePath = QFileDialog::getOpenFileName(
      this, tr("Select a patch file to open"), QDir::homePath(),
      tr("Text Files (*.patch);;All Files (*)"));

  if (filePath.isEmpty()) {
    LOG_DEBUG("Apply patch cancelled - no file selected");
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_DEBUG("Applying patch from file: '{}'", filePath.toStdString());
  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->applyPatch(guard, filePath); });

  LOG_DEBUG("Apply patch operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onApplyStash(const int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Apply stash requested at index: {} for repository: '{}'",
            stashIndex, _path.toStdString());

  const QMessageBox::StandardButton reply =
      QMessageBox::question(this,                               // parent widget
                            "Confirm Action",                   // dialog title
                            "Do you want to apply this stash?", // message text
                            QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Apply stash cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->applyStash(guard, stashIndex); });

  LOG_DEBUG("Apply stash operation dispatched for index: {}", stashIndex);
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteStash(const int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete stash requested at index: {} for repository: '{}'",
            stashIndex, _path.toStdString());

  const QMessageBox::StandardButton reply =
      QMessageBox::question(this,             // parent widget
                            "Confirm Action", // dialog title
                            "Do you want to delete this stash?", // message text
                            QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Delete stash cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  const bool closeStashViewer = (stashIndex == _currentStashIndex);

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->deleteStash(guard, stashIndex,
                         [=](const WeakPtr &guard, const int) {
                           Util::Dispatcher::dispatchToMainThread(guard, [=]() {
                             if (closeStashViewer) {
                               onStashContentCloseClicked();
                             }
                             showSpinnerOverlay(false);
                           });
                         });
  });

  LOG_DEBUG("Delete stash operation dispatched for index: {}", stashIndex);
  LOG_PERF(0, "Exit");
}

void RepoContainer::onPopStash(const int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Pop stash requested at index: {} for repository: '{}'", stashIndex,
            _path.toStdString());

  const QMessageBox::StandardButton reply =
      QMessageBox::question(this,                             // parent widget
                            "Confirm Action",                 // dialog title
                            "Do you want to pop this stash?", // message text
                            QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Pop stash cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { popStash(guard, stashIndex); });

  LOG_DEBUG("Pop stash operation dispatched for index: {}", stashIndex);
  LOG_PERF(0, "Exit");
}

void RepoContainer::onClearAllStashes() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Clear all stashes requested for repository: '{}'",
            _path.toStdString());

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this,             // parent widget
      "Confirm Action", // dialog title
      "Are you sure you want to delete all stashes?\n\nThis action cannot be "
      "undone.", // message text
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Clear all stashes cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->deleteAllStashes(guard, [=](const WeakPtr &guard) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        if (_currentStashIndex >= 0) {
          onStashContentCloseClicked();
        }
      });
    });
  });

  LOG_DEBUG("Clear all stashes operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onAddRemote() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Add remote requested for repository: '{}'", _path.toStdString());

  auto overlayWidget = new Widgets::AddRemoteOverlayWidget(this);

  connect(overlayWidget, &Widgets::AddRemoteOverlayWidget::submitted, this,
          [=](const QString &remoteName, const QString &fetchUrl,
              const QString &pushUrl) {
            LOG_DEBUG("Add remote submitted: name='{}', fetchUrl='{}'",
                      remoteName.toStdString(), fetchUrl.toStdString());

            overlayWidget->deleteLater();

            startOverlayTimer();

            const WeakPtr guard = getSelf().toWeakRef();
            _dispatcher.dispatch([=]() {
              _bridge->addRemote(guard, remoteName, fetchUrl, pushUrl);
            });
          });

  connect(overlayWidget, &Widgets::AddRemoteOverlayWidget::canceled, this,
          [=]() {
            LOG_DEBUG("Add remote dialog cancelled");
            overlayWidget->deleteLater();
          });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_PERF(0, "Exit");
}

void RepoContainer::onEditRemote(int remoteIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Edit remote {} requested", remoteIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];

  auto overlayWidget = new Widgets::EditRemoteOverlayWidget(this);
  overlayWidget->setRemoteInfo(remote.name, remote.fetchUrl, remote.pushUrl);

  connect(overlayWidget, &Widgets::EditRemoteOverlayWidget::submitted, this,
          [=](const QString &remoteName, const QString &fetchUrl,
              const QString &pushUrl) {
            LOG_DEBUG("Edit remote submitted: name='{}' -> '{}', fetchUrl='{}'",
                      remote.name.toStdString(), remoteName.toStdString(),
                      fetchUrl.toStdString());

            overlayWidget->deleteLater();

            startOverlayTimer();

            const WeakPtr guard = getSelf().toWeakRef();
            _dispatcher.dispatch([=]() {
              _bridge->editRemote(guard, remoteIndex, remoteName, fetchUrl,
                                  pushUrl);
            });
          });

  connect(overlayWidget, &Widgets::EditRemoteOverlayWidget::canceled, this,
          [=]() {
            LOG_DEBUG("Edit remote dialog cancelled");
            overlayWidget->deleteLater();
          });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_PERF(0, "Exit");
}

void RepoContainer::onFetchRemote(int remoteIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Fetch remote {} requested", remoteIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];
  if (remote.name.isEmpty()) {
    LOG_DEBUG("Remote name is empty");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Executing fetch for remote '{}' in dispatcher",
              remote.name.toStdString());
    _bridge->fetchRemote(guard, remote.name);
  });

  LOG_DEBUG("Fetch remote operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteRemote(int remoteIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete remote {} requested", remoteIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Confirm Action"),
      QString(tr("Are you sure you want to delete the remote '%1'?\n\nThis "
                 "action cannot be undone."))
          .arg(remote.name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Delete remote cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();
  _dispatcher.dispatch([=]() { _bridge->deleteRemote(guard, remoteIndex); });

  LOG_DEBUG("Delete remote operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSetDefaultRemote(int remoteIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Set default remote {} requested", remoteIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  if (_activeBranch.isEmpty() || _activeBranch == ALL_BRANCHES) {
    LOG_DEBUG("No active branch to set upstream for");
    showErrorToast(tr("Please checkout a branch first."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  const QString remoteName = ui->remotesPage->remotes()[remoteIndex].name;

  const auto upstreamOverlay = new UpstreamInputOverlayWidget(this);
  upstreamOverlay->setBranchPlaceholderText(tr("Remote branch name"));

  // Populate the combo box with available remotes
  upstreamOverlay->setRemotes(ui->remotesPage->remotes());
  upstreamOverlay->setRemoteName(remoteName);

  connect(upstreamOverlay, &UpstreamInputOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Set upstream dialog cancelled");
    upstreamOverlay->hide();
    delete upstreamOverlay;
  });

  const WeakPtr guard = getSelf().toWeakRef();

  connect(upstreamOverlay, &UpstreamInputOverlayWidget::submitted, this,
          [&, upstreamOverlay, guard](const QString &remote,
                                      const QString &branch) {
            LOG_DEBUG(
                "Setting upstream for branch '{}': remote='{}', branch='{}'",
                _activeBranch.toStdString(), remote.toStdString(),
                branch.toStdString());
            upstreamOverlay->hide();
            delete upstreamOverlay;

            _dispatcher.dispatch([=]() {
              _bridge->setUpstream(guard, _activeBranch, remote, branch);
            });
          });

  _dispatcher.dispatch([=]() {
    const auto [upstreamRemote, upstreamBranch] =
        _bridge->getUpstreamForBranch(guard, _activeBranch);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      upstreamOverlay->setRemoteName(remoteName);
      if (!upstreamBranch.isEmpty()) {
        upstreamOverlay->setRemoteBranchName(upstreamBranch);
      } else {
        upstreamOverlay->setRemoteBranchName(_activeBranch);
      }
      upstreamOverlay->selectAllBranchText();
      upstreamOverlay->show();
      upstreamOverlay->raise();
    });
  });

  LOG_DEBUG("Set upstream dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onAddTarget(int remoteIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Add target to remote {} requested", remoteIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];

  const auto addTargetOverlay = new AddTargetOverlayWidget(this);
  addTargetOverlay->setRemoteName(remote.name);

  connect(addTargetOverlay, &AddTargetOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Add target dialog cancelled");
    addTargetOverlay->hide();
    delete addTargetOverlay;
  });

  const WeakPtr guard = getSelf().toWeakRef();

  connect(addTargetOverlay, &AddTargetOverlayWidget::submitted, this,
          [=](const QString &targetName, const QString &fetchUrl,
              const QString &pushUrl) {
            LOG_DEBUG("Adding target '{}' to remote '{}'",
                      targetName.toStdString(), remote.name.toStdString());
            addTargetOverlay->hide();
            delete addTargetOverlay;

            _dispatcher.dispatch([=]() {
              _bridge->addTarget(guard, remoteIndex, targetName, fetchUrl,
                                 pushUrl);
            });
          });

  addTargetOverlay->show();
  addTargetOverlay->raise();

  LOG_DEBUG("Add target dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onEditTarget(int remoteIndex, int targetIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Edit target {}/{} requested", remoteIndex, targetIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_DEBUG("Invalid target index: {}", targetIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &target = remote.targets[targetIndex];

  const auto editTargetOverlay = new EditTargetOverlayWidget(this);
  editTargetOverlay->setTargetInfo(remote.name, target.name, target.fetchUrl,
                                   target.pushUrl);

  connect(editTargetOverlay, &EditTargetOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Edit target dialog cancelled");
    editTargetOverlay->hide();
    delete editTargetOverlay;
  });

  const WeakPtr guard = getSelf().toWeakRef();

  connect(editTargetOverlay, &EditTargetOverlayWidget::submitted, this,
          [=](const QString &fetchUrl, const QString &pushUrl) {
            LOG_DEBUG("Editing target '{}' on remote '{}'",
                      target.name.toStdString(), remote.name.toStdString());
            editTargetOverlay->hide();
            delete editTargetOverlay;

            _dispatcher.dispatch([=]() {
              _bridge->editTarget(guard, remoteIndex, targetIndex, fetchUrl,
                                  pushUrl);
            });
          });

  editTargetOverlay->show();
  editTargetOverlay->raise();

  LOG_DEBUG("Edit target dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteTarget(int remoteIndex, int targetIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete target {}/{} requested", remoteIndex, targetIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_DEBUG("Invalid target index: {}", targetIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  // Prevent deletion of the only target
  if (remote.targets.size() == 1) {
    showErrorToast(tr("Cannot delete the only target. Delete the remote "
                      "instead."),
                   tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &target = remote.targets[targetIndex];

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Confirm Action"),
      QString(tr("Are you sure you want to delete the target '%1' from "
                 "remote '%2'?\n\nThis action cannot be undone."))
          .arg(target.name, remote.name),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Delete target cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();
  _dispatcher.dispatch(
      [=]() { _bridge->deleteTarget(guard, remoteIndex, targetIndex); });

  LOG_DEBUG("Delete target operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSetDefaultTarget(int remoteIndex, int targetIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Set default target {}/{} requested", remoteIndex, targetIndex);

  if (remoteIndex < 0 || remoteIndex >= ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = ui->remotesPage->remotes()[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_DEBUG("Invalid target index: {}", targetIndex);
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();
  _dispatcher.dispatch(
      [=]() { _bridge->setDefaultTarget(guard, remoteIndex, targetIndex); });

  LOG_DEBUG("Set default target operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSplitterMoved(const int pos, const int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("pos = {}, index = {}", pos, index);

  const QString splitterState = ui->splitter->saveState().toHex();
  _configManager.setString(ConfigManager::ConfigKey::GitRepoSplitterPos,
                           splitterState);
  _configManager.save();

  stretchBranchHistoryTableColumns();

  LOG_PERF(0, "Exit");
}

void RepoContainer::onHistoryHeaderSectionResized(const int logicalIndex,
                                                  const int oldSize,
                                                  const int newSize) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("logicalIndex = {}, oldSize = {}, newSize = {}", logicalIndex,
            oldSize, newSize);

  if (_applyingHistoryColumnWidths) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QHeaderView *header = ui->historyView->horizontalHeader();
  QStringList widths;
  widths.reserve(header->count());
  for (int column = 0; column < header->count(); ++column) {
    widths << QString::number(header->sectionSize(column));
  }

  _configManager.setString(
      ConfigManager::ConfigKey::GitRepoHistoryViewColumnWidths,
      widths.join(','));
  _configManager.save();

  LOG_PERF(0, "Exit");
}

void RepoContainer::showTooltipForSummaryColumn(
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

void RepoContainer::onCheckoutCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Checkout commit requested: '{}'", commit.toStdString());

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Checking out commit '{}' in repository '{}'",
              commit.toStdString(), _path.toStdString());
    _bridge->checkoutCommit(guard, commit);
  });

  LOG_DEBUG("Checkout commit operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onAbortButtonClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Abort merge button clicked for repository: '{}'",
            _path.toStdString());

  const WeakPtr guard = getSelf().toWeakRef();

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Aborting merge operation");
    _bridge->abortMerge(guard);
  });

  LOG_DEBUG("Abort merge operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onCommitButtonClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Commit button clicked for repository: '{}'", _path.toStdString());

  const Common::UserProfile &currentProfile =
      _userProfileManager.currentProfile();
  const QString summary = ui->lineEditSummary->text().trimmed();
  const QString description = ui->textEditDescription->toPlainText().trimmed();
  const bool toAmend = _actionCommitAmend->isChecked();
  const bool signoff = _actionCommitSignOff->isChecked();
  const bool signCommit =
      _actionCommitSign->isVisible() && _actionCommitSign->isChecked();

  LOG_DEBUG("Commit details - Author: '{}', Amend: {}, Sign-off: {}, Sign "
            "commit: {}, Summary length: {}, "
            "Merge in progress: {}",
            currentProfile.author.toStdString(), toAmend, signoff, signCommit,
            summary.length(), _mergeInProgress);

  const WeakPtr guard = getSelf().toWeakRef();

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  _dispatcher.dispatch([=]() {
    if (toAmend) {
      LOG_DEBUG("Performing amend operation");
      _bridge->amend(guard, summary, description, currentProfile.author,
                     currentProfile.email, signoff, signCommit);
    } else {
      LOG_DEBUG("Performing new commit operation");
      // If merge is in progress, cleanup must be true to conclude the merge
      const bool cleanup = _mergeInProgress;
      _bridge->commit(guard, summary, description, currentProfile.author,
                      currentProfile.email, cleanup, signoff, signCommit);
    }
  });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onMergeButtonClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Merge button clicked for repository: '{}'", _path.toStdString());

  const Common::UserProfile &currentProfile =
      _userProfileManager.currentProfile();
  const QString summary = ui->lineEditSummary->text().trimmed();
  const QString description = ui->textEditDescription->toPlainText().trimmed();
  const bool signoff = _actionCommitSignOff->isChecked();
  const bool signCommit =
      _actionCommitSign->isVisible() && _actionCommitSign->isChecked();

  LOG_DEBUG("Merge details - Author: '{}', Sign-off: {}, Sign commit: {}, "
            "Summary length: {}",
            currentProfile.author.toStdString(), signoff, signCommit,
            summary.length());

  const WeakPtr guard = getSelf().toWeakRef();

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Performing merge commit operation");
    _bridge->commit(guard, summary, description, currentProfile.author,
                    currentProfile.email, true, signoff, signCommit);
  });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onCommitAndMergeIndexTreeUnstageAllButtonClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Unstage all files button clicked for repository: '{}'",
            _path.toStdString());

  _commitAndMergeIndexTreeUnstageAllButton->setVisible(false);
  LOG_DEBUG("Unstage all button hidden");

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Unstaging all files");
    unstageAll(guard);
  });

  LOG_DEBUG("Unstage all operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onCommitAndMergeWorkingTreeTreeStageAllButtonClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stage all files button clicked for repository: '{}'",
            _path.toStdString());

  _commitAndMergeWorkingDirTreeStageAllButton->setVisible(false);
  LOG_DEBUG("Stage all button hidden");

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Staging all files");
    stageAll(guard);
  });

  LOG_DEBUG("Stage all operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onCreateBranchFromCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Create branch from commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  const WeakPtr guard = getSelf().toWeakRef();

  const auto overlayWidget = new TextInputOverlayWidget(this);
  overlayWidget->setPlaceholderText(tr("Branch name here"));

  connect(overlayWidget, &TextInputOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Create branch dialog cancelled");
    overlayWidget->hide();
    delete overlayWidget;
  });

  connect(overlayWidget, &TextInputOverlayWidget::submitted, this,
          [=](const QString &branchName) {
            LOG_DEBUG("Creating branch '{}' from commit '{}'",
                      branchName.toStdString(), commit.toStdString());
            overlayWidget->hide();
            delete overlayWidget;

            if (!showWarningForBranchCheckout()) {
              LOG_DEBUG("Create branch cancelled by user");
              return;
            }

            startOverlayTimer();

            _dispatcher.dispatch([=]() {
              _bridge->createBranchFromCommit(
                  guard, commit, branchName,
                  [=](const WeakPtr &guard, const QString &branchName) {
                    _bridge->checkoutBranch(
                        guard, branchName,
                        static_cast<int>(GitRepo::BranchType::Local),
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

  LOG_DEBUG("Create branch dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onCreatePatchFromCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Create patch from commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { _bridge->createPatchFromCommit(guard, commit); });

  LOG_DEBUG("Create patch operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onCreateTagFromCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Create tag from commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  const WeakPtr guard = getSelf().toWeakRef();

  const auto overlayWidget = new AnnotatedTagOverlayWidget(this);

  connect(overlayWidget, &AnnotatedTagOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Create tag dialog cancelled");
    overlayWidget->hide();
    delete overlayWidget;
  });

  connect(
      overlayWidget, &AnnotatedTagOverlayWidget::submitted, this,
      [=](const QString &tagName, const QString &message, bool isAnnotated) {
        LOG_DEBUG("Creating tag '{}' from commit '{}', annotated: {}",
                  tagName.toStdString(), commit.toStdString(), isAnnotated);
        overlayWidget->hide();
        delete overlayWidget;

        startOverlayTimer();

        _dispatcher.dispatch([=]() {
          _bridge->createTagFromCommit(guard, commit, tagName, message,
                                       isAnnotated);
        });
      });

  overlayWidget->show();
  overlayWidget->raise();

  LOG_DEBUG("Create tag dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete branch requested: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  const QString question =
      QString("Are you sure that you want to delete branch '%1'?")
          .arg(branchName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_DEBUG("Delete branch cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for branch deletion");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->deleteBranch(guard, branchName); });

  LOG_DEBUG("Delete branch operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteRemoteBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete remote branch requested: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  const QString question =
      QString("Are you sure that you want to delete branch '%1' from remote?")
          .arg(branchName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_DEBUG("Delete remote branch cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for remote branch deletion");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { _bridge->deleteRemoteBranch(guard, branchName); });

  LOG_DEBUG("Delete remote branch operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteTag(const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete tag requested: '{}' in repository '{}'",
            tagName.toStdString(), _path.toStdString());

  const QString question =
      QString("Are you sure that you want to delete tag '%1'?").arg(tagName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_DEBUG("Delete tag cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for tag deletion");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->deleteTag(guard, tagName); });

  LOG_DEBUG("Delete tag operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onPushTag(const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Push tag requested: '{}' in repository '{}'",
            tagName.toStdString(), _path.toStdString());

  const auto reply = QMessageBox::question(
      this, tr("Confirm action"),
      QString(tr("Are you sure that you want to push tag '%1' to the remote?"))
          .arg(tagName),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Push tag cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for tag push");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->pushTag(guard, tagName); });

  LOG_DEBUG("Push tag operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onPushAllTags() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Push all tags requested for repository '{}'", _path.toStdString());

  const auto reply = QMessageBox::question(
      this, tr("Confirm action"),
      tr("Are you sure that you want to push all tags to the remote?"),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Push all tags cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for push all tags");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->pushAllTags(guard); });

  LOG_DEBUG("Push all tags operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteRemoteTag(const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete remote tag requested: '{}' in repository '{}'",
            tagName.toStdString(), _path.toStdString());

  const QString question =
      QString("Are you sure that you want to delete tag '%1' from remote?")
          .arg(tagName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_DEBUG("Delete remote tag cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for remote tag deletion");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->deleteRemoteTag(guard, tagName); });

  LOG_DEBUG("Delete remote tag operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDiffCloseClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Diff viewer close requested");

  closeDiffViewerIfOpen();

  LOG_DEBUG("Diff viewer closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::closeDiffViewerIfOpen() {
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

void RepoContainer::onDiffUnifiedClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Switching to unified diff view");

  ui->pushButtonDiffSideBySide->setChecked(false);

  // Show unified diff marks when switching to unified view
  _unifiedDiffMarks->show();

  ui->stackedWidgetDiff->setCurrentIndex(
      static_cast<int>(DiffStackIndexes::DiffUnified));

  // Save the preference (false = unified)
  _configManager.setBoolean(ConfigManager::ConfigKey::GitRepoDiffViewType,
                            false);
  _configManager.save();

  LOG_DEBUG("Unified diff view activated");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDiffSideBySideClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Switching to side-by-side diff view");

  ui->pushButtonDiffUnified->setChecked(false);

  // Hide unified diff marks when switching to side-by-side view
  _unifiedDiffMarks->hide();

  ui->stackedWidgetDiff->setCurrentIndex(
      static_cast<int>(DiffStackIndexes::DiffSideBySide));

  // Save the preference (true = side-by-side)
  _configManager.setBoolean(ConfigManager::ConfigKey::GitRepoDiffViewType,
                            true);
  _configManager.save();

  LOG_DEBUG("Side-by-side diff view activated");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onDisplayTreeExpandAllButtonClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Expand/collapse all files button clicked");

  if (_displayTreeExpanded) {
    ui->treeViewPageDisplayFilesTree->collapseAll();
    _displayTreeExpandAllButton->setText(tr("Expand all"));
    _displayTreeExpanded = false;
    LOG_DEBUG("Collapsed all tree items");
    LOG_PERF(0, "Exit");
    return;
  }

  ui->treeViewPageDisplayFilesTree->expandAll();
  _displayTreeExpandAllButton->setText(tr("Collapse all"));
  _displayTreeExpanded = true;

  LOG_DEBUG("Expanded all tree items");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onHardResetToCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Hard reset to commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this,                 // parent widget
      "Confirm hard reset", // dialog title
      "Do you want to hard reset to this commit? "
      "This will discard all uncommitted changes.", // message text
      QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Hard reset cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->reset(guard, commit, static_cast<int>(GitRepo::ResetType::Hard));
  });

  LOG_DEBUG("Hard reset operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onHistorySelectionChanged(
    const QItemSelection &selected, const QItemSelection &deselected) {
  LOG_PERF(0, "Enter");

  const QModelIndexList indexes = selected.indexes();
  if (indexes.isEmpty()) {
    LOG_DEBUG("History selection empty");
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_DEBUG("History selection changed - {} item(s) selected", indexes.count());
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
      const QString commitHash =
          shaColumnModelIndex.data(Qt::DisplayRole).toString();
      LOG_DEBUG("Showing commit '{}'", commitHash.toStdString());
      ui->stackedWidgetCommitAndMerge->setCurrentIndex(
          static_cast<int>(CommitAndMergeStackIndexes::Display));

      _dispatcher.clearPool();
      _dispatcher.dispatch([=]() { showCommit(guard, commitHash); });
    }
  } else {
    LOG_DEBUG("Showing commit and merge interface");
    ui->stackedWidgetCommitAndMerge->setCurrentIndex(
        static_cast<int>(CommitAndMergeStackIndexes::CommitAndMerge));

    _dispatcher.clearPool();
    _dispatcher.dispatch([=]() { _bridge->showCommitAndMerge(guard); });
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onHistoryViewRightClicked(const QModelIndex &index,
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
  QAction actionCheckoutCommit(tr("Checkout commit"), this);
  QAction actionCreateBranchFromCommit(tr("Create branch from commit"), this);
  QAction actionCopyShaToClipboard(tr("Copy SHA to clipboard"), this);
  QAction actionSoftResetToCommit(tr("Soft reset"), this);
  QAction actionMixedResetToCommit(tr("Mixed reset"), this);
  QAction actionHardResetToCommit(tr("Hard reset"), this);
  QAction actionRevertCommit(tr("Revert"), this);
  QAction actionCherryPickCommit(tr("Cherry-pick"), this);
  QAction actionApplyPatch(tr("Apply patch"), this);
  QAction actionRenameBranch(QString(tr("Rename %1")).arg(_activeBranch), this);
  QAction actionCreateTagFromCommit(tr("Create tag from commit"), this);
  QAction actionCreatePatchFromCommit(tr("Create patch from commit"), this);

  connect(&actionPull, &QAction::triggered, this, &RepoContainer::onPull);
  connect(&actionPush, &QAction::triggered, this, &RepoContainer::onPush);

  connect(&actionCheckoutCommit, &QAction::triggered, this,
          [=]() { onCheckoutCommit(commit); });
  connect(&actionCreateBranchFromCommit, &QAction::triggered, this,
          [=]() { onCreateBranchFromCommit(commit); });
  connect(&actionCopyShaToClipboard, &QAction::triggered, this, [=]() {
    QGuiApplication::clipboard()->setText(commit);
    showToast(tr("Commit SHA copied to clipboard."), tr("Info"));
  });
  connect(&actionSoftResetToCommit, &QAction::triggered, this,
          [=]() { onSoftResetToCommit(commit); });
  connect(&actionMixedResetToCommit, &QAction::triggered, this,
          [=]() { onMixedResetToCommit(commit); });
  connect(&actionHardResetToCommit, &QAction::triggered, this,
          [=]() { onHardResetToCommit(commit); });
  connect(&actionRevertCommit, &QAction::triggered, this,
          [=]() { onRevertCommit(commit); });
  connect(&actionCherryPickCommit, &QAction::triggered, this,
          [=]() { onCherryPickCommit(); });
  connect(&actionApplyPatch, &QAction::triggered, this,
          &RepoContainer::onApplyPatch);
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
  menu.addSeparator();

  if (!commit.isEmpty()) {
    menu.addAction(&actionCheckoutCommit);
    menu.addAction(&actionCopyShaToClipboard);
    menu.addSeparator();
    menu.addAction(&actionCreateBranchFromCommit);
    menu.addMenu(&resetMenu);
    menu.addAction(&actionRevertCommit);
    menu.addAction(&actionCherryPickCommit);
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

void RepoContainer::onHistoryViewItemEntered(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.column() == COLUMN_SUMMARY) {
    const auto statusFieldsMap =
        index.data(StatusRole).value<Models::StatusFieldsMap>();
    if (index.row() == 0 && !statusFieldsMap.isEmpty()) {
      LOG_DEBUG("Summary column tooltip requested with status fields");
      showTooltipForSummaryColumn(index);
      LOG_PERF(0, "Exit");
      return;
    }
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onListViewTabWidgetFlatSelected(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_DEBUG("Invalid model index for file selection");
    LOG_PERF(0, "Exit");
    return;
  }

  QString filePath = index.data(Qt::DisplayRole).toString();
  LOG_DEBUG("File selected from list view: '{}'", filePath.toStdString());

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
    LOG_DEBUG("File size {} exceeds maximum diff size {}", fileInfo.size(),
              maxDiffFileSize);
    showErrorToast(tr("File is too large to be displayed."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    const int blobSize = _bridge->getFileSizeFromCommit(filePath, QString());
    if (blobSize > maxDiffFileSize) {
      LOG_DEBUG("Blob size {} exceeds maximum", blobSize);
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
    LOG_DEBUG("Showing diff for file '{}' - conflicted: {}",
              filePath.toStdString(), conflicted);
    if (!conflicted) {
      _bridge->diff(guard, filePath);
    } else {
      _bridge->diffThreeWay(guard, filePath);
    }
  });

  LOG_DEBUG("File diff generation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onListViewTabWidgetFlatSelectedFromCommit(
    const QModelIndex &index, const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("File selected from commit: commit='{}'", commit.toStdString());

  if (!index.isValid()) {
    LOG_DEBUG("Invalid model index for file selection");
    LOG_PERF(0, "Exit");
    return;
  }

  QString filePath = index.data(Qt::DisplayRole).toString();
  LOG_DEBUG("File path: '{}'", filePath.toStdString());

  filePath = QDir::fromNativeSeparators(filePath);

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    // Check file size from commit before generating diff
    const qint64 maxDiffFileSize = _configManager.getLong(
        ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
    const qint64 fileSize = _bridge->getFileSizeFromCommit(filePath, commit);

    if (fileSize > 0 && fileSize > maxDiffFileSize) {
      LOG_DEBUG("File size {} exceeds maximum {}", fileSize, maxDiffFileSize);
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        showErrorToast(tr("File is too large to be displayed."), tr("Error"));
        return;
      });

      return;
    }

    Util::Dispatcher::dispatchToMainThread(guard,
                                           [=]() { startOverlayTimer(); });

    LOG_DEBUG("Generating diff from commit for file '{}'",
              filePath.toStdString());
    _bridge->diffFromCommit(guard, filePath, commit);
  });

  LOG_DEBUG("File diff from commit dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onMergeDiffCloseClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Merge diff viewer close requested");

  _lastDiffedCommit.clear();
  _lastDiffedFilePath.clear();
  _lastDiffRepoStatusChanged = false;

  ui->stackedWidgetMiddle->setCurrentIndex(
      static_cast<int>(MainViewPanes::History));

  stretchBranchHistoryTableColumns();

  LOG_DEBUG("Returned to history view");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onMergeDiffDownClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Merge diff down navigation");

  LOG_PERF(0, "Exit");
}

void RepoContainer::onMergeDiffUpClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Merge diff up navigation");

  LOG_PERF(0, "Exit");
}

void RepoContainer::onMergeDiffSaveClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Merge diff save requested");

  if (!ui->mergeDiffViewer->areConflictsResolved()) {
    LOG_DEBUG("Unresolved conflicts detected - requesting confirmation");
    const QString question =
        QString("All conflicts in the file are not resolved.\nAre you sure "
                "that you want to save this file?");

    const auto reply =
        QMessageBox::question(this, tr("Confirm action"), question,
                              QMessageBox::Yes, QMessageBox::No);
    if (reply == QMessageBox::No) {
      LOG_DEBUG("Save cancelled due to unresolved conflicts");
      LOG_PERF(0, "Exit");
      return;
    }
  }

  QString mergedContent = ui->mergeDiffViewer->mergedContent();
  LOG_DEBUG("Merged content retrieved - size: {} bytes", mergedContent.size());

  if (ui->mergeDiffViewer->shouldConvertToWindowsFile()) {
    LOG_DEBUG("Converting line endings to Windows format");
    mergedContent = mergedContent.replace("\n", "\r\n");
  }

  const QString filePath = ui->mergeDiffViewer->filePath();
  LOG_DEBUG("Saving merge result to file: '{}'", filePath.toStdString());

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { saveThreeWayMergeResultFile(guard, filePath, mergedContent); });

  LOG_DEBUG("Save merge result operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onTreeViewTabWidgetTreeSelected(
    const QModelIndex &index, const Models::GitCommitTreeModel &model) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_DEBUG("Invalid model index for tree view selection");
    LOG_PERF(0, "Exit");
    return;
  }

  // Check if it is leaf node
  if (model.rowCount(index) > 0) {
    LOG_DEBUG("Non-leaf node selected - directory");
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
  LOG_DEBUG("File selected from tree view: '{}'", filePath.toStdString());

  filePath = QDir::fromNativeSeparators(filePath);

  // Check file size for large files
  const QString fullPath = QDir(_path).filePath(filePath);
  const QFileInfo fileInfo(fullPath);
  const qint64 maxDiffFileSize =
      _configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);

  if (fileInfo.exists() && fileInfo.size() > maxDiffFileSize) {
    LOG_DEBUG("File size {} exceeds maximum {}", fileInfo.size(),
              maxDiffFileSize);
    showErrorToast(tr("File is too large to be displayed."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    const int blobSize = _bridge->getFileSizeFromCommit(filePath, QString());
    if (blobSize > maxDiffFileSize) {
      LOG_DEBUG("Blob size {} exceeds maximum", blobSize);
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
    LOG_DEBUG("Showing diff for tree file '{}' - conflicted: {}",
              filePath.toStdString(), conflicted);
    if (!conflicted) {
      _bridge->diff(guard, filePath);
    } else {
      _bridge->diffThreeWay(guard, filePath);
    }
  });

  LOG_DEBUG("Tree file diff generation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onTreeViewTabWidgetTreeSelectedFromCommit(
    const QModelIndex &index, const Models::GitCommitTreeModel &model,
    const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("File selected from commit tree - commit='{}'",
            commit.toStdString());

  if (!index.isValid()) {
    LOG_DEBUG("Invalid model index for tree view selection");
    LOG_PERF(0, "Exit");
    return;
  }

  // Check if it is leaf node
  if (model.rowCount(index) > 0) {
    LOG_DEBUG("Non-leaf node selected - directory");
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
  LOG_DEBUG("File path: '{}'", filePath.toStdString());

  filePath = QDir::fromNativeSeparators(filePath);

  // Check file size from commit before generating diff
  const qint64 maxDiffFileSize =
      _configManager.getLong(ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
  const qint64 fileSize = _bridge->getFileSizeFromCommit(filePath, commit);

  if (fileSize > 0 && fileSize > maxDiffFileSize) {
    LOG_DEBUG("File size {} exceeds maximum {}", fileSize, maxDiffFileSize);
    showErrorToast(tr("File is too large to be displayed."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Generating diff from commit for tree file '{}'",
              filePath.toStdString());
    _bridge->diffFromCommit(guard, filePath, commit);
  });

  LOG_DEBUG("Tree file diff from commit dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onLocalBranchMoved(const QString &sourceBranch,
                                       const QString &destBranch) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Branch move requested: '{}' -> '{}' in repository '{}'",
            sourceBranch.toStdString(), destBranch.toStdString(),
            _path.toStdString());

  QMenu menu(this);
  QAction actionMerge(tr("Merge"), this);
  QAction actionRebase(tr("Rebase"), this);

  menu.addAction(&actionMerge);
  menu.addAction(&actionRebase);

  const QAction *selectedAction = menu.exec(QCursor::pos());
  if (!selectedAction) {
    LOG_DEBUG("Branch move cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  const QString question = selectedAction == &actionMerge
                               ? QString("Are you sure that you want to merge "
                                         "branch '%1' to branch '%2'?")
                                     .arg(sourceBranch, destBranch)
                               : QString("Are you sure that you want to rebase "
                                         "branch '%1' onto branch '%2'?")
                                     .arg(sourceBranch, destBranch);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_DEBUG("Branch move operation cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  if (selectedAction == &actionMerge) {
    _dispatcher.dispatch(
        [=]() { _bridge->merge(guard, sourceBranch, destBranch); });
    LOG_DEBUG("Branch merge operation dispatched");
  } else if (selectedAction == &actionRebase) {
    _dispatcher.dispatch(
        [=]() { _bridge->rebase(guard, sourceBranch, destBranch); });
    LOG_DEBUG("Branch rebase operation dispatched");
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onLocalBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Branch selected: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  if (!showWarningForBranchCheckout()) {
    LOG_DEBUG("Create branch cancelled by user");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Checking out branch '{}'", branchName.toStdString());
    _bridge->checkoutBranch(guard, branchName,
                            static_cast<int>(GitRepo::BranchType::Local));
  });

  LOG_DEBUG("Branch checkout operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onLocalBranchRightClicked(const QString &branchName,
                                              const QPoint &pos) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Branch right-clicked: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  QAction actionCheckoutBranch(tr("Checkout branch"), this);
  QAction actionRanameBranch(tr("Rename branch"), this);
  QAction actionSetUpstream(tr("Set Upstream"), this);
  QAction actionDeleteBranch(tr("Delete branch"), this);

  connect(&actionCheckoutBranch, &QAction::triggered, this, [=]() {
    LOG_DEBUG("Checkout branch action triggered");

    if (!showWarningForBranchCheckout()) {
      LOG_DEBUG("Create branch cancelled by user");
      return;
    }

    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() {
      _bridge->checkoutBranch(guard, branchName,
                              static_cast<int>(GitRepo::BranchType::Local));
    });
  });

  connect(&actionRanameBranch, &QAction::triggered, this,
          [=]() { onRenameBranch(branchName); });

  connect(&actionSetUpstream, &QAction::triggered, this,
          [=]() { onSetUpstream(branchName); });

  connect(&actionDeleteBranch, &QAction::triggered, this,
          [=]() { onDeleteBranch(branchName); });

  QMenu menu;
  menu.addAction(&actionCheckoutBranch);
  menu.addAction(&actionRanameBranch);
  menu.addAction(&actionSetUpstream);
  menu.addSeparator();
  menu.addAction(&actionDeleteBranch);

  menu.exec(pos);

  LOG_DEBUG("Branch context menu closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onMixedResetToCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Mixed reset to commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this,                                         // parent widget
      "Confirm mixed reset",                        // dialog title
      "Do you want to mixed reset to this commit?", // message text
      QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Mixed reset cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->reset(guard, commit, static_cast<int>(GitRepo::ResetType::Mixed));
  });

  LOG_DEBUG("Mixed reset operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onPop() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Pop stash operation initiated for repository: '{}'",
            _path.toStdString());

  const int stashIndex = ui->stashesPage->lastStash();
  if (stashIndex == -1) {
    LOG_DEBUG("No stashes available to pop");
    LOG_PERF(0, "Exit");
    return;
  }

  LOG_DEBUG("Popping stash at index: {}", stashIndex);
  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    popStash(guard, stashIndex);
    _bridge->getStashes(guard);
  });

  LOG_DEBUG("Pop stash operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onPull(bool rebase) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("{} operation initiated for repository: '{}'",
            rebase ? "Pull with rebase" : "Pull", _path.toStdString());

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Executing {} operation in dispatcher",
              rebase ? "pull with rebase" : "pull");
    _bridge->pull(guard, rebase);
  });

  LOG_DEBUG("{} operation dispatched", rebase ? "Pull with rebase" : "Pull");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onFetchAll() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Fetch all operation initiated for repository: '{}'",
            _path.toStdString());

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Executing fetch all operation in dispatcher");
    _bridge->fetchAll(guard);
  });

  LOG_DEBUG("Fetch all operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onPush(bool force) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("{}push operation initiated for repository: '{}'",
            force ? "Force " : "", _path.toStdString());

  if (_activeBranch.isEmpty() || _activeBranch == ALL_BRANCHES) {
    LOG_DEBUG("No active branch to push");
    showErrorToast(tr("Please checkout a branch first."), tr("Error"));
    LOG_PERF(0, "Exit");
    return;
  }

  const WeakPtr guard = getSelf().toWeakRef();
  const QString branchName = _activeBranch;

  const auto remotes = ui->remotesPage->remotes();
  if (remotes.isEmpty()) {
    LOG_DEBUG("No remotes available to push branch '{}'",
              branchName.toStdString());
    showErrorToast(tr("Please add a remote before pushing."), tr("Error"));
    return;
  }

  if (force) {
    const auto reply = QMessageBox::question(
        this, tr("Confirm force push"),
        QString(tr("Force pushing branch '%1' rewrites the remote history and "
                   "can overwrite commits on the server.\n\nDo you want to "
                   "continue?"))
            .arg(branchName),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (reply == QMessageBox::No) {
      LOG_DEBUG("Force push cancelled by user");
      LOG_PERF(0, "Exit");
      return;
    }
  }

  _dispatcher.dispatch([=]() {
    const auto [upstreamRemote, upstreamBranch] =
        _bridge->getUpstreamForBranch(guard, branchName);
    if (!upstreamRemote.isEmpty() && !upstreamBranch.isEmpty()) {
      LOG_DEBUG("Executing {}push operation in dispatcher",
                force ? "force " : "");
      _bridge->push(guard, force);
      return;
    }

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      const auto upstreamOverlay = new UpstreamInputOverlayWidget(this);
      upstreamOverlay->setBranchPlaceholderText(tr("Remote branch name"));
      upstreamOverlay->setRemotes(remotes);
      upstreamOverlay->setRemoteBranchName(branchName);

      const QString activeRemote = ui->remotesPage->activeRemote();
      if (!activeRemote.isEmpty()) {
        upstreamOverlay->setRemoteName(activeRemote);
      } else {
        upstreamOverlay->setRemoteName(QStringLiteral("origin"));
      }

      connect(upstreamOverlay, &UpstreamInputOverlayWidget::canceled, this,
              [=]() {
                LOG_DEBUG("Push upstream dialog cancelled");
                upstreamOverlay->hide();
                delete upstreamOverlay;
              });

      connect(
          upstreamOverlay, &UpstreamInputOverlayWidget::submitted, this,
          [=](const QString &remote, const QString &branch) {
            const QString remoteName = remote.trimmed();
            const QString remoteBranchName = branch.trimmed();
            if (remoteName.isEmpty() || remoteBranchName.isEmpty()) {
              showErrorToast(
                  tr("Please select a remote and enter a branch name."),
                  tr("Error"));
              return;
            }

            LOG_DEBUG(
                "Setting upstream before {}push for branch '{}': remote='{}', "
                "branch='{}'",
                force ? "force " : "", branchName.toStdString(),
                remoteName.toStdString(), remoteBranchName.toStdString());
            upstreamOverlay->hide();
            delete upstreamOverlay;

            startOverlayTimer();
            _dispatcher.dispatch([=]() {
              _bridge->setUpstream(
                  guard, branchName, remoteName, remoteBranchName,
                  [=](const WeakPtr &guard) { _bridge->push(guard, force); });
            });
          });

      upstreamOverlay->selectAllBranchText();
      upstreamOverlay->show();
      upstreamOverlay->raise();
    });
  });

  LOG_DEBUG("{}push operation dispatched", force ? "Force " : "");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onStash() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash operation initiated for repository: '{}'",
            _path.toStdString());

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Creating stash");
    _bridge->stash(guard);
  });

  LOG_DEBUG("Stash operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onStashSelected(int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash selected at index: {} in repository '{}'", stashIndex,
            _path.toStdString());

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->showStashContent(guard, stashIndex); });

  LOG_DEBUG("Show stash content operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSubmitRenameBranch(const QString &oldName,
                                         const QString &newName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Submitting branch rename: '{}' -> '{}' in repository '{}'",
            oldName.toStdString(), newName.toStdString(), _path.toStdString());

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { _bridge->renameBranch(guard, oldName, newName); });

  LOG_DEBUG("Branch rename operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSubmitRenameTag(const QString &oldName,
                                      const QString &newName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Submitting tag rename: '{}' -> '{}' in repository '{}'",
            oldName.toStdString(), newName.toStdString(), _path.toStdString());

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->renameTag(guard, oldName, newName); });

  LOG_DEBUG("Tag rename operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSubmoduleRightClicked(const QString &submoduleName,
                                            const QPoint &pos) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Submodule right-clicked: '{}' in repository '{}'",
            submoduleName.toStdString(), _path.toStdString());

  QAction actionOpenSubmodule(tr("Open"), this);
  QAction actionUpdateSubmodule(tr("Update"), this);
  QAction actionDeleteSubmodule(tr("Delete"), this);

  connect(&actionOpenSubmodule, &QAction::triggered, this, [=]() {
    LOG_DEBUG("Open submodule action triggered");
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch(
        [=]() { _bridge->openSubmodule(guard, submoduleName); });
  });

  connect(&actionUpdateSubmodule, &QAction::triggered, this, [=]() {
    LOG_DEBUG("Update submodule action triggered");
    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch(
        [=]() { _bridge->updateSubmodule(guard, submoduleName); });
  });

  connect(&actionDeleteSubmodule, &QAction::triggered, this,
          [=]() { onDeleteSubmodule(submoduleName); });

  QMenu menu;
  menu.addAction(&actionOpenSubmodule);
  menu.addAction(&actionUpdateSubmodule);
  menu.addSeparator();
  menu.addAction(&actionDeleteSubmodule);

  menu.exec(pos);

  LOG_DEBUG("Submodule context menu closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSubmodulePanelRightClicked(const QPoint &pos) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Submodule panel right-clicked for repository: '{}'",
            _path.toStdString());

  QAction actionAddSubmodule(tr("Add"), this);

  connect(&actionAddSubmodule, &QAction::triggered, this,
          &RepoContainer::onAddSubmodule);

  QMenu menu;
  menu.addAction(&actionAddSubmodule);

  menu.exec(pos);

  LOG_DEBUG("Submodule panel context menu closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSubmoduleSelected(const QString &submoduleName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Submodule selected: '{}' in repository '{}'",
            submoduleName.toStdString(), _path.toStdString());

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->openSubmodule(guard, submoduleName); });

  LOG_DEBUG("Open submodule operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onTagRightClicked(const QString &tagName,
                                      const QPoint &pos) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Tag right-clicked: '{}' in repository '{}'", tagName.toStdString(),
            _path.toStdString());

  QAction actionCheckoutTag(tr("Checkout tag"), this);
  QAction actionRanameTag(tr("Rename tag"), this);
  QAction actionPushTag(tr("Push to Remote"), this);
  QAction actionDeleteRemoteTag(tr("Delete from Remote"), this);
  QAction actionDeleteTag(tr("Delete tag"), this);

  connect(&actionCheckoutTag, &QAction::triggered, this, [=]() {
    LOG_DEBUG("Checkout tag action triggered");

    if (!showWarningForBranchCheckout()) {
      LOG_DEBUG("Create branch cancelled by user");
      return;
    }

    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() {
      _bridge->checkoutBranch(guard, tagName,
                              static_cast<int>(GitRepo::BranchType::Tag));
    });
  });

  connect(&actionRanameTag, &QAction::triggered, this,
          [=]() { onRenameTag(tagName); });

  connect(&actionPushTag, &QAction::triggered, this,
          [=]() { onPushTag(tagName); });

  connect(&actionDeleteRemoteTag, &QAction::triggered, this,
          [=]() { onDeleteRemoteTag(tagName); });

  connect(&actionDeleteTag, &QAction::triggered, this,
          [=]() { onDeleteTag(tagName); });

  QMenu menu;
  menu.addAction(&actionCheckoutTag);
  menu.addAction(&actionRanameTag);
  menu.addAction(&actionPushTag);
  menu.addAction(&actionDeleteRemoteTag);
  menu.addSeparator();
  menu.addAction(&actionDeleteTag);

  menu.exec(pos);

  LOG_DEBUG("Tag context menu closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onTagsEmptyAreaRightClicked(const QPoint &pos) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Empty area in tags panel right-clicked in repository '{}'",
            _path.toStdString());

  QAction actionPushAllTags(tr("Push all tags"), this);

  connect(&actionPushAllTags, &QAction::triggered, this,
          [=]() { onPushAllTags(); });

  QMenu menu;
  menu.addAction(&actionPushAllTags);

  menu.exec(pos);

  LOG_DEBUG("Tags empty area context menu closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onRemoteBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Remote branch selected: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  if (!showWarningForBranchCheckout()) {
    LOG_DEBUG("Create branch cancelled by user");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->checkoutBranch(guard, branchName,
                            static_cast<int>(GitRepo::BranchType::Remote));
  });

  LOG_DEBUG("Remote branch checkout operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onRemoteBranchRightClicked(const QString &branchName,
                                               const QPoint &pos) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Remote branch right-clicked: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  QAction actionCheckoutBranch(tr("Checkout branch"), this);
  QAction actionDeleteRemoteBranch(tr("Delete branch from remote"), this);

  connect(&actionCheckoutBranch, &QAction::triggered, this, [=]() {
    LOG_DEBUG("Checkout remote branch action triggered");

    if (!showWarningForBranchCheckout()) {
      LOG_DEBUG("Create branch cancelled by user");
      return;
    }

    startOverlayTimer();

    const WeakPtr guard = getSelf().toWeakRef();

    _dispatcher.dispatch([=]() {
      _bridge->checkoutBranch(guard, branchName,
                              static_cast<int>(GitRepo::BranchType::Remote));
    });
  });

  connect(&actionDeleteRemoteBranch, &QAction::triggered, this,
          [=]() { onDeleteRemoteBranch(branchName); });

  QMenu menu;
  menu.addAction(&actionCheckoutBranch);
  menu.addSeparator();
  menu.addAction(&actionDeleteRemoteBranch);

  menu.exec(pos);

  LOG_DEBUG("Remote branch context menu closed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onRenameBranch(const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Rename branch dialog requested for: '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  const auto inputOverlay = new TextInputOverlayWidget(this);
  inputOverlay->setPlaceholderText(tr("New branch name"));
  inputOverlay->setText(branchName);

  connect(inputOverlay, &TextInputOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Rename branch dialog cancelled");
    inputOverlay->hide();
    delete inputOverlay;
  });
  connect(inputOverlay, &TextInputOverlayWidget::submitted, this,
          [=](const QString &newName) {
            LOG_DEBUG("Branch rename dialog submitted: new name='{}'",
                      newName.toStdString());
            inputOverlay->hide();
            delete inputOverlay;
            onSubmitRenameBranch(branchName, newName);
          });

  inputOverlay->show();
  inputOverlay->raise();

  QTimer::singleShot(0, inputOverlay,
                     [inputOverlay]() { inputOverlay->focusAndSelectAll(); });

  LOG_DEBUG("Rename branch dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onRenameTag(const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Rename tag dialog requested for: '{}' in repository '{}'",
            tagName.toStdString(), _path.toStdString());

  const auto inputOverlay = new TextInputOverlayWidget(this);
  inputOverlay->setPlaceholderText(tr("New tag name"));

  connect(inputOverlay, &TextInputOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Rename tag dialog cancelled");
    inputOverlay->hide();
    delete inputOverlay;
  });
  connect(inputOverlay, &TextInputOverlayWidget::submitted, this,
          [=](const QString &newName) {
            LOG_DEBUG("Tag rename dialog submitted: new name='{}'",
                      newName.toStdString());
            inputOverlay->hide();
            delete inputOverlay;
            onSubmitRenameTag(tagName, newName);
          });

  inputOverlay->show();
  inputOverlay->raise();

  LOG_DEBUG("Rename tag dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onRepoDirChanged(const QString &path) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Repository directory changed: '{}'", path.toStdString());

  const qlonglong holdbackTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoDirWatcherHoldbackTimeout);

  _repoDirWatcherTimer->stop();
  _repoDirWatcherTimer->start(holdbackTimeout);

  LOG_DEBUG("Dir watcher timer restarted with timeout: {}ms", holdbackTimeout);
  LOG_PERF(0, "Exit");
}

void RepoContainer::onRepoDirChangedTimeout() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG(
      "Repository directory change timeout triggered for repository: '{}'",
      _path.toStdString());

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    LOG_DEBUG("Preparing watcher and refreshing repository status");
    prepareWatcher(guard);
    _bridge->refreshStatus(guard);

    _lastDiffRepoStatusChanged = true;
  });

  LOG_DEBUG("Dir watcher timeout operation dispatched");
  LOG_PERF(0, "Exit");
}

static QString
commitSummaryFromHistoryModel(const Models::GitHistoryModel *historyModel,
                              const QString &commit) {
  if (!historyModel || commit.isEmpty()) {
    return {};
  }

  for (int i = 0; i < historyModel->rowCount(QModelIndex()); ++i) {
    const QString rowFullSHA =
        historyModel
            ->data(
                historyModel->index(i, COLUMN_COMMIT_FULL_SHA, QModelIndex()),
                Qt::DisplayRole)
            .toString();
    if (rowFullSHA == commit) {
      return historyModel
          ->data(historyModel->index(i, COLUMN_SUMMARY, QModelIndex()),
                 Qt::DisplayRole)
          .toString();
    }
  }

  return {};
}

void RepoContainer::onRevertCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Revert commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  const QString summaryToRevert =
      commitSummaryFromHistoryModel(_historyModel.data(), commit);

  const QString question =
      QString(tr("Do you want to revert commit '%1'?\n\nThis will create a new "
                 "commit that reverses the changes."))
          .arg(summaryToRevert.isEmpty() ? commit : summaryToRevert);

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this, tr("Confirm revert"), question, QMessageBox::Yes | QMessageBox::No,
      QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Revert commit cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->revert(guard, commit); });

  LOG_DEBUG("Revert commit operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onCherryPickCommit() {
  LOG_PERF(0, "Enter");

  const auto inputOverlay = new TextInputOverlayWidget(this);
  inputOverlay->setPlaceholderText(tr("Commit SHA to cherry-pick"));

  connect(inputOverlay, &TextInputOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Cherry-pick dialog cancelled");
    inputOverlay->hide();
    delete inputOverlay;
  });

  const WeakPtr guard = getSelf().toWeakRef();

  connect(inputOverlay, &TextInputOverlayWidget::submitted, this,
          [=](const QString &selectedCommit) {
            LOG_DEBUG("Cherry-picking commit '{}'",
                      selectedCommit.toStdString());
            inputOverlay->hide();
            delete inputOverlay;

            startOverlayTimer();
            LOG_DEBUG("Spinner overlay timer started");

            _dispatcher.dispatch(
                [=]() { _bridge->cherryPick(guard, selectedCommit); });

            LOG_DEBUG("Cherry-pick commit operation dispatched");
          });

  inputOverlay->show();
  inputOverlay->raise();

  LOG_PERF(0, "Exit");
}

void RepoContainer::onSetUpstream(const QString &branchName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Set upstream dialog requested for branch '{}' in repository '{}'",
            branchName.toStdString(), _path.toStdString());

  const auto upstreamOverlay = new UpstreamInputOverlayWidget(this);
  upstreamOverlay->setBranchPlaceholderText(tr("Remote branch name"));

  // Populate the combo box with available remotes
  upstreamOverlay->setRemotes(ui->remotesPage->remotes());

  connect(upstreamOverlay, &UpstreamInputOverlayWidget::canceled, this, [=]() {
    LOG_DEBUG("Set upstream dialog cancelled");
    upstreamOverlay->hide();
    delete upstreamOverlay;
  });

  const WeakPtr guard = getSelf().toWeakRef();

  connect(
      upstreamOverlay, &UpstreamInputOverlayWidget::submitted, this,
      [&, upstreamOverlay, guard, branchName](const QString &remote,
                                              const QString &branch) {
        LOG_DEBUG("Setting upstream for branch '{}': remote='{}', branch='{}'",
                  branchName.toStdString(), remote.toStdString(),
                  branch.toStdString());
        upstreamOverlay->hide();
        delete upstreamOverlay;

        _dispatcher.dispatch(
            [=]() { _bridge->setUpstream(guard, branchName, remote, branch); });
      });

  _dispatcher.dispatch([=]() {
    const auto [upstreamRemote, upstreamBranch] =
        _bridge->getUpstreamForBranch(guard, branchName);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      upstreamOverlay->setRemoteName(upstreamRemote);
      upstreamOverlay->setRemoteBranchName(upstreamBranch);
      upstreamOverlay->selectAllBranchText();
      upstreamOverlay->show();
      upstreamOverlay->raise();
    });
  });

  LOG_DEBUG("Set upstream dialog displayed");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onSoftResetToCommit(const QString &commit) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Soft reset to commit requested: '{}' in repository '{}'",
            commit.toStdString(), _path.toStdString());

  const QMessageBox::StandardButton reply = QMessageBox::question(
      this,                                        // parent widget
      "Confirm soft reset",                        // dialog title
      "Do you want to soft reset to this commit?", // message text
      QMessageBox::Yes | QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Soft reset cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->reset(guard, commit, static_cast<int>(GitRepo::ResetType::Soft));
  });

  LOG_DEBUG("Soft reset operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onTagSelected(const QString &tagName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Tag selected: '{}' in repository '{}'", tagName.toStdString(),
            _path.toStdString());

  if (!showWarningForBranchCheckout()) {
    LOG_DEBUG("Create branch cancelled by user");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->checkoutBranch(guard, tagName,
                            static_cast<int>(GitRepo::BranchType::Tag));
  });

  LOG_DEBUG("Tag checkout operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::onShowSpinnerOverlay() {
  LOG_PERF(0, "Enter");

  showSpinnerOverlay();

  LOG_PERF(0, "Exit");
}

void RepoContainer::onDeleteSubmodule(const QString &submoduleName) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Delete submodule requested: '{}' in repository '{}'",
            submoduleName.toStdString(), _path.toStdString());

  const QString question =
      QString("Are you sure that you want to delete submodule '%1'?")
          .arg(submoduleName);

  const auto reply = QMessageBox::question(this, tr("Confirm action"), question,
                                           QMessageBox::Yes, QMessageBox::No);
  if (reply == QMessageBox::No) {
    LOG_DEBUG("Delete submodule cancelled by user");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();
  LOG_DEBUG("Spinner overlay timer started for submodule deletion");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch(
      [=]() { _bridge->deleteSubmodule(guard, submoduleName); });

  LOG_DEBUG("Delete submodule operation dispatched");
  LOG_PERF(0, "Exit");
}

void RepoContainer::resizeSectionToViewport(const int section) {
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

void RepoContainer::restorePreviousSectionWidth() {
  LOG_PERF(0, "Enter");

  ui->historyView->setColumnWidth(_resizedHeaderSection,
                                  _resizedHeaderSectionPreviousWidth);
  _resizedHeaderSection = -1;

  LOG_PERF(0, "Exit");
}

void RepoContainer::persistHistoryColumnVisibility() {
  LOG_PERF(0, "Enter");

  const QHeaderView *header = ui->historyView->horizontalHeader();
  QStringList hiddenColumns;
  hiddenColumns.reserve(header->count());
  for (int column = 0; column < header->count(); ++column) {
    if (header->isSectionHidden(column)) {
      hiddenColumns << QString::number(column);
    }
  }

  _configManager.setString(
      ConfigManager::ConfigKey::GitRepoHistoryViewHiddenColumns,
      hiddenColumns.join(','));
  _configManager.save();

  LOG_PERF(0, "Exit");
}

void RepoContainer::applyHistoryColumnVisibility() {
  LOG_PERF(0, "Enter");

  const QString savedHiddenColumns = _configManager.getString(
      ConfigManager::ConfigKey::GitRepoHistoryViewHiddenColumns);

  QSet<int> hiddenColumns;
  for (const QString &value :
       savedHiddenColumns.split(',', Qt::SkipEmptyParts)) {
    bool ok = false;
    const int column = value.toInt(&ok);
    if (ok) {
      hiddenColumns.insert(column);
    }
  }

  QHeaderView *header = ui->historyView->horizontalHeader();
  QSignalBlocker blocker(header);
  for (int column = 0; column < header->count(); ++column) {
    header->setSectionHidden(column, hiddenColumns.contains(column));
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onHistoryHeaderContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QMenu menu(ui->historyView->horizontalHeader());
  const QHeaderView *header = ui->historyView->horizontalHeader();
  const int historyColumnCount = header->count();

  for (int column = 0; column < historyColumnCount; ++column) {
    const QString columnName =
        header->model()
            ? header->model()
                  ->headerData(column, Qt::Horizontal, Qt::DisplayRole)
                  .toString()
            : QString();
    QAction *action = menu.addAction(
        columnName.isEmpty() ? tr("Column %1").arg(column + 1) : columnName);
    action->setCheckable(true);
    action->setChecked(!header->isSectionHidden(column));
    connect(action, &QAction::toggled, this,
            [this, column](const bool checked) {
              QSignalBlocker headerBlocker(ui->historyView->horizontalHeader());
              ui->historyView->horizontalHeader()->setSectionHidden(column,
                                                                    !checked);
              persistHistoryColumnVisibility();
            });
  }

  menu.addSeparator();
  QAction *showAllAction = menu.addAction(tr("Show all columns"));
  connect(showAllAction, &QAction::triggered, this, [this]() {
    QSignalBlocker headerBlocker(ui->historyView->horizontalHeader());
    QHeaderView *header = ui->historyView->horizontalHeader();
    for (int column = 0; column < header->count(); ++column) {
      header->setSectionHidden(column, false);
    }
    persistHistoryColumnVisibility();
  });

  menu.exec(ui->historyView->horizontalHeader()->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void RepoContainer::saveRepoInRecentList() const {
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

void RepoContainer::saveThreeWayMergeResultFile(const WeakPtr &guard,
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

  _bridge->stageFiles(guard, QStringList() << filePath);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    ui->pushButtonMergeDiffSave->setEnabled(false);
  });

  LOG_PERF(0, "Exit");
}

void RepoContainer::startOverlayTimer() const {
  LOG_PERF(0, "Enter");

  _spinnerOverlayTimer->start(0);

  LOG_PERF(0, "Exit");
}

void RepoContainer::stopOverlayTimer() const {
  LOG_PERF(0, "Enter");

  _spinnerOverlayTimer->stop();

  LOG_PERF(0, "Exit");
}

void RepoContainer::stopWatcher(const WeakPtr &guard) {
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

void RepoContainer::onGraphColumnExpand(int section) {
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

void RepoContainer::onWorkingTreeFlatListViewContextMenuRequested(
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
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);
  const QStringList existingFiles =
      getSelectedFilesWhichExist(ui->listViewTabWidgetWorkingTreeFlat);

  QMenu contextMenu;

  if (!files.isEmpty()) {
    QAction *copyPathAction =
        contextMenu.addAction(tr("Copy file path to clipboard"));
    connect(copyPathAction, &QAction::triggered, this,
            [=]() { copyFilePathsToClipboard(files); });
  }

  contextMenu.addAction(ui->actionWorkingTreeFlatShowInFileManager);
  contextMenu.addSeparator();

  contextMenu.addAction(ui->actionWorkingTreeFlatStage);
  if (!conflicted) {
    contextMenu.addAction(ui->actionWorkingTreeFlatDiscard);
  }

  if (!existingFiles.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &RepoContainer::onWorkingTreeFlatListViewIgnoreSelected);
    connect(ignoreRecursiveAction, &QAction::triggered, this,
            &RepoContainer::onWorkingTreeFlatListViewIgnoreRecursiveSelected);

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

void RepoContainer::onWorkingTreeFlatListViewStageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->stageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeFlatListViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeFlatListViewIgnoreRecursiveSelected(
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

void RepoContainer::onWorkingTreeFlatListViewCreateFilePatchSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeFlatListViewDeleteSelected(bool checked) {
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

void RepoContainer::onWorkingTreeFlatListViewDiscardSelected(bool checked) {
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

  _dispatcher.dispatch([=]() { _bridge->discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeTreeTreeViewContextMenuRequested(
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
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);
  const QStringList existingFiles =
      getSelectedFilesWhichExist(ui->treeViewTabWidgetWorkingTreeTree);

  QMenu contextMenu;

  if (!files.isEmpty()) {
    QAction *copyPathAction =
        contextMenu.addAction(tr("Copy file path to clipboard"));
    connect(copyPathAction, &QAction::triggered, this,
            [=]() { copyFilePathsToClipboard(files); });
  }

  contextMenu.addAction(ui->actionWorkingTreeTreeShowInFileManager);
  contextMenu.addSeparator();

  contextMenu.addAction(ui->actionWorkingTreeTreeStage);
  if (!conflicted) {
    contextMenu.addAction(ui->actionWorkingTreeTreeDiscard);
  }

  if (!existingFiles.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &RepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected);
    connect(ignoreRecursiveAction, &QAction::triggered, this,
            &RepoContainer::onWorkingTreeTreeTreeViewIgnoreRecursiveSelected);

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

void RepoContainer::onWorkingTreeTreeTreeViewStageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->stageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  const WeakPtr &guard = getSelf().toWeakRef();

  closeDiffViewerIfOpen();

  startOverlayTimer();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeTreeTreeViewIgnoreRecursiveSelected(
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

void RepoContainer::onWorkingTreeTreeTreeViewCreateFilePatchSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeTreeTreeViewDeleteSelected(bool checked) {
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

void RepoContainer::onWorkingTreeTreeTreeViewDiscardSelected(bool checked) {
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

  _dispatcher.dispatch([=]() { _bridge->discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  if (const QModelIndex index = ui->listViewTabWidgetIndexFlat->indexAt(pos);
      !index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);
  const QStringList existingFiles =
      getSelectedFilesWhichExist(ui->listViewTabWidgetIndexFlat);

  QMenu contextMenu;
  if (!files.isEmpty()) {
    QAction *copyPathAction =
        contextMenu.addAction(tr("Copy file path to clipboard"));
    connect(copyPathAction, &QAction::triggered, this,
            [=]() { copyFilePathsToClipboard(files); });
  }

  contextMenu.addAction(ui->actionIndexFlatShowInFileManager);
  contextMenu.addSeparator();

  contextMenu.addAction(ui->actionIndexFlatUnstage);
  contextMenu.addAction(ui->actionIndexFlatDiscard);

  if (!existingFiles.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &RepoContainer::onIndexFlatListViewIgnoreSelected);
    connect(ignoreRecursiveAction, &QAction::triggered, this,
            &RepoContainer::onIndexFlatListViewIgnoreRecursiveSelected);

    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexFlatCreateFilePatch);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexFlatDelete);
  }

  contextMenu.exec(ui->listViewTabWidgetIndexFlat->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewUnstageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->unstageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewIgnoreRecursiveSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFilesRecursive(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewCreateFilePatchSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewDeleteSelected(bool checked) {
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

void RepoContainer::onIndexFlatListViewDiscardSelected(bool checked) {
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

  _dispatcher.dispatch([=]() { _bridge->discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewContextMenuRequested(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->treeViewTabWidgetIndexTree->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);
  const QStringList existingFiles =
      getSelectedFilesWhichExist(ui->treeViewTabWidgetIndexTree);

  QMenu contextMenu;
  if (!files.isEmpty()) {
    QAction *copyPathAction =
        contextMenu.addAction(tr("Copy file path to clipboard"));
    connect(copyPathAction, &QAction::triggered, this,
            [=]() { copyFilePathsToClipboard(files); });
  }

  contextMenu.addAction(ui->actionIndexTreeShowInFileManager);
  contextMenu.addSeparator();

  contextMenu.addAction(ui->actionIndexTreeUnstage);
  contextMenu.addAction(ui->actionIndexTreeDiscard);

  if (!existingFiles.isEmpty()) {
    QMenu *ignoreMenu = contextMenu.addMenu(tr("Ignore"));
    QAction *ignoreFileAction = ignoreMenu->addAction(tr("Ignore this file"));
    QAction *ignoreRecursiveAction = ignoreMenu->addAction(
        tr("Ignore all files recursively inside file's parent folder"));

    connect(ignoreFileAction, &QAction::triggered, this,
            &RepoContainer::onIndexTreeTreeViewIgnoreSelected);
    connect(ignoreRecursiveAction, &QAction::triggered, this,
            &RepoContainer::onIndexTreeTreeViewIgnoreRecursiveSelected);

    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexTreeCreateFilePatch);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionIndexTreeDelete);
  }

  contextMenu.exec(ui->treeViewTabWidgetIndexTree->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewUnstageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->unstageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewIgnoreRecursiveSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  closeDiffViewerIfOpen();

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFilesRecursive(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewCreateFilePatchSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  startOverlayTimer();

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { _bridge->diffFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewDeleteSelected(bool checked) {
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

void RepoContainer::onIndexTreeTreeViewDiscardSelected(bool checked) {
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

  _dispatcher.dispatch([=]() { _bridge->discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onPageDisplayFilesFlatListViewContextMenuRequested(
    const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listViewPageDisplayFilesFlat->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files = getSelectedFiles(ui->listViewPageDisplayFilesFlat);
  const QStringList existingFiles =
      getSelectedFilesWhichExist(ui->listViewPageDisplayFilesFlat);
  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QMenu contextMenu;
  QAction *copyPathAction =
      contextMenu.addAction(tr("Copy file path to clipboard"));
  connect(copyPathAction, &QAction::triggered, this,
          [=]() { copyFilePathsToClipboard(files); });

  if (!existingFiles.isEmpty()) {
    QAction *showInFileManagerAction =
        contextMenu.addAction(tr("Show in File manager"));
    connect(showInFileManagerAction, &QAction::triggered, this,
            [=]() { FileManager::revealInFileManager(existingFiles); });
  }

  contextMenu.exec(ui->listViewPageDisplayFilesFlat->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void RepoContainer::onPageDisplayFilesTreeViewContextMenuRequested(
    const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->treeViewPageDisplayFilesTree->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files = getSelectedFiles(ui->treeViewPageDisplayFilesTree);
  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QStringList items;
  const QModelIndexList listIndexes =
      ui->treeViewPageDisplayFilesTree->selectionModel()->selectedIndexes();

  for (const auto &selectedIndex : listIndexes) {
    const int rowCount =
        ui->treeViewPageDisplayFilesTree->model()->rowCount(selectedIndex);

    if (rowCount > 0) {
      QModelIndex current = selectedIndex;
      QString fullPath = current.data().toString();
      while (current.parent().isValid()) {
        fullPath =
            current.parent().data().toString() + QDir::separator() + fullPath;
        current = current.parent();
      }
      items += QDir::fromNativeSeparators(fullPath);
    } else {
      const QStringList traversedFiles = traverseModelIndexSubtree(
          selectedIndex, ui->treeViewPageDisplayFilesTree->model());
      for (const auto &traversedFile : traversedFiles) {
        items += QDir::fromNativeSeparators(traversedFile);
      }
    }
  }

  QStringList itemsThatExist;
  for (const auto &item : items) {
    const QString fullPath = QDir(_path).filePath(item);
    if (QFileInfo::exists(fullPath)) {
      itemsThatExist += fullPath;
    }
  }

  QMenu contextMenu;
  QAction *copyPathAction =
      contextMenu.addAction(tr("Copy file path to clipboard"));
  connect(copyPathAction, &QAction::triggered, this,
          [=]() { copyFilePathsToClipboard(files); });

  if (!itemsThatExist.isEmpty()) {
    QAction *showInFileManagerAction =
        contextMenu.addAction(tr("Show in File manager"));
    connect(showInFileManagerAction, &QAction::triggered, this,
            [=]() { FileManager::revealInFileManager(itemsThatExist); });
  }

  contextMenu.exec(ui->treeViewPageDisplayFilesTree->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeFlatListViewShowInFileManagerSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFilesWhichExist(ui->listViewTabWidgetWorkingTreeFlat);

  if (!files.isEmpty()) {
    FileManager::revealInFileManager(files);
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onWorkingTreeTreeTreeViewShowInFileManagerSelected(
    bool checked) {
  LOG_PERF(0, "Enter");

  QStringList items;

  const QModelIndexList listIndexes =
      ui->treeViewTabWidgetWorkingTreeTree->selectionModel()->selectedIndexes();

  for (const auto &index : listIndexes) {
    const GuiManager::Models::GitCommitListModificationType modificationType =
        static_cast<GuiManager::Models::GitCommitListModificationType>(
            index.data(Qt::UserRole).toInt());

    if (modificationType ==
        GuiManager::Models::GitCommitListModificationType::Conflicted) {
      continue;
    }

    // Check if selected item is a folder (has children)
    const int rowCount =
        ui->treeViewTabWidgetWorkingTreeTree->model()->rowCount(index);
    if (rowCount > 0) {
      // It's a folder - construct its path without traversing children
      QModelIndex current = index;
      QString fullPath = current.data().toString();
      while (current.parent().isValid()) {
        fullPath =
            current.parent().data().toString() + QDir::separator() + fullPath;
        current = current.parent();
      }
      items += QDir::fromNativeSeparators(fullPath);
    } else {
      // It's a file - use the normal getSelectedFiles logic
      const QStringList traversedFiles = traverseModelIndexSubtree(
          index, ui->treeViewTabWidgetWorkingTreeTree->model());

      for (const auto &traversedFile : traversedFiles) {
        items += QDir::fromNativeSeparators(traversedFile);
      }
    }
  }

  // Filter to only existing files/folders
  QStringList itemsThatExist;
  for (const auto &item : items) {
    const QString fullPath = QDir(_path).filePath(item);
    if (QFileInfo::exists(fullPath)) {
      itemsThatExist += fullPath;
    }
  }

  if (!itemsThatExist.isEmpty()) {
    FileManager::revealInFileManager(itemsThatExist);
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexFlatListViewShowInFileManagerSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
      getSelectedFilesWhichExist(ui->listViewTabWidgetIndexFlat);

  if (!files.isEmpty()) {
    FileManager::revealInFileManager(files);
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::onIndexTreeTreeViewShowInFileManagerSelected(bool checked) {
  LOG_PERF(0, "Enter");

  QStringList items;

  const QModelIndexList listIndexes =
      ui->treeViewTabWidgetIndexTree->selectionModel()->selectedIndexes();

  for (const auto &index : listIndexes) {
    const GuiManager::Models::GitCommitListModificationType modificationType =
        static_cast<GuiManager::Models::GitCommitListModificationType>(
            index.data(Qt::UserRole).toInt());

    if (modificationType ==
        GuiManager::Models::GitCommitListModificationType::Conflicted) {
      continue;
    }

    // Check if selected item is a folder (has children)
    const int rowCount =
        ui->treeViewTabWidgetIndexTree->model()->rowCount(index);
    if (rowCount > 0) {
      // It's a folder - construct its path without traversing children
      QModelIndex current = index;
      QString fullPath = current.data().toString();
      while (current.parent().isValid()) {
        fullPath =
            current.parent().data().toString() + QDir::separator() + fullPath;
        current = current.parent();
      }
      items += QDir::fromNativeSeparators(fullPath);
    } else {
      // It's a file - use the normal getSelectedFiles logic
      const QStringList traversedFiles = traverseModelIndexSubtree(
          index, ui->treeViewTabWidgetIndexTree->model());

      for (const auto &traversedFile : traversedFiles) {
        items += QDir::fromNativeSeparators(traversedFile);
      }
    }
  }

  // Filter to only existing files/folders
  QStringList itemsThatExist;
  for (const auto &item : items) {
    const QString fullPath = QDir(_path).filePath(item);
    if (QFileInfo::exists(fullPath)) {
      itemsThatExist += fullPath;
    }
  }

  if (!itemsThatExist.isEmpty()) {
    FileManager::revealInFileManager(itemsThatExist);
  }

  LOG_PERF(0, "Exit");
}

void RepoContainer::updateModelActiveBranch(const WeakPtr &guard,
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

void RepoContainer::clearSummaryAndDescription() {
  LOG_PERF(0, "Enter");

  ui->lineEditSummary->setText(QString());
  ui->textEditDescription->setText(QString());

  LOG_PERF(0, "Exit");
}

void RepoContainer::refreshSigningControls() {
  LOG_PERF(0, "Enter");

  const bool signingEnabled = _configManager.getBoolean(
      ConfigManager::ConfigKey::GitRepoSigningEnabled);
  _actionCommitSign->setVisible(signingEnabled);

  LOG_PERF(0, "Exit");
}

void RepoContainer::deleteFiles(const WeakPtr &guard,
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

  _bridge->refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard,
                                         [&]() { showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void RepoContainer::popStash(const WeakPtr &guard, int stashIndex) {
  LOG_PERF(0, "Enter");

  _bridge->applyStash(guard, stashIndex,
                      [=](const WeakPtr &guard, const int index) {
                        _bridge->deleteStash(
                            guard, index, [=](const WeakPtr &guard, const int) {
                              Util::Dispatcher::dispatchToMainThread(
                                  guard, [&]() { showSpinnerOverlay(false); });
                            });
                      });

  LOG_PERF(0, "Exit")
}

void RepoContainer::onStashApplyClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash apply button clicked");

  if (_currentStashIndex < 0) {
    LOG_ERROR(Log::ErrorId::Gui, "No stash index available");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    _bridge->applyStash(guard, _currentStashIndex,
                        [=](const WeakPtr &guard, const int) {
                          Util::Dispatcher::dispatchToMainThread(guard, [&]() {
                            ui->stackedWidgetMiddle->setCurrentIndex(
                                static_cast<int>(MainViewPanes::History));
                            showSpinnerOverlay(false);
                          });
                        });
  });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onStashPopClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash pop button clicked");

  if (_currentStashIndex < 0) {
    LOG_ERROR(Log::ErrorId::Gui, "No stash index available");
    LOG_PERF(0, "Exit");
    return;
  }

  startOverlayTimer();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    popStash(guard, _currentStashIndex);

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::History));
      showSpinnerOverlay(false);
    });
  });

  LOG_PERF(0, "Exit");
}

void RepoContainer::onStashContentCloseClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash content close button clicked");

  _currentStashIndex = -1;
  _lastStashCommit.clear();

  ui->scintillaStashDiffUnified->clear();
  ui->scintillaStashDiffUnified->markerDeleteAll();
  ui->sideBySideDiffViewerStash->clearContent();
  ui->stackedWidgetStashDiff->setCurrentIndex(0);
  ui->pushButtonStashDiffUnified->setChecked(true);
  ui->pushButtonStashDiffSideBySide->setChecked(false);

  ui->stackedWidgetMiddle->setCurrentIndex(
      static_cast<int>(MainViewPanes::History));

  LOG_PERF(0, "Exit");
}

void RepoContainer::onStashDiffUnifiedClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash unified diff button clicked");

  ui->pushButtonStashDiffUnified->setChecked(true);
  ui->pushButtonStashDiffSideBySide->setChecked(false);
  ui->stackedWidgetStashDiff->setCurrentIndex(0); // Unified view

  LOG_PERF(0, "Exit");
}

void RepoContainer::onStashDiffSideBySideClicked() {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash side-by-side diff button clicked");

  ui->pushButtonStashDiffUnified->setChecked(false);
  ui->pushButtonStashDiffSideBySide->setChecked(true);
  ui->stackedWidgetStashDiff->setCurrentIndex(1); // Side-by-side view

  LOG_PERF(0, "Exit");
}

bool RepoContainer::getActiveLocalBranch() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit")

  return _bridge->getActiveBranchOrTag(
      getSelf().toWeakRef(), static_cast<int>(GitRepo::BranchType::Local));
}

bool RepoContainer::getActiveRemoteBranch() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit")

  return _bridge->getActiveBranchOrTag(
      getSelf().toWeakRef(), static_cast<int>(GitRepo::BranchType::Remote));
}

bool RepoContainer::getActiveTag() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit")

  return _bridge->getActiveBranchOrTag(
      getSelf().toWeakRef(), static_cast<int>(GitRepo::BranchType::Tag));
}

void RepoContainer::copyFilePathsToClipboard(const QStringList &filePaths) {
  LOG_PERF(0, "Enter");

  QStringList uniqueFilePaths = filePaths;
  uniqueFilePaths.removeDuplicates();

  if (uniqueFilePaths.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  QStringList absoluteFilePaths;
  absoluteFilePaths.reserve(uniqueFilePaths.size());
  for (const auto &filePath : uniqueFilePaths) {
    absoluteFilePaths += QDir(_path).filePath(filePath);
  }

  QGuiApplication::clipboard()->setText(absoluteFilePaths.join("\n"));

  if (uniqueFilePaths.size() == 1) {
    showToast(tr("File path copied to clipboard."), tr("Info"));
  } else {
    showToast(
        tr("%1 file paths copied to clipboard.").arg(uniqueFilePaths.size()),
        tr("Info"));
  }

  LOG_PERF(0, "Exit");
}

QStringList RepoContainer::getSelectedFiles(QListView *listView) {
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
    files += QDir::fromNativeSeparators(index.data().toString());
  }

  LOG_PERF(0, "Exit");

  return files;
}

QStringList RepoContainer::getSelectedFilesWhichExist(QListView *listView) {
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

QStringList RepoContainer::getSelectedFiles(QTreeView *treeView) {
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
    const QStringList traversedFiles =
        traverseModelIndexSubtree(index, treeView->model());

    for (const auto &traversedFile : traversedFiles) {
      files += QDir::fromNativeSeparators(traversedFile);
    }
  }

  LOG_PERF(0, "Exit");

  return files;
}

QStringList RepoContainer::getSelectedFilesWhichExist(QTreeView *treeView) {
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

QStringList
RepoContainer::getSubdirectories(const QString &path,
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

QStringList RepoContainer::getAllFiles(const QString &path,
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

void RepoContainer::hideAllWidgets() const {
  LOG_PERF(0, "Enter");

  ui->splitter->hide();

  LOG_PERF(0, "Exit")
}

void RepoContainer::ignoreFiles(const WeakPtr &guard,
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

  _bridge->refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
  });

  LOG_PERF(0, "Exit")
}

void RepoContainer::ignoreFilesRecursive(const WeakPtr &guard,
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

  _bridge->refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    showSpinnerOverlay(false);
    simulateClickOnFirstHistoryRow();
  });

  LOG_PERF(0, "Exit");
}

void RepoContainer::prepareWatcher(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    delete _repoDirWatcher;
#ifdef Q_OS_WIN32
    _repoDirWatcher = new Util::DirectoryWatcher(_path, this);
    connect(_repoDirWatcher, &Util::DirectoryWatcher::directoryChanged, this,
            &RepoContainer::onRepoDirChanged, Qt::QueuedConnection);
    _repoDirWatcher->startWatching();
#else
    _repoDirWatcher = new QFileSystemWatcher(this);
    connect(_repoDirWatcher, &QFileSystemWatcher::directoryChanged, this,
            &RepoContainer::onRepoDirChanged);
    connect(_repoDirWatcher, &QFileSystemWatcher::fileChanged, this,
            &RepoContainer::onRepoDirChanged);

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

bool RepoContainer::refreshHistory(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  bool ok = true;

  ok &= _bridge->getLocalBranches();
  ok &= ok && getActiveLocalBranch();
  ok &= ok && _bridge->getRemoteBranches();
  ok &= ok && getActiveRemoteBranch();
  ok &= ok && _bridge->getTags();
  ok &= ok && getActiveTag();
  ok &= ok && _bridge->getSubmodules();
  ok &= ok && _bridge->getRemotes(guard);
  ok &= ok && _bridge->getStashes(guard);

  if (!ok) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });
  }

  LOG_PERF(0, "Exit")

  return ok;
}

void RepoContainer::resetCommitAndMergeIndexTreeUnstageAllButton() const {
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

void RepoContainer::resetCommitAndMergeWorkingTreeTreeStageAllButton() {
  LOG_PERF(0, "Enter");

  _commitAndMergeWorkingDirTreeStageAllButton->setText(tr("Stage all"));

  int rowCount = 0;

  if (_commitListModel) {
    for (int i = 0; i < _commitListModel->rowCount(); ++i) {
      ++rowCount;
    }
  }

  _commitAndMergeWorkingDirTreeStageAllButton->setVisible(rowCount > 0);

  LOG_PERF(0, "Exit")
}

void RepoContainer::resetCommitButton() const {
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
  _actionCommitAmend->setVisible(!_mergeInProgress);

  LOG_PERF(0, "Exit")
}

void RepoContainer::resetDisplayTreeExpandedButton() {
  LOG_PERF(0, "Enter");

  _displayTreeExpandAllButton->setText(tr("Expand all"));
  _displayTreeExpanded = false;

  LOG_PERF(0, "Exit")
}

void RepoContainer::stretchBranchHistoryTableColumns() {
  LOG_PERF(0, "Enter");

  QHeaderView *header = ui->historyView->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::Interactive);

  const int columnCount = header->count();
  if (columnCount <= 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString savedWidths = _configManager.getString(
      ConfigManager::ConfigKey::GitRepoHistoryViewColumnWidths);
  const QStringList widthValues = savedWidths.split(',', Qt::SkipEmptyParts);

  if (widthValues.size() == columnCount) {
    bool restoreOk = true;
    _applyingHistoryColumnWidths = true;
    for (int column = 0; column < columnCount; ++column) {
      if (header->isSectionHidden(column)) {
        continue;
      }

      bool ok = false;
      const int width = widthValues[column].toInt(&ok);
      if (!ok || width <= 0) {
        restoreOk = false;
        break;
      }

      ui->historyView->setColumnWidth(column, width);
    }
    _applyingHistoryColumnWidths = false;

    if (restoreOk) {
      LOG_PERF(0, "Exit");
      return;
    }

    LOG_WARN(
        "Invalid saved history view column widths, falling back to defaults");
  }

  const auto *historyModel =
      qobject_cast<Models::GitHistoryModel *>(ui->historyView->model());
  if (!historyModel) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int viewportWidth = ui->historyView->viewport()->width();
  int occupiedWidth = 0;

  _applyingHistoryColumnWidths = true;

  if (!header->isSectionHidden(COLUMN_HISTORY_SLICE)) {
    const int sliceWidth =
        std::max(1, historyModel->columnWidth(COLUMN_HISTORY_SLICE));
    ui->historyView->setColumnWidth(COLUMN_HISTORY_SLICE, sliceWidth);
    occupiedWidth += sliceWidth;
  }

  for (int column = 0; column < columnCount; ++column) {
    if (column == COLUMN_HISTORY_SLICE || column == COLUMN_SUMMARY ||
        header->isSectionHidden(column)) {
      continue;
    }

    const int width = std::max(1, historyModel->columnWidth(column));
    ui->historyView->setColumnWidth(column, width);
    occupiedWidth += width;
  }

  if (!header->isSectionHidden(COLUMN_SUMMARY)) {
    const int summaryWidth = std::max(1, viewportWidth - occupiedWidth);
    ui->historyView->setColumnWidth(COLUMN_SUMMARY, summaryWidth);
  }

  _applyingHistoryColumnWidths = false;

  LOG_PERF(0, "Exit");
}

void RepoContainer::showAllWidgets() const {
  LOG_PERF(0, "Enter");

  ui->splitter->show();

  LOG_PERF(0, "Exit")
}

void RepoContainer::showCommit(const WeakPtr &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus historyAndStatus =
      _historyModel->historyWithStatus();

  if (!historyAndStatus.history.commitInfos.contains(commit)) {
    LOG_PERF(0, "Exit");
    return;
  }

  const GitRepo::GitCommitStatus commitStatus =
      _bridge->getCommitStatus(guard, commit);

  const GitRepo::GitCommitInfo commitInfo =
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
    const QString signedText = commitInfo.signedCommit ? tr("Yes") : tr("No");
    setIconLabel(ui->labelPageDisplaySignedValue,
                 themedIcon(commitInfo.signedCommit ? ":/icons/ok.png"
                                                    : ":/icons/cancel.png"),
                 tr("Signed: %1").arg(signedText));

    const QString signatureType = commitInfo.signatureType.isEmpty()
                                      ? tr("Unknown")
                                      : commitInfo.signatureType;
    const bool isOpenPgpSignatureType =
        commitInfo.signatureType.compare("OpenPGP", Qt::CaseInsensitive) == 0;
    const bool isSshSignatureType =
        commitInfo.signatureType.compare("SSH", Qt::CaseInsensitive) == 0;
    if (isOpenPgpSignatureType) {
      ui->labelPageDisplaySignatureTypeValue->setPixmap(QPixmap());
      ui->labelPageDisplaySignatureTypeValue->setText(signatureType);
      ui->labelPageDisplaySignatureTypeValue->setAlignment(Qt::AlignLeft |
                                                           Qt::AlignVCenter);
      ui->labelPageDisplaySignatureTypeValue->setToolTip(
          tr("Signature type: %1").arg(signatureType));
      ui->labelPageDisplaySignatureTypeValue->setStatusTip(
          tr("Signature type: %1").arg(signatureType));
      ui->labelPageDisplaySignatureTypeValue->setAccessibleName(
          tr("Signature type: %1").arg(signatureType));
    } else {
      setIconLabel(ui->labelPageDisplaySignatureTypeValue,
                   themedIcon(isSshSignatureType ? ":/icons/ssh-key.svg"
                                                 : ":/icons/id-card.png"),
                   tr("Signature type: %1").arg(signatureType));
    }
    const bool hasSignatureBlock =
        commitInfo.signedCommit && !commitInfo.signature.isEmpty();
    _currentSignature = hasSignatureBlock ? commitInfo.signature : QString();
    ui->widgetPageDisplaySignatureSection->setVisible(hasSignatureBlock);
    ui->pushButtonPageDisplaySignatureCopy->setEnabled(hasSignatureBlock);
    ui->toolButtonPageDisplaySignature->setChecked(false);
    ui->widgetPageDisplaySignatureContent->setVisible(false);

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

void RepoContainer::showErrorPage(const bool error) const {
  LOG_PERF(0, "Enter");

  ui->stackedWidget1->setCurrentIndex(error ? 2 : 1);

  ui->spinner->stop();

  LOG_PERF(0, "Exit")
}

void RepoContainer::showMainSpinnerPage(const bool spinnerEnabled) const {
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

void RepoContainer::showSpinnerOverlay(bool spinnerEnabled) const {
  LOG_PERF(0, "Enter");

  if (spinnerEnabled) {
    _spinnerOverlay->startSpinning();
    _spinnerOverlay->setText("");
    _spinnerOverlay->raise();
    _spinnerOverlay->show();
  } else {
    stopOverlayTimer();
    _spinnerOverlay->hide();
    _spinnerOverlay->stopSpinning();
    _spinnerOverlay->setText("");
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::showToast(const QString &message, const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new InfoToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

void RepoContainer::showErrorToast(const QString &message,
                                   const QString &title) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
    _toastHost.addToast(new ErrorToast(message, title));
  });

  LOG_PERF(0, "Exit")
}

bool RepoContainer::showWarningForBranchCheckout() const {
  LOG_PERF(0, "Enter");
  if (!_historyModel->hasStatus()) {
    LOG_PERF(0, "Exit")
    return true;
  }

  const QMessageBox::StandardButton reply = QMessageBox::question(
      ui->historyView, tr("Uncommitted Changes"),
      tr("You have uncommitted changes on the current branch.\n\n"
         "Do you really want to switch to the new branch?\n\n"
         "To keep your changes, stash them before switching."),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  if (reply != QMessageBox::Yes) {
    LOG_DEBUG("Branch checkout cancelled by user due to uncommitted changes");
    LOG_PERF(0, "Exit");
    return false;
  }

  LOG_PERF(0, "Exit")

  return true;
}

void RepoContainer::simulateClickOnHistoryRow(int row) const {
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

void RepoContainer::simulateClickOnFirstHistoryRow() const {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->historyView->model()->index(0, 0);

  ui->historyView->selectRow(0);
  ui->historyView->setCurrentIndex(index);
  ui->historyView->scrollToTop();

  LOG_PERF(0, "Exit")
}

void RepoContainer::stageAll(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus historyAndStatus =
      _historyModel->historyWithStatus();

  const QStringList modifiedFiles = historyAndStatus.status.newFiles +
                                    historyAndStatus.status.modifiedFiles +
                                    historyAndStatus.status.deletedFiles +
                                    historyAndStatus.status.renamedFiles +
                                    historyAndStatus.status.conflictedFiles;

  if (!modifiedFiles.isEmpty()) {
    _bridge->stageFiles(guard, modifiedFiles);
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::unstageAll(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus historyAndStatus =
      _historyModel->historyWithStatus();

  const QStringList stagedFiles = historyAndStatus.status.newStagedFiles +
                                  historyAndStatus.status.modifiedStagedFiles +
                                  historyAndStatus.status.deletedStagedFiles +
                                  historyAndStatus.status.renamedStagedFiles;

  if (!stagedFiles.isEmpty()) {
    _bridge->unstageFiles(guard, stagedFiles);
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateBranchHistory(
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

void RepoContainer::updateLocalBranchesModel(
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

void RepoContainer::updateLocalBranchesModelActiveBranch(
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

      // Update the active remote in RemotesPage to match the branch's upstream
      _dispatcher.dispatch([=]() {
        auto [upstreamRemote, upstreamBranch] = _bridge->getUpstream(guard);
        if (upstreamRemote.isEmpty()) {
          upstreamRemote = ui->remotesPage->activeRemote();
        }
        Util::Dispatcher::dispatchToMainThread(guard, [=]() {
          ui->remotesPage->updateActiveRemote(upstreamRemote);
        });
      });
    } else {
      ui->remotePage->clearCached();
      ui->tagsPage->clearCached();
    }
  }

  _dispatcher.dispatch([=]() { (void)_bridge->getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateRemoteBranchesModel(
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

void RepoContainer::updateRemoteBranchesModelWithStatus(
    const WeakPtr &guard,
    const QVector<GitRepo::BranchInfo> &branchesWithStatus) const {
  LOG_PERF(0, "Enter");

  // Create a new vector with ALL_BRANCHES entry if there are branches
  QVector<GitRepo::BranchInfo> finalBranchesWithStatus;

  if (!branchesWithStatus.isEmpty()) {
    // Add a special entry for "All branches" without status
    GitRepo::BranchInfo allBranches;
    allBranches.name = ALL_BRANCHES;
    allBranches.statusInfo.status = GitRepo::BranchStatusType::Unknown;
    finalBranchesWithStatus.append(allBranches);
  }

  finalBranchesWithStatus += branchesWithStatus;

  if (const auto locked = guard.lock()) {
    ui->remotePage->updateModelWithStatus(finalBranchesWithStatus);
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateRemoteBranchesModelActiveBranch(
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

  _dispatcher.dispatch([=]() { (void)_bridge->getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateStashesModel(const WeakPtr &guard,
                                       const QStringList &stashes) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->stashesPage->updateModel(stashes);
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateTagsModelActiveTag(const WeakPtr &guard,
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

  _dispatcher.dispatch([=]() { (void)_bridge->getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateSubmodulesModel(const WeakPtr &guard,
                                          const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->submodulesPage->updateModel(tags);
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateSubmodulesModelWithStatus(
    const WeakPtr &guard,
    const QVector<GitRepo::SubmoduleInfo> &submodulesWithStatus) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->submodulesPage->updateModelWithStatus(submodulesWithStatus);
  }

  LOG_PERF(0, "Exit")
}

void RepoContainer::updateTagsModel(const WeakPtr &guard,
                                    const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->tagsPage->updateModel(tags);
  }

  LOG_PERF(0, "Exit")
}
} // namespace GuiManager::Containers
