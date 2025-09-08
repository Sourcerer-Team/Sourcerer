#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"
#include "widgets/ClickableHeaderView.h"
#include "widgets/ErrorToast.h"
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
      _historyModel(new Models::GitHistoryModel()), _dispatcher(1),
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
  ui->historyView->setItemDelegateForColumn(
      0, new HistorySliceTableColumnDelegate(ui->historyView));
  ui->historyView->setMouseTracking(true);
  ui->historyView->viewport()->setMouseTracking(true);
  ui->historyView->viewport()->installEventFilter(this);
  ui->historyView->setModel(_historyModel.get());
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

  connect(ui->tagsPage, &GitTagsPage::tagSelected, this,
          &GitRepoContainer::onTagSelected);

  connect(_spinnerOverlayTimer, &QTimer::timeout, this,
          &GitRepoContainer::onShowSpinnerOverlay);

  connect(header, &ClickableHeaderView::headerDoubleClicked, this,
          &GitRepoContainer::onGraphColumnExpand);

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

  refreshViews();
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

  LOCKGUARD(_tooltipsMutex);

  _tooltipsToShow.clear();
  _tooltipsToShow += commit;

  const auto weak = getSelf().toWeakRef();

  _dispatcher.dispatch([&, commit, index, weak]() {
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

      if (const auto locked = weak.lock()) {
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

    if (!success) {
      _spinnerOverlayTimer->stop();

      showSpinnerOverlay(false);

      Util::Dispatcher::dispatchToMainThread(guard, [&]() {
        showErrorToast(
            tr("Failed to checkout branch.\nPlease check if there are "
               "some uncommited files left."));
      });

      LOG_PERF(0, "Exit")
      return;
    }

    if (event.type() !=
        EventManager::EventType::GitRepoManagerCheckoutBranchResult) {
      _spinnerOverlayTimer->stop();

      showSpinnerOverlay(false);

      Util::Dispatcher::dispatchToMainThread(guard, [&]() {
        showErrorToast(
            tr("Failed to checkout branch.\nPlease check if there are "
               "some uncommited files left."));
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the active local branch."));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetActiveBranchResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the active local branch."));
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

bool GitRepoContainer::getBranchHistory(const WeakPtr &guard) const {
  LOG_PERF(0, "Enter");

  // Release allocated memory
  _historyModel->updateHistory(GitRepo::GitHistory());

  const QString eventUuid = Uuid::createUuid();

  const int token = _eventManager.startWait();

  GitRepoGetBranchHistoryCommand command;
  command.uuid = _uuid;
  command.branchName = _activeBranch;
  command.branchType = _activeBranchType;
  command.syncUuid = eventUuid;

  EventManager::Event historyEvent(
      EventManager::EventType::GitRepoManagerGetBranchHistory, eventUuid);
  historyEvent.addData(command);
  _eventManager.send(historyEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult};

  const auto &[success, event] =
      _eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _spinnerOverlayTimer->stop();
      showSpinnerOverlay(false);
      ui->historyView->setVisible(false);
      showErrorToast(QString(tr("Failed to retrieve the history of branch %1."))
                         .arg(_activeBranch));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _spinnerOverlayTimer->stop();
      showSpinnerOverlay(false);
      ui->historyView->setVisible(false);
      showErrorToast(QString(tr("Failed to retrieve the history of branch %1."))
                         .arg(_activeBranch));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto history = commandResult.value.value<GitRepo::GitHistory>();

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { ui->labelProgress->setText("0%"); });

  updateBranchHistory(guard, history);

  LOG_PERF(0, "Exit")

  return true;
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of local branches."));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListLocalBranchesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of local branches."));
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of remote branches."));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListRemoteBranchesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of remote branches."));
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of submodules."));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListSubmodulesResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of submodules."));
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
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of tags."));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerListTagsResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      showErrorToast(tr("Failed to retrieve the list of tags."));
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

void GitRepoContainer::refreshViews() {
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

void GitRepoContainer::showErrorToast(const QString &message) const {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing a toast with the message '{}'.", message);

  _toastHost.addToast(new ErrorToast(message));

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateBranchHistory(const WeakPtr &guard,
                                           const GitRepo::GitHistory &history) const {
  LOG_PERF(0, "Enter");

  _historyModel->updateHistory(history, [&](const int progress) {
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
  });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateLocalBranchesModel(
    const WeakPtr &guard, const QStringList &localBranches) const {
  LOG_PERF(0, "Enter");

  QStringList finalBranches;
  finalBranches += ALL_BRANCHES;
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
    ui->remotePage->updateActiveBranch(
        QString());                           // deselect remote branch if any
    ui->tagsPage->updateActiveTag(QString()); // deselect remote branch if any
  }

  _dispatcher.dispatch([=]() { (void)getBranchHistory(guard); });

  LOG_PERF(0, "Exit")
}

void GitRepoContainer::updateRemoteBranchesModel(
    const WeakPtr &guard, const QStringList &remoteBranches) const {
  LOG_PERF(0, "Enter");

  QStringList finalBranches;
  finalBranches += ALL_BRANCHES;
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
    ui->localPage->updateActiveBranch(
        QString()); // deselect local branch if any
    ui->remotePage->updateActiveBranch(activeBranch);
    ui->tagsPage->updateActiveTag(QString()); // deselect local branch if any
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
    ui->localPage->updateActiveBranch(
        QString()); // deselect local branch if any
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
