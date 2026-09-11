#include "containers/GitRepoManagerBridgeImpl.h"
#include "DarkThemeLexersHighlighter.h"
#include "MainWindow.h"
#include "MousePositionCorrection.h"
#include "QScintillaLexerFactory.h"
#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"
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
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QMovie>
#include <QPainter>
#include <QPixmap>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QStyleOptionViewItem>
#include <QSvgRenderer>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <Qsci/qsciscintilla.h>

#include <algorithm>
#include <functional>
#include <set>
#include <utility>

#define THREAD_ID "GitRepoContainer"

#define ALL_BRANCHES "*"
#define HEAD "HEAD"

#define MAXIMUM_PARENTS_IN_LABEL 3
#define MAX_COMMIT_SHA_LENGTH 7

#define GIT_IGNORE_FILENAME ".gitignore"
#define GIT_SUBDIR ".git"

using namespace GuiManager::Widgets;
using namespace GitRepoManager::Structs;

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

GitRepoManagerBridgeImpl::GitRepoManagerBridgeImpl(GitRepoContainer &container)
    : _container(container) {}

void GitRepoManagerBridgeImpl::blame(
    const QWeakPointer<GitRepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoBlameCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event blameEvent(EventManager::EventType::GitRepoManagerBlame,
                                 eventUuid);
  blameEvent.addData(command);
  _container._eventManager.send(blameEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerBlameError,
                           EventManager::EventType::GitRepoManagerBlameResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to retrieve blame."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit");
    return;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerBlameResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit");
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto blameResult =
      commandResult.value.value<GitRepoBlameCommandResult>();

  QStringList blameLines;
  QStringList blameLineNumbers;
  QStringList blameAuthors;
  QString widestAuthor;
  QString lexerFilePath;

  const auto appendSeparator = [&]() {
    if (!blameLines.isEmpty()) {
      blameLines += QString();
      blameLineNumbers += QString();
      blameAuthors += QString();
    }
  };

  for (const auto &fileInfo : blameResult.report.fileInfos) {
    appendSeparator();
    blameLines += QStringLiteral("File: %1").arg(fileInfo.filePath);
    blameLineNumbers += QString();
    blameAuthors += QString();

    if (lexerFilePath.isEmpty() && !fileInfo.filePath.isEmpty()) {
      lexerFilePath = fileInfo.filePath;
    }

    if (!fileInfo.errorMessage.isEmpty()) {
      blameLines += QStringLiteral("  [Failed to retrieve blame: %1]")
                        .arg(fileInfo.errorMessage);
      blameLineNumbers += QString();
      blameAuthors += QString();
      continue;
    }

    for (const auto &lineInfo : fileInfo.lineInfos) {
      blameLines += lineInfo.content;
      blameLineNumbers += QString::number(lineInfo.lineNumber);
      blameAuthors += lineInfo.author;
      if (lineInfo.author.size() > widestAuthor.size()) {
        widestAuthor = lineInfo.author;
      }
    }
  }

  int maxLineNumberDigits = 1;
  for (const auto &lineNumber : std::as_const(blameLineNumbers)) {
    if (!lineNumber.isEmpty()) {
      maxLineNumberDigits =
          std::max(maxLineNumberDigits, static_cast<int>(lineNumber.size()));
    }
  }
  const int lineWidth = maxLineNumberDigits;
  const QString lineNumberMask = QString(lineWidth, QChar('9'));

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    QsciLexer *lexer = nullptr;
    if (!lexerFilePath.isEmpty()) {
      const GuiManager::QScintillaLexerFactory factory;
      lexer = factory.createLexerForFile(lexerFilePath,
                                         _container.ui->scintillaBlameEditor);
    }

    _container.ui->scintillaBlameEditor->setLexer(nullptr);
    _container.ui->scintillaBlameEditor->setText(blameLines.join('\n'));
    _container.ui->scintillaBlameEditor->setLexer(lexer);

    _container.ui->scintillaBlameEditor->setMarginWidth(0, lineNumberMask);
    _container.ui->scintillaBlameEditor->setMarginWidth(
        1, widestAuthor.isEmpty() ? QStringLiteral(" ") : widestAuthor);

    for (int row = 0; row < blameLineNumbers.size(); ++row) {
      _container.ui->scintillaBlameEditor->setMarginText(
          row, 0, blameLineNumbers.at(row));
      _container.ui->scintillaBlameEditor->setMarginText(row, 1,
                                                         blameAuthors.at(row));
    }

    _container.showSpinnerOverlay(false);
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Blame));
  });

  LOG_PERF(0, "Exit");
}

