#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"
#include "widgets/ClickableHeaderView.h"
#include "widgets/HistorySliceTableColumnDelegate.h"
#include "widgets/InfoToast.h"

#include <Dispatcher.h>
#include <GitRepoManagerStructs.h>
#include <Log.h>
#include <Uuid.h>

#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <QToolTip>

#include <QScrollBar>
#include <functional>
#include <utility>

#define THREAD_ID "GitRepoContainer"

#define ALL_BRANCHES "*"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

namespace GuiManager::Containers {

GitRepoContainer::GitRepoContainer(QString path, QWidget *parent)
    : ContainerWidget(Widgets::ContainerType::Unknown, parent),
      ui(new Ui::GitRepoContainer), _uuid(Uuid::createUuid()),
      _path(std::move(path)), _activeBranchType(GitRepo::BranchType::All),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _eventManager(EventManager::EventManager::getInstance()),
      _toastHost(ToastHostWidget::getInstance()),
      _historyModel(new Models::GitHistoryModel(this)), _dispatcher(1),
      _mousePos(-1, -1), _spinnerOverlay(new SpinnerOverlayWidget(this)),
      _spinnerOverlayTimer(new QTimer(this)), _quitGettingHistory(false),
      _resizedHeaderSection(-1), _resizedHeaderSectionPreviousWidth(-1) {
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
  ui->splitter->setStretchFactor(0, 1);
  ui->splitter->setStretchFactor(1, 8);
  ui->splitter->setStretchFactor(2, 1);
  ui->submodulesPage->setContentsMargins(0, 0, 0, 0);
  ui->historyView->setModel(_historyModel);
  ui->historyView->setItemDelegateForColumn(
      0, new HistorySliceTableColumnDelegate(ui->historyView));
  ui->historyView->setMouseTracking(true);
  ui->historyView->viewport()->setMouseTracking(true);
  ui->historyView->viewport()->installEventFilter(this);
  ui->spinner->setColor(Qt::lightGray);

  auto *header = new ClickableHeaderView(Qt::Horizontal, ui->historyView);
  ui->historyView->setHorizontalHeader(header);

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

  connect(_spinnerOverlayTimer, &QTimer::timeout, this,
          &GitRepoContainer::onShowSpinnerOverlay);

  connect(header, &ClickableHeaderView::headerDoubleClicked, this,
          &GitRepoContainer::onGraphColumnExpand);

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTagsError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerListTagsResult,
                       eventCallbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.subscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerOpenRepoResult,
                       eventCallbackFunc));

  refreshViews();

  LOG_PERF(0, "Exit")
}

GitRepoContainer::~GitRepoContainer() {
  LOG_PERF(0, "Enter");

  closeRepo();

  _historyModel->quitUpdate();

  _dispatcher.joinAll();

  const auto eventCallbackFunc =
      std::bind(&GitRepoContainer::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetActiveBranchResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListLocalBranchesResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesError,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListSubmodulesResult,
      eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerListTagsError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerListTagsResult,
                       eventCallbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerOpenRepoError, eventCallbackFunc));
  _eventManager.unsubscribe(
      CAPTURE_CALLBACK(EventManager::EventType::GitRepoManagerOpenRepoResult,
                       eventCallbackFunc));

  delete _errorPixmap;
  delete ui;

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

  /*
  _spinnerOverlay->show();
  _spinnerOverlay->raise();
  */

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

