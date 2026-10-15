#include "containers/SvnRepoManagerBridgeImpl.h"
#include "DarkThemeLexersHighlighter.h"
#include "MainWindow.h"
#include "MousePositionCorrection.h"
#include "QScintillaLexerFactory.h"
#include "containers/GitRepoManagerBridgeImpl.h"
#include "containers/RepoContainer.h"
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
#include <Log.h>
#include <StylesheetManager.h>
#include <SvnRepoManagerStructs.h>
#include <Uuid.h>

#include <QAbstractItemDelegate>
#include <QCheckBox>
#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QDirIterator>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyleOptionViewItem>
#include <QSvgRenderer>
#include <QTextStream>
#include <QThread>
#include <QThreadPool>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <Qsci/qsciscintilla.h>

#include <algorithm>
#include <functional>
#include <set>
#include <utility>

#define THREAD_ID "RepoContainer"

#define ALL_BRANCHES "*"
#define HEAD "HEAD"

#define MAXIMUM_PARENTS_IN_LABEL 3
#define MAX_COMMIT_SHA_LENGTH 7

#define GIT_IGNORE_FILENAME ".gitignore"
#define GIT_SUBDIR ".git"

using namespace GuiManager::Widgets;
using namespace SvnRepoManager::Structs;

namespace GuiManager::Containers {

enum class MainViewPanes {
  Unknown = -1,

  History = 0,
  Diff = 1,
  Blame = 2,
  ThreeWayDiff = 3,
  StashContent = 4,