void GitRepoManagerBridgeImpl::setUpstream(
    const QWeakPointer<GitRepoContainer> &guard, const QString &branchName,
    const QString &remoteName, const QString &remoteBranchName,
    const std::function<void(const QWeakPointer<GitRepoContainer> &)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoSetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.remoteBranchName = remoteBranchName;
  command.localBranchName = branchName;

  EventManager::Event setUpstreamStashEvent(
      EventManager::EventType::GitRepoManagerSetUpstream, eventUuid);
  setUpstreamStashEvent.addData(command);
  _container._eventManager.send(setUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerSetUpstreamError,
      EventManager::EventType::GitRepoManagerSetUpstreamResult};

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
      EventManager::EventType::GitRepoManagerSetUpstreamResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::stash(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event stashFilesEvent(
      EventManager::EventType::GitRepoManagerStash, eventUuid);
  stashFilesEvent.addData(command);
  _container._eventManager.send(stashFilesEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerStashError,
                           EventManager::EventType::GitRepoManagerStashResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerStashResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
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

void GitRepoManagerBridgeImpl::abortMerge(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoAbortMergeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event abortMergeEvent(
      EventManager::EventType::GitRepoManagerAbortMerge, eventUuid);
  abortMergeEvent.addData(command);
  _container._eventManager.send(abortMergeEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerAbortMergeError,
      EventManager::EventType::GitRepoManagerAbortMergeResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerAbortMergeResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::addSubmodule(
    const QWeakPointer<GitRepoContainer> &guard, const QString &url,
    const QString &name, const QString &branch) {
  LOG_PERF(0, "Enter");

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoAddSubmoduleCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.url = url;
  command.name = name;
  command.branch = branch;

  EventManager::Event addSubmoduleEvent(
      EventManager::EventType::GitRepoManagerAddSubmodule, eventUuid);
  addSubmoduleEvent.addData(command);
  _container._eventManager.send(addSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerAddSubmoduleError,
      EventManager::EventType::GitRepoManagerAddSubmoduleResult};

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
    _container.prepareWatcher(guard);

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerAddSubmoduleResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });
    _container.prepareWatcher(guard);

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const bool addSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);
  _container.prepareWatcher(guard);

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

void GitRepoManagerBridgeImpl::amend(
    const QWeakPointer<GitRepoContainer> &guard, const QString &summary,
    const QString &description, const QString &author, const QString &email,
    bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoAmendCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;
  command.signoff = signoff;
  command.signCommit = signCommit;

  EventManager::Event amendEvent(EventManager::EventType::GitRepoManagerAmend,
                                 eventUuid);
  amendEvent.addData(command);
  _container._eventManager.send(amendEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerAmendError,
                           EventManager::EventType::GitRepoManagerAmendResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerAmendResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::applyPatch(
    const QWeakPointer<GitRepoContainer> &guard, const QString &filePath) {
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

  GitRepoApplyPatchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.patchContent = content;

  EventManager::Event applyPatchEvent(
      EventManager::EventType::GitRepoManagerApplyPatch, eventUuid);
  applyPatchEvent.addData(command);
  _container._eventManager.send(applyPatchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerApplyPatchError,
      EventManager::EventType::GitRepoManagerApplyPatchResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerApplyPatchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::applyStash(
    const QWeakPointer<GitRepoContainer> &guard, const int stashIndex,
    const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoApplyStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event applyStashEvent(
      EventManager::EventType::GitRepoManagerApplyStash, eventUuid);
  applyStashEvent.addData(command);
  _container._eventManager.send(applyStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerApplyStashError,
      EventManager::EventType::GitRepoManagerApplyStashResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerApplyStashResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::checkoutBranch(
    const QWeakPointer<GitRepoContainer> &guard, const QString &branchName,
    const GitRepo::BranchType branchType,
    const std::function<void(const QWeakPointer<GitRepoContainer> &)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  if (branchName == ALL_BRANCHES) {
    _container.updateModelActiveBranch(guard, branchName, branchType);
    getBranchHistory(guard);
  } else {
    _container.stopWatcher(guard);

    const QString eventUuid = Uuid::createUuid();

    const int token = _container._eventManager.startWait();

    GitRepoCheckoutBranchCommand command;
    command.uuid = _container._uuid;
    command.syncUuid = eventUuid;
    command.branchName = branchName;
    command.branchType = branchType;

    EventManager::Event checkoutBranchEvent(
        EventManager::EventType::GitRepoManagerCheckoutBranch, eventUuid);
    checkoutBranchEvent.addData(command);
    _container._eventManager.send(checkoutBranchEvent);

    static std::set types = {
        EventManager::EventType::GitRepoManagerCheckoutBranchError,
        EventManager::EventType::GitRepoManagerCheckoutBranchResult};

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
        EventManager::EventType::GitRepoManagerCheckoutBranchResult) {
      const auto error = event.data<GitRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showErrorToast(error.message, _container.tr("Error"));
      });

      _container.prepareWatcher(guard);

      LOG_PERF(0, "Exit")
      return;
    }

    (void)getLocalBranches();

    const auto commandResult = event.data<GitRepoResult>();

    const auto result =
        commandResult.value.value<GitRepoCheckoutBranchCommandResult>();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.updateModelActiveBranch(guard, result.branchName,
                                         result.branchType);

      _container._dispatcher.dispatch([=]() {
        getBranchHistory(guard);

        Util::Dispatcher::dispatchToMainThread(
            guard, [=]() { _container.showSpinnerOverlay(false); });
      });
    });

    if (branchType == GitRepo::BranchType::Tag) {
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

void GitRepoManagerBridgeImpl::checkoutCommit(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoCheckoutCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event checkoutCommitEvent(
      EventManager::EventType::GitRepoManagerCheckoutCommit, eventUuid);
  checkoutCommitEvent.addData(command);
  _container._eventManager.send(checkoutCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCheckoutCommitError,
      EventManager::EventType::GitRepoManagerCheckoutCommitResult};

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
      EventManager::EventType::GitRepoManagerCheckoutCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::closeRepo() const {
  LOG_PERF(0, "Enter");

  GitRepoCloseCommand closeCommand;
  closeCommand.uuid = _container._uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::GitRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _container._eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void GitRepoManagerBridgeImpl::commit(
    const QWeakPointer<GitRepoContainer> &guard, const QString &summary,
    const QString &description, const QString &author, const QString &email,
    bool cleanup, bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoCommitStagedCommand command;
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
      EventManager::EventType::GitRepoManagerCommitStaged, eventUuid);
  commitStagedEvent.addData(command);
  _container._eventManager.send(commitStagedEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCommitStagedError,
      EventManager::EventType::GitRepoManagerCommitStagedResult};

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
      EventManager::EventType::GitRepoManagerCommitStagedResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::createBranchFromCommit(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
    const QString &branchName,
    const std::function<void(const QWeakPointer<GitRepoContainer> &,
                             const QString &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoCreateBranchFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.branchName = branchName;

  EventManager::Event createBranchFromCommitEvent(
      EventManager::EventType::GitRepoManagerCreateBranchFromCommit, eventUuid);
  createBranchFromCommitEvent.addData(command);
  _container._eventManager.send(createBranchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitError,
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitResult};

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
      EventManager::EventType::GitRepoManagerCreateBranchFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  _container.refreshHistory(guard);

  if (onSuccess) {
    onSuccess(guard, branchName);
  } else {
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showSpinnerOverlay(false); });
  }

  LOG_PERF(0, "Exit")
}

void GitRepoManagerBridgeImpl::createPatchFromCommit(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoCreatePatchFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event createPatchFromCommitEvent(
      EventManager::EventType::GitRepoManagerCreatePatchFromCommit, eventUuid);
  createPatchFromCommitEvent.addData(command);
  _container._eventManager.send(createPatchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitError,
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitResult};

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
      EventManager::EventType::GitRepoManagerCreatePatchFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto result =
      commandResult.value.value<GitRepoCreatePatchFromCommitCommandResult>();

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

void GitRepoManagerBridgeImpl::createTagFromCommit(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
    const QString &tagName, const QString &message, bool isAnnotated) {
  LOG_PERF(0, "Enter");

  const QString author =
      Common::UserProfileManager::instance().currentProfile().author;
  const QString email =
      Common::UserProfileManager::instance().currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoCreateTagFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.tagName = tagName;
  command.author = author;
  command.email = email;
  command.message = isAnnotated ? message : QString();

  EventManager::Event createTagFromCommitEvent(
      EventManager::EventType::GitRepoManagerCreateTagFromCommit, eventUuid);
  createTagFromCommitEvent.addData(command);
  _container._eventManager.send(createTagFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCreateTagFromCommitError,
      EventManager::EventType::GitRepoManagerCreateTagFromCommitResult};

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
      EventManager::EventType::GitRepoManagerCreateTagFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.showSpinnerOverlay(false); });

  LOG_PERF(0, "Exit")
}

void GitRepoManagerBridgeImpl::deleteBranch(
    const QWeakPointer<GitRepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDeleteBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteBranchEvent(
      EventManager::EventType::GitRepoManagerDeleteBranch, eventUuid);
  deleteBranchEvent.addData(command);
  _container._eventManager.send(deleteBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteBranchError,
      EventManager::EventType::GitRepoManagerDeleteBranchResult};

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
      EventManager::EventType::GitRepoManagerDeleteBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::deleteRemoteBranch(
    const QWeakPointer<GitRepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDeleteRemoteBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteRemoteBranchEvent(
      EventManager::EventType::GitRepoManagerDeleteRemoteBranch, eventUuid);
  deleteRemoteBranchEvent.addData(command);
  _container._eventManager.send(deleteRemoteBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteRemoteBranchError,
      EventManager::EventType::GitRepoManagerDeleteRemoteBranchResult};

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
      EventManager::EventType::GitRepoManagerDeleteRemoteBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::deleteStash(
    const QWeakPointer<GitRepoContainer> &guard, int stashIndex,
    const std::function<void(const QWeakPointer<GitRepoContainer> &, int)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDeleteStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event deleteStashEvent(
      EventManager::EventType::GitRepoManagerDeleteStash, eventUuid);
  deleteStashEvent.addData(command);
  _container._eventManager.send(deleteStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteStashError,
      EventManager::EventType::GitRepoManagerDeleteStashResult};

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
      EventManager::EventType::GitRepoManagerDeleteStashResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
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

void GitRepoManagerBridgeImpl::deleteAllStashes(
    const QWeakPointer<GitRepoContainer> &guard,
    const std::function<void(const QWeakPointer<GitRepoContainer> &)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDeleteAllStashesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event deleteAllStashesEvent(
      EventManager::EventType::GitRepoManagerDeleteAllStashes, eventUuid);
  deleteAllStashesEvent.addData(command);
  _container._eventManager.send(deleteAllStashesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteAllStashesError,
      EventManager::EventType::GitRepoManagerDeleteAllStashesResult};

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
      EventManager::EventType::GitRepoManagerDeleteAllStashesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
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

void GitRepoManagerBridgeImpl::deleteTag(
    const QWeakPointer<GitRepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDeleteTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::GitRepoManagerDeleteTag, eventUuid);
  deleteTagEvent.addData(command);
  _container._eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteTagError,
      EventManager::EventType::GitRepoManagerDeleteTagResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerDeleteTagResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::pushTag(
    const QWeakPointer<GitRepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoPushTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event pushTagEvent(
      EventManager::EventType::GitRepoManagerPushTag, eventUuid);
  pushTagEvent.addData(command);
  _container._eventManager.send(pushTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerPushTagError,
      EventManager::EventType::GitRepoManagerPushTagResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerPushTagResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::pushAllTags(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoPushAllTagsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event pushAllTagsEvent(
      EventManager::EventType::GitRepoManagerPushAllTags, eventUuid);
  pushAllTagsEvent.addData(command);
  _container._eventManager.send(pushAllTagsEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerPushAllTagsError,
      EventManager::EventType::GitRepoManagerPushAllTagsResult};

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
      EventManager::EventType::GitRepoManagerPushAllTagsResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::deleteRemoteTag(
    const QWeakPointer<GitRepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDeleteRemoteTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteRemoteTagEvent(
      EventManager::EventType::GitRepoManagerDeleteRemoteTag, eventUuid);
  deleteRemoteTagEvent.addData(command);
  _container._eventManager.send(deleteRemoteTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteRemoteTagError,
      EventManager::EventType::GitRepoManagerDeleteRemoteTagResult};

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
      EventManager::EventType::GitRepoManagerDeleteRemoteTagResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::openSubmodule(
    const QWeakPointer<GitRepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetSubmodulePathCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::GitRepoManagerGetSubmodulePath, eventUuid);
  deleteTagEvent.addData(command);
  _container._eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetSubmodulePathError,
      EventManager::EventType::GitRepoManagerGetSubmodulePathResult};

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
      EventManager::EventType::GitRepoManagerGetSubmodulePathResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto result =
      commandResult.value.value<GitRepoGetSubmodulePathCommandResult>();

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

void GitRepoManagerBridgeImpl::showStashContent(
    const QWeakPointer<GitRepoContainer> &guard, int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing stash content for index: {}", stashIndex);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetStashStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event getStashStatusEvent(
      EventManager::EventType::GitRepoManagerGetStashStatus, eventUuid);
  getStashStatusEvent.addData(command);
  _container._eventManager.send(getStashStatusEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetStashStatusError,
      EventManager::EventType::GitRepoManagerGetStashStatusResult};

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
      EventManager::EventType::GitRepoManagerGetStashStatusResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto result = event.data<GitRepoResult>();
  const auto stashStatus = result.value.value<GitRepo::GitCommitStatus>();

  // Store the stash index for later use (for apply/pop buttons)
  _container._currentStashIndex = stashIndex;
  _container._lastStashCommit = stashStatus.commit;

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

void GitRepoManagerBridgeImpl::onStashFileSelected(const QModelIndex &index) {
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

  const QWeakPointer<GitRepoContainer> guard = _container.getSelf().toWeakRef();

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

    GitRepoDiffFilesFromCommitCommand command;
    command.uuid = _container._uuid;
    command.syncUuid = eventUuid;
    command.files += filePath;
    command.commit = _container._lastStashCommit;

    EventManager::Event diffFilesFromCommitEvent(
        EventManager::EventType::GitRepoManagerDiffFilesFromCommit, eventUuid);
    diffFilesFromCommitEvent.addData(command);
    _container._eventManager.send(diffFilesFromCommitEvent);

    static std::set types = {
        EventManager::EventType::GitRepoManagerDiffFilesFromCommitError,
        EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult};

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
        EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult) {
      const auto error = event.data<GitRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showSpinnerOverlay(false);
        _container.showErrorToast(error.message, _container.tr("Error"));
      });

      return;
    }

    const auto commandResult = event.data<GitRepoResult>();
    const auto filesDiff =
        commandResult.value.value<GitRepoDiffFilesFromCommitCommandResult>();

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

void GitRepoManagerBridgeImpl::deleteSubmodule(
    const QWeakPointer<GitRepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  // Stop watcher to release file handles before deletion
  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  const QString fullPath = QDir(_container._path).filePath(submoduleName);

  GitRepoDeleteSubmoduleCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;
  command.submodulePath = fullPath;

  EventManager::Event deleteSubmoduleEvent(
      EventManager::EventType::GitRepoManagerDeleteSubmodule, eventUuid);
  deleteSubmoduleEvent.addData(command);
  _container._eventManager.send(deleteSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteSubmoduleError,
      EventManager::EventType::GitRepoManagerDeleteSubmoduleResult};

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
      EventManager::EventType::GitRepoManagerDeleteSubmoduleResult) {
    const auto error = event.data<GitRepoError>();
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::diff(const QWeakPointer<GitRepoContainer> &guard,
                                    const QString &filePath) {
  LOG_PERF(0, "Enter");

  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDiffFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::GitRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _container._eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiffFilesError,
      EventManager::EventType::GitRepoManagerDiffFilesResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerDiffFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto filesDiff =
      commandResult.value.value<GitRepoDiffFilesCommandResult>();

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

void GitRepoManagerBridgeImpl::diffThreeWay(
    const QWeakPointer<GitRepoContainer> &guard, const QString &filePath) {
  LOG_PERF(0, "Enter");

  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListConflictsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;

  EventManager::Event listConflictsEvent(
      EventManager::EventType::GitRepoManagerListConflicts, eventUuid);
  listConflictsEvent.addData(command);
  _container._eventManager.send(listConflictsEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListConflictsError,
      EventManager::EventType::GitRepoManagerListConflictsResult};

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
      EventManager::EventType::GitRepoManagerListConflictsResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto listConflictsResult =
      commandResult.value.value<GitRepoListConflictsCommandResult>();

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

void GitRepoManagerBridgeImpl::diffFromCommit(
    const QWeakPointer<GitRepoContainer> &guard, const QString &filePath,
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

  GitRepoDiffFilesFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;
  command.commit = commit;

  EventManager::Event diffFilesFromCommitEvent(
      EventManager::EventType::GitRepoManagerDiffFilesFromCommit, eventUuid);
  diffFilesFromCommitEvent.addData(command);
  _container._eventManager.send(diffFilesFromCommitEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiffFilesFromCommitError,
      EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult};

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
      EventManager::EventType::GitRepoManagerDiffFilesFromCommitResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto filesDiff =
      commandResult.value.value<GitRepoDiffFilesFromCommitCommandResult>();

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

void GitRepoManagerBridgeImpl::diffFiles(
    const QWeakPointer<GitRepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDiffFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::GitRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _container._eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiffFilesError,
      EventManager::EventType::GitRepoManagerDiffFilesResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerDiffFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto filesDiff =
      commandResult.value.value<GitRepoDiffFilesCommandResult>();

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

void GitRepoManagerBridgeImpl::discardFiles(
    const QWeakPointer<GitRepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoDiscardFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event discardFilesEvent(
      EventManager::EventType::GitRepoManagerDiscardFiles, eventUuid);
  discardFilesEvent.addData(command);
  _container._eventManager.send(discardFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDiscardFilesError,
      EventManager::EventType::GitRepoManagerDiscardFilesResult};

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
      EventManager::EventType::GitRepoManagerDiscardFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

bool GitRepoManagerBridgeImpl::getActiveBranchOrTag(
    const QWeakPointer<GitRepoContainer> &guard,
    const GitRepo::BranchType branchType) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetActiveBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchType = branchType;

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::GitRepoManagerGetActiveBranch, eventUuid);
  getActiveBranchEvent.addData(command);
  _container._eventManager.send(getActiveBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetActiveBranchError,
      EventManager::EventType::GitRepoManagerGetActiveBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();

    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the active local branch."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetActiveBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto result =
      commandResult.value.value<GitRepoGetActiveBranchCommandResult>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateModelActiveBranch(guard, result.branchName, branchType);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoManagerBridgeImpl::getBranchHistory(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString historyEventUuid = Uuid::createUuid();

  const int historyToken = _container._eventManager.startWait();

  GitRepoGetBranchHistoryCommand historyCommand;
  historyCommand.uuid = _container._uuid;
  historyCommand.branchName = _container._activeBranch;
  historyCommand.branchType = _container._activeBranchType;
  historyCommand.syncUuid = historyEventUuid;

  EventManager::Event historyEvent(
      EventManager::EventType::GitRepoManagerGetBranchHistory,
      historyEventUuid);
  historyEvent.addData(historyCommand);
  _container._eventManager.send(historyEvent);

  static std::set historyTypes = {
      EventManager::EventType::GitRepoManagerGetBranchHistoryError,
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult};

  const auto &[historySuccess, historyResultEvent] =
      _container._eventManager.waitForAnswer(historyToken, historyEventUuid,
                                             historyTypes);

  if (!historySuccess) {
    _container.releaseBranchHistorySemaphore();

    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to retrieve the history of branch %1."))
              .arg(_container._activeBranch),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (historyResultEvent.type() !=
      EventManager::EventType::GitRepoManagerGetBranchHistoryResult) {
    const auto error = historyResultEvent.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto historyCommandResult = historyResultEvent.data<GitRepoResult>();

  auto history = historyCommandResult.value.value<GitRepo::GitHistory>();

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _container._eventManager.startWait();

  GitRepoStatusCommand statusCommand;
  statusCommand.uuid = _container._uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::GitRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _container._eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::GitRepoManagerStatusError,
      EventManager::EventType::GitRepoManagerStatusResult};

  const auto &[statusSuccess, statusResultEvent] =
      _container._eventManager.waitForAnswer(statusToken, statusEventUuid,
                                             statusTypes);

  if (!statusSuccess) {
    _container.releaseBranchHistorySemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the repository status."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::GitRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto statusCommandResult = statusResultEvent.data<GitRepoResult>();

  const auto status = statusCommandResult.value.value<GitRepo::GitStatus>();

  GitRepo::GitHistoryWithStatus historyWithStatus;
  historyWithStatus.history = history;
  historyWithStatus.status = status;

  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.ui->labelProgress->setText("0%"); });

  _container.updateBranchHistory(guard, historyWithStatus);

  _container.releaseBranchHistorySemaphore();

  LOG_PERF(0, "Exit")

  return true;
}

QStringList GitRepoManagerBridgeImpl::getCommitBranches(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetCommitBranchesCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event branchesEvent(
      EventManager::EventType::GitRepoManagerGetCommitBranches, eventUuid);
  branchesEvent.addData(command);
  _container._eventManager.send(branchesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetCommitBranchesError,
      EventManager::EventType::GitRepoManagerGetCommitBranchesResult};

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
      EventManager::EventType::GitRepoManagerGetCommitBranchesResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto branches = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return branches;
}

QStringList GitRepoManagerBridgeImpl::getCommitTags(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  GitRepoGetCommitTagsCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event tagsEvent(
      EventManager::EventType::GitRepoManagerGetCommitTags, eventUuid);
  tagsEvent.addData(command);
  _container._eventManager.send(tagsEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetCommitTagsError,
      EventManager::EventType::GitRepoManagerGetCommitTagsResult};

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
      EventManager::EventType::GitRepoManagerGetCommitTagsResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto tags = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return tags;
}

GitRepo::GitCommitStatus GitRepoManagerBridgeImpl::getCommitStatus(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetCommitStatusCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event commitStatusEvent(
      EventManager::EventType::GitRepoManagerGetCommitStatus, eventUuid);
  commitStatusEvent.addData(command);
  _container._eventManager.send(commitStatusEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetCommitStatusError,
      EventManager::EventType::GitRepoManagerGetCommitStatusResult};

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

bool GitRepoManagerBridgeImpl::getLocalBranches() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListLocalBranchesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listLocalEvent(
      EventManager::EventType::GitRepoManagerListLocalBranches, eventUuid);
  listLocalEvent.addData(command);
  _container._eventManager.send(listLocalEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListLocalBranchesError,
      EventManager::EventType::GitRepoManagerListLocalBranchesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<GitRepoContainer> guard = _container.getSelf().toWeakRef();

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of local branches."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListLocalBranchesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto localBranches = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateLocalBranchesModel(guard, localBranches);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoManagerBridgeImpl::getRemoteBranches() {
  LOG_PERF(0, "Enter");

  const QWeakPointer<GitRepoContainer> guard = _container.getSelf().toWeakRef();

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListRemoteBranchesWithStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemoteEvent(
      EventManager::EventType::GitRepoManagerListRemoteBranchesWithStatus,
      eventUuid);
  listRemoteEvent.addData(command);
  _container._eventManager.send(listRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListRemoteBranchesWithStatusError,
      EventManager::EventType::
          GitRepoManagerListRemoteBranchesWithStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of remote branches."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::
                          GitRepoManagerListRemoteBranchesWithStatusResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto branchesWithStatus =
      commandResult.value.value<QVector<GitRepo::BranchInfo>>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateRemoteBranchesModelWithStatus(guard, branchesWithStatus);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoManagerBridgeImpl::getStashes(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListStashesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listStashesEvent(
      EventManager::EventType::GitRepoManagerListStashes, eventUuid);
  listStashesEvent.addData(command);
  _container._eventManager.send(listStashesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListStashesError,
      EventManager::EventType::GitRepoManagerListStashesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of stashes."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListStashesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto stashes = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateStashesModel(guard, stashes);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoManagerBridgeImpl::getRemotes(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListRemotesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemotesEvent(
      EventManager::EventType::GitRepoManagerListRemotes, eventUuid);
  listRemotesEvent.addData(command);
  _container._eventManager.send(listRemotesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListRemotesError,
      EventManager::EventType::GitRepoManagerListRemotesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of remotes."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListRemotesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto remotes =
      commandResult.value.value<QVector<GitRepo::RemoteInfo>>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.ui->remotesPage->updateModel(remotes);
    _container.releaseHistoryRefreshSemaphore();

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
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoManagerBridgeImpl::getSubmodules() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListSubmodulesWithStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listSubmodulesEvent(
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatus,
      eventUuid);
  listSubmodulesEvent.addData(command);
  _container._eventManager.send(listSubmodulesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatusError,
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<GitRepoContainer> guard = _container.getSelf().toWeakRef();

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of submodules."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerListSubmodulesWithStatusResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto submodulesWithStatus =
      commandResult.value.value<QVector<GitRepo::SubmoduleInfo>>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateSubmodulesModelWithStatus(guard, submodulesWithStatus);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool GitRepoManagerBridgeImpl::getTags() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoListTagsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listTagsEvent(
      EventManager::EventType::GitRepoManagerListTags, eventUuid);
  listTagsEvent.addData(command);
  _container._eventManager.send(listTagsEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerListTagsError,
      EventManager::EventType::GitRepoManagerListTagsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<GitRepoContainer> guard = _container.getSelf().toWeakRef();

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of tags."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::GitRepoManagerListTagsResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<GitRepoResult>();

  const auto tags = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateTagsModel(guard, tags);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

QPair<QString, QString> GitRepoManagerBridgeImpl::getUpstream(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = _container._activeBranch;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::GitRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _container._eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetUpstreamError,
      EventManager::EventType::GitRepoManagerGetUpstreamResult};

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
      EventManager::EventType::GitRepoManagerGetUpstreamResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showSpinnerOverlay(false); });

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

QPair<QString, QString> GitRepoManagerBridgeImpl::getUpstreamForBranch(
    const QWeakPointer<GitRepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = branchName;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::GitRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _container._eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetUpstreamError,
      EventManager::EventType::GitRepoManagerGetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() !=
      EventManager::EventType::GitRepoManagerGetUpstreamResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<GitRepoResult>();
  const auto [remoteName, remoteBranchName] =
      commandResult.value.value<GitRepoGetUpstreamCommandResult>();

  LOG_PERF(0, "Exit")

  return qMakePair(remoteName, remoteBranchName);
}

void GitRepoManagerBridgeImpl::merge(
    const QWeakPointer<GitRepoContainer> &guard, const QString &sourceBranch,
    const QString &destBranch) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoMergeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;

  EventManager::Event mergeEvent(EventManager::EventType::GitRepoManagerMerge,
                                 eventUuid);
  mergeEvent.addData(command);
  _container._eventManager.send(mergeEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerMergeError,
                           EventManager::EventType::GitRepoManagerMergeResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerMergeResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::rebase(
    const QWeakPointer<GitRepoContainer> &guard, const QString &sourceBranch,
    const QString &destBranch) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoRebaseCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;

  EventManager::Event rebaseEvent(EventManager::EventType::GitRepoManagerRebase,
                                  eventUuid);
  rebaseEvent.addData(command);
  _container._eventManager.send(rebaseEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerRebaseError,
                           EventManager::EventType::GitRepoManagerRebaseResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerRebaseResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<GitRepoResult>();
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

bool GitRepoManagerBridgeImpl::openRepo(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const Common::UserProfile profile =
      Common::UserProfileManager::instance().currentProfile();

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoOpenCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.path = _container._path;
  command.author = profile.author;
  command.email = profile.email;

  EventManager::Event openRepoEvent(
      EventManager::EventType::GitRepoManagerOpenRepo, eventUuid);
  openRepoEvent.addData(command);
  _container._eventManager.send(openRepoEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerOpenRepoError,
      EventManager::EventType::GitRepoManagerOpenRepoResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerOpenRepoResult) {
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

void GitRepoManagerBridgeImpl::pull(const QWeakPointer<GitRepoContainer> &guard,
                                    bool rebase) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoPullCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.activeBranchName = _container._activeBranch;
  command.rebase = rebase;

  EventManager::Event pullEvent(EventManager::EventType::GitRepoManagerPull,
                                eventUuid);
  pullEvent.addData(command);
  _container._eventManager.send(pullEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerPullError,
                           EventManager::EventType::GitRepoManagerPullResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerPullResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pulled = commandResult.value.toBool(); !pulled) {
    const auto error = event.data<GitRepoError>();
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
               : _container.tr("Git repository is up to-date."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void GitRepoManagerBridgeImpl::fetchAll(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoFetchAllCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event fetchAllEvent(
      EventManager::EventType::GitRepoManagerFetchAll, eventUuid);
  fetchAllEvent.addData(command);
  _container._eventManager.send(fetchAllEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerFetchAllError,
      EventManager::EventType::GitRepoManagerFetchAllResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerFetchAllResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::fetchRemote(
    const QWeakPointer<GitRepoContainer> &guard, const QString &remoteName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoFetchRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;

  EventManager::Event fetchRemoteEvent(
      EventManager::EventType::GitRepoManagerFetchRemote, eventUuid);
  fetchRemoteEvent.addData(command);
  _container._eventManager.send(fetchRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerFetchRemoteError,
      EventManager::EventType::GitRepoManagerFetchRemoteResult};

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
      EventManager::EventType::GitRepoManagerFetchRemoteResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::push(const QWeakPointer<GitRepoContainer> &guard,
                                    bool force) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoPushCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.activeBranchName = _container._activeBranch;
  command.force = force;

  EventManager::Event pushEvent(EventManager::EventType::GitRepoManagerPush,
                                eventUuid);
  pushEvent.addData(command);
  _container._eventManager.send(pushEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerPushError,
                           EventManager::EventType::GitRepoManagerPushResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerPushResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::addRemote(
    const QWeakPointer<GitRepoContainer> &guard, const QString &remoteName,
    const QString &fetchUrl, const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding remote '{}' with fetch URL '{}'", remoteName.toStdString(),
            fetchUrl.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoAddRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event addRemoteEvent(
      EventManager::EventType::GitRepoManagerAddRemote, eventUuid);
  addRemoteEvent.addData(command);
  _container._eventManager.send(addRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerAddRemoteError,
      EventManager::EventType::GitRepoManagerAddRemoteResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerAddRemoteResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::editRemote(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
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

  GitRepoUpdateRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.newRemoteName = remoteName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event updateRemoteEvent(
      EventManager::EventType::GitRepoManagerUpdateRemote, eventUuid);
  updateRemoteEvent.addData(command);
  _container._eventManager.send(updateRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerUpdateRemoteError,
      EventManager::EventType::GitRepoManagerUpdateRemoteResult};

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
      EventManager::EventType::GitRepoManagerUpdateRemoteResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::deleteRemote(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex) {
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

  GitRepoRemoveRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;

  EventManager::Event removeRemoteEvent(
      EventManager::EventType::GitRepoManagerRemoveRemote, eventUuid);
  removeRemoteEvent.addData(command);
  _container._eventManager.send(removeRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerRemoveRemoteError,
      EventManager::EventType::GitRepoManagerRemoveRemoteResult};

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
      EventManager::EventType::GitRepoManagerRemoveRemoteResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::setDefaultRemote(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex) {
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

  GitRepoSetDefaultRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;

  EventManager::Event setDefaultRemoteEvent(
      EventManager::EventType::GitRepoManagerSetDefaultRemote, eventUuid);
  setDefaultRemoteEvent.addData(command);
  _container._eventManager.send(setDefaultRemoteEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerSetDefaultRemoteError,
      EventManager::EventType::GitRepoManagerSetDefaultRemoteResult};

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
      EventManager::EventType::GitRepoManagerSetDefaultRemoteResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::addTarget(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
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

  GitRepoAddTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = targetName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event addTargetEvent(
      EventManager::EventType::GitRepoManagerAddTarget, eventUuid);
  addTargetEvent.addData(command);
  _container._eventManager.send(addTargetEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerAddTargetError,
      EventManager::EventType::GitRepoManagerAddTargetResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerAddTargetResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::editTarget(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
    int targetIndex, const QString &newFetchUrl, const QString &newPushUrl) {
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

  GitRepoEditTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;
  command.newFetchUrl = newFetchUrl;
  command.newPushUrl = newPushUrl;

  EventManager::Event editTargetEvent(
      EventManager::EventType::GitRepoManagerEditTarget, eventUuid);
  editTargetEvent.addData(command);
  _container._eventManager.send(editTargetEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerEditTargetError,
      EventManager::EventType::GitRepoManagerEditTargetResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerEditTargetResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::deleteTarget(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
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

  GitRepoDeleteTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;

  EventManager::Event deleteTargetEvent(
      EventManager::EventType::GitRepoManagerDeleteTarget, eventUuid);
  deleteTargetEvent.addData(command);
  _container._eventManager.send(deleteTargetEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerDeleteTargetError,
      EventManager::EventType::GitRepoManagerDeleteTargetResult};

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
      EventManager::EventType::GitRepoManagerDeleteTargetResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::setDefaultTarget(
    const QWeakPointer<GitRepoContainer> &guard, int remoteIndex,
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

  GitRepoSetDefaultTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;

  EventManager::Event setDefaultTargetEvent(
      EventManager::EventType::GitRepoManagerSetDefaultTarget, eventUuid);
  setDefaultTargetEvent.addData(command);
  _container._eventManager.send(setDefaultTargetEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerSetDefaultTargetError,
      EventManager::EventType::GitRepoManagerSetDefaultTargetResult};

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
      EventManager::EventType::GitRepoManagerSetDefaultTargetResult) {
    const auto error = event.data<GitRepoError>();
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

void GitRepoManagerBridgeImpl::renameBranch(
    const QWeakPointer<GitRepoContainer> &guard, const QString &oldName,
    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoRenameBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.oldBranchName = oldName;
  command.newBranchName = newName;

  EventManager::Event renameBranchEvent(
      EventManager::EventType::GitRepoManagerRenameBranch, eventUuid);
  renameBranchEvent.addData(command);
  _container._eventManager.send(renameBranchEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerRenameBranchError,
      EventManager::EventType::GitRepoManagerRenameBranchResult};

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
      EventManager::EventType::GitRepoManagerRenameBranchResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::renameTag(
    const QWeakPointer<GitRepoContainer> &guard, const QString &oldName,
    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoRenameTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.oldTagName = oldName;
  command.newTagName = newName;

  EventManager::Event renameTagEvent(
      EventManager::EventType::GitRepoManagerRenameTag, eventUuid);
  renameTagEvent.addData(command);
  _container._eventManager.send(renameTagEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerRenameTagError,
      EventManager::EventType::GitRepoManagerRenameTagResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerRenameTagResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::reset(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit,
    GitRepo::ResetType type) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoResetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.resetType = type;

  EventManager::Event resetEvent(EventManager::EventType::GitRepoManagerReset,
                                 eventUuid);
  resetEvent.addData(command);
  _container._eventManager.send(resetEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerResetError,
                           EventManager::EventType::GitRepoManagerResetResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerResetResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::revert(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString summaryToRevert =
      commitSummaryFromHistoryModel(_container._historyModel.data(), commit);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoRevertCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;
  command.message = QString("[%1] %2").arg(
      _container.tr("Revert"),
      summaryToRevert.isEmpty() ? commit : summaryToRevert);

  EventManager::Event revertEvent(EventManager::EventType::GitRepoManagerRevert,
                                  eventUuid);
  revertEvent.addData(command);
  _container._eventManager.send(revertEvent);

  static std::set types = {EventManager::EventType::GitRepoManagerRevertError,
                           EventManager::EventType::GitRepoManagerRevertResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerRevertResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::cherryPick(
    const QWeakPointer<GitRepoContainer> &guard, const QString &commit) {
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

  GitRepoCherryPickCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;

  EventManager::Event cherryPickEvent(
      EventManager::EventType::GitRepoManagerCherryPick, eventUuid);
  cherryPickEvent.addData(command);
  _container._eventManager.send(cherryPickEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerCherryPickError,
      EventManager::EventType::GitRepoManagerCherryPickResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerCherryPickResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::refreshStatus(
    const QWeakPointer<GitRepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _container._eventManager.startWait();

  GitRepoStatusCommand statusCommand;
  statusCommand.uuid = _container._uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::GitRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _container._eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::GitRepoManagerStatusError,
      EventManager::EventType::GitRepoManagerStatusResult};

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
      EventManager::EventType::GitRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto statusCommandResult = statusResultEvent.data<GitRepoResult>();

  const auto status = statusCommandResult.value.value<GitRepo::GitStatus>();

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

    _container._historyModel->updateStatus(status);

    _container.simulateClickOnHistoryRow(newRow);
  });

  LOG_PERF(0, "Exit")
}

void GitRepoManagerBridgeImpl::showCommitAndMerge(
    QWeakPointer<GitRepoContainer> guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoGetIsMergeInProgressCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event getIsMergeInProgressEvent(
      EventManager::EventType::GitRepoManagerGetIsMergeInProgress, eventUuid);
  getIsMergeInProgressEvent.addData(command);
  _container._eventManager.send(getIsMergeInProgressEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressError,
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressResult};

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
      EventManager::EventType::GitRepoManagerGetIsMergeInProgressResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

void GitRepoManagerBridgeImpl::stageFiles(
    const QWeakPointer<GitRepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    _container.showSpinnerOverlay(false);
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoStageFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event stageFilesEvent(
      EventManager::EventType::GitRepoManagerStageFiles, eventUuid);
  stageFilesEvent.addData(command);
  _container._eventManager.send(stageFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerStageFilesError,
      EventManager::EventType::GitRepoManagerStageFilesResult};

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

  if (event.type() != EventManager::EventType::GitRepoManagerStageFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::unstageFiles(
    const QWeakPointer<GitRepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    _container.showSpinnerOverlay(false);
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoUnstageFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event unstageFilesEvent(
      EventManager::EventType::GitRepoManagerUnstageFiles, eventUuid);
  unstageFilesEvent.addData(command);
  _container._eventManager.send(unstageFilesEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerUnstageFilesError,
      EventManager::EventType::GitRepoManagerUnstageFilesResult};

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
      EventManager::EventType::GitRepoManagerUnstageFilesResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();
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

void GitRepoManagerBridgeImpl::updateSubmodule(
    const QWeakPointer<GitRepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  GitRepoSubmoduleUpdateCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event submoduleUpdateEvent(
      EventManager::EventType::GitRepoManagerSubmoduleUpdate, eventUuid);
  submoduleUpdateEvent.addData(command);
  _container._eventManager.send(submoduleUpdateEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerSubmoduleUpdateError,
      EventManager::EventType::GitRepoManagerSubmoduleUpdateResult};

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
      EventManager::EventType::GitRepoManagerSubmoduleUpdateResult) {
    const auto error = event.data<GitRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showSpinnerOverlay(false);
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<GitRepoResult>();

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

qint64 GitRepoManagerBridgeImpl::getFileSizeFromCommit(const QString &filePath,
                                                       const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  GitRepoGetFileSizeFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;
  command.commit = commit;

  EventManager::Event getFileSizeEvent(
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommit, eventUuid);
  getFileSizeEvent.addData(command);
  _container._eventManager.send(getFileSizeEvent);

  static std::set types = {
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommitError,
      EventManager::EventType::GitRepoManagerGetFileSizeFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

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

} // namespace GuiManager::Containers
