#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"
#include "delegates/ElidePartialColorDelegate.h"
#include "delegates/HistorySliceTableColumnDelegate.h"
#include "delegates/HistorySummaryTableColumnDelegate.h"
#include "models/GitCommitListModel.h"
#include "models/GitCommitTreeModel.h"
#include "widgets/ClickableHeaderView.h"
#include "widgets/ErrorToast.h"
#include "widgets/InfoToast.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <QCheckBox>
#include <QFile>
#include <QFileDialog>
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

#define IGNORE_FILENAME ".gitignore"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

namespace GuiManager::Containers {

enum class CommitAndMergeStackIndexes {
  Unknown = -1,

  CommitAndMerge = 0,
  Display = 1,

  last
};

QStringList traverseModelIndexSubtree(const QModelIndex &parent,
                                      const QAbstractItemModel *model) {
  if (!parent.isValid()) {
    return {};
  }

  QStringList files;

  int rowCount = model->rowCount(parent);

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
    : ContainerWidget(Widgets::ContainerType::Unknown, parent),
      ui(new Ui::GitRepoContainer), _uuid(Uuid::createUuid()),
      _path(std::move(path)), _activeBranchType(GitRepo::BranchType::All),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _userProfileManager(Common::UserProfileManager::instance()),
      _toastHost(ToastHostWidget::getInstance()),
      _historyModel(new Models::GitHistoryModel()), _dispatcher(1),
      _mousePos(-1, -1), _spinnerOverlay(new SpinnerOverlayWidget(this)),
      _spinnerOverlayTimer(new QTimer(this)), _quitGettingHistory(false),
    _resizedHeaderSection(-1), _resizedHeaderSectionPreviousWidth(-1),
    _displayTreeExpanded(false), _commitListModel(nullptr),
    _commitTreeModel(nullptr) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);
  ui->localPage->setContentsMargins(0, 0, 0, 0);
  ui->remotePage->setContentsMargins(0, 0, 0, 0);
  ui->tagsPage->setContentsMargins(0, 0, 0, 0);
  ui->toolBox->setItemText(0, tr("Local Branches"));
  ui->toolBox->setItemText(1, tr("Remote Branches"));
  ui->toolBox->setItemText(2, tr("Tags"));
  ui->toolBox->setItemText(3, tr("Submodules"));
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
  ui->spinner->setColor(Qt::lightGray);

  auto *header = new ClickableHeaderView(Qt::Horizontal, ui->historyView);
  ui->historyView->setHorizontalHeader(header);
  ui->pushButtonAbort->setVisible(false);
  ui->pushButtonCommit->setEnabled(false);

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
  _commitAndMergeWorkingDirTreeStageAllButton->setCursor(
    Qt::PointingHandCursor);
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
  _commitAndMergeIndexTreeUnstageAllButton->setCursor(Qt::PointingHandCursor);
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
  _displayTreeExpandAllButton->setCursor(Qt::PointingHandCursor);
  _displayTreeExpandAllButton->setFlat(true);
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

  qApp->setStyleSheet(qApp->styleSheet());

  // Set placeholder text color via palette
  QPalette palette = ui->textEditDescription->palette();
  palette.setColor(QPalette::PlaceholderText, QColor("#808080")); // Bright blue
  ui->textEditDescription->setPalette(palette);

  _errorPixmap = new QPixmap(QSize(128, 128));
  _errorPixmap->fill(Qt::transparent);

  QSvgRenderer svgRenderer(QStringLiteral(":/icons/error.svg"));
  QPainter painter(_errorPixmap);
  svgRenderer.render(&painter);

  ui->labelErrorImage->setPixmap(*_errorPixmap);

  _historyModel->setSelectionModel(ui->historyView->selectionModel());

  showMainSpinnerPage(false);
  showSpinnerOverlay();

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

  connect(ui->remotePage, &RemoteGitBranchesPage::branchSelected, this,
          &GitRepoContainer::onRemoteBranchSelected);

  connect(ui->tagsPage, &GitTagsPage::tagSelected, this,
          &GitRepoContainer::onTagSelected);

  connect(_spinnerOverlayTimer, &QTimer::timeout, this,
          &GitRepoContainer::onShowSpinnerOverlay);