  last
};

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

SvnRepoManagerBridgeImpl::SvnRepoManagerBridgeImpl(RepoContainer &container)
    : _container(container) {}

RepoFeatures SvnRepoManagerBridgeImpl::features() const {
  return RepoFeatures::Shelves | RepoFeatures::Externals |
         RepoFeatures::CommitInfoRevision |
         RepoFeatures::HistoryColumnRevision |
         RepoFeatures::HistoryContextMenuCopyToClipboardRevision |
         RepoFeatures::SearchBarRevision | RepoFeatures::StashInfoTitleShelf |
         RepoFeatures::CherryPickDialogRevision;
}

void SvnRepoManagerBridgeImpl::setUpstream(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName,
    const QString &remoteName, const QString &remoteBranchName,
    const std::function<void(const QWeakPointer<RepoContainer> &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoSetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.remoteBranchName = remoteBranchName;
  command.localBranchName = branchName;

  EventManager::Event setUpstreamStashEvent(
      EventManager::EventType::SvnRepoManagerSetUpstream, eventUuid);
  setUpstreamStashEvent.addData(command);
  _container._eventManager.send(setUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerSetUpstreamError,
      EventManager::EventType::SvnRepoManagerSetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to set upstream.")),
          _container.tr("Apply"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerSetUpstreamResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool setUpstreamSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!setUpstreamSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to set upstream."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  // Only refresh the remotes if we're setting upstream for the active branch
  if (branchName == _container._activeBranch) {
    getRemotes(guard);
  }

  if (onSuccess) {
    onSuccess(guard);
  } else {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(_container.tr("Successfully set upstream."),
                           _container.tr("Info"));
    });
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::stash(const QWeakPointer<RepoContainer> &guard,
                                     const QString &note) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.note = note;

  EventManager::Event stashFilesEvent(
      EventManager::EventType::SvnRepoManagerStash, eventUuid);
  stashFilesEvent.addData(command);
  _container._eventManager.send(stashFilesEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerStashError,
                           EventManager::EventType::SvnRepoManagerStashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to stash changes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerStashResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool stashSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!stashSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to stash changes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(
      guard, [&]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::abortMerge(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoAbortMergeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event abortMergeEvent(
      EventManager::EventType::SvnRepoManagerAbortMerge, eventUuid);
  abortMergeEvent.addData(command);
  _container._eventManager.send(abortMergeEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerAbortMergeError,
      EventManager::EventType::SvnRepoManagerAbortMergeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to abort merge.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerAbortMergeResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool abortSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!abortSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to abort merge."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::addSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &url,
    const QString &name, const QString &branch) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoAddSubmoduleCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.url = url;
  command.name = name;
  command.branch = branch;

  EventManager::Event addSubmoduleEvent(
      EventManager::EventType::SvnRepoManagerAddSubmodule, eventUuid);
  addSubmoduleEvent.addData(command);
  _container._eventManager.send(addSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerAddSubmoduleError,
      EventManager::EventType::SvnRepoManagerAddSubmoduleResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to add submodule.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerAddSubmoduleResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool addSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!addSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to add submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(
      guard, [&]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::amend(const QWeakPointer<RepoContainer> &guard,
                                     const QString &summary,
                                     const QString &description,
                                     const QString &author,
                                     const QString &email, bool signoff,
                                     bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoAmendCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;
  command.signoff = signoff;
  command.signCommit = signCommit;

  EventManager::Event amendEvent(EventManager::EventType::SvnRepoManagerAmend,
                                 eventUuid);
  amendEvent.addData(command);
  _container._eventManager.send(amendEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerAmendError,
                           EventManager::EventType::SvnRepoManagerAmendResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to amend changes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerAmendResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to amend changes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    if (commitSuccess) {
      _container.clearSummaryAndDescription();
    }
    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::applyPatch(
    const QWeakPointer<RepoContainer> &guard, const QString &filePath) {
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(
          _container.tr("Can't open file \"%1\" for reading").arg(filePath),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  QTextStream in(&file);
  const QString content = in.readAll();

  file.close();

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoApplyPatchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.patchContent = content;

  EventManager::Event applyPatchEvent(
      EventManager::EventType::SvnRepoManagerApplyPatch, eventUuid);
  applyPatchEvent.addData(command);
  _container._eventManager.send(applyPatchEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerApplyPatchError,
      EventManager::EventType::SvnRepoManagerApplyPatchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to apply patch.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerApplyPatchResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool applySuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!applySuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to apply patch."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(
      guard, [&]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::applyStash(
    const QWeakPointer<RepoContainer> &guard, const int stashIndex,
    const std::function<void(const QWeakPointer<RepoContainer> &, int)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoApplyStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event applyStashEvent(
      EventManager::EventType::SvnRepoManagerApplyStash, eventUuid);
  applyStashEvent.addData(command);
  _container._eventManager.send(applyStashEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerApplyStashError,
      EventManager::EventType::SvnRepoManagerApplyStashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to apply stash.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerApplyStashResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  // Extract result data - now contains success and hasConflicts information
  QMap<QString, QVariant> resultData = commandResult.value.toMap();
  const bool applySuccess = resultData.value("success", false).toBool();
  const bool hasConflicts = resultData.value("hasConflicts", false).toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!applySuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to apply stash."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  // If stash was applied but has conflicts, show merge conflict resolution UI
  if (hasConflicts) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(
          _container.tr(
              "Stash applied with conflicts. Resolve conflicts to continue."),
          _container.tr("Conflicts"));
    });
  }

  if (onSuccess) {
    onSuccess(guard, stashIndex);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { _container.showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::checkoutBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName,
    const int branchType,
    const std::function<void(const QWeakPointer<RepoContainer> &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  if (branchName == ALL_BRANCHES) {
    _container.updateModelActiveBranch(
        guard, branchName, static_cast<GitRepo::BranchType>(branchType));
  } else {
    _container.stopWatcher(guard);

    const QString eventUuid = Uuid::createUuid();

    const int token = _container._eventManager.startWait();

    SvnRepoCheckoutBranchCommand command;
    command.uuid = _container._uuid;
    command.syncUuid = eventUuid;
    command.branchName = branchName;
    command.branchType = static_cast<SvnRepo::BranchType>(branchType);

    EventManager::Event checkoutBranchEvent(
        EventManager::EventType::SvnRepoManagerCheckoutBranch, eventUuid);
    checkoutBranchEvent.addData(command);
    _container._eventManager.send(checkoutBranchEvent);

    static std::set types = {
        EventManager::EventType::SvnRepoManagerCheckoutBranchError,
        EventManager::EventType::SvnRepoManagerCheckoutBranchResult};

    const auto &[success, event] =
        _container._eventManager.waitForAnswer(token, eventUuid, types);

    QString additionalClarificationMessage;

    if (_container._historyModel->hasStatus()) {
      additionalClarificationMessage = _container.tr(
          "\nPlease "
          "commit your changes or stash them before you switch branches.");
    }

    if (!success) {
      closeRepo();

      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showErrorPage();
        _container.showErrorToast(
            QString(_container.tr("Failed to checkout branch. %1"))
                .arg(additionalClarificationMessage),
            _container.tr("Checkout"));
      });

      LOG_PERF(0, "Exit")
      return;
    }

    if (event.type() !=
        EventManager::EventType::SvnRepoManagerCheckoutBranchResult) {
      const auto error = event.data<SvnRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showErrorToast(error.message, _container.tr("Error"));
      });

      _container.prepareWatcher(guard);

      LOG_PERF(0, "Exit")
      return;
    }

    (void)getLocalBranches();

    const auto commandResult = event.data<SvnRepoResult>();

    const auto result =
        commandResult.value.value<SvnRepoCheckoutBranchCommandResult>();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.updateModelActiveBranch(
          guard, result.branchName,
          static_cast<GitRepo::BranchType>(result.branchType));
      _container.showSpinnerOverlay(false);

      // The history reads the active branch, so it is loaded only after
      // updateModelActiveBranch has set it. Otherwise the previous branch's
      // history is shown after every checkout.
      _container._dispatcher.dispatch([=]() { getBranchHistory(guard); });
    });

    if (branchType == static_cast<int>(GitRepo::BranchType::Tag)) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showToast(
            _container
                .tr("You are now in detached HEAD state after checking out "
                    "tag '%1'.")
                .arg(result.branchName),
            _container.tr("Warning"));
      });
    }

    _container.prepareWatcher(guard);

    if (onSuccess) {
      onSuccess(guard);
    }
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::checkoutCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoCheckoutCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event checkoutCommitEvent(
      EventManager::EventType::SvnRepoManagerCheckoutCommit, eventUuid);
  checkoutCommitEvent.addData(command);
  _container._eventManager.send(checkoutCommitEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerCheckoutCommitError,
      EventManager::EventType::SvnRepoManagerCheckoutCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  QString additionalClarificationMessage;

  if (_container._historyModel->hasStatus()) {
    additionalClarificationMessage =
        _container.tr("\nPlease "
                      "commit your changes or stash them before you checkout.");
  }

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to checkout commit. %1"))
              .arg(additionalClarificationMessage),
          _container.tr("Checkout"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerCheckoutCommitResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<SvnRepoResult>();
  const bool checkoutSuccess = commandResult.value.toBool();

  if (!checkoutSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(
          QString(_container.tr("Failed to checkout commit. %1"))
              .arg(additionalClarificationMessage),
          _container.tr("Checkout"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container
                             .tr("You are now in detached HEAD state after "
                                 "checking out commit '%1'.")
                             .arg(commit),
                         _container.tr("Warning"));
  });

  _container.prepareWatcher(guard);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::closeRepo() const {
  LOG_PERF(0, "Enter");

  SvnRepoCloseCommand closeCommand;
  closeCommand.uuid = _container._uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::SvnRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _container._eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::commit(const QWeakPointer<RepoContainer> &guard,
                                      const QString &summary,
                                      const QString &description,
                                      const QString &author,
                                      const QString &email, bool cleanup,
                                      bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoCommitStagedCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;
  command.cleanup = cleanup;
  command.signoff = signoff;
  command.signCommit = signCommit;

  EventManager::Event commitStagedEvent(
      EventManager::EventType::SvnRepoManagerCommitStaged, eventUuid);
  commitStagedEvent.addData(command);
  _container._eventManager.send(commitStagedEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerCommitStagedError,
      EventManager::EventType::SvnRepoManagerCommitStagedResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to commit staged changes."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerCommitStagedResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(
          _container.tr("Failed to commit staged changes."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    if (commitSuccess) {
      _container.clearSummaryAndDescription();
    }
    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::createBranchFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit,
    const QString &branchName,
    const std::function<void(const QWeakPointer<RepoContainer> &,
                             const QString &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoCreateBranchFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.branchName = branchName;

  EventManager::Event createBranchFromCommitEvent(
      EventManager::EventType::SvnRepoManagerCreateBranchFromCommit, eventUuid);
  createBranchFromCommitEvent.addData(command);
  _container._eventManager.send(createBranchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerCreateBranchFromCommitError,
      EventManager::EventType::SvnRepoManagerCreateBranchFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to create branch from commit."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerCreateBranchFromCommitResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  _container.refreshHistory(guard);

  if (onSuccess) {
    onSuccess(guard, branchName);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::createPatchFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoCreatePatchFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event createPatchFromCommitEvent(
      EventManager::EventType::SvnRepoManagerCreatePatchFromCommit, eventUuid);
  createPatchFromCommitEvent.addData(command);
  _container._eventManager.send(createPatchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerCreatePatchFromCommitError,
      EventManager::EventType::SvnRepoManagerCreatePatchFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to create patch from commit."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerCreatePatchFromCommitResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto result =
      commandResult.value.value<SvnRepoCreatePatchFromCommitCommandResult>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);

    if (result.patchContent.isEmpty()) {
      _container.showErrorToast(
          _container.tr("Failed to create patch from commit."),
          _container.tr("Error"));
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

void SvnRepoManagerBridgeImpl::createTagFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit,
    const QString &tagName, const QString &message, bool isAnnotated) {
  LOG_PERF(0, "Enter");

  const QString author =
      Common::UserProfileManager::instance().currentProfile().author;
  const QString email =
      Common::UserProfileManager::instance().currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoCreateTagFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.tagName = tagName;
  command.author = author;
  command.email = email;
  command.message = isAnnotated ? message : QString();

  EventManager::Event createTagFromCommitEvent(
      EventManager::EventType::SvnRepoManagerCreateTagFromCommit, eventUuid);
  createTagFromCommitEvent.addData(command);
  _container._eventManager.send(createTagFromCommitEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerCreateTagFromCommitError,
      EventManager::EventType::SvnRepoManagerCreateTagFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to create tag from commit."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerCreateTagFromCommitResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteBranchEvent(
      EventManager::EventType::SvnRepoManagerDeleteBranch, eventUuid);
  deleteBranchEvent.addData(command);
  _container._eventManager.send(deleteBranchEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteBranchError,
      EventManager::EventType::SvnRepoManagerDeleteBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete branch.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteBranchResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(
      guard, [&]() { _container.showSpinnerOverlay(false); });

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to delete branch."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteRemoteBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteRemoteBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteRemoteBranchEvent(
      EventManager::EventType::SvnRepoManagerDeleteRemoteBranch, eventUuid);
  deleteRemoteBranchEvent.addData(command);
  _container._eventManager.send(deleteRemoteBranchEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteRemoteBranchError,
      EventManager::EventType::SvnRepoManagerDeleteRemoteBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete remote branch.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteRemoteBranchResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(
          _container.tr("Failed to delete remote branch."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(
        _container.tr("Successfully deleted branch from remote."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteStash(
    const QWeakPointer<RepoContainer> &guard, int stashIndex,
    const std::function<void(const QWeakPointer<RepoContainer> &, int)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event deleteStashEvent(
      EventManager::EventType::SvnRepoManagerDeleteStash, eventUuid);
  deleteStashEvent.addData(command);
  _container._eventManager.send(deleteStashEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteStashError,
      EventManager::EventType::SvnRepoManagerDeleteStashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete stash.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteStashResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete stash."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (onSuccess) {
    onSuccess(guard, stashIndex);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { _container.showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteAllStashes(
    const QWeakPointer<RepoContainer> &guard,
    const std::function<void(const QWeakPointer<RepoContainer> &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteAllStashesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event deleteAllStashesEvent(
      EventManager::EventType::SvnRepoManagerDeleteAllStashes, eventUuid);
  deleteAllStashesEvent.addData(command);
  _container._eventManager.send(deleteAllStashesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteAllStashesError,
      EventManager::EventType::SvnRepoManagerDeleteAllStashesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete all stashes.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteAllStashesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete all stashes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("All stashes deleted successfully."),
                         _container.tr("Success"));
  });

  if (onSuccess) {
    onSuccess(guard);
  }

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteTag(
    const QWeakPointer<RepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::SvnRepoManagerDeleteTag, eventUuid);
  deleteTagEvent.addData(command);
  _container._eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteTagError,
      EventManager::EventType::SvnRepoManagerDeleteTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(QString(_container.tr("Failed to delete tag.")),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerDeleteTagResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete tag."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(
      guard, [&]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::pushTag(const QWeakPointer<RepoContainer> &guard,
                                       const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoPushTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event pushTagEvent(
      EventManager::EventType::SvnRepoManagerPushTag, eventUuid);
  pushTagEvent.addData(command);
  _container._eventManager.send(pushTagEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerPushTagError,
      EventManager::EventType::SvnRepoManagerPushTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerPushTagResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to push tag to remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully pushed tag to remote."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::pushAllTags(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoPushAllTagsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event pushAllTagsEvent(
      EventManager::EventType::SvnRepoManagerPushAllTags, eventUuid);
  pushAllTagsEvent.addData(command);
  _container._eventManager.send(pushAllTagsEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerPushAllTagsError,
      EventManager::EventType::SvnRepoManagerPushAllTagsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerPushAllTagsResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(
          _container.tr("Failed to push all tags to remote."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(
        _container.tr("Successfully pushed all tags to remote."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::deleteRemoteTag(
    const QWeakPointer<RepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteRemoteTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteRemoteTagEvent(
      EventManager::EventType::SvnRepoManagerDeleteRemoteTag, eventUuid);
  deleteRemoteTagEvent.addData(command);
  _container._eventManager.send(deleteRemoteTagEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteRemoteTagError,
      EventManager::EventType::SvnRepoManagerDeleteRemoteTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete remote tag.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteRemoteTagResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete remote tag."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully deleted tag from remote."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::openSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetSubmodulePathCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::SvnRepoManagerGetSubmodulePath, eventUuid);
  deleteTagEvent.addData(command);
  _container._eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetSubmodulePathError,
      EventManager::EventType::SvnRepoManagerGetSubmodulePathResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to get submodule path.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetSubmodulePathResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto result =
      commandResult.value.value<SvnRepoGetSubmodulePathCommandResult>();

  if (result.submodulePath.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to get submodule path."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);

    const QString fullPath =
        QDir(_container._path).filePath(result.submodulePath);
    emit _container.openRepoRequested(fullPath);
  });

  LOG_PERF(0, "Exit");
}

static GitRepo::GitCommitStatus
convertSvnCommitStatusToGitCommitStatus(const SvnRepo::CommitStatus &status);

void SvnRepoManagerBridgeImpl::showStashContent(
    const QWeakPointer<RepoContainer> &guard, int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing stash content for index: {}", stashIndex);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetStashStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event getStashStatusEvent(
      EventManager::EventType::SvnRepoManagerGetStashStatus, eventUuid);
  getStashStatusEvent.addData(command);
  _container._eventManager.send(getStashStatusEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetStashStatusError,
      EventManager::EventType::SvnRepoManagerGetStashStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(
          QString(_container.tr("Failed to get stash content.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetStashStatusResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto result = event.data<SvnRepoResult>();
  const auto stashStatus = convertSvnCommitStatusToGitCommitStatus(
      result.value.value<SvnRepo::CommitStatus>());

  // Store the stash index for later use (for apply/pop buttons)
  _container._currentStashIndex = stashIndex;
  _container._lastStashCommit = stashStatus.commit;

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateStashContentInfo(stashIndex, stashStatus);

    // Update flat list view
    _container._stashFilesModel.reset(
        Models::GitCommitListModel::from(stashStatus));
    if (_container._stashFilesModel) {
      _container.ui->listViewStashFilesFlat->setModel(
          _container._stashFilesModel.get());

      QObject::disconnect(
          _container.ui->listViewStashFilesFlat,
          &GuiManager::Widgets::RightClickSelectListView::pressed, &_container,
          nullptr);

      QObject::connect(
          _container.ui->listViewStashFilesFlat,
          &GuiManager::Widgets::RightClickSelectListView::pressed, &_container,
          [=](const QModelIndex &index) { onStashFileSelected(index); });
    }

    // Update tree view
    _container._stashFilesTreeModel.reset(
        Models::GitCommitTreeModel::from(stashStatus));
    if (_container._stashFilesTreeModel) {
      _container.ui->treeViewStashFilesTree->setModel(
          _container._stashFilesTreeModel.get());

      QObject::disconnect(
          _container.ui->treeViewStashFilesTree,
          &GuiManager::Widgets::RightClickSelectTreeView::pressed, &_container,
          nullptr);

      QObject::connect(
          _container.ui->treeViewStashFilesTree,
          &GuiManager::Widgets::RightClickSelectTreeView::pressed, &_container,
          [=](const QModelIndex &index) { onStashFileSelected(index); });
    }

    // Preselect the first file and show its diff
    if (_container._stashFilesModel &&
        _container._stashFilesModel->rowCount() > 0) {
      const QModelIndex firstIndex = _container._stashFilesModel->index(0, 0);
      _container.ui->listViewStashFilesFlat->setCurrentIndex(firstIndex);
      onStashFileSelected(firstIndex);
    }

    // Switch to stash content page
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::StashContent));

    _container.showSpinnerOverlay(false);
  });

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::onStashFileSelected(const QModelIndex &index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stash file selected");

  if (!index.isValid() || _container._lastStashCommit.isEmpty()) {
    LOG_DEBUG("Invalid index or stash commit");
    LOG_PERF(0, "Exit");
    return;
  }

  if (index.model() && index.model()->hasChildren(index)) {
    LOG_DEBUG("Ignoring folder node selection in stash tree view");
    LOG_PERF(0, "Exit");
    return;
  }

  QString filePath = index.data(Qt::DisplayRole).toString();
  QModelIndex current = index;
  while (current.parent().isValid()) {
    current = current.parent();
    filePath =
        current.data(Qt::DisplayRole).toString() + QDir::separator() + filePath;
  }

  LOG_DEBUG("Stash file path: '{}'", filePath.toStdString());

  filePath = QDir::fromNativeSeparators(filePath);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

  _container._dispatcher.dispatch([=]() {
    // Check file size from commit before generating diff
    const qint64 maxDiffFileSize = _container._configManager.getLong(
        ConfigManager::ConfigKey::GitRepoMaxDiffFileSize);
    const qint64 fileSize =
        getFileSizeFromCommit(filePath, _container._lastStashCommit);

    if (fileSize > 0 && fileSize > maxDiffFileSize) {
      LOG_DEBUG("File size {} exceeds maximum {}", fileSize, maxDiffFileSize);
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showErrorToast(
            _container.tr("File is too large to be displayed."),
            _container.tr("Error"));
        return;
      });

      return;
    }

    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.startOverlayTimer(); });

    LOG_DEBUG("Generating diff from stash commit for file '{}'",
              filePath.toStdString());

    // Send command to get diff
    const QString eventUuid = Uuid::createUuid();
    const int token = _container._eventManager.startWait();

    SvnRepoDiffFilesFromCommitCommand command;
    command.uuid = _container._uuid;
    command.syncUuid = eventUuid;
    command.files += filePath;
    command.commit = _container._lastStashCommit;

    EventManager::Event diffFilesFromCommitEvent(
        EventManager::EventType::SvnRepoManagerDiffFilesFromCommit, eventUuid);
    diffFilesFromCommitEvent.addData(command);
    _container._eventManager.send(diffFilesFromCommitEvent);

    static std::set types = {
        EventManager::EventType::SvnRepoManagerDiffFilesFromCommitError,
        EventManager::EventType::SvnRepoManagerDiffFilesFromCommitResult};

    const auto &[success, event] =
        _container._eventManager.waitForAnswer(token, eventUuid, types);

    if (!success) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showErrorToast(_container.tr("Failed to create file diff."),
                                  _container.tr("Error"));
      });

      LOG_DEBUG("Failed to get diff for stash file");
      return;
    }

    if (event.type() !=
        EventManager::EventType::SvnRepoManagerDiffFilesFromCommitResult) {
      const auto error = event.data<SvnRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showErrorToast(error.message, _container.tr("Error"));
      });

      return;
    }

    const auto commandResult = event.data<SvnRepoResult>();
    const auto filesDiff =
        commandResult.value.value<SvnRepoDiffFilesFromCommitCommandResult>();

    if (filesDiff.diff.isEmpty()) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showToast(_container.tr("No difference in stash file."),
                             _container.tr("Info"));
      });

      return;
    }

    // Display diff in stash viewers
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      const QString normalized =
          QString(filesDiff.diff).replace("\r\n", "\n").replace('\r', '\n');
      const int lineCount = normalized.count('\n') + 1;
      const int width = QString::number(lineCount).length();
      const QString mask = QString::number(0).rightJustified(width + 1, '0');

      _container.ui->scintillaStashDiffUnified->setMarginWidth(0, mask);
      _container.ui->scintillaStashDiffUnified->setText(filesDiff.diff);

      if (!filesDiff.deltas.isEmpty() && !filesDiff.repoContent.isEmpty()) {
        const QFileInfo fileInfo(filesDiff.repoContent.firstKey());
        _container.ui->sideBySideDiffViewerStash->loadAndHighlight(
            filesDiff.repoContent.first(), filesDiff.deltas.first(),
            fileInfo.absoluteFilePath());
      }

      _container.ui->stackedWidgetStashDiff->setCurrentIndex(
          0); // Show unified view by default
      _container.showSpinnerOverlay(false);
    });
  });

  LOG_DEBUG("Stash file diff dispatched");
  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::deleteSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  // Stop watcher to release file handles before deletion
  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  const QString fullPath = QDir(_container._path).filePath(submoduleName);

  SvnRepoDeleteSubmoduleCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;
  command.submodulePath = fullPath;

  EventManager::Event deleteSubmoduleEvent(
      EventManager::EventType::SvnRepoManagerDeleteSubmodule, eventUuid);
  deleteSubmoduleEvent.addData(command);
  _container._eventManager.send(deleteSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteSubmoduleError,
      EventManager::EventType::SvnRepoManagerDeleteSubmoduleResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to delete submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteSubmoduleResult) {
    const auto error = event.data<SvnRepoError>();
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool commandSuccess = commandResult.value.toBool();

  if (!commandSuccess) {
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  _container.prepareWatcher(guard);

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::diff(const QWeakPointer<RepoContainer> &guard,
                                    const QString &filePath) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_container._lastDiffRepoStatusChanged &&
      _container._lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::Diff));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDiffFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::SvnRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _container._eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDiffFilesError,
      EventManager::EventType::SvnRepoManagerDiffFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create file diff."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerDiffFilesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto filesDiff =
      commandResult.value.value<SvnRepoDiffFilesCommandResult>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(
          _container.tr("No difference compared to the HEAD commit."),
          _container.tr("Info"));
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

    _container.ui->scintillaTextEditorUnified->setMarginWidth(0, mask);
    _container.ui->scintillaTextEditorUnified->setText(filesDiff.diff);

    // Collect added and deleted lines for scrollbar marks
    QVector<int> deletedLines;
    QVector<int> addedLines;

    // Mark hunk headers
    for (int i = 0; i < _container.ui->scintillaTextEditorUnified->lines();
         ++i) {
      const QString lineText =
          _container.ui->scintillaTextEditorUnified->text(i);
      if (lineText.startsWith("@@ ")) {
        _container.ui->scintillaTextEditorUnified->markerAdd(i, 1);
      } else if (lineText.startsWith("-") && !lineText.startsWith("---")) {
        deletedLines.append(i);
      } else if (lineText.startsWith("+") && !lineText.startsWith("+++")) {
        addedLines.append(i);
      }
    }

    // Update scrollbar marks
    _container._unifiedDiffMarks->setMarks(deletedLines, addedLines);

    // Only show marks if unified view is selected
    if (_container.ui->pushButtonDiffUnified->isChecked()) {
      _container._unifiedDiffMarks->show();
    } else {
      _container._unifiedDiffMarks->hide();
    }

    if (!filesDiff.deltas.isEmpty() && !filesDiff.repoContent.isEmpty()) {
      const QFileInfo fileInfo(filesDiff.repoContent.firstKey());
      _container.ui->sideBySideDiffViewer->loadAndHighlight(
          filesDiff.repoContent.first(), filesDiff.deltas.first(),
          fileInfo.absoluteFilePath());
    }

    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Diff));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::resolveTreeConflictInteractively(
    const QWeakPointer<RepoContainer> &guard, const QString &filePath) {
  LOG_PERF(0, "Enter");

  enum class TreeConflictChoice { Cancel, KeepLocal, TakeIncoming };
  TreeConflictChoice choice = TreeConflictChoice::Cancel;

  const auto showChoiceDialog = [&]() {
    QMessageBox box(_container.window());
    box.setWindowTitle(_container.tr("Tree conflict"));
    box.setIcon(QMessageBox::Warning);
    box.setText(_container.tr(
        "\"%1\" cannot be merged automatically because an unversioned local "
        "file blocks the incoming change.")
                    .arg(filePath));
    box.setInformativeText(_container.tr(
        "Keep local keeps your file. Take incoming replaces it with the "
        "repository version and renames your file to \"%1.local\".")
                               .arg(QFileInfo(filePath).fileName()));
    QPushButton *keepButton =
        box.addButton(_container.tr("Keep local"), QMessageBox::AcceptRole);
    QPushButton *incomingButton =
        box.addButton(_container.tr("Take incoming"), QMessageBox::AcceptRole);
    box.addButton(QMessageBox::Cancel);
    box.exec();

    if (box.clickedButton() == keepButton) {
      choice = TreeConflictChoice::KeepLocal;
    } else if (box.clickedButton() == incomingButton) {
      choice = TreeConflictChoice::TakeIncoming;
    }
  };

  if (QThread::currentThread() == qApp->thread()) {
    showChoiceDialog();
  } else {
    QMetaObject::invokeMethod(qApp, showChoiceDialog,
                              Qt::BlockingQueuedConnection);
  }

  if (choice == TreeConflictChoice::Cancel) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  SvnRepoResolveTreeConflictCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;
  command.keepLocal = choice == TreeConflictChoice::KeepLocal;

  EventManager::Event resolveEvent(
      EventManager::EventType::SvnRepoManagerResolveTreeConflict, eventUuid);
  resolveEvent.addData(command);
  _container._eventManager.send(resolveEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerResolveTreeConflictError,
      EventManager::EventType::SvnRepoManagerResolveTreeConflictResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success ||
      event.type() !=
          EventManager::EventType::SvnRepoManagerResolveTreeConflictResult) {
    const QString message = success ? event.data<SvnRepoError>().message
                                    : _container.tr("Failed to resolve tree conflict.");
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(message, _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Tree conflict resolved."),
                         _container.tr("Info"));
  });

  refreshStatus(guard);

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::diffThreeWay(
    const QWeakPointer<RepoContainer> &guard, const QString &filePath) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_container._lastDiffRepoStatusChanged &&
      _container._lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::Diff));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListConflictsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;

  EventManager::Event listConflictsEvent(
      EventManager::EventType::SvnRepoManagerListConflicts, eventUuid);
  listConflictsEvent.addData(command);
  _container._eventManager.send(listConflictsEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListConflictsError,
      EventManager::EventType::SvnRepoManagerListConflictsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create file diff."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerListConflictsResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto listConflictsResult =
      commandResult.value.value<SvnRepoListConflictsCommandResult>();

  if (listConflictsResult.treeConflict) {
    resolveTreeConflictInteractively(guard, filePath);
    LOG_PERF(0, "Exit")
    return;
  }

  if (listConflictsResult.fileContentWithMarkers.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(
          _container.tr("No difference compared to the HEAD commit."),
          _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    // Hide unified diff marks when switching to three-way diff
    _container._unifiedDiffMarks->hide();

    _container.ui->pushButtonMergeDiffSave->setEnabled(true);
    _container.ui->mergeDiffViewer->loadAndHighlight(
        listConflictsResult.fileContentWithMarkers, filePath);

    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::ThreeWayDiff));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::diffFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &filePath,
    const QString &commit) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_container._lastDiffRepoStatusChanged &&
      _container._lastDiffedCommit == commit &&
      _container._lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.ui->stackedWidgetMiddle->setCurrentIndex(
          static_cast<int>(MainViewPanes::Diff));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  _container._lastDiffedCommit = commit;
  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDiffFilesFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;
  command.commit = commit;

  EventManager::Event diffFilesFromCommitEvent(
      EventManager::EventType::SvnRepoManagerDiffFilesFromCommit, eventUuid);
  diffFilesFromCommitEvent.addData(command);
  _container._eventManager.send(diffFilesFromCommitEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDiffFilesFromCommitError,
      EventManager::EventType::SvnRepoManagerDiffFilesFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create file diff."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDiffFilesFromCommitResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto filesDiff =
      commandResult.value.value<SvnRepoDiffFilesFromCommitCommandResult>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(
          _container.tr("No difference compared to the previous revision."),
          _container.tr("Info"));
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

    _container.ui->scintillaTextEditorUnified->setMarginWidth(0, mask);
    _container.ui->scintillaTextEditorUnified->setText(filesDiff.diff);

    // Collect added and deleted lines for scrollbar marks
    QVector<int> deletedLines;
    QVector<int> addedLines;

    // Mark hunk headers
    for (int i = 0; i < _container.ui->scintillaTextEditorUnified->lines();
         ++i) {
      const QString lineText =
          _container.ui->scintillaTextEditorUnified->text(i);
      if (lineText.startsWith("@@ ")) {
        _container.ui->scintillaTextEditorUnified->markerAdd(i, 1);
      } else if (lineText.startsWith("-") && !lineText.startsWith("---")) {
        deletedLines.append(i);
      } else if (lineText.startsWith("+") && !lineText.startsWith("+++")) {
        addedLines.append(i);
      }
    }

    // Update scrollbar marks
    _container._unifiedDiffMarks->setMarks(deletedLines, addedLines);

    // Load the saved diff view preference from ConfigManager
    // false = unified (default), true = side-by-side
    const bool useSideBySideDiff = _container._configManager.getBoolean(
        ConfigManager::ConfigKey::GitRepoDiffViewType);

    _container.ui->pushButtonDiffUnified->setChecked(!useSideBySideDiff);
    _container.ui->pushButtonDiffSideBySide->setChecked(useSideBySideDiff);

    if (useSideBySideDiff) {
      _container._unifiedDiffMarks->hide();
    } else {
      _container._unifiedDiffMarks->show();
    }

    if (!filesDiff.deltas.isEmpty() && !filesDiff.repoContent.isEmpty()) {
      const QFileInfo fileInfo(filesDiff.repoContent.firstKey());
      _container.ui->sideBySideDiffViewer->loadAndHighlight(
          filesDiff.repoContent.first(), filesDiff.deltas.first(),
          fileInfo.absoluteFilePath());
    }

    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Diff));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::diffFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDiffFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::SvnRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _container._eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDiffFilesError,
      EventManager::EventType::SvnRepoManagerDiffFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create diff file."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerDiffFilesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto filesDiff =
      commandResult.value.value<SvnRepoDiffFilesCommandResult>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(
          _container.tr("No difference compared to the HEAD commit."),
          _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);

    const QString filePath = QFileDialog::getSaveFileName(
        nullptr, _container.tr("Save Diff File"), QString(),
        _container.tr("Patch Files (*.patch)"));
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                   QIODevice::Text)) {
      _container.showErrorToast(
          _container.tr("Failed to create diff file %1.").arg(filePath),
          _container.tr("Error"));
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

void SvnRepoManagerBridgeImpl::discardFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDiscardFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event discardFilesEvent(
      EventManager::EventType::SvnRepoManagerDiscardFiles, eventUuid);
  discardFilesEvent.addData(command);
  _container._eventManager.send(discardFilesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDiscardFilesError,
      EventManager::EventType::SvnRepoManagerDiscardFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to discard file(s)."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDiscardFilesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool discarded = commandResult.value.toBool();

  if (!discarded) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showToast(_container.tr("Failed to discard file(s)."),
                           _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.showSpinnerOverlay(false);
    _container.simulateClickOnFirstHistoryRow();
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

bool SvnRepoManagerBridgeImpl::getActiveBranchOrTag(
    const QWeakPointer<RepoContainer> &guard, const int branchType) {
  LOG_PERF(0, "Enter");

  if (branchType == static_cast<int>(GitRepo::BranchType::Remote)) {
    // Subversion has no remote tracking branches. The checked out branch is
    // only selected in the local list, so it is not reported as remote, which
    // would clear the local selection.
    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return true;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetActiveBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchType = static_cast<SvnRepo::BranchType>(branchType);

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::SvnRepoManagerGetActiveBranch, eventUuid);
  getActiveBranchEvent.addData(command);
  _container._eventManager.send(getActiveBranchEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetActiveBranchError,
      EventManager::EventType::SvnRepoManagerGetActiveBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the active local branch."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetActiveBranchResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto result =
      commandResult.value.value<SvnRepoGetActiveBranchCommandResult>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateModelActiveBranch(
        guard, result.branchName, static_cast<GitRepo::BranchType>(branchType));
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

static GitRepo::GitStatus
convertSvnStatusToGitStatus(const SvnRepo::SvnStatus &status) {
  GitRepo::GitStatus gitStatus;
  gitStatus.path = status.path;
  gitStatus.newFiles = status.newFiles;
  gitStatus.modifiedFiles = status.modifiedFiles;
  gitStatus.deletedFiles = status.deletedFiles;
  gitStatus.typeChangedFiles = status.typeChangedFiles;
  gitStatus.renamedFiles = status.renamedFiles;
  gitStatus.unreadableFiles = status.unreadableFiles;
  gitStatus.newStagedFiles = status.newStagedFiles;
  gitStatus.modifiedStagedFiles = status.modifiedStagedFiles;
  gitStatus.deletedStagedFiles = status.deletedStagedFiles;
  gitStatus.typeChangedStagedFiles = status.typeChangedStagedFiles;
  gitStatus.renamedStagedFiles = status.renamedStagedFiles;
  gitStatus.ignoredFiles = status.ignoredFiles;
  gitStatus.conflictedFiles = status.conflictedFiles;

  return gitStatus;
}

static GitRepo::GitHistory
convertSvnHistoryToGitHistory(const SvnRepo::SvnHistory &history) {
  GitRepo::GitHistory gitHistory;
  gitHistory.commits = history.commits;
  gitHistory.valid = history.valid;
  gitHistory.hasMore = history.hasMore;

  for (const auto &key : history.commitInfos.keys()) {
    const SvnRepo::SvnCommitInfo &commitInfo = history.commitInfos[key];

    GitRepo::GitCommitInfo gitCommitInfo;
    gitCommitInfo.message = commitInfo.message;
    gitCommitInfo.summary = commitInfo.summary;
    gitCommitInfo.authorName = commitInfo.authorName;
    gitCommitInfo.authorEmail = commitInfo.authorEmail;
    gitCommitInfo.timeStamp = commitInfo.timeStamp;
    gitCommitInfo.signedCommit = commitInfo.signedCommit;
    gitCommitInfo.signatureType = commitInfo.signatureType;
    gitCommitInfo.signature = commitInfo.signature;
    gitCommitInfo.branches = commitInfo.branches;
    gitCommitInfo.parents = commitInfo.parents;
    gitCommitInfo.children = commitInfo.children;

    gitHistory.commitInfos[key] = gitCommitInfo;
  }

  return gitHistory;
}

static GitRepo::GitCommitStatus
convertSvnCommitStatusToGitCommitStatus(const SvnRepo::CommitStatus &status) {
  GitRepo::GitCommitStatus gitCommitStatus;
  gitCommitStatus.commit = status.commit;
  gitCommitStatus.authorName = status.author;
  gitCommitStatus.summary = status.message.section('\n', 0, 0).trimmed();
  if (status.timeStamp >= 0) {
    gitCommitStatus.committerDate = QLocale().toString(
        QDateTime::fromMSecsSinceEpoch(status.timeStamp), QLocale::ShortFormat);
  }
  gitCommitStatus.newFiles = status.newFiles;
  gitCommitStatus.modifiedFiles = status.modifiedFiles;
  gitCommitStatus.deletedFiles = status.deletedFiles;

  return gitCommitStatus;
}

bool SvnRepoManagerBridgeImpl::getBranchHistory(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString historyEventUuid = Uuid::createUuid();

  const int historyToken = _container._eventManager.startWait();

  SvnRepoGetBranchHistoryCommand historyCommand;
  historyCommand.uuid = _container._uuid;
  historyCommand.branchName = _container._activeBranch;
  historyCommand.branchType =
      static_cast<SvnRepo::BranchType>(_container._activeBranchType);
  historyCommand.page = 0;
  historyCommand.syncUuid = historyEventUuid;

  EventManager::Event historyEvent(
      EventManager::EventType::SvnRepoManagerGetBranchHistory,
      historyEventUuid);
  historyEvent.addData(historyCommand);
  _container._eventManager.send(historyEvent);

  static std::set historyTypes = {
      EventManager::EventType::SvnRepoManagerGetBranchHistoryError,
      EventManager::EventType::SvnRepoManagerGetBranchHistoryResult};

  const auto &[historySuccess, historyResultEvent] =
      _container._eventManager.waitForAnswer(historyToken, historyEventUuid,
                                             historyTypes);

  if (!historySuccess) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to retrieve the history of branch %1."))
              .arg(_container._activeBranch),
          _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (historyResultEvent.type() !=
      EventManager::EventType::SvnRepoManagerGetBranchHistoryResult) {
    const auto error = historyResultEvent.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto historyCommandResult = historyResultEvent.data<SvnRepoResult>();

  auto history = historyCommandResult.value.value<SvnRepo::SvnHistory>();

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _container._eventManager.startWait();

  SvnRepoStatusCommand statusCommand;
  statusCommand.uuid = _container._uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::SvnRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _container._eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::SvnRepoManagerStatusError,
      EventManager::EventType::SvnRepoManagerStatusResult};

  const auto &[statusSuccess, statusResultEvent] =
      _container._eventManager.waitForAnswer(statusToken, statusEventUuid,
                                             statusTypes);

  if (!statusSuccess) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the repository status."),
          _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::SvnRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto statusCommandResult = statusResultEvent.data<SvnRepoResult>();

  const auto status = statusCommandResult.value.value<SvnRepo::SvnStatus>();

  GitRepo::GitHistoryWithStatus historyWithStatus;
  historyWithStatus.history = convertSvnHistoryToGitHistory(history);
  historyWithStatus.status = convertSvnStatusToGitStatus(status);
  historyWithStatus.branchName = _container._activeBranch;
  historyWithStatus.branchType = _container._activeBranchType;
  historyWithStatus.uuid = _container._uuid;
  historyWithStatus.page = 0;

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.ui->labelProgress->setText("0%"); });

  _container.updateBranchHistory(guard, historyWithStatus);

  _container.releaseBranchHistorySemaphore();

  LOG_PERF(0, "Exit")

  return true;
}

QStringList SvnRepoManagerBridgeImpl::getCommitBranches(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetCommitBranchesCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event branchesEvent(
      EventManager::EventType::SvnRepoManagerGetCommitBranches, eventUuid);
  branchesEvent.addData(command);
  _container._eventManager.send(branchesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetCommitBranchesError,
      EventManager::EventType::SvnRepoManagerGetCommitBranchesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { _container.showErrorPage(); });
    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetCommitBranchesResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto branches = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return branches;
}

QStringList SvnRepoManagerBridgeImpl::getCommitTags(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  SvnRepoGetCommitTagsCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event tagsEvent(
      EventManager::EventType::SvnRepoManagerGetCommitTags, eventUuid);
  tagsEvent.addData(command);
  _container._eventManager.send(tagsEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetCommitTagsError,
      EventManager::EventType::SvnRepoManagerGetCommitTagsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { _container.showErrorPage(); });
    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetCommitTagsResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto tags = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return tags;
}

GitRepo::GitCommitStatus SvnRepoManagerBridgeImpl::getCommitStatus(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetCommitStatusCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event commitStatusEvent(
      EventManager::EventType::SvnRepoManagerGetCommitStatus, eventUuid);
  commitStatusEvent.addData(command);
  _container._eventManager.send(commitStatusEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetCommitStatusError,
      EventManager::EventType::SvnRepoManagerGetCommitStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    ::Util::Dispatcher::dispatchToMainThread(
        guard, [&]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetCommitStatusResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto commitStatus = convertSvnCommitStatusToGitCommitStatus(
      commandResult.value.value<SvnRepo::CommitStatus>());

  LOG_PERF(0, "Exit");

  return commitStatus;
}

bool SvnRepoManagerBridgeImpl::getLocalBranches() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListLocalBranchesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listLocalEvent(
      EventManager::EventType::SvnRepoManagerListLocalBranches, eventUuid);
  listLocalEvent.addData(command);
  _container._eventManager.send(listLocalEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListLocalBranchesError,
      EventManager::EventType::SvnRepoManagerListLocalBranchesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of local branches."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerListLocalBranchesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto localBranches = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateLocalBranchesModel(guard, localBranches);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

static GitRepo::BranchInfo
convertSvnBranchInfoToGitBranchInfo(const SvnRepo::BranchInfo &branchInfo) {
  GitRepo::BranchInfo info;
  info.name = branchInfo.name;
  info.statusInfo.status =
      static_cast<GitRepo::BranchStatusType>(branchInfo.statusInfo.status);
  info.statusInfo.aheadCount = branchInfo.statusInfo.aheadCount;
  info.statusInfo.behindCount = branchInfo.statusInfo.behindCount;

  return info;
}

bool SvnRepoManagerBridgeImpl::getRemoteBranches() {
  LOG_PERF(0, "Enter");

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListRemoteBranchesWithStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemoteEvent(
      EventManager::EventType::SvnRepoManagerListRemoteBranchesWithStatus,
      eventUuid);
  listRemoteEvent.addData(command);
  _container._eventManager.send(listRemoteEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListRemoteBranchesWithStatusError,
      EventManager::EventType::
          SvnRepoManagerListRemoteBranchesWithStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of remote branches."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::
                          SvnRepoManagerListRemoteBranchesWithStatusResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto branchesWithStatus =
      commandResult.value.value<QVector<SvnRepo::BranchInfo>>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    QVector<GitRepo::BranchInfo> statuses;
    for (const auto &info : branchesWithStatus) {
      statuses += convertSvnBranchInfoToGitBranchInfo(info);
    }
    _container.updateRemoteBranchesModelWithStatus(guard, statuses);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool SvnRepoManagerBridgeImpl::getStashes(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListStashesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listStashesEvent(
      EventManager::EventType::SvnRepoManagerListStashes, eventUuid);
  listStashesEvent.addData(command);
  _container._eventManager.send(listStashesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListStashesError,
      EventManager::EventType::SvnRepoManagerListStashesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of stashes."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerListStashesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto stashes = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateStashesModel(guard, stashes);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

static GitRepo::RemoteInfo
convertSvnRemoteInfoToGitRemoteInfo(const SvnRepo::RemoteInfo &info) {
  GitRepo::RemoteInfo remoteInfo;
  remoteInfo.name = info.name;
  remoteInfo.fetchUrl = info.fetchUrl;
  remoteInfo.pushUrl = info.pushUrl;
  remoteInfo.isDefault = info.isDefault;

  for (const auto &target : info.targets) {
    GitRepo::RemoteTarget gitTarget;
    gitTarget.fetchUrl = target.fetchUrl;
    gitTarget.name = target.name;
    gitTarget.pushUrl = target.pushUrl;

    remoteInfo.targets += gitTarget;
  }

  return remoteInfo;
}

bool SvnRepoManagerBridgeImpl::getRemotes(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListRemotesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemotesEvent(
      EventManager::EventType::SvnRepoManagerListRemotes, eventUuid);
  listRemotesEvent.addData(command);
  _container._eventManager.send(listRemotesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListRemotesError,
      EventManager::EventType::SvnRepoManagerListRemotesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of remotes."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerListRemotesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto remotes =
      commandResult.value.value<QVector<SvnRepo::RemoteInfo>>();

  QVector<GitRepo::RemoteInfo> gitRemotes;

  for (const auto &info : remotes) {
    gitRemotes += convertSvnRemoteInfoToGitRemoteInfo(info);
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.ui->remotesPage->updateModel(gitRemotes);

    // Set the active remote to the current branch's upstream remote
    if (!_container._activeBranch.isEmpty() &&
        _container._activeBranch != ALL_BRANCHES) {
      _container._dispatcher.dispatch([=]() {
        auto [upstreamRemote, upstreamBranch] = getUpstream(guard);
        if (!upstreamRemote.isEmpty()) {
          Util::Dispatcher::dispatchToMainThread(guard, [=]() {
            _container.ui->remotesPage->updateActiveRemote(upstreamRemote);
          });
        }
      });
    }
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

static GitRepo::SubmoduleInfo
convertSvnSubmoduleInfoToGitSubmoduleInfo(const SvnRepo::SubmoduleInfo &info) {
  GitRepo::SubmoduleInfo gitInfo;
  gitInfo.statusInfo.status =
      static_cast<GitRepo::SubmoduleStatusType>(info.statusInfo.status);
  gitInfo.commit = info.commit;
  gitInfo.name = info.name;

  return gitInfo;
}

bool SvnRepoManagerBridgeImpl::getSubmodules() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListSubmodulesWithStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listSubmodulesEvent(
      EventManager::EventType::SvnRepoManagerListSubmodulesWithStatus,
      eventUuid);
  listSubmodulesEvent.addData(command);
  _container._eventManager.send(listSubmodulesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusError,
      EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of submodules."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerListSubmodulesWithStatusResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto submodulesWithStatus =
      commandResult.value.value<QVector<SvnRepo::SubmoduleInfo>>();

  QVector<GitRepo::SubmoduleInfo> gitSubmodules;

  for (const auto &info : submodulesWithStatus) {
    gitSubmodules += convertSvnSubmoduleInfoToGitSubmoduleInfo(info);
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateSubmodulesModelWithStatus(guard, gitSubmodules);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool SvnRepoManagerBridgeImpl::getTags() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoListTagsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listTagsEvent(
      EventManager::EventType::SvnRepoManagerListTags, eventUuid);
  listTagsEvent.addData(command);
  _container._eventManager.send(listTagsEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerListTagsError,
      EventManager::EventType::SvnRepoManagerListTagsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of tags."),
          _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerListTagsResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  const auto tags = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateTagsModel(guard, tags);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

QPair<QString, QString> SvnRepoManagerBridgeImpl::getUpstream(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = _container._activeBranch;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::SvnRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _container._eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetUpstreamError,
      EventManager::EventType::SvnRepoManagerGetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to get upstream.")),
          _container.tr("Apply"));
    });

    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetUpstreamResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showSpinnerOverlay(false); });

    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto [remoteName, remoteBranchName] =
      commandResult.value.value<SvnRepoGetUpstreamCommandResult>();

  refreshStatus(guard);

  LOG_PERF(0, "Exit")

  return qMakePair(remoteName, remoteBranchName);
}

QPair<QString, QString> SvnRepoManagerBridgeImpl::getUpstreamForBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = branchName;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::SvnRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _container._eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetUpstreamError,
      EventManager::EventType::SvnRepoManagerGetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetUpstreamResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto [remoteName, remoteBranchName] =
      commandResult.value.value<SvnRepoGetUpstreamCommandResult>();

  LOG_PERF(0, "Exit")

  return qMakePair(remoteName, remoteBranchName);
}

void SvnRepoManagerBridgeImpl::merge(const QWeakPointer<RepoContainer> &guard,
                                     const QString &sourceBranch,
                                     const QString &destBranch) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoMergeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;

  EventManager::Event mergeEvent(EventManager::EventType::SvnRepoManagerMerge,
                                 eventUuid);
  mergeEvent.addData(command);
  _container._eventManager.send(mergeEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerMergeError,
                           EventManager::EventType::SvnRepoManagerMergeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to merge branches."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerMergeResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<SvnRepoResult>();
  const auto merged = commandResult.value.toBool();

  if (!merged) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(_container.tr("Merge has failed."),
                           _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::rebase(const QWeakPointer<RepoContainer> &guard,
                                      const QString &sourceBranch,
                                      const QString &destBranch,
                                      bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoRebaseCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;
  command.signCommit = signCommit;

  EventManager::Event rebaseEvent(EventManager::EventType::SvnRepoManagerRebase,
                                  eventUuid);
  rebaseEvent.addData(command);
  _container._eventManager.send(rebaseEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerRebaseError,
                           EventManager::EventType::SvnRepoManagerRebaseResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to rebase branches."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerRebaseResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<SvnRepoResult>();
  const auto rebased = commandResult.value.toBool();

  if (!rebased) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(_container.tr("Rebase has failed."),
                           _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit")
}

bool SvnRepoManagerBridgeImpl::openRepo(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoOpenCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.path = _container._path;

  EventManager::Event openRepoEvent(
      EventManager::EventType::SvnRepoManagerOpenRepo, eventUuid);
  openRepoEvent.addData(command);
  _container._eventManager.send(openRepoEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerOpenRepoError,
      EventManager::EventType::SvnRepoManagerOpenRepoResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerOpenRepoResult) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  const bool refreshHistorySuccess = _container.refreshHistory(guard);

  if (refreshHistorySuccess) {
    _container.saveRepoInRecentList();
  }

  LOG_PERF(0, "Exit")

  return refreshHistorySuccess;
}

void SvnRepoManagerBridgeImpl::pull(const QWeakPointer<RepoContainer> &guard,
                                    bool rebase) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoPullCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.activeBranchName = _container._activeBranch;
  command.rebase = rebase;

  EventManager::Event pullEvent(EventManager::EventType::SvnRepoManagerPull,
                                eventUuid);
  pullEvent.addData(command);
  _container._eventManager.send(pullEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerPullError,
                           EventManager::EventType::SvnRepoManagerPullResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerPullResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pulled = commandResult.value.toBool(); !pulled) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);
  getBranchHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(
        rebase ? _container.tr("Pull with rebase completed successfully.")
               : _container.tr("Subversion repository is up to-date."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::fetchAll(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoFetchAllCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event fetchAllEvent(
      EventManager::EventType::SvnRepoManagerFetchAll, eventUuid);
  fetchAllEvent.addData(command);
  _container._eventManager.send(fetchAllEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerFetchAllError,
      EventManager::EventType::SvnRepoManagerFetchAllResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerFetchAllResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool fetched = commandResult.value.toBool(); !fetched) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to fetch from remotes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Fetch completed successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::fetchRemote(
    const QWeakPointer<RepoContainer> &guard, const QString &remoteName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoFetchRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;

  EventManager::Event fetchRemoteEvent(
      EventManager::EventType::SvnRepoManagerFetchRemote, eventUuid);
  fetchRemoteEvent.addData(command);
  _container._eventManager.send(fetchRemoteEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerFetchRemoteError,
      EventManager::EventType::SvnRepoManagerFetchRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerFetchRemoteResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool fetched = commandResult.value.toBool(); !fetched) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to fetch from remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Fetch completed successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::push(const QWeakPointer<RepoContainer> &guard,
                                    bool force) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoPushCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.activeBranchName = _container._activeBranch;
  command.force = force;

  EventManager::Event pushEvent(EventManager::EventType::SvnRepoManagerPush,
                                eventUuid);
  pushEvent.addData(command);
  _container._eventManager.send(pushEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerPushError,
                           EventManager::EventType::SvnRepoManagerPushResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerPushResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(
        force ? _container.tr("Successfully force-pushed to remote.")
              : _container.tr("Successfully pushed to remote."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::addRemote(
    const QWeakPointer<RepoContainer> &guard, const QString &remoteName,
    const QString &fetchUrl, const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding remote '{}' with fetch URL '{}'", remoteName.toStdString(),
            fetchUrl.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoAddRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event addRemoteEvent(
      EventManager::EventType::SvnRepoManagerAddRemote, eventUuid);
  addRemoteEvent.addData(command);
  _container._eventManager.send(addRemoteEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerAddRemoteError,
      EventManager::EventType::SvnRepoManagerAddRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to add remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerAddRemoteResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Remote added successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::editRemote(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    const QString &remoteName, const QString &fetchUrl,
    const QString &pushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  LOG_DEBUG("Updating remote '{}' with fetch URL '{}'",
            remote.name.toStdString(), fetchUrl.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoUpdateRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.newRemoteName = remoteName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event updateRemoteEvent(
      EventManager::EventType::SvnRepoManagerUpdateRemote, eventUuid);
  updateRemoteEvent.addData(command);
  _container._eventManager.send(updateRemoteEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerUpdateRemoteError,
      EventManager::EventType::SvnRepoManagerUpdateRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to update remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerUpdateRemoteResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Remote updated successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteRemote(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  LOG_DEBUG("Deleting remote '{}'", remote.name.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoRemoveRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;

  EventManager::Event removeRemoteEvent(
      EventManager::EventType::SvnRepoManagerRemoveRemote, eventUuid);
  removeRemoteEvent.addData(command);
  _container._eventManager.send(removeRemoteEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerRemoveRemoteError,
      EventManager::EventType::SvnRepoManagerRemoveRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerRemoveRemoteResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Remote deleted successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::setDefaultRemote(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  LOG_DEBUG("Setting default remote to '{}'", remote.name.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoSetDefaultRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;

  EventManager::Event setDefaultRemoteEvent(
      EventManager::EventType::SvnRepoManagerSetDefaultRemote, eventUuid);
  setDefaultRemoteEvent.addData(command);
  _container._eventManager.send(setDefaultRemoteEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerSetDefaultRemoteError,
      EventManager::EventType::SvnRepoManagerSetDefaultRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to set default remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerSetDefaultRemoteResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Default remote set successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::addTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    const QString &targetName, const QString &fetchUrl,
    const QString &pushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  LOG_DEBUG("Adding target '{}' to remote '{}'", targetName.toStdString(),
            remote.name.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoAddTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = targetName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event addTargetEvent(
      EventManager::EventType::SvnRepoManagerAddTarget, eventUuid);
  addTargetEvent.addData(command);
  _container._eventManager.send(addTargetEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerAddTargetError,
      EventManager::EventType::SvnRepoManagerAddTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to add target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerAddTargetResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.ui->remotesPage->expandRemote(remote.name);
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Target added successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::editTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex, int targetIndex,
    const QString &newFetchUrl, const QString &newPushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_DEBUG("Invalid target index: {}", targetIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid target index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &target = remote.targets[targetIndex];
  LOG_DEBUG("Editing target '{}' on remote '{}'", target.name.toStdString(),
            remote.name.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoEditTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;
  command.newFetchUrl = newFetchUrl;
  command.newPushUrl = newPushUrl;

  EventManager::Event editTargetEvent(
      EventManager::EventType::SvnRepoManagerEditTarget, eventUuid);
  editTargetEvent.addData(command);
  _container._eventManager.send(editTargetEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerEditTargetError,
      EventManager::EventType::SvnRepoManagerEditTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to edit target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerEditTargetResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Target edited successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::deleteTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    int targetIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_DEBUG("Invalid target index: {}", targetIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid target index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &target = remote.targets[targetIndex];
  LOG_DEBUG("Deleting target '{}' from remote '{}'", target.name.toStdString(),
            remote.name.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoDeleteTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;

  EventManager::Event deleteTargetEvent(
      EventManager::EventType::SvnRepoManagerDeleteTarget, eventUuid);
  deleteTargetEvent.addData(command);
  _container._eventManager.send(deleteTargetEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerDeleteTargetError,
      EventManager::EventType::SvnRepoManagerDeleteTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to delete target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerDeleteTargetResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Target deleted successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::setDefaultTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    int targetIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid remote index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &remote = _container.ui->remotesPage->remotes()[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_DEBUG("Invalid target index: {}", targetIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Invalid target index."),
                                _container.tr("Error"));
    });
    LOG_PERF(0, "Exit")
    return;
  }

  const auto &target = remote.targets[targetIndex];
  LOG_DEBUG("Setting default target to '{}' for remote '{}'",
            target.name.toStdString(), remote.name.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoSetDefaultTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;

  EventManager::Event setDefaultTargetEvent(
      EventManager::EventType::SvnRepoManagerSetDefaultTarget, eventUuid);
  setDefaultTargetEvent.addData(command);
  _container._eventManager.send(setDefaultTargetEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerSetDefaultTargetError,
      EventManager::EventType::SvnRepoManagerSetDefaultTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to set default target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerSetDefaultTargetResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Default target set successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::renameBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &oldName,
    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoRenameBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.oldBranchName = oldName;
  command.newBranchName = newName;

  EventManager::Event renameBranchEvent(
      EventManager::EventType::SvnRepoManagerRenameBranch, eventUuid);
  renameBranchEvent.addData(command);
  _container._eventManager.send(renameBranchEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerRenameBranchError,
      EventManager::EventType::SvnRepoManagerRenameBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to rename branch"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerRenameBranchResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to rename branch"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully renamed branch."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::renameTag(
    const QWeakPointer<RepoContainer> &guard, const QString &oldName,
    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoRenameTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.oldTagName = oldName;
  command.newTagName = newName;

  EventManager::Event renameTagEvent(
      EventManager::EventType::SvnRepoManagerRenameTag, eventUuid);
  renameTagEvent.addData(command);
  _container._eventManager.send(renameTagEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerRenameTagError,
      EventManager::EventType::SvnRepoManagerRenameTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to rename tag"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerRenameTagResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to rename tag"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully renamed tag."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::reset(const QWeakPointer<RepoContainer> &guard,
                                     const QString &commit, const int type) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoResetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.resetType = static_cast<SvnRepo::ResetType>(type);

  EventManager::Event resetEvent(EventManager::EventType::SvnRepoManagerReset,
                                 eventUuid);
  resetEvent.addData(command);
  _container._eventManager.send(resetEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerResetError,
                           EventManager::EventType::SvnRepoManagerResetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerResetResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to reset to commit."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully reset to commit."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::revert(const QWeakPointer<RepoContainer> &guard,
                                      const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString summaryToRevert =
      commitSummaryFromHistoryModel(_container._historyModel.data(), commit);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoRevertCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;
  command.message = QString("[%1] %2").arg(
      _container.tr("Revert"),
      summaryToRevert.isEmpty() ? commit : summaryToRevert);

  EventManager::Event revertEvent(EventManager::EventType::SvnRepoManagerRevert,
                                  eventUuid);
  revertEvent.addData(command);
  _container._eventManager.send(revertEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerRevertError,
                           EventManager::EventType::SvnRepoManagerRevertResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerRevertResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to revert commit."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully reverted commit."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::cherryPick(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  QString summaryCherryPick;

  for (int i = 0; i < _container._historyModel->rowCount(QModelIndex()); ++i) {
    const QString rowFullSHA =
        _container._historyModel
            ->data(_container._historyModel->index(i, COLUMN_COMMIT_FULL_SHA,
                                                   QModelIndex()),
                   Qt::DisplayRole)
            .toString();
    if (rowFullSHA == commit) {
      summaryCherryPick = _container._historyModel
                              ->data(_container._historyModel->index(
                                         i, COLUMN_SUMMARY, QModelIndex()),
                                     Qt::DisplayRole)
                              .toString();
      break;
    }
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoCherryPickCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;

  EventManager::Event cherryPickEvent(
      EventManager::EventType::SvnRepoManagerCherryPick, eventUuid);
  cherryPickEvent.addData(command);
  _container._eventManager.send(cherryPickEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerCherryPickError,
      EventManager::EventType::SvnRepoManagerCherryPickResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerCherryPickResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool picked = commandResult.value.toBool(); !picked) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to cherry-pick commit."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Successfully cherry-picked commit."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::refreshStatus(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _container._eventManager.startWait();

  SvnRepoStatusCommand statusCommand;
  statusCommand.uuid = _container._uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::SvnRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _container._eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::SvnRepoManagerStatusError,
      EventManager::EventType::SvnRepoManagerStatusResult};

  const auto &[statusSuccess, statusResultEvent] =
      _container._eventManager.waitForAnswer(statusToken, statusEventUuid,
                                             statusTypes);

  if (!statusSuccess) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the repository status."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::SvnRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto statusCommandResult = statusResultEvent.data<SvnRepoResult>();

  const auto status = statusCommandResult.value.value<SvnRepo::SvnStatus>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const QModelIndex currentIndex = _container.ui->historyView->currentIndex();
    const int currentRow = currentIndex.row();

    int offset = 0;

    if (!_container._historyModel->hasStatus() && status.hasChanges()) {
      ++offset;
    } else if (_container._historyModel->hasStatus() && !status.hasChanges()) {
      --offset;
    }

    int newRow = currentRow + offset;
    if (newRow < 0) {
      newRow = 0;
    }

    _container._historyModel->updateStatus(convertSvnStatusToGitStatus(status));

    _container.simulateClickOnHistoryRow(newRow);
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::showCommitAndMerge(
    QWeakPointer<RepoContainer> guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoGetIsMergeInProgressCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event getIsMergeInProgressEvent(
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgress, eventUuid);
  getIsMergeInProgressEvent.addData(command);
  _container._eventManager.send(getIsMergeInProgressEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgressError,
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgressResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to get merge progress status."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerGetIsMergeInProgressResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  _container._mergeInProgress = commandResult.value.toBool();

  const GitRepo::GitHistoryWithStatus historyAndStatus =
      _container._historyModel->historyWithStatus();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container._commitListModel.reset(
        Models::GitCommitListModel::from(historyAndStatus.status));
    if (_container._commitListModel) {
      _container.ui->listViewTabWidgetWorkingTreeFlat->setModel(
          _container._commitListModel.get());

      QObject::disconnect(
          _container.ui->listViewTabWidgetWorkingTreeFlat,
          &GuiManager::Widgets::RightClickSelectListView::pressed, &_container,
          nullptr);

      QObject::connect(_container.ui->listViewTabWidgetWorkingTreeFlat,
                       &GuiManager::Widgets::RightClickSelectListView::pressed,
                       &_container, [=](const QModelIndex &index) {
                         _container.onListViewTabWidgetFlatSelected(index);
                       });
    }

    _container._commitTreeModel.reset(
        Models::GitCommitTreeModel::from(historyAndStatus.status));
    if (_container._commitTreeModel) {
      _container.ui->treeViewTabWidgetWorkingTreeTree->setModel(
          _container._commitTreeModel.get());

      QObject::disconnect(
          _container.ui->treeViewTabWidgetWorkingTreeTree,
          &GuiManager::Widgets::RightClickSelectTreeView::pressed, &_container,
          nullptr);

      QObject::connect(_container.ui->treeViewTabWidgetWorkingTreeTree,
                       &GuiManager::Widgets::RightClickSelectTreeView::pressed,
                       &_container, [=](const QModelIndex &index) {
                         _container.onTreeViewTabWidgetTreeSelected(
                             index, *_container._commitTreeModel);
                       });
    }

    _container._commitStagedListModel.reset(
        Models::GitCommitListModel::fromStaged(historyAndStatus.status));
    if (_container._commitStagedListModel) {
      _container.ui->listViewTabWidgetIndexFlat->setModel(
          _container._commitStagedListModel.get());

      QObject::disconnect(
          _container.ui->listViewTabWidgetIndexFlat,
          &GuiManager::Widgets::RightClickSelectListView::pressed, &_container,
          nullptr);

      QObject::connect(_container.ui->listViewTabWidgetIndexFlat,
                       &GuiManager::Widgets::RightClickSelectListView::pressed,
                       &_container, [=](const QModelIndex &index) {
                         _container.onListViewTabWidgetFlatSelected(index);
                       });
    }

    _container._commitStagedTreeModel.reset(
        Models::GitCommitTreeModel::fromStaged(historyAndStatus.status));
    if (_container._commitStagedTreeModel) {
      _container.ui->treeViewTabWidgetIndexTree->setModel(
          _container._commitStagedTreeModel.get());

      QObject::disconnect(
          _container.ui->treeViewTabWidgetIndexTree,
          &GuiManager::Widgets::RightClickSelectTreeView::pressed, &_container,
          nullptr);

      QObject::connect(_container.ui->treeViewTabWidgetIndexTree,
                       &GuiManager::Widgets::RightClickSelectTreeView::pressed,
                       &_container, [=](const QModelIndex &index) {
                         _container.onTreeViewTabWidgetTreeSelected(
                             index, *_container._commitStagedTreeModel);
                       });
    }

    _container.resetCommitButton();
    _container.resetCommitAndMergeIndexTreeUnstageAllButton();
    _container.resetCommitAndMergeWorkingTreeTreeStageAllButton();
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::stageFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    _container.showSpinnerOverlay(false);
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoStageFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event stageFilesEvent(
      EventManager::EventType::SvnRepoManagerStageFiles, eventUuid);
  stageFilesEvent.addData(command);
  _container._eventManager.send(stageFilesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerStageFilesError,
      EventManager::EventType::SvnRepoManagerStageFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to stage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerStageFilesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool stageAllSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!stageAllSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to stage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.showSpinnerOverlay(false);
    _container.simulateClickOnFirstHistoryRow();
    _container.resetCommitAndMergeWorkingTreeTreeStageAllButton();
  });

  LOG_PERF(0, "Enter");
}

void SvnRepoManagerBridgeImpl::unstageFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    _container.showSpinnerOverlay(false);
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoUnstageFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event unstageFilesEvent(
      EventManager::EventType::SvnRepoManagerUnstageFiles, eventUuid);
  unstageFilesEvent.addData(command);
  _container._eventManager.send(unstageFilesEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerUnstageFilesError,
      EventManager::EventType::SvnRepoManagerUnstageFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to unstage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerUnstageFilesResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const bool unstageAllSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!unstageAllSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to unstage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.showSpinnerOverlay(false);
    _container.simulateClickOnFirstHistoryRow();
    _container.resetCommitAndMergeIndexTreeUnstageAllButton();
  });

  LOG_PERF(0, "Exit")
}

void SvnRepoManagerBridgeImpl::updateSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoSubmoduleUpdateCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event submoduleUpdateEvent(
      EventManager::EventType::SvnRepoManagerSubmoduleUpdate, eventUuid);
  submoduleUpdateEvent.addData(command);
  _container._eventManager.send(submoduleUpdateEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerSubmoduleUpdateError,
      EventManager::EventType::SvnRepoManagerSubmoduleUpdateResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to update submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::SvnRepoManagerSubmoduleUpdateResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<SvnRepoResult>();

  if (const bool commandSuccess = commandResult.value.toBool();
      !commandSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(_container.tr("Failed to update submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    _container.showToast(_container.tr("Submodule updated successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

qint64 SvnRepoManagerBridgeImpl::getFileSizeFromCommit(const QString &filePath,
                                                       const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  SvnRepoGetFileSizeFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;
  command.commit = commit;

  EventManager::Event getFileSizeEvent(
      EventManager::EventType::SvnRepoManagerGetFileSizeFromCommit, eventUuid);
  getFileSizeEvent.addData(command);
  _container._eventManager.send(getFileSizeEvent);

  static std::set types = {
      EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitError,
      EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success ||
      event.type() !=
          EventManager::EventType::SvnRepoManagerGetFileSizeFromCommitResult) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  const auto commandResult = event.data<SvnRepoResult>();
  const auto fileSizeResult =
      commandResult.value.value<SvnRepoGetFileSizeFromCommitCommandResult>();

  LOG_PERF(0, "Exit");
  return fileSizeResult.fileSize;
}

void SvnRepoManagerBridgeImpl::reportUnsupportedOperation(
    const QString &operation) const {
  LOG_WARN("{} is not supported for Subversion repositories.", operation);

  _container.showErrorToast(
      QString(_container.tr("%1 is not supported for Subversion repositories."))
          .arg(operation),
      _container.tr("Error"));
}

void SvnRepoManagerBridgeImpl::blame(const QWeakPointer<RepoContainer> &guard,
                                     const QStringList &files) {
  LOG_PERF(0, "Enter");

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  SvnRepoBlameCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event blameEvent(EventManager::EventType::SvnRepoManagerBlame,
                                 eventUuid);
  blameEvent.addData(command);
  _container._eventManager.send(blameEvent);

  static std::set types = {EventManager::EventType::SvnRepoManagerBlameError,
                           EventManager::EventType::SvnRepoManagerBlameResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to retrieve blame."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit");
    return;
  }

  if (event.type() != EventManager::EventType::SvnRepoManagerBlameResult) {
    const auto error = event.data<SvnRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit");
    return;
  }

  const auto blameResult =
      event.data<SvnRepoResult>().value.value<SvnRepoBlameCommandResult>();

  GitRepo::BlameReport report;
  report.valid = blameResult.report.valid;
  for (const SvnRepo::BlameFileInfo &svnFileInfo :
       blameResult.report.fileInfos) {
    GitRepo::BlameFileInfo fileInfo;
    fileInfo.filePath = svnFileInfo.filePath;
    fileInfo.errorMessage = svnFileInfo.errorMessage;
    for (const SvnRepo::BlameLineInfo &svnLineInfo : svnFileInfo.lineInfos) {
      GitRepo::BlameLineInfo lineInfo;
      lineInfo.filePath = svnLineInfo.filePath;
      lineInfo.lineNumber = svnLineInfo.lineNumber;
      lineInfo.content = svnLineInfo.content;
      lineInfo.commit = svnLineInfo.commit;
      lineInfo.author = svnLineInfo.author;
      lineInfo.email = svnLineInfo.email;
      lineInfo.summary = svnLineInfo.summary;
      fileInfo.lineInfos.append(lineInfo);
    }
    report.fileInfos.append(fileInfo);
  }

  GitRepoManagerBridgeImpl::showBlameReport(_container, guard, report);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::abortRebase(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);

  reportUnsupportedOperation(_container.tr("Aborting a rebase"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::addWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &path,
    const QString &branch, bool createBranch, const QString &baseBranch) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(path);
  Q_UNUSED(branch);
  Q_UNUSED(createBranch);
  Q_UNUSED(baseBranch);

  reportUnsupportedOperation(_container.tr("Adding a worktree"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::removeWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &name, bool force) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(name);
  Q_UNUSED(force);

  reportUnsupportedOperation(_container.tr("Removing a worktree"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::lockWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &name,
    const QString &reason) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(name);
  Q_UNUSED(reason);

  reportUnsupportedOperation(_container.tr("Locking a worktree"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::unlockWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &name) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(name);

  reportUnsupportedOperation(_container.tr("Unlocking a worktree"));

  LOG_PERF(0, "Exit");
}

bool SvnRepoManagerBridgeImpl::getWorktrees(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);

  // Subversion has no worktrees, the history refresh only waits for this
  // request to finish.
  _container.releaseHistoryRefreshSemaphore();

  LOG_PERF(0, "Exit");

  return true;
}

void SvnRepoManagerBridgeImpl::commitRebase(
    const QWeakPointer<RepoContainer> &guard, const QString &summary,
    const QString &description, const QString &author, const QString &email,
    bool cleanup, bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(summary);
  Q_UNUSED(description);
  Q_UNUSED(author);
  Q_UNUSED(email);
  Q_UNUSED(cleanup);
  Q_UNUSED(signoff);
  Q_UNUSED(signCommit);

  reportUnsupportedOperation(_container.tr("Committing during a rebase"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::getBranchHistoryPage(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName,
    const GitRepo::BranchType branchType, const int page,
    const quint64 generation, const bool lowPriority) {
  LOG_PERF(0, "Enter");

  const QString uuid = _container._uuid;
  EventManager::EventManager *eventManager = &_container._eventManager;
  const QWeakPointer<Models::GitHistoryModel> historyModel =
      _container._historyModel;

  const auto loadPage = [=]() {
    // A low priority page waits for the other requests and may be outdated
    // (searching stopped, other branch) by the time it starts.
    if (lowPriority) {
      const auto model = historyModel.toStrongRef();
      if (!model || !model->isCurrentGeneration(generation)) {
        return;
      }
    }

    const QString eventUuid = Uuid::createUuid();

    const int token = eventManager->startWait();

    SvnRepoGetBranchHistoryCommand command;
    command.uuid = uuid;
    command.branchName = branchName;
    command.branchType = static_cast<SvnRepo::BranchType>(branchType);
    command.syncUuid = eventUuid;
    command.page = page;

    EventManager::Event event(
        EventManager::EventType::SvnRepoManagerGetBranchHistory, eventUuid);
    event.addData(command);
    eventManager->send(event);

    static const std::set types = {
        EventManager::EventType::SvnRepoManagerGetBranchHistoryError,
        EventManager::EventType::SvnRepoManagerGetBranchHistoryResult};

    const auto &[success, resultEvent] =
        eventManager->waitForAnswer(token, eventUuid, types);

    const bool ok =
        success &&
        resultEvent.type() ==
            EventManager::EventType::SvnRepoManagerGetBranchHistoryResult;

    GitRepo::GitHistory history;
    if (ok) {
      history = convertSvnHistoryToGitHistory(
          resultEvent.data<SvnRepoResult>().value.value<SvnRepo::SvnHistory>());
    }

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      if (ok) {
        _container._historyModel->appendPage(history, page, generation);
      } else {
        _container._historyModel->pageFailed(generation);
      }
    });
  };

  if (lowPriority) {
    // Loading the whole history must not hold back other requests, so it
    // waits until the container's dispatcher has nothing else to do.
    _container._dispatcher.dispatch(loadPage, nullptr, nullptr,
                                    Util::DispatcherChannel::Any,
                                    Util::DispatcherPriority::Low);
  } else {
    // Not the container's dispatcher: its pool is cleared whenever the
    // selection changes, which would drop the request and leave the model
    // waiting for it.
    QThreadPool::globalInstance()->start(loadPage);
  }

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::squash(const QWeakPointer<RepoContainer> &guard,
                                      const QString &sourceBranch,
                                      const QString &destBranch,
                                      bool signCommit) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(sourceBranch);
  Q_UNUSED(destBranch);
  Q_UNUSED(signCommit);

  reportUnsupportedOperation(_container.tr("Squash"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::bisect(const QWeakPointer<RepoContainer> &guard,
                                      GitRepo::BisectAction action,
                                      const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(action);
  Q_UNUSED(commit);

  reportUnsupportedOperation(_container.tr("Bisect"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::startGitFlowBranch(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowBranchType branchType, const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(branchType);
  Q_UNUSED(branchName);

  reportUnsupportedOperation(_container.tr("Git Flow"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::finishGitFlowBranch(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowBranchType branchType, const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(branchType);
  Q_UNUSED(branchName);

  reportUnsupportedOperation(_container.tr("Git Flow"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::runGitLabFlowAction(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitLabFlowAction action, const QString &branchName,
    const QString &commit) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(action);
  Q_UNUSED(branchName);
  Q_UNUSED(commit);

  reportUnsupportedOperation(_container.tr("GitLab Flow"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::runGitHubFlowAction(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitHubFlowAction action, const QString &branchName) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(action);
  Q_UNUSED(branchName);

  reportUnsupportedOperation(_container.tr("GitHub Flow"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::runGitFlowNextAction(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowNextAction action, const QString &name,
    const QString &argument) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(action);
  Q_UNUSED(name);
  Q_UNUSED(argument);

  reportUnsupportedOperation(_container.tr("GitFlow-Next"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::applyGitFlowNextWorkflow(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowNextWorkflow &workflow) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(workflow);

  reportUnsupportedOperation(_container.tr("GitFlow-Next"));

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::getWorkflowType(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  // Subversion repositories have no branching workflow, the default one is
  // kept.
  Q_UNUSED(guard);

  LOG_PERF(0, "Exit");
}

void SvnRepoManagerBridgeImpl::setWorkflowType(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::WorkflowType workflow) {
  LOG_PERF(0, "Enter");

  Q_UNUSED(guard);
  Q_UNUSED(workflow);

  reportUnsupportedOperation(_container.tr("Changing the workflow"));

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Containers