void GitRepoContainer::onHistoryViewItemEntered(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.column() != COLUMN_HISTORY_SLICE) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QModelIndex shaIndex =
      _historyModel->index(index.row(), COLUMN_COMMIT_FULL_SHA, QModelIndex());
  if (!shaIndex.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString commit = shaIndex.data(Qt::DisplayRole).toString();
  const QModelIndex slicesIndex =
      _historyModel->index(index.row(), COLUMN_HISTORY_SLICE, QModelIndex());
  if (!slicesIndex.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto slices = slicesIndex.data(Qt::DisplayRole).value<Models::Slices>();
  const qsizetype height = ui->historyView->rowHeight(index.row());
  int posX = 0;

  for (int i = 0; i < slices.size(); ++i) {
    if (slices[i].main) {
      posX = i;
      break;
    }
  }

  const QRect viewportRect = ui->historyView->viewport()->rect();
  const int row = (_mousePos.y() - viewportRect.top()) / height;

  if (row < 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QPoint topLeftLocal(viewportRect.left() + posX * height,
                            viewportRect.top() + row * height);
  const QPoint bottomRightLocal(viewportRect.left() + (posX + 1) * height,
                                viewportRect.top() + (row + 1) * height);
  const QPoint topLeftGlobal =
      ui->historyView->viewport()->mapToGlobal(topLeftLocal);
  const QPoint bottomRightGlobal =
      ui->historyView->viewport()->mapToGlobal(bottomRightLocal);

  if (topLeftGlobal.x() < _mousePos.x() &&
      _mousePos.x() > bottomRightGlobal.x()) {
    LOG_PERF(0, "Exit");
    return;
  }

  LOCKGUARD(_tooltipsMutex);

  _tooltipsToShow.clear();
  _tooltipsToShow += commit;

  _dispatcher.dispatch([&, commit, index]() {
    LOG_PERF(0, "Enter");

    const QStringList commitBranches = getCommitBranches(commit);

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

    Util::Dispatcher::dispatchToMainThread([=]() {
      QToolTip::showText(_mousePos, tooltipText, ui->historyView, {},
                         static_cast<int>(tooltipDisplayTime));
    });

    LOG_PERF(0, "Exit");
  });

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onLocalBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");

  ui->historyView->setVisible(false);

  const qlonglong spinnerOverlayTimerTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);

  _spinnerOverlayTimer->start(spinnerOverlayTimerTimeout);

  updateLocalBranchesModelActiveBranch(branchName);

  LOG_PERF(0, "Exit");
}

void GitRepoContainer::onRemoteBranchSelected(const QString &branchName) {
  LOG_PERF(0, "Enter");

  ui->historyView->setVisible(false);

  const qlonglong spinnerOverlayTimerTimeout = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);

  _spinnerOverlayTimer->start(spinnerOverlayTimerTimeout);

  updateRemoteBranchesModelActiveBranch(branchName);

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

void GitRepoContainer::closeRepo() const {
  LOG_PERF(0, "Enter");

  // Release allocated memory
  _historyModel->updateHistory(GitRepo::GitHistory());

  GitRepoCloseCommand closeCommand;
  closeCommand.uuid = _uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::GitRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", static_cast<int>(eventType));

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerGetActiveBranchError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerGetActiveBranchResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    const auto [branchName, branchType] =
        commandResult.value.value<GitRepoGetActiveBranchCommandResult>();

    Util::Dispatcher::dispatchToMainThread([=]() {
      switch (branchType) {
      case GitRepo::BranchType::Local: {
        updateLocalBranchesModelActiveBranch(branchName);
        break;
      }
      case GitRepo::BranchType::Remote: {
        updateRemoteBranchesModelActiveBranch(branchName);
        break;
      }
      default: {
        LOG_WARN("Unsupported branch type {} for branch name \"{}\"",
                 branchType, branchName);
        break;
      }
      }
    });
    break;
  }
  case EventManager::EventType::GitRepoManagerGetBranchHistoryError: {
    _spinnerOverlayTimer->stop();

    showErrorPage();

    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerGetBranchHistoryResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    const GitRepo::GitHistory history =
        commandResult.value.value<GitRepo::GitHistory>();
    _dispatcher.dispatch([=]() { updateBranchHistory(history); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranchesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListLocalBranchesResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    const QStringList localBranches = commandResult.value.value<QStringList>();
    Util::Dispatcher::dispatchToMainThread(
        [=]() { updateLocalBranchesModel(localBranches); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemoteBranchesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListRemoteBranchesResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    const QStringList remoteBranches = commandResult.value.value<QStringList>();
    Util::Dispatcher::dispatchToMainThread(
        [=]() { updateRemoteBranchesModel(remoteBranches); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListSubmodulesError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListSubmodulesResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    const QStringList submodules = commandResult.value.value<QStringList>();
    Util::Dispatcher::dispatchToMainThread(
        [=]() { updateSubmodulesModel(submodules); });
    break;
  }
  case EventManager::EventType::GitRepoManagerListTagsError: {
    // Show a toast with error
    break;
  }
  case EventManager::EventType::GitRepoManagerListTagsResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    const QStringList tags = commandResult.value.value<QStringList>();
    Util::Dispatcher::dispatchToMainThread([=]() { updateTagsModel(tags); });
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoError: {
    // Show a toast with error
    if (const GitRepoError gitError = event.data<GitRepoError>();
        gitError.uuid != _uuid)
      break;

    Util::Dispatcher::dispatchToMainThread([=]() {
      showErrorPage();
      showToast(tr("Can't open repository"));
    });
    break;
  }
  case EventManager::EventType::GitRepoManagerOpenRepoResult: {
    const auto commandResult = event.data<GitRepoResult>();
    if (commandResult.uuid != _uuid) {
      break;
    }

    getLocalBranches();
    getActiveLocalBranch();
    getRemoteBranches();
    getTags();
    getSubmodules();
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getActiveLocalBranch() const {
  LOG_PERF(0, "Enter");

  GitRepoGetActiveBranchCommand command;
  command.uuid = _uuid;
  command.branchType = GitRepo::BranchType::Local;

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::GitRepoManagerGetActiveBranch);
  getActiveBranchEvent.addData(command);
  _eventManager.send(getActiveBranchEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getActiveRemoteBranch() const {
  LOG_PERF(0, "Enter");

  GitRepoGetActiveBranchCommand command;
  command.uuid = _uuid;
  command.branchType = GitRepo::BranchType::Remote;

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::GitRepoManagerGetActiveBranch);
  getActiveBranchEvent.addData(command);
  _eventManager.send(getActiveBranchEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getBranchHistory() const {
  LOG_PERF(0, "Enter");

  // Release allocated memory
  _historyModel->updateHistory(GitRepo::GitHistory());

  GitRepoGetBranchHistoryCommand command;
  command.uuid = _uuid;
  command.branchName = _activeBranch;
  command.branchType = _activeBranchType;

  EventManager::Event historyEvent(
      EventManager::EventType::GitRepoManagerGetBranchHistory);
  historyEvent.addData(command);
  _eventManager.send(historyEvent);

  ui->labelProgress->setText("0%");

  LOG_PERF(0, "Exit")
}

QStringList GitRepoContainer::getCommitBranches(const QString &commit) const {
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
  LOG_PERF(0, "Exit")
  return branches;
}

void GitRepoContainer::getLocalBranches() const {
  LOG_PERF(0, "Enter");

  GitRepoListLocalBranchesCommand command;
  command.uuid = _uuid;

  EventManager::Event listLocalEvent(
      EventManager::EventType::GitRepoManagerListLocalBranches);
  listLocalEvent.addData(command);
  _eventManager.send(listLocalEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getRemoteBranches() const {
  LOG_PERF(0, "Enter");

  GitRepoListRemoteBranchesCommand command;
  command.uuid = _uuid;

  EventManager::Event listRemoteEvent(
      EventManager::EventType::GitRepoManagerListRemoteBranches);
  listRemoteEvent.addData(command);
  _eventManager.send(listRemoteEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getSubmodules() const {
  LOG_PERF(0, "Enter");

  GitRepoListSubmodulesCommand command;
  command.uuid = _uuid;

  EventManager::Event listSubmodulesEvent(
      EventManager::EventType::GitRepoManagerListSubmodules);
  listSubmodulesEvent.addData(command);
  _eventManager.send(listSubmodulesEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::getTags() const {
  LOG_PERF(0, "Enter");

  GitRepoListTagsCommand command;
  command.uuid = _uuid;

  EventManager::Event listTagsEvent(
      EventManager::EventType::GitRepoManagerListTags);
  listTagsEvent.addData(command);
  _eventManager.send(listTagsEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::openRepo() const {
  LOG_PERF(0, "Enter");

  GitRepoOpenCommand openCommand;
  openCommand.uuid = _uuid;
  openCommand.path = _path;

  EventManager::Event openRepoEvent(
      EventManager::EventType::GitRepoManagerOpenRepo);
  openRepoEvent.addData(openCommand);
  _eventManager.send(openRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::refreshViews() const {
  LOG_PERF(0, "Enter");

  openRepo();

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

void GitRepoContainer::showToast(const QString &message) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  _toastHost.addToast(new InfoToast(message));

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateBranchHistory(
    const GitRepo::GitHistory &history) const {
  LOG_PERF(0, "Enter");

  _historyModel->updateHistory(history, [&](const int progress) {
    Util::Dispatcher::dispatchToMainThread([=]() {
      const QString progressStr = QString("%1%").arg(progress);
      if (_spinnerOverlay) {
        _spinnerOverlay->setText(progressStr);
      }
    });
  });

  Util::Dispatcher::dispatchToMainThread([&]() {
    _spinnerOverlayTimer->stop();
    showSpinnerOverlay(false);
    stretchBranchHistoryTableColumns();
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModel(
    const QStringList &localBranches) const {
  LOG_PERF(0, "Enter");

  QStringList finalBranches;
  finalBranches += ALL_BRANCHES;
  finalBranches += localBranches;

  ui->localPage->updateModel(finalBranches);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModelActiveBranch(
    const QString &activeBranch, const bool updatePages) {
  LOG_PERF(0, "Enter");

  _activeBranch = activeBranch;
  _activeBranchType = GitRepo::BranchType::Local;

  if (updatePages) {
    ui->localPage->updateActiveBranch(activeBranch);
    ui->remotePage->updateActiveBranch(
        QString()); // deselect remote branch if any
  }

  getBranchHistory();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModel(
    const QStringList &remoteBranches) const {
  LOG_PERF(0, "Enter");

  QStringList finalBranches;
  finalBranches += ALL_BRANCHES;
  finalBranches += remoteBranches;

  ui->remotePage->updateModel(finalBranches);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModelActiveBranch(
    const QString &activeBranch, const bool updatePages) {
  LOG_PERF(0, "Enter");

  _activeBranch = activeBranch;
  _activeBranchType = GitRepo::BranchType::Remote;

  if (updatePages) {
    ui->localPage->updateActiveBranch(
        QString()); // deselect local branch if any
    ui->remotePage->updateActiveBranch(activeBranch);
  }

  getBranchHistory();

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateSubmodulesModel(const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  ui->submodulesPage->updateModel(tags);

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateTagsModel(const QStringList &tags) const {
  LOG_PERF(0, "Enter");

  ui->tagsPage->updateModel(tags);

  LOG_PERF(0, "Exit")
}

} // namespace GuiManager::Containers