  connect(header, &ClickableHeaderView::headerDoubleClicked, this,
          &GitRepoContainer::onGraphColumnExpand);

  connect(ui->historyView->selectionModel(),
          &QItemSelectionModel::selectionChanged, this,
          &GitRepoContainer::onHistorySelectionChanged);

  connect(
    ui->tabWidgetPageDisplayFiles, &QTabWidget::currentChanged, this,
    [=](int index) { _displayTreeExpandAllButton->setVisible(index != 0); });

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

  connect(ui->pushButtonCommit, &QPushButton::clicked, this,
          &GitRepoContainer::onCommitButtonClicked);

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

  LOG_PERF(0, "Exit")
}

GitRepoContainer::~GitRepoContainer() {
  LOG_PERF(0, "Enter");

  // Also quit GitRepo jobs
  closeRepo();

  _historyModel->quitUpdate();

  delete _errorPixmap;

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::init() {
  ContainerWidget::init();

  openRepo();
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
    }
  }

  return QObject::eventFilter(watched, event);
}

void GitRepoContainer::hideEvent(QHideEvent *event) {
  LOG_PERF(0, "Enter");

  //_spinnerOverlay->hide();

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

  const QModelIndex shaIndex =
      _historyModel->index(index.row(), COLUMN_COMMIT_FULL_SHA, QModelIndex());
  if (!shaIndex.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString commit = shaIndex.data(Qt::DisplayRole).toString();

  LOCKGUARD(_tooltipsMutex);

  _tooltipsToShow.clear();
  _tooltipsToShow += commit;

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

      if (const auto locked = guard.lock()) {
        const QPoint viewportPos =
            ui->historyView->viewport()->mapFromGlobal(_mousePos);

        if (const QModelIndex idx = ui->historyView->indexAt(viewportPos);
            idx != index) {
          LOG_PERF(0, "Exit");
          return;
        }
      }
    }

    const qlonglong tooltipDisplayTime =
        _configManager.getLong(ConfigManager::ConfigKey::TooltipDisplayTime);

    const QString tooltipText =
        QString("On branches:\n\n%1").arg(commitBranches.join("\n"));

    Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
      QToolTip::showText(_mousePos, tooltipText, ui->historyView, {},
                         static_cast<int>(tooltipDisplayTime));
    });

    LOG_PERF(0, "Exit");
  });
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

void GitRepoContainer::onCommitButtonClicked() {
  LOG_PERF(0, "Enter");

  const Common::UserProfile &currentProfile =
    _userProfileManager.currentProfile();
  const QString summary = ui->lineEditSummary->text().trimmed();
  const QString description = ui->textEditDescription->toPlainText().trimmed();

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() {
    commit(guard, summary, description, currentProfile.author,
           currentProfile.email);
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCommitAndMergeIndexTreeUnstageAllButtonClicked() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { unstageAll(guard); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onCommitAndMergeWorkingTreeTreeStageAllButtonClicked() {
  LOG_PERF(0, "Enter");

  const WeakPtr guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stageAll(guard); });

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

void GitRepoContainer::onLocalBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");

  ui->historyView->setVisible(false);

  const qlonglong spinnerOverlayTimerTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);

  _spinnerOverlayTimer->start(spinnerOverlayTimerTimeout);

  checkoutBranch(getSelf().toWeakRef(), branchName, GitRepo::BranchType::Local);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRemoteBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");

  ui->historyView->setVisible(false);

  const qlonglong spinnerOverlayTimerTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);

  _spinnerOverlayTimer->start(spinnerOverlayTimerTimeout);

  checkoutBranch(getSelf().toWeakRef(), branchName,
                 GitRepo::BranchType::Remote);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onTagSelected(const QString &tagName) {
  LOG_PERF(0, "Enter");

  ui->historyView->setVisible(false);

  const qlonglong spinnerOverlayTimerTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);

  _spinnerOverlayTimer->start(spinnerOverlayTimerTimeout);

  checkoutBranch(getSelf().toWeakRef(), tagName, GitRepo::BranchType::Tag);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onShowSpinnerOverlay() const {
  LOG_PERF(0, "Enter");

  showSpinnerOverlay();

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

  const QStringList files =
    getSelectedFilesWhichExist(ui->listViewTabWidgetWorkingTreeFlat);

  QMenu contextMenu;
  contextMenu.addAction(ui->actionWorkingTreeFlatStage);
  contextMenu.addAction(ui->actionWorkingTreeFlatDiscard);

  if (!files.isEmpty()) {
    contextMenu.addAction(ui->actionWorkingTreeFlatIgnore);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionWorkingTreeFlatCreateFilePatch);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionWorkingTreeFlatDelete);
  }

  contextMenu.exec(ui->listViewTabWidgetWorkingTreeFlat->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewStageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
    getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
    getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeFlatListViewCreateFilePatchSelected(
  bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
    getSelectedFiles(ui->listViewTabWidgetWorkingTreeFlat);

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

  const QStringList files =
    getSelectedFilesWhichExist(ui->treeViewTabWidgetWorkingTreeTree);

  QMenu contextMenu;
  contextMenu.addAction(ui->actionWorkingTreeTreeStage);
  contextMenu.addAction(ui->actionWorkingTreeTreeDiscard);

  if (!files.isEmpty()) {
    contextMenu.addAction(ui->actionWorkingTreeTreeIgnore);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionWorkingTreeTreeCreateFilePatch);
    contextMenu.addSeparator();
    contextMenu.addAction(ui->actionWorkingTreeTreeDelete);
  }

  contextMenu.exec(ui->treeViewTabWidgetWorkingTreeTree->mapToGlobal(pos));

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewStageSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
    getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { stageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
    getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onWorkingTreeTreeTreeViewCreateFilePatchSelected(
  bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files =
    getSelectedFiles(ui->treeViewTabWidgetWorkingTreeTree);

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

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { discardFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewContextMenuRequested(
  const QPoint &pos) {
  LOG_PERF(0, "Enter");

  const QModelIndex index = ui->listViewTabWidgetIndexFlat->indexAt(pos);
  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QStringList files =
    getSelectedFilesWhichExist(ui->listViewTabWidgetIndexFlat);

  QMenu contextMenu;
  contextMenu.addAction(ui->actionIndexFlatUnstage);
  contextMenu.addAction(ui->actionIndexFlatDiscard);

  if (!files.isEmpty()) {
    contextMenu.addAction(ui->actionIndexFlatIgnore);
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

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { unstageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexFlatListViewCreateFilePatchSelected(
  bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->listViewTabWidgetIndexFlat);

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
    contextMenu.addAction(ui->actionIndexTreeIgnore);
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

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { unstageFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewIgnoreSelected(bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

  const WeakPtr &guard = getSelf().toWeakRef();

  _dispatcher.dispatch([=]() { ignoreFiles(guard, files); });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onIndexTreeTreeViewCreateFilePatchSelected(
  bool checked) {
  LOG_PERF(0, "Enter");

  const QStringList files = getSelectedFiles(ui->treeViewTabWidgetIndexTree);

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

void GitRepoContainer::checkoutBranch(const WeakPtr &guard,
                                      const QString &branchName,
                                      const GitRepo::BranchType branchType) {
  LOG_PERF(0, "Enter");

  if (branchName == ALL_BRANCHES) {
    updateModelActiveBranch(guard, branchName, branchType);
  } else {
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
        _spinnerOverlayTimer->stop();

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
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _spinnerOverlayTimer->stop();

        showSpinnerOverlay(false);

        showErrorToast(QString(tr("Failed to checkout branch. %1"))
                           .arg(additionalClarificationMessage),
                       tr("Checkout"));
      });

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
  }

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
                              const QString &email) {
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to commit staged changes."), tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    if (commitSuccess) {
      clearSummaryAndDescription();
    }
  });

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
      showErrorToast(tr("Failed to delete some files."), tr("Error"));
    });
  }

  refreshStatus();

  Util::Dispatcher::dispatchToMainThread(
    guard, [&]() { simulateClickOnFirstHistoryRow(); });

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
      showErrorPage();
      showErrorToast(tr("Failed to create diff file."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerDiffFilesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to create diff file."), tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const QString diff = commandResult.value.toString();

  if (diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showToast(tr("No difference compared to the HEAD commit."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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
    out << diff;
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
      showErrorPage();
      showErrorToast(tr("Failed to discard file(s)."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerDiscardFilesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to discard file(s)."), tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool discarded = commandResult.value.toBool();

  if (!discarded) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showToast(tr("Failed to discard file(s)."), tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  deleteFiles(guard, files);

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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the active local branch."),
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

  // Release allocated memory
  _historyModel->updateHistoryAndStatus(GitRepo::GitHistoryWithStatus());

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
      _spinnerOverlayTimer->stop();
      showSpinnerOverlay(false);
      ui->historyView->setVisible(false);
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _spinnerOverlayTimer->stop();
      showSpinnerOverlay(false);
      ui->historyView->setVisible(false);
      showErrorToast(QString(tr("Failed to retrieve the history of branch %1."))
                         .arg(_activeBranch),
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
      _spinnerOverlayTimer->stop();
      showSpinnerOverlay(false);
      ui->historyView->setVisible(false);
      showErrorPage();
      showErrorToast(tr("Failed to retrieve the repository status."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::GitRepoManagerStatusResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _spinnerOverlayTimer->stop();
      showSpinnerOverlay(false);
      ui->historyView->setVisible(false);
      showErrorToast(tr("Failed to retrieve the repository status."),
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of local branches."),
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of remote branches."),
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of submodules."),
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of tags."), tr("Error"));
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

void GitRepoContainer::ignoreFiles(const WeakPtr &guard,
                                   const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString ignoreFilePath =
    QDir::toNativeSeparators(_path + QDir::separator() + IGNORE_FILENAME);

  QFile file(ignoreFilePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showErrorToast(
        QString(tr("Failed to open %1 for writing.")).arg(IGNORE_FILENAME),
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

  refreshStatus();

  Util::Dispatcher::dispatchToMainThread(
    guard, [&]() { simulateClickOnFirstHistoryRow(); });

  LOG_PERF(0, "Exit")
}

bool GitRepoContainer::openRepo() {
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

  const WeakPtr guard = getSelf().toWeakRef();

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

  LOG_PERF(0, "Exit")

  return refreshHistorySuccess;
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

  if (!ok) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showSpinnerOverlay(false);
      showErrorPage();
    });
  }

  LOG_PERF(0, "Exit")

  return ok;
}

void GitRepoContainer::refreshStatus() {
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

  const WeakPtr &guard = getSelf().toWeakRef();

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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the repository status."),
                     tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto statusCommandResult = statusResultEvent.data<GitRepoResult>();

  const auto status = statusCommandResult.value.value<GitRepo::GitStatus>();

  _historyModel->updateStatus(status);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetCommitAndMergeIndexTreeUnstageAllButton() {
  LOG_PERF(0, "Enter");

  _commitAndMergeIndexTreeUnstageAllButton->setText(tr("Unstage all"));
  _commitAndMergeIndexTreeUnstageAllButton->setVisible(
    _commitStagedListModel && _commitStagedListModel->rowCount() > 0);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetCommitAndMergeWorkingTreeTreeStageAllButton() {
  LOG_PERF(0, "Enter");

  _commitAndMergeWorkingDirTreeStageAllButton->setText(tr("Stage all"));
  _commitAndMergeWorkingDirTreeStageAllButton->setVisible(
    _commitListModel && _commitListModel->rowCount() > 0);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::resetCommitButton() {
  LOG_PERF(0, "Enter");

  ui->pushButtonCommit->setEnabled(
    _commitStagedListModel && _commitStagedListModel->rowCount() > 0 &&
    (!ui->lineEditSummary->text().isEmpty() ||
                                                                         !ui->textEditDescription->toPlainText().isEmpty()));

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

  const int totalWidth = ui->historyView->viewport()->width();
  int colSummary = totalWidth / 2;
  if (constexpr int maxSummaryWidth = 600; colSummary > maxSummaryWidth)
    colSummary = maxSummaryWidth;

  const int remaining = totalWidth - colSummary;
  const int colOther = remaining / 5;

  ui->historyView->setColumnWidth(0, colOther);
  ui->historyView->setColumnWidth(1, colSummary);
  ui->historyView->setColumnWidth(2, colOther);
  ui->historyView->setColumnWidth(3, colOther);
  ui->historyView->setColumnWidth(4, colOther);
  ui->historyView->setColumnWidth(5, colOther);

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
    }

    _commitTreeModel.reset(Models::GitCommitTreeModel::from(commitStatus));
    if (_commitTreeModel) {
      ui->treeViewPageDisplayFilesTree->setModel(_commitTreeModel.get());
    }
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::showCommitAndMerge(WeakPtr guard) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus &historyAndStatus =
    _historyModel->historyWithStatus();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _commitListModel.reset(
      Models::GitCommitListModel::from(historyAndStatus.status));
    if (_commitListModel) {
      ui->listViewTabWidgetWorkingTreeFlat->setModel(_commitListModel.get());
    }

    _commitTreeModel.reset(
      Models::GitCommitTreeModel::from(historyAndStatus.status));
    if (_commitTreeModel) {
      ui->treeViewTabWidgetWorkingTreeTree->setModel(_commitTreeModel.get());
    }

    _commitStagedListModel.reset(
      Models::GitCommitListModel::fromStaged(historyAndStatus.status));
    if (_commitStagedListModel) {
      ui->listViewTabWidgetIndexFlat->setModel(_commitStagedListModel.get());
    }

    _commitStagedTreeModel.reset(
      Models::GitCommitTreeModel::fromStaged(historyAndStatus.status));
    if (_commitStagedTreeModel) {
      ui->treeViewTabWidgetIndexTree->setModel(_commitStagedTreeModel.get());
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

  ui->historyView->setVisible(!spinnerEnabled);

  if (spinnerEnabled) {
    _spinnerOverlay->startSpinning();
    _spinnerOverlay->setText("0%");
    _spinnerOverlay->raise();
    _spinnerOverlay->show();
  } else {
    _spinnerOverlay->hide();
    _spinnerOverlay->stopSpinning();
    _spinnerOverlay->setText("");
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

void GuiManager::Containers::GitRepoContainer::simulateClickOnFirstHistoryRow()
  const {
  LOG_PERF(0, "Enter");

  const QModelIndex index =
    ui->historyView->model()->index(0, 0); // First row, first column
  ui->historyView->selectionModel()->select(
    index, QItemSelectionModel::Select | QItemSelectionModel::Rows);
  ui->historyView->setCurrentIndex(index);

  ui->historyView->setFocus();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::stageAll(const WeakPtr &guard) {
  LOG_PERF(0, "Enter");

  const GitRepo::GitHistoryWithStatus &historyAndStatus =
    _historyModel->historyWithStatus();

  const QStringList modifiedFiles = historyAndStatus.status.newFiles +
                                    historyAndStatus.status.modifiedFiles +
                                    historyAndStatus.status.deletedFiles +
                                    historyAndStatus.status.renamedFiles +
                                    historyAndStatus.status.conflictedFiles;

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
      showErrorPage();
      showErrorToast(tr("Failed to stage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerStageFilesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to stage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool stageAllSuccess = commandResult.value.toBool();

  refreshStatus();

  if (!stageAllSuccess) {

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to stage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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
      showErrorPage();
      showErrorToast(tr("Failed to unstage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerUnstageFilesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to unstage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool unstageAllSuccess = commandResult.value.toBool();

  refreshStatus();

  if (!unstageAllSuccess) {

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to unstage files."), tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    simulateClickOnFirstHistoryRow();
    resetCommitAndMergeIndexTreeUnstageAllButton();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateBranchHistory(
    const WeakPtr &guard,
    const GitRepo::GitHistoryWithStatus &historyWithStatus) const {
  LOG_PERF(0, "Enter");

  _historyModel->updateHistoryAndStatus(
      historyWithStatus, [&](const int progress) {
        Util::Dispatcher::dispatchToMainThread(guard, [=]() {
          const QString progressStr = QString("%1%").arg(progress);
          if (_spinnerOverlay) {
            _spinnerOverlay->setText(progressStr);
          }
        });
      });

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _spinnerOverlayTimer->stop();
    showSpinnerOverlay(false);
    stretchBranchHistoryTableColumns();

    // Simulate click on first row
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

void GitRepoContainer::updateSubmodulesModel(const WeakPtr &guard,
                                             const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  if (const auto locked = guard.lock()) {
    ui->submodulesPage->updateModel(tags);
  }

  LOG_PERF(0, "Exit")
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
