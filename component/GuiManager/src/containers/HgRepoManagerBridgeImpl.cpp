// MIT License
// Copyright (c) 2026 Sourcerer

#include "containers/HgRepoManagerBridgeImpl.h"
#include "DarkThemeLexersHighlighter.h"
#include "MainWindow.h"
#include "MousePositionCorrection.h"
#include "QScintillaLexerFactory.h"
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
#include <HgRepoManagerStructs.h>
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
#include <QTextBlock>
#include <QTextStream>
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

#define HEAD "HEAD"

#define MAXIMUM_PARENTS_IN_LABEL 3
#define MAX_COMMIT_SHA_LENGTH 7

#define GIT_IGNORE_FILENAME ".gitignore"
#define GIT_SUBDIR ".git"

using namespace GuiManager::Widgets;
using namespace HgRepoManager::Structs;

namespace GuiManager::Containers {

// HgRepoManager answers with plain values. These give them the field names of
// the GitRepoManager results the code below was written for.
struct BlameResult {
  GitRepo::BlameReport report;
};

struct BranchResult {
  QString branchName;
  GitRepo::BranchType branchType = GitRepo::BranchType::Unknown;
};

struct PatchResult {
  QString patchContent;
};

struct SubmodulePathResult {
  QString submodulePath;
};

struct ConflictsResult {
  QString fileContentWithMarkers;
};

struct UpstreamResult {
  QString remoteName;
  QString remoteBranchName;
};

static UpstreamResult upstreamResult(const QVariant &value) {
  const auto upstream = value.value<HgRepoUpstreamResult>();
  return {upstream.remoteName, upstream.remoteBranchName};
}

struct FileSizeResult {
  qint64 fileSize = -1;
};

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

// Shows the unified diff of a file, highlighted with the file's language, and
// marks its hunk headers, deleted and added lines. Returns the deleted and
// added lines.
static std::pair<QVector<int>, QVector<int>>
showUnifiedDiff(QsciScintilla *editor, const QString &diff,
                const QString &filePath) {
  const QString normalized =
      QString(diff).replace("\r\n", "\n").replace('\r', '\n');
  const int lineCount = normalized.count('\n') + 1;
  const int width = QString::number(lineCount).length();
  const QString mask = QString::number(0).rightJustified(width + 1, '0');

  QsciLexer *previousLexer = editor->lexer();
  editor->setLexer(nullptr);
  delete previousLexer;

  editor->setTreeSitterHighlightingEnabled(false);
  editor->markerDeleteAll();
  editor->setMarginWidth(0, mask);
  editor->setText(diff);
  editor->verticalScrollBar()->triggerAction(QAbstractSlider::SliderToMinimum);

  // The lexer has to be attached before theming so its styles get the
  // current theme colors.
  editor->setLexer(
      QScintillaLexerFactory::createDiffViewerLexerForFile(filePath, editor));

  DarkThemeLexersHighlighter highlighter;
  highlighter.applyDarkTheme(editor);

  editor->setTreeSitterHighlightingEnabled(true);

  QVector<int> deletedLines;
  QVector<int> addedLines;

  // Only lines inside hunks are content, the "---" and "+++" lines before
  // them are file headers.
  bool inHunk = false;
  for (QTextBlock block = editor->document()->begin(); block.isValid();
       block = block.next()) {
    const QString lineText = block.text();
    const int line = block.blockNumber();

    if (lineText.startsWith("diff ")) {
      inHunk = false;
    } else if (lineText.startsWith("@@")) {
      inHunk = true;
      editor->markerAdd(line, RepoContainer::UnifiedDiffHunkMarker);
    } else if (inHunk && lineText.startsWith('-')) {
      editor->markerAdd(line, RepoContainer::UnifiedDiffDeletedMarker);
      deletedLines.append(line);
    } else if (inHunk && lineText.startsWith('+')) {
      editor->markerAdd(line, RepoContainer::UnifiedDiffAddedMarker);
      addedLines.append(line);
    }
  }

  return {deletedLines, addedLines};
}

HgRepoManagerBridgeImpl::HgRepoManagerBridgeImpl(RepoContainer &container)
    : _container(container) {}

RepoFeatures HgRepoManagerBridgeImpl::features() const {
  // Like Git, with shelves for stashes, Strip for Reset, bookmarks and named
  // branches, subrepos of three kinds and phase badges. Shelves have no SHA
  // or parent.
  return RepoFeatures::GitFlow | RepoFeatures::Remotes |
         RepoFeatures::Worktrees | RepoFeatures::CommitInfoSha |
         RepoFeatures::CommitInfoEmail | RepoFeatures::HistoryColumnSha |
         RepoFeatures::HistoryContextMenuCopyToClipboard |
         RepoFeatures::HistoryContextMenuCopyToClipboardSHA |
         RepoFeatures::HistoryContextMenuStrip |
         RepoFeatures::HistoryColumnEmail | RepoFeatures::SearchBarSha |
         RepoFeatures::PushAvailable | RepoFeatures::StashInfoAuthor |
         RepoFeatures::StashInfoEmail | RepoFeatures::Shelves |
         RepoFeatures::StashInfoTitleShelf | RepoFeatures::BranchKinds |
         RepoFeatures::SubmodulesTitleSubrepos | RepoFeatures::SubrepoKinds |
         RepoFeatures::HistoryPhaseBadges |
         RepoFeatures::HistoryViewContextMenuBisect |
         RepoFeatures::HistoryViewContextMenuCheckoutCommit |
         RepoFeatures::LocalBranchesDragAndDropRebase |
         RepoFeatures::LocalBranchesDragAndDropSquash |
         RepoFeatures::CherryPickDialogSHA | RepoFeatures::GitLFS |
         RepoFeatures::LocalBranchesContextMenuUpstream |
         RepoFeatures::TagsContextMenuPushToRemote |
         RepoFeatures::TagsContextMenuDeleteFromRemote |
         RepoFeatures::ExternalsAddDialogPlaceholderSubmoduleName |
         RepoFeatures::PullMenu | RepoFeatures::PushMenu |
         RepoFeatures::AllBranchesItem;
}

void HgRepoManagerBridgeImpl::blame(const QWeakPointer<RepoContainer> &guard,
                                    const QStringList &files,
                                    const QString & /*commit*/) {
  LOG_PERF(0, "Enter");

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoBlameCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event blameEvent(EventManager::EventType::HgRepoManagerBlame,
                                 eventUuid);
  blameEvent.addData(command);
  _container._eventManager.send(blameEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerBlameError,
                           EventManager::EventType::HgRepoManagerBlameResult};

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

  if (event.type() != EventManager::EventType::HgRepoManagerBlameResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit");
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto blameResult =
      BlameResult{commandResult.value.value<GitRepo::BlameReport>()};

  showBlameReport(_container, guard, blameResult.report);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::showBlameReport(
    RepoContainer &container, const QWeakPointer<RepoContainer> &guard,
    const GitRepo::BlameReport &report) {
  LOG_PERF(0, "Enter");

  QStringList blameLines;
  QStringList blameLineNumbers;
  QStringList blameAuthors;
  QStringList blameCommits;
  QStringList blameCommitToolTips;
  QString widestAuthor;
  QString lexerFilePath;

  // Adds a line that is not blamed to a commit.
  const auto appendPlainLine = [&](const QString &line) {
    blameLines += line;
    blameLineNumbers += QString();
    blameAuthors += QString();
    blameCommits += QString();
    blameCommitToolTips += QString();
  };

  const auto appendSeparator = [&]() {
    if (!blameLines.isEmpty()) {
      appendPlainLine(QString());
    }
  };

  for (const auto &fileInfo : report.fileInfos) {
    appendSeparator();
    appendPlainLine(QStringLiteral("File: %1").arg(fileInfo.filePath));

    if (lexerFilePath.isEmpty() && !fileInfo.filePath.isEmpty()) {
      lexerFilePath = fileInfo.filePath;
    }

    if (!fileInfo.errorMessage.isEmpty()) {
      appendPlainLine(QStringLiteral("  [Failed to retrieve blame: %1]")
                          .arg(fileInfo.errorMessage));
      continue;
    }

    for (const auto &lineInfo : fileInfo.lineInfos) {
      const QString author =
          lineInfo.email.isEmpty()
              ? lineInfo.author
              : QStringLiteral("%1 <%2>").arg(lineInfo.author, lineInfo.email);
      // Uncommitted lines are blamed to the all zeros commit, which is not
      // in the history.
      const bool committed =
          !lineInfo.commit.isEmpty() &&
          lineInfo.commit.count(QChar('0')) != lineInfo.commit.size();

      blameLines += lineInfo.content;
      blameLineNumbers += QString::number(lineInfo.lineNumber);
      blameAuthors += author;
      blameCommits += committed ? lineInfo.commit : QString();
      blameCommitToolTips +=
          committed ? container
                          .tr("%1\n%2\n\nClick to show the commit in the "
                              "history.")
                          .arg(lineInfo.commit, lineInfo.summary)
                    : container.tr("Not committed yet");

      if (author.size() > widestAuthor.size()) {
        widestAuthor = author;
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

  Util::Dispatcher::dispatchToMainThread(guard, [=, &container]() {
    auto *editor = container.ui->scintillaBlameEditor;

    QsciLexer *previousLexer = editor->lexer();
    editor->setLexer(nullptr);
    delete previousLexer;

    editor->setTreeSitterHighlightingEnabled(false);
    editor->setText(blameLines.join('\n'));
    editor->verticalScrollBar()->triggerAction(
        QAbstractSlider::SliderToMinimum);

    // The lexer has to be attached before theming so its styles get the
    // current theme colors. It is picked by the first blamed file.
    editor->setLexer(QScintillaLexerFactory::createDiffViewerLexerForFile(
        lexerFilePath, editor));

    DarkThemeLexersHighlighter highlighter;
    highlighter.applyDarkTheme(editor);

    editor->setTreeSitterHighlightingEnabled(true);

    constexpr int commitMargin = RepoContainer::BlameCommitMargin;
    constexpr int authorMargin = RepoContainer::BlameAuthorMargin;
    constexpr int shortShaLength = RepoContainer::BlameShortShaLength;

    container._blameCommits = blameCommits;

    editor->setMarginWidth(0, lineNumberMask);
    editor->setMarginWidth(commitMargin,
                           QString(shortShaLength + 1, QChar('9')));
    editor->setMarginWidth(authorMargin, widestAuthor.isEmpty()
                                             ? QStringLiteral(" ")
                                             : widestAuthor);

    for (int row = 0; row < blameLineNumbers.size(); ++row) {
      editor->setMarginText(row, 0, blameLineNumbers.at(row));
      editor->setMarginText(row, commitMargin,
                            blameCommits.at(row).left(shortShaLength));
      editor->setMarginToolTip(row, commitMargin, blameCommitToolTips.at(row));
      editor->setMarginText(row, authorMargin, blameAuthors.at(row));
    }

    container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Blame));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::setUpstream(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName,
    const QString &remoteName, const QString &remoteBranchName,
    const std::function<void(const QWeakPointer<RepoContainer> &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoSetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.remoteBranchName = remoteBranchName;
  command.localBranchName = branchName;

  EventManager::Event setUpstreamStashEvent(
      EventManager::EventType::HgRepoManagerSetUpstream, eventUuid);
  setUpstreamStashEvent.addData(command);
  _container._eventManager.send(setUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerSetUpstreamError,
      EventManager::EventType::HgRepoManagerSetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to set upstream.")),
          _container.tr("Apply"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerSetUpstreamResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool setUpstreamSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!setUpstreamSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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
      _container.showToast(_container.tr("Successfully set upstream."),
                           _container.tr("Info"));
    });
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::stash(const QWeakPointer<RepoContainer> &guard,
                                    const QString &note) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.note = note;

  EventManager::Event stashFilesEvent(
      EventManager::EventType::HgRepoManagerStash, eventUuid);
  stashFilesEvent.addData(command);
  _container._eventManager.send(stashFilesEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerStashError,
                           EventManager::EventType::HgRepoManagerStashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to stash changes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerStashResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool stashSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!stashSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to stash changes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::abortMerge(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoAbortMergeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event abortMergeEvent(
      EventManager::EventType::HgRepoManagerAbortMerge, eventUuid);
  abortMergeEvent.addData(command);
  _container._eventManager.send(abortMergeEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerAbortMergeError,
      EventManager::EventType::HgRepoManagerAbortMergeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to abort merge.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerAbortMergeResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool abortSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!abortSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to abort merge."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::abortRebase(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoAbortRebaseCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event abortRebaseEvent(
      EventManager::EventType::HgRepoManagerAbortRebase, eventUuid);
  abortRebaseEvent.addData(command);
  _container._eventManager.send(abortRebaseEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerAbortRebaseError,
      EventManager::EventType::HgRepoManagerAbortRebaseResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to abort rebase.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerAbortRebaseResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool abortSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!abortSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to abort rebase."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::addSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &url,
    const QString &name, const QString &branch) {
  addSubmoduleOfKind(guard, url, name, branch, SubrepoKind::Mercurial);
}

void HgRepoManagerBridgeImpl::addSubmoduleOfKind(
    const QWeakPointer<RepoContainer> &guard, const QString &url,
    const QString &name, const QString &branch, const SubrepoKind kind) {
  LOG_PERF(0, "Enter");

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoAddSubmoduleCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.url = url;
  command.name = name;
  command.branch = branch;
  switch (kind) {
  case SubrepoKind::Git:
    command.kind = HgRepo::HgSubrepoKind::Git;
    break;
  case SubrepoKind::Subversion:
    command.kind = HgRepo::HgSubrepoKind::Subversion;
    break;
  default:
    command.kind = HgRepo::HgSubrepoKind::Mercurial;
    break;
  }

  EventManager::Event addSubmoduleEvent(
      EventManager::EventType::HgRepoManagerAddSubmodule, eventUuid);
  addSubmoduleEvent.addData(command);
  _container._eventManager.send(addSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerAddSubmoduleError,
      EventManager::EventType::HgRepoManagerAddSubmoduleResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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
      EventManager::EventType::HgRepoManagerAddSubmoduleResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });
    _container.prepareWatcher(guard);

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool addSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);
  _container.prepareWatcher(guard);

  if (!addSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to add submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::addWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &path,
    const QString &branch, const bool createBranch, const QString &baseBranch) {
  addWorktreeOfKind(guard, path, branch, createBranch, baseBranch,
                    BranchKind::Bookmark);
}

void HgRepoManagerBridgeImpl::addWorktreeOfKind(
    const QWeakPointer<RepoContainer> &guard, const QString &path,
    const QString &branch, const bool createBranch, const QString &baseBranch,
    const BranchKind kind) {
  LOG_PERF(0, "Enter");

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoAddWorktreeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.name = QFileInfo(QDir::cleanPath(path)).fileName();
  command.path = QDir::cleanPath(path);
  command.branch = branch;
  command.createBranch = createBranch;
  command.baseBranch = baseBranch;
  command.kind = kind == BranchKind::NamedBranch
                     ? HgRepo::HgBranchKind::NamedBranch
                     : HgRepo::HgBranchKind::Bookmark;

  EventManager::Event addWorktreeEvent(
      EventManager::EventType::HgRepoManagerAddWorktree, eventUuid);
  addWorktreeEvent.addData(command);
  _container._eventManager.send(addWorktreeEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerAddWorktreeError,
      EventManager::EventType::HgRepoManagerAddWorktreeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to add worktree.")),
          _container.tr("Error"));
    });
    _container.prepareWatcher(guard);

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerAddWorktreeResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });
    _container.prepareWatcher(guard);

    LOG_PERF(0, "Exit")
    return;
  }

  // A new branch may have been created, so refresh everything.
  _container.refreshHistory(guard);
  _container.prepareWatcher(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::removeWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &name,
    const bool force) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoRemoveWorktreeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.name = name;
  command.force = force;

  EventManager::Event removeWorktreeEvent(
      EventManager::EventType::HgRepoManagerRemoveWorktree, eventUuid);
  removeWorktreeEvent.addData(command);
  _container._eventManager.send(removeWorktreeEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerRemoveWorktreeError,
      EventManager::EventType::HgRepoManagerRemoveWorktreeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to remove worktree.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerRemoveWorktreeResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getWorktrees(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::lockWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &name,
    const QString &reason) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoLockWorktreeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.name = name;
  command.reason = reason;

  EventManager::Event lockWorktreeEvent(
      EventManager::EventType::HgRepoManagerLockWorktree, eventUuid);
  lockWorktreeEvent.addData(command);
  _container._eventManager.send(lockWorktreeEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerLockWorktreeError,
      EventManager::EventType::HgRepoManagerLockWorktreeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to lock worktree.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerLockWorktreeResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getWorktrees(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::unlockWorktree(
    const QWeakPointer<RepoContainer> &guard, const QString &name) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoUnlockWorktreeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.name = name;

  EventManager::Event unlockWorktreeEvent(
      EventManager::EventType::HgRepoManagerUnlockWorktree, eventUuid);
  unlockWorktreeEvent.addData(command);
  _container._eventManager.send(unlockWorktreeEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerUnlockWorktreeError,
      EventManager::EventType::HgRepoManagerUnlockWorktreeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to unlock worktree.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerUnlockWorktreeResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getWorktrees(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::amend(const QWeakPointer<RepoContainer> &guard,
                                    const QString &summary,
                                    const QString &description,
                                    const QString &author, const QString &email,
                                    bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoAmendCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;
  command.signoff = signoff;
  command.signCommit = signCommit;

  EventManager::Event amendEvent(EventManager::EventType::HgRepoManagerAmend,
                                 eventUuid);
  amendEvent.addData(command);
  _container._eventManager.send(amendEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerAmendError,
                           EventManager::EventType::HgRepoManagerAmendResult};

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

  if (event.type() != EventManager::EventType::HgRepoManagerAmendResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::applyPatch(
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

  HgRepoApplyPatchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.patchContent = content;

  EventManager::Event applyPatchEvent(
      EventManager::EventType::HgRepoManagerApplyPatch, eventUuid);
  applyPatchEvent.addData(command);
  _container._eventManager.send(applyPatchEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerApplyPatchError,
      EventManager::EventType::HgRepoManagerApplyPatchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to apply patch.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerApplyPatchResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool applySuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!applySuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to apply patch."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::applyStash(
    const QWeakPointer<RepoContainer> &guard, const int stashIndex,
    const std::function<void(const QWeakPointer<RepoContainer> &, int)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoApplyStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event applyStashEvent(
      EventManager::EventType::HgRepoManagerApplyStash, eventUuid);
  applyStashEvent.addData(command);
  _container._eventManager.send(applyStashEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerApplyStashError,
      EventManager::EventType::HgRepoManagerApplyStashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to apply stash.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerApplyStashResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  // Extract result data - now contains success and hasConflicts information
  QMap<QString, QVariant> resultData = commandResult.value.toMap();
  const bool applySuccess = resultData.value("success", false).toBool();
  const bool hasConflicts = resultData.value("hasConflicts", false).toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!applySuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to apply stash."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  // If stash was applied but has conflicts, show merge conflict resolution UI
  if (hasConflicts) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(
          _container.tr(
              "Stash applied with conflicts. Resolve conflicts to continue."),
          _container.tr("Conflicts"));
    });
  }

  if (onSuccess) {
    onSuccess(guard, stashIndex);
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::checkoutBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName,
    const int branchType,
    const std::function<void(const QWeakPointer<RepoContainer> &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  if (branchName == ALL_BRANCHES) {
    // The checked out branch does not change, so _activeBranch is left alone.
    // Only the history that is shown changes.
    _container._historyViewAll = true;

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.updateAllBranchesActions();
      _container.ui->localPage->updateActiveBranch(QString(ALL_BRANCHES));
      _container.ui->remotePage->updateActiveBranch(QString());
      _container.ui->tagsPage->updateActiveTag(QString());
    });

    getBranchHistory(guard);
  } else {
    _container.stopWatcher(guard);

    const QString eventUuid = Uuid::createUuid();

    const int token = _container._eventManager.startWait();

    HgRepoCheckoutBranchCommand command;
    command.uuid = _container._uuid;
    command.syncUuid = eventUuid;
    command.branchName = branchName;
    command.branchType = static_cast<GitRepo::BranchType>(branchType);

    EventManager::Event checkoutBranchEvent(
        EventManager::EventType::HgRepoManagerCheckoutBranch, eventUuid);
    checkoutBranchEvent.addData(command);
    _container._eventManager.send(checkoutBranchEvent);

    static std::set types = {
        EventManager::EventType::HgRepoManagerCheckoutBranchError,
        EventManager::EventType::HgRepoManagerCheckoutBranchResult};

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
        EventManager::EventType::HgRepoManagerCheckoutBranchResult) {
      const auto error = event.data<HgRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showErrorToast(error.message, _container.tr("Error"));
      });

      _container.prepareWatcher(guard);

      LOG_PERF(0, "Exit")
      return;
    }

    (void)getLocalBranches();

    const auto commandResult = event.data<HgRepoResult>();

    const auto result = BranchResult{command.branchName, command.branchType};

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      // A real checkout leaves the history of all branches.
      _container._historyViewAll = false;
      _container.updateAllBranchesActions();
      _container.updateModelActiveBranch(guard, result.branchName,
                                         result.branchType);

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

void HgRepoManagerBridgeImpl::checkoutCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoCheckoutCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event checkoutCommitEvent(
      EventManager::EventType::HgRepoManagerCheckoutCommit, eventUuid);
  checkoutCommitEvent.addData(command);
  _container._eventManager.send(checkoutCommitEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCheckoutCommitError,
      EventManager::EventType::HgRepoManagerCheckoutCommitResult};

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
      EventManager::EventType::HgRepoManagerCheckoutCommitResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<HgRepoResult>();
  const bool checkoutSuccess = commandResult.value.toBool();

  if (!checkoutSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(
          QString(_container.tr("Failed to checkout commit. %1"))
              .arg(additionalClarificationMessage),
          _container.tr("Checkout"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container
                             .tr("You are now in detached HEAD state after "
                                 "checking out commit '%1'.")
                             .arg(commit),
                         _container.tr("Warning"));
  });

  _container.prepareWatcher(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::closeRepo() const {
  LOG_PERF(0, "Enter");

  HgRepoCloseRepoCommand closeCommand;
  closeCommand.uuid = _container._uuid;

  EventManager::Event closeRepoEvent(
      EventManager::EventType::HgRepoManagerCloseRepo);
  closeRepoEvent.addData(closeCommand);
  _container._eventManager.send(closeRepoEvent);

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::commit(const QWeakPointer<RepoContainer> &guard,
                                     const QString &summary,
                                     const QString &description,
                                     const QString &author,
                                     const QString &email, bool cleanup,
                                     bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoCommitStagedCommand command;
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
      EventManager::EventType::HgRepoManagerCommitStaged, eventUuid);
  commitStagedEvent.addData(command);
  _container._eventManager.send(commitStagedEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCommitStagedError,
      EventManager::EventType::HgRepoManagerCommitStagedResult};

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
      EventManager::EventType::HgRepoManagerCommitStagedResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::commitRebase(
    const QWeakPointer<RepoContainer> &guard, const QString &summary,
    const QString &description, const QString &author, const QString &email,
    bool cleanup, bool signoff, bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoCommitRebaseCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.author = author;
  command.email = email;
  command.summary = summary;
  command.message = description;
  command.cleanup = cleanup;
  command.signoff = signoff;
  command.signCommit = signCommit;

  EventManager::Event commitRebaseEvent(
      EventManager::EventType::HgRepoManagerCommitRebase, eventUuid);
  commitRebaseEvent.addData(command);
  _container._eventManager.send(commitRebaseEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCommitRebaseError,
      EventManager::EventType::HgRepoManagerCommitRebaseResult};

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
      EventManager::EventType::HgRepoManagerCommitRebaseResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const bool commitSuccess = commandResult.value.toBool();

  if (!commitSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::createBranchFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit,
    const QString &branchName,
    const std::function<void(const QWeakPointer<RepoContainer> &,
                             const QString &)> &onSuccess) {
  createBranchFromCommitOfKind(guard, commit, branchName, BranchKind::Bookmark,
                               onSuccess);
}

void HgRepoManagerBridgeImpl::createBranchFromCommitOfKind(
    const QWeakPointer<RepoContainer> &guard, const QString &commit,
    const QString &branchName, const BranchKind kind,
    const std::function<void(const QWeakPointer<RepoContainer> &,
                             const QString &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoCreateBranchFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.branchName = branchName;
  command.kind = kind == BranchKind::NamedBranch
                     ? HgRepo::HgBranchKind::NamedBranch
                     : HgRepo::HgBranchKind::Bookmark;

  EventManager::Event createBranchFromCommitEvent(
      EventManager::EventType::HgRepoManagerCreateBranchFromCommit, eventUuid);
  createBranchFromCommitEvent.addData(command);
  _container._eventManager.send(createBranchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCreateBranchFromCommitError,
      EventManager::EventType::HgRepoManagerCreateBranchFromCommitResult};

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
      EventManager::EventType::HgRepoManagerCreateBranchFromCommitResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  _container.refreshHistory(guard);

  if (onSuccess) {
    // Bookmarks are listed as "@name".
    onSuccess(guard, kind == BranchKind::NamedBranch
                         ? branchName
                         : QStringLiteral("@") + branchName);
  }

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::createPatchFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoCreatePatchFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;

  EventManager::Event createPatchFromCommitEvent(
      EventManager::EventType::HgRepoManagerCreatePatchFromCommit, eventUuid);
  createPatchFromCommitEvent.addData(command);
  _container._eventManager.send(createPatchFromCommitEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCreatePatchFromCommitError,
      EventManager::EventType::HgRepoManagerCreatePatchFromCommitResult};

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
      EventManager::EventType::HgRepoManagerCreatePatchFromCommitResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto result = PatchResult{commandResult.value.toString()};

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

void HgRepoManagerBridgeImpl::createTagFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &commit,
    const QString &tagName, const QString &message, bool isAnnotated) {
  LOG_PERF(0, "Enter");

  const QString author =
      Common::UserProfileManager::instance().currentProfile().author;
  const QString email =
      Common::UserProfileManager::instance().currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoCreateTagFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.tagName = tagName;
  command.author = author;
  command.email = email;
  command.message = isAnnotated ? message : QString();

  EventManager::Event createTagFromCommitEvent(
      EventManager::EventType::HgRepoManagerCreateTagFromCommit, eventUuid);
  createTagFromCommitEvent.addData(command);
  _container._eventManager.send(createTagFromCommitEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCreateTagFromCommitError,
      EventManager::EventType::HgRepoManagerCreateTagFromCommitResult};

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
      EventManager::EventType::HgRepoManagerCreateTagFromCommitResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  _container.refreshHistory(guard);

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::deleteBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  // Named branches stay in history, they can only be closed.
  if (!branchName.startsWith(QLatin1Char('@'))) {
    closeBranch(guard, branchName);
    LOG_PERF(0, "Exit");
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDeleteBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteBranchEvent(
      EventManager::EventType::HgRepoManagerDeleteBranch, eventUuid);
  deleteBranchEvent.addData(command);
  _container._eventManager.send(deleteBranchEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteBranchError,
      EventManager::EventType::HgRepoManagerDeleteBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete branch.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDeleteBranchResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

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

void HgRepoManagerBridgeImpl::deleteRemoteBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDeleteRemoteBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = branchName;

  EventManager::Event deleteRemoteBranchEvent(
      EventManager::EventType::HgRepoManagerDeleteRemoteBranch, eventUuid);
  deleteRemoteBranchEvent.addData(command);
  _container._eventManager.send(deleteRemoteBranchEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteRemoteBranchError,
      EventManager::EventType::HgRepoManagerDeleteRemoteBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete remote branch.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDeleteRemoteBranchResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(
          _container.tr("Failed to delete remote branch."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(
        _container.tr("Successfully deleted branch from remote."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::deleteStash(
    const QWeakPointer<RepoContainer> &guard, int stashIndex,
    const std::function<void(const QWeakPointer<RepoContainer> &, int)>
        &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDeleteStashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event deleteStashEvent(
      EventManager::EventType::HgRepoManagerDeleteStash, eventUuid);
  deleteStashEvent.addData(command);
  _container._eventManager.send(deleteStashEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteStashError,
      EventManager::EventType::HgRepoManagerDeleteStashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete stash.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerDeleteStashResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to delete stash."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (onSuccess) {
    onSuccess(guard, stashIndex);
  }

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::deleteAllStashes(
    const QWeakPointer<RepoContainer> &guard,
    const std::function<void(const QWeakPointer<RepoContainer> &)> &onSuccess) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDeleteAllStashesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event deleteAllStashesEvent(
      EventManager::EventType::HgRepoManagerDeleteAllStashes, eventUuid);
  deleteAllStashesEvent.addData(command);
  _container._eventManager.send(deleteAllStashesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteAllStashesError,
      EventManager::EventType::HgRepoManagerDeleteAllStashesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete all stashes.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDeleteAllStashesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  refreshStatus(guard);
  getStashes(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to delete all stashes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.showToast(_container.tr("All stashes deleted successfully."),
                         _container.tr("Success"));
  });

  if (onSuccess) {
    onSuccess(guard);
  }

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::deleteTag(
    const QWeakPointer<RepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDeleteTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::HgRepoManagerDeleteTag, eventUuid);
  deleteTagEvent.addData(command);
  _container._eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteTagError,
      EventManager::EventType::HgRepoManagerDeleteTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(QString(_container.tr("Failed to delete tag.")),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerDeleteTagResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to delete tag."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::pushTag(const QWeakPointer<RepoContainer> &guard,
                                      const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoPushTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event pushTagEvent(
      EventManager::EventType::HgRepoManagerPushTag, eventUuid);
  pushTagEvent.addData(command);
  _container._eventManager.send(pushTagEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerPushTagError,
                           EventManager::EventType::HgRepoManagerPushTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerPushTagResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to push tag to remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully pushed tag to remote."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::pushAllTags(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoPushAllTagsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event pushAllTagsEvent(
      EventManager::EventType::HgRepoManagerPushAllTags, eventUuid);
  pushAllTagsEvent.addData(command);
  _container._eventManager.send(pushAllTagsEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerPushAllTagsError,
      EventManager::EventType::HgRepoManagerPushAllTagsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerPushAllTagsResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(
          _container.tr("Failed to push all tags to remote."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(
        _container.tr("Successfully pushed all tags to remote."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::deleteRemoteTag(
    const QWeakPointer<RepoContainer> &guard, const QString &tagName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDeleteRemoteTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.tagName = tagName;

  EventManager::Event deleteRemoteTagEvent(
      EventManager::EventType::HgRepoManagerDeleteRemoteTag, eventUuid);
  deleteRemoteTagEvent.addData(command);
  _container._eventManager.send(deleteRemoteTagEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteRemoteTagError,
      EventManager::EventType::HgRepoManagerDeleteRemoteTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to delete remote tag.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDeleteRemoteTagResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool deleteSuccess = commandResult.value.toBool();

  _container.refreshHistory(guard);

  if (!deleteSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to delete remote tag."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully deleted tag from remote."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::openSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetSubmodulePathCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event deleteTagEvent(
      EventManager::EventType::HgRepoManagerGetSubmodulePath, eventUuid);
  deleteTagEvent.addData(command);
  _container._eventManager.send(deleteTagEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetSubmodulePathError,
      EventManager::EventType::HgRepoManagerGetSubmodulePathResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to get submodule path.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerGetSubmodulePathResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto result = SubmodulePathResult{commandResult.value.toString()};

  if (result.submodulePath.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to get submodule path."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const QString fullPath =
        QDir(_container._path).filePath(result.submodulePath);
    // A subrepo opens in a container of its own kind, which its
    // administration directory tells.
    emit _container.openRepoWithTypeRequested(fullPath,
                                              repositoryTypeOf(fullPath));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::showStashContent(
    const QWeakPointer<RepoContainer> &guard, int stashIndex) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Showing stash content for index: {}", stashIndex);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetStashStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.index = stashIndex;

  EventManager::Event getStashStatusEvent(
      EventManager::EventType::HgRepoManagerGetStashStatus, eventUuid);
  getStashStatusEvent.addData(command);
  _container._eventManager.send(getStashStatusEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetStashStatusError,
      EventManager::EventType::HgRepoManagerGetStashStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(
          QString(_container.tr("Failed to get stash content.")),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerGetStashStatusResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto result = event.data<HgRepoResult>();
  const auto stashStatus = result.value.value<GitRepo::GitCommitStatus>();

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
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::onStashFileSelected(const QModelIndex &index) {
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

    HgRepoDiffFilesFromCommitCommand command;
    command.uuid = _container._uuid;
    command.syncUuid = eventUuid;
    command.files += filePath;
    command.commit = _container._lastStashCommit;

    EventManager::Event diffFilesFromCommitEvent(
        EventManager::EventType::HgRepoManagerDiffFilesFromCommit, eventUuid);
    diffFilesFromCommitEvent.addData(command);
    _container._eventManager.send(diffFilesFromCommitEvent);

    static std::set types = {
        EventManager::EventType::HgRepoManagerDiffFilesFromCommitError,
        EventManager::EventType::HgRepoManagerDiffFilesFromCommitResult};

    const auto &[success, event] =
        _container._eventManager.waitForAnswer(token, eventUuid, types);

    if (!success) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showErrorToast(_container.tr("Failed to create file diff."),
                                  _container.tr("Error"));
      });

      LOG_DEBUG("Failed to get diff for stash file");
      return;
    }

    if (event.type() !=
        EventManager::EventType::HgRepoManagerDiffFilesFromCommitResult) {
      const auto error = event.data<HgRepoError>();
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showErrorToast(error.message, _container.tr("Error"));
      });

      return;
    }

    const auto commandResult = event.data<HgRepoResult>();
    const auto filesDiff = commandResult.value.value<GitRepo::FilesDiff>();

    if (filesDiff.diff.isEmpty()) {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showToast(_container.tr("No difference in stash file."),
                             _container.tr("Info"));
      });

      return;
    }

    // Display diff in stash viewers
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      showUnifiedDiff(_container.ui->scintillaStashDiffUnified, filesDiff.diff,
                      filePath);

      if (!filesDiff.deltas.isEmpty() && !filesDiff.repoContent.isEmpty()) {
        const QFileInfo fileInfo(filesDiff.repoContent.firstKey());
        _container.ui->sideBySideDiffViewerStash->loadAndHighlight(
            filesDiff.repoContent.first(), filesDiff.deltas.first(),
            fileInfo.absoluteFilePath());
      }

      _container.ui->stackedWidgetStashDiff->setCurrentIndex(
          0); // Show unified view by default
    });
  });

  LOG_DEBUG("Stash file diff dispatched");
  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::deleteSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  // Stop watcher to release file handles before deletion
  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  const QString fullPath = QDir(_container._path).filePath(submoduleName);

  HgRepoDeleteSubmoduleCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;
  command.submodulePath = fullPath;

  EventManager::Event deleteSubmoduleEvent(
      EventManager::EventType::HgRepoManagerDeleteSubmodule, eventUuid);
  deleteSubmoduleEvent.addData(command);
  _container._eventManager.send(deleteSubmoduleEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteSubmoduleError,
      EventManager::EventType::HgRepoManagerDeleteSubmoduleResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to delete submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDeleteSubmoduleResult) {
    const auto error = event.data<HgRepoError>();
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool commandSuccess = commandResult.value.toBool();

  if (!commandSuccess) {
    _container.prepareWatcher(guard);

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to delete submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  _container.prepareWatcher(guard);

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::diff(const QWeakPointer<RepoContainer> &guard,
                                   const QString &filePath) {
  LOG_PERF(0, "Enter");

  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDiffFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::HgRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _container._eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDiffFilesError,
      EventManager::EventType::HgRepoManagerDiffFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create file diff."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerDiffFilesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto filesDiff = commandResult.value.value<GitRepo::FilesDiff>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(
          _container.tr("No difference compared to the HEAD commit."),
          _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const auto [deletedLines, addedLines] = showUnifiedDiff(
        _container.ui->scintillaTextEditorUnified, filesDiff.diff, filePath);

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

    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Diff));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::diffThreeWay(
    const QWeakPointer<RepoContainer> &guard, const QString &filePath) {
  LOG_PERF(0, "Enter");

  _container._lastDiffedFilePath = filePath;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListConflictsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;

  EventManager::Event listConflictsEvent(
      EventManager::EventType::HgRepoManagerListConflicts, eventUuid);
  listConflictsEvent.addData(command);
  _container._eventManager.send(listConflictsEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListConflictsError,
      EventManager::EventType::HgRepoManagerListConflictsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create file diff."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerListConflictsResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto listConflictsResult =
      ConflictsResult{commandResult.value.toString()};

  if (listConflictsResult.fileContentWithMarkers.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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

    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::ThreeWayDiff));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::diffFromCommit(
    const QWeakPointer<RepoContainer> &guard, const QString &filePath,
    const QString &commit) {
  LOG_PERF(0, "Enter");

  // Guard against malicious multiple calls
  if (!_container._lastDiffRepoStatusChanged &&
      _container._lastDiffedCommit == commit &&
      _container._lastDiffedFilePath == filePath) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoDiffFilesFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files += filePath;
  command.commit = commit;

  EventManager::Event diffFilesFromCommitEvent(
      EventManager::EventType::HgRepoManagerDiffFilesFromCommit, eventUuid);
  diffFilesFromCommitEvent.addData(command);
  _container._eventManager.send(diffFilesFromCommitEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDiffFilesFromCommitError,
      EventManager::EventType::HgRepoManagerDiffFilesFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create file diff."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDiffFilesFromCommitResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto filesDiff = commandResult.value.value<GitRepo::FilesDiff>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(
          _container.tr("No difference compared to the HEAD commit."),
          _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const auto [deletedLines, addedLines] = showUnifiedDiff(
        _container.ui->scintillaTextEditorUnified, filesDiff.diff, filePath);

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

    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::Diff));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::diffFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDiffFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event diffFilesEvent(
      EventManager::EventType::HgRepoManagerDiffFiles, eventUuid);
  diffFilesEvent.addData(command);
  _container._eventManager.send(diffFilesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDiffFilesError,
      EventManager::EventType::HgRepoManagerDiffFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to create diff file."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerDiffFilesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto filesDiff = commandResult.value.value<GitRepo::FilesDiff>();

  if (filesDiff.diff.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(
          _container.tr("No difference compared to the HEAD commit."),
          _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

void HgRepoManagerBridgeImpl::discardFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoDiscardFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event discardFilesEvent(
      EventManager::EventType::HgRepoManagerDiscardFiles, eventUuid);
  discardFilesEvent.addData(command);
  _container._eventManager.send(discardFilesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDiscardFilesError,
      EventManager::EventType::HgRepoManagerDiscardFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to discard file(s)."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDiscardFilesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool discarded = commandResult.value.toBool();

  if (!discarded) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(_container.tr("Failed to discard file(s)."),
                           _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.simulateClickOnFirstHistoryRow();
    _container.ui->stackedWidgetMiddle->setCurrentIndex(
        static_cast<int>(MainViewPanes::History));
  });

  LOG_PERF(0, "Exit")
}

bool HgRepoManagerBridgeImpl::getActiveBranchOrTag(
    const QWeakPointer<RepoContainer> &guard, const int branchType) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetActiveBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchType = static_cast<GitRepo::BranchType>(branchType);

  EventManager::Event getActiveBranchEvent(
      EventManager::EventType::HgRepoManagerGetActiveBranch, eventUuid);
  getActiveBranchEvent.addData(command);
  _container._eventManager.send(getActiveBranchEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetActiveBranchError,
      EventManager::EventType::HgRepoManagerGetActiveBranchResult};

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
      EventManager::EventType::HgRepoManagerGetActiveBranchResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto result = BranchResult{commandResult.value.toString()};

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateModelActiveBranch(
        guard, result.branchName, static_cast<GitRepo::BranchType>(branchType));
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool HgRepoManagerBridgeImpl::getBranchHistory(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString historyEventUuid = Uuid::createUuid();

  const int historyToken = _container._eventManager.startWait();

  const bool viewAll = _container._historyViewAll;
  const QString historyBranchName =
      viewAll ? QString(ALL_BRANCHES) : _container._activeBranch;
  const GitRepo::BranchType historyBranchType =
      viewAll ? GitRepo::BranchType::All : _container._activeBranchType;

  HgRepoGetBranchHistoryCommand historyCommand;
  historyCommand.uuid = _container._uuid;
  historyCommand.branchName = historyBranchName;
  historyCommand.branchType = historyBranchType;
  historyCommand.syncUuid = historyEventUuid;
  historyCommand.page = 0;

  EventManager::Event historyEvent(
      EventManager::EventType::HgRepoManagerGetBranchHistory, historyEventUuid);
  historyEvent.addData(historyCommand);
  _container._eventManager.send(historyEvent);

  static std::set historyTypes = {
      EventManager::EventType::HgRepoManagerGetBranchHistoryError,
      EventManager::EventType::HgRepoManagerGetBranchHistoryResult};

  const auto &[historySuccess, historyResultEvent] =
      _container._eventManager.waitForAnswer(historyToken, historyEventUuid,
                                             historyTypes);

  if (!historySuccess) {
    _container.releaseBranchHistorySemaphore();

    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
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
      EventManager::EventType::HgRepoManagerGetBranchHistoryResult) {
    const auto error = historyResultEvent.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto historyCommandResult = historyResultEvent.data<HgRepoResult>();

  auto history = historyCommandResult.value.value<GitRepo::GitHistory>();

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _container._eventManager.startWait();

  HgRepoStatusCommand statusCommand;
  statusCommand.uuid = _container._uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::HgRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _container._eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::HgRepoManagerStatusError,
      EventManager::EventType::HgRepoManagerStatusResult};

  const auto &[statusSuccess, statusResultEvent] =
      _container._eventManager.waitForAnswer(statusToken, statusEventUuid,
                                             statusTypes);

  if (!statusSuccess) {
    _container.releaseBranchHistorySemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the repository status."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (statusResultEvent.type() !=
      EventManager::EventType::HgRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseBranchHistorySemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto statusCommandResult = statusResultEvent.data<HgRepoResult>();

  const auto status = statusCommandResult.value.value<GitRepo::GitStatus>();

  GitRepo::GitHistoryWithStatus historyWithStatus;
  historyWithStatus.history = history;
  historyWithStatus.status = status;
  historyWithStatus.branchName = historyBranchName;
  historyWithStatus.branchType = historyBranchType;
  historyWithStatus.uuid = _container._uuid;
  historyWithStatus.page = 0;

  _container.updateBranchHistory(guard, historyWithStatus);

  _container.releaseBranchHistorySemaphore();

  LOG_PERF(0, "Exit")

  return true;
}

void HgRepoManagerBridgeImpl::getBranchHistoryPage(
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

    HgRepoGetBranchHistoryCommand command;
    command.uuid = uuid;
    command.branchName = branchName;
    command.branchType = branchType;
    command.syncUuid = eventUuid;
    command.page = page;

    EventManager::Event event(
        EventManager::EventType::HgRepoManagerGetBranchHistory, eventUuid);
    event.addData(command);
    eventManager->send(event);

    static const std::set types = {
        EventManager::EventType::HgRepoManagerGetBranchHistoryError,
        EventManager::EventType::HgRepoManagerGetBranchHistoryResult};

    const auto &[success, resultEvent] =
        eventManager->waitForAnswer(token, eventUuid, types);

    const bool ok =
        success &&
        resultEvent.type() ==
            EventManager::EventType::HgRepoManagerGetBranchHistoryResult;

    GitRepo::GitHistory history;
    if (ok) {
      history =
          resultEvent.data<HgRepoResult>().value.value<GitRepo::GitHistory>();
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

  LOG_PERF(0, "Exit")
}

QStringList HgRepoManagerBridgeImpl::getCommitBranches(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetCommitBranchesCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event branchesEvent(
      EventManager::EventType::HgRepoManagerGetCommitBranches, eventUuid);
  branchesEvent.addData(command);
  _container._eventManager.send(branchesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetCommitBranchesError,
      EventManager::EventType::HgRepoManagerGetCommitBranchesResult};

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
      EventManager::EventType::HgRepoManagerGetCommitBranchesResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto branches = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return branches;
}

QStringList
HgRepoManagerBridgeImpl::getCommitTags(const QWeakPointer<RepoContainer> &guard,
                                       const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  HgRepoGetCommitTagsCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event tagsEvent(
      EventManager::EventType::HgRepoManagerGetCommitTags, eventUuid);
  tagsEvent.addData(command);
  _container._eventManager.send(tagsEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetCommitTagsError,
      EventManager::EventType::HgRepoManagerGetCommitTagsResult};

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
      EventManager::EventType::HgRepoManagerGetCommitTagsResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto tags = commandResult.value.value<QStringList>();

  LOG_PERF(0, "Exit");

  return tags;
}

GitRepo::GitCommitStatus HgRepoManagerBridgeImpl::getCommitStatus(
    const QWeakPointer<RepoContainer> &guard, const QString &commit) const {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetCommitStatusCommand command;
  command.uuid = _container._uuid;
  command.commitOid = commit;
  command.syncUuid = eventUuid;

  EventManager::Event commitStatusEvent(
      EventManager::EventType::HgRepoManagerGetCommitStatus, eventUuid);
  commitStatusEvent.addData(command);
  _container._eventManager.send(commitStatusEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetCommitStatusError,
      EventManager::EventType::HgRepoManagerGetCommitStatusResult};

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
      EventManager::EventType::HgRepoManagerGetCommitStatusResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto commitStatus =
      commandResult.value.value<GitRepo::GitCommitStatus>();

  LOG_PERF(0, "Exit");

  return commitStatus;
}

bool HgRepoManagerBridgeImpl::getLocalBranches() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListLocalBranchesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listLocalEvent(
      EventManager::EventType::HgRepoManagerListLocalBranches, eventUuid);
  listLocalEvent.addData(command);
  _container._eventManager.send(listLocalEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListLocalBranchesError,
      EventManager::EventType::HgRepoManagerListLocalBranchesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

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
      EventManager::EventType::HgRepoManagerListLocalBranchesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  // Named branches by name, bookmarks as "@name", shown in two sections.
  const auto branchInfos =
      commandResult.value.value<QVector<HgRepo::HgBranchInfo>>();
  QStringList localBranches;
  Models::BranchSection namedBranches{_container.tr("Named branches"), {}, {}};
  Models::BranchSection bookmarks{_container.tr("Bookmarks"), {}, {}};
  for (const HgRepo::HgBranchInfo &info : branchInfos) {
    if (info.kind == HgRepo::HgBranchKind::Bookmark) {
      bookmarks.branches += QStringLiteral("@") + info.name;
      bookmarks.labels += info.name;
      localBranches += QStringLiteral("@") + info.name;
    } else {
      namedBranches.branches += info.name;
      namedBranches.labels +=
          info.isNew ? _container.tr("%1 (new)").arg(info.name) : info.name;
      localBranches += info.name;
    }
  }

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateLocalBranchesModel(guard, localBranches);
    if (const auto locked = guard.lock()) {
      _container.ui->localPage->updateModel(
          QVector<Models::BranchSection>{namedBranches, bookmarks});
    }
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool HgRepoManagerBridgeImpl::getRemoteBranches() {
  LOG_PERF(0, "Enter");

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListRemoteBranchesWithStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemoteEvent(
      EventManager::EventType::HgRepoManagerListRemoteBranchesWithStatus,
      eventUuid);
  listRemoteEvent.addData(command);
  _container._eventManager.send(listRemoteEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListRemoteBranchesWithStatusError,
      EventManager::EventType::HgRepoManagerListRemoteBranchesWithStatusResult};

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
                          HgRepoManagerListRemoteBranchesWithStatusResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto branchesWithStatus =
      commandResult.value.value<QVector<GitRepo::BranchInfo>>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateRemoteBranchesModelWithStatus(guard, branchesWithStatus);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool HgRepoManagerBridgeImpl::getStashes(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListStashesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listStashesEvent(
      EventManager::EventType::HgRepoManagerListStashes, eventUuid);
  listStashesEvent.addData(command);
  _container._eventManager.send(listStashesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListStashesError,
      EventManager::EventType::HgRepoManagerListStashesResult};

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

  if (event.type() != EventManager::EventType::HgRepoManagerListStashesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto stashes = commandResult.value.value<QStringList>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateStashesModel(guard, stashes);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool HgRepoManagerBridgeImpl::getRemotes(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListRemotesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listRemotesEvent(
      EventManager::EventType::HgRepoManagerListRemotes, eventUuid);
  listRemotesEvent.addData(command);
  _container._eventManager.send(listRemotesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListRemotesError,
      EventManager::EventType::HgRepoManagerListRemotesResult};

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

  if (event.type() != EventManager::EventType::HgRepoManagerListRemotesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

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

bool HgRepoManagerBridgeImpl::getSubmodules() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListSubmodulesWithStatusCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listSubmodulesEvent(
      EventManager::EventType::HgRepoManagerListSubmodulesWithStatus,
      eventUuid);
  listSubmodulesEvent.addData(command);
  _container._eventManager.send(listSubmodulesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListSubmodulesWithStatusError,
      EventManager::EventType::HgRepoManagerListSubmodulesWithStatusResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

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
      EventManager::EventType::HgRepoManagerListSubmodulesWithStatusResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto submodulesWithStatus =
      commandResult.value.value<HgRepoSubmodulesResult>().infos;

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateSubmodulesModelWithStatus(guard, submodulesWithStatus);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool HgRepoManagerBridgeImpl::getWorktrees(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListWorktreesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listWorktreesEvent(
      EventManager::EventType::HgRepoManagerListWorktrees, eventUuid);
  listWorktreesEvent.addData(command);
  _container._eventManager.send(listWorktreesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListWorktreesError,
      EventManager::EventType::HgRepoManagerListWorktreesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    _container.releaseHistoryRefreshSemaphore();
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to retrieve the list of worktrees."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerListWorktreesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto worktrees =
      commandResult.value.value<QVector<GitRepo::WorktreeInfo>>();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateWorktreesModel(guard, worktrees);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

bool HgRepoManagerBridgeImpl::getTags() {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoListTagsCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event listTagsEvent(
      EventManager::EventType::HgRepoManagerListTags, eventUuid);
  listTagsEvent.addData(command);
  _container._eventManager.send(listTagsEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerListTagsError,
      EventManager::EventType::HgRepoManagerListTagsResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  const QWeakPointer<RepoContainer> guard = _container.getSelf().toWeakRef();

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

  if (event.type() != EventManager::EventType::HgRepoManagerListTagsResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.releaseHistoryRefreshSemaphore();

    LOG_PERF(0, "Exit")
    return false;
  }

  const auto commandResult = event.data<HgRepoResult>();

  const auto tags = commandResult.value.value<HgRepoTagsResult>().tags;

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.updateTagsModel(guard, tags);
    _container.releaseHistoryRefreshSemaphore();
  });

  LOG_PERF(0, "Exit")

  return true;
}

QPair<QString, QString>
HgRepoManagerBridgeImpl::getUpstream(const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = _container._activeBranch;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::HgRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _container._eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetUpstreamError,
      EventManager::EventType::HgRepoManagerGetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          QString(_container.tr("Failed to get upstream.")),
          _container.tr("Apply"));
    });

    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() != EventManager::EventType::HgRepoManagerGetUpstreamResult) {
    const auto error = event.data<HgRepoError>();

    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto [remoteName, remoteBranchName] =
      upstreamResult(commandResult.value);

  refreshStatus(guard);

  LOG_PERF(0, "Exit")

  return qMakePair(remoteName, remoteBranchName);
}

QPair<QString, QString> HgRepoManagerBridgeImpl::getUpstreamForBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoGetUpstreamCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.localBranchName = branchName;

  EventManager::Event getUpstreamStashEvent(
      EventManager::EventType::HgRepoManagerGetUpstream, eventUuid);
  getUpstreamStashEvent.addData(command);
  _container._eventManager.send(getUpstreamStashEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetUpstreamError,
      EventManager::EventType::HgRepoManagerGetUpstreamResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    LOG_PERF(0, "Exit")
    return {};
  }

  if (event.type() != EventManager::EventType::HgRepoManagerGetUpstreamResult) {
    LOG_PERF(0, "Exit")
    return {};
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto [remoteName, remoteBranchName] =
      upstreamResult(commandResult.value);

  LOG_PERF(0, "Exit")

  return qMakePair(remoteName, remoteBranchName);
}

void HgRepoManagerBridgeImpl::merge(const QWeakPointer<RepoContainer> &guard,
                                    const QString &sourceBranch,
                                    const QString &destBranch) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoMergeCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;

  EventManager::Event mergeEvent(EventManager::EventType::HgRepoManagerMerge,
                                 eventUuid);
  mergeEvent.addData(command);
  _container._eventManager.send(mergeEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerMergeError,
                           EventManager::EventType::HgRepoManagerMergeResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to merge branches."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerMergeResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<HgRepoResult>();
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

void HgRepoManagerBridgeImpl::rebase(const QWeakPointer<RepoContainer> &guard,
                                     const QString &sourceBranch,
                                     const QString &destBranch,
                                     bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoRebaseCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;
  command.signCommit = signCommit;

  EventManager::Event rebaseEvent(EventManager::EventType::HgRepoManagerRebase,
                                  eventUuid);
  rebaseEvent.addData(command);
  _container._eventManager.send(rebaseEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerRebaseError,
                           EventManager::EventType::HgRepoManagerRebaseResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to rebase branches."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerRebaseResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  const auto commandResult = event.data<HgRepoResult>();
  const auto rebased = commandResult.value.toBool();

  if (!rebased) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showToast(_container.tr("Rebase has failed."),
                           _container.tr("Info"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::squash(const QWeakPointer<RepoContainer> &guard,
                                     const QString &sourceBranch,
                                     const QString &destBranch,
                                     bool signCommit) {
  LOG_PERF(0, "Enter");

  const QString author = _container._userProfileManager.currentProfile().author;
  const QString email = _container._userProfileManager.currentProfile().email;

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoSquashCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.sourceBranch = sourceBranch;
  command.destinationBranch = destBranch;
  command.author = author;
  command.email = email;
  command.signCommit = signCommit;

  EventManager::Event squashEvent(EventManager::EventType::HgRepoManagerSquash,
                                  eventUuid);
  squashEvent.addData(command);
  _container._eventManager.send(squashEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerSquashError,
                           EventManager::EventType::HgRepoManagerSquashResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to squash branches."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerSquashResult) {
    const auto error = event.data<HgRepoError>();
    // Conflicts and failures can leave the working tree changed.
    _container.refreshHistory(guard);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::bisect(const QWeakPointer<RepoContainer> &guard,
                                     GitRepo::BisectAction action,
                                     const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoBisectCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.action = action;
  command.commit = commit;

  EventManager::Event bisectEvent(EventManager::EventType::HgRepoManagerBisect,
                                  eventUuid);
  bisectEvent.addData(command);
  _container._eventManager.send(bisectEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerBisectError,
                           EventManager::EventType::HgRepoManagerBisectResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to run bisect."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerBisectResult) {
    const auto error = event.data<HgRepoError>();
    // A failed checkout can leave the session active with new marks.
    _container.refreshHistory(guard);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto result = event.data<HgRepoResult>();
  const auto status = result.value.value<GitRepo::BisectStatus>();

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    const auto shortSha = [](const QString &sha) { return sha.left(7); };

    QString message;
    if (action == GitRepo::BisectAction::Reset) {
      message = _container.tr("Bisect finished. The original state has been "
                              "restored.");
    } else if (status.finished && status.ambiguous) {
      message = _container
                    .tr("Only skipped commits are left to test. The "
                        "first bad commit is %1 or one of the skipped "
                        "commits.")
                    .arg(shortSha(status.firstBadCommit));
    } else if (status.finished) {
      message =
          _container.tr("%1 is the first bad commit: %2")
              .arg(shortSha(status.firstBadCommit), status.firstBadSummary);
    } else if (status.waitingForGood && status.waitingForBad) {
      message = _container.tr("Bisect started. Mark a good and a bad "
                              "commit to continue.");
    } else if (status.waitingForGood) {
      message = _container.tr("Mark a good commit to continue bisecting.");
    } else if (status.waitingForBad) {
      message = _container.tr("Mark a bad commit to continue bisecting.");
    } else {
      message =
          _container
              .tr("Bisecting: %1 revisions left to test (about %2 steps). "
                  "Checked out %3: %4")
              .arg(QString::number(status.remaining),
                   QString::number(status.steps),
                   shortSha(status.currentCommit), status.currentSummary);
    }

    _container.showToast(message, _container.tr("Info"));
  });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::sendGitFlowCommand(
    const QWeakPointer<RepoContainer> &guard,
    const HgRepoGitFlowCommand &gitFlowCommand, const QString &failureMessage,
    const QString &successMessage, const QString &conflictMessage) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command = gitFlowCommand;

  _container.stopWatcher(guard);

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event gitFlowEvent(
      EventManager::EventType::HgRepoManagerGitFlow, eventUuid);
  gitFlowEvent.addData(command);
  _container._eventManager.send(gitFlowEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerGitFlowError,
                           EventManager::EventType::HgRepoManagerGitFlowResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(failureMessage, _container.tr("Error"));
    });

    _container.prepareWatcher(guard);
    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerGitFlowResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    _container.prepareWatcher(guard);
    LOG_PERF(0, "Exit")
    return;
  }

  const bool conflicted =
      event.data<HgRepoResult>().value.value<HgRepoGitFlowResult>().conflicted;

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    if (conflicted && !conflictMessage.isEmpty()) {
      _container.showErrorToast(conflictMessage, _container.tr("Conflicts"));
    } else {
      _container.showToast(successMessage, _container.tr("Info"));
    }
  });

  _container.prepareWatcher(guard);
  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::startGitFlowBranch(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowBranchType branchType, const QString &branchName) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command;
  command.branchType = branchType;
  command.start = true;
  command.branchName = branchName;
  command.developmentBranchName =
      _container._configManager
          .getString(
              ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName)
          .trimmed();

  sendGitFlowCommand(
      guard, command, _container.tr("Failed to start GitFlow branch."),
      _container.tr("Successfully started branch '%1'.").arg(branchName));

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::finishGitFlowBranch(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowBranchType branchType, const QString &branchName) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command;
  command.branchType = branchType;
  command.start = false;
  command.branchName = branchName;
  command.developmentBranchName =
      _container._configManager
          .getString(
              ConfigManager::ConfigKey::GitRepoGitFlowDevelopmentBranchName)
          .trimmed();
  command.author = _container._userProfileManager.currentProfile().author;
  command.email = _container._userProfileManager.currentProfile().email;

  sendGitFlowCommand(
      guard, command, _container.tr("Failed to finish GitFlow branch."),
      _container.tr("Successfully finished branch '%1'.").arg(branchName));

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::runGitLabFlowAction(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitLabFlowAction action, const QString &branchName,
    const QString &commit) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command;
  command.workflow = GitRepo::WorkflowType::GitLabFlow;
  command.gitLabFlowAction = action;
  command.branchName = branchName;
  command.commit = commit;
  command.mainBranchName = _container.gitLabFlowMainBranchName();
  command.environmentChain = _container.gitLabFlowEnvironmentChain();
  command.author = _container._userProfileManager.currentProfile().author;
  command.email = _container._userProfileManager.currentProfile().email;

  const QString shortCommit = commit.left(7);

  switch (action) {
  case GitRepo::GitLabFlowAction::StartFeature:
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to start GitLab Flow feature branch."),
        _container.tr("Successfully started branch '%1'.").arg(branchName));
    break;
  case GitRepo::GitLabFlowAction::FinishFeature:
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to finish GitLab Flow feature branch."),
        _container.tr("Successfully finished branch '%1'.").arg(branchName));
    break;
  case GitRepo::GitLabFlowAction::Promote:
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to promote branch '%1'.").arg(branchName),
        _container.tr("Successfully promoted '%1' to '%2'.")
            .arg(branchName, GitRepo::gitLabFlowNextEnvironment(
                                 command.environmentChain, branchName)));
    break;
  case GitRepo::GitLabFlowAction::StartRelease:
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to start GitLab Flow release branch."),
        _container.tr("Successfully started release branch '%1'.")
            .arg(branchName));
    break;
  case GitRepo::GitLabFlowAction::CherryPickToRelease:
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to cherry-pick commit to release branch."),
        _container.tr("Successfully cherry-picked commit %1 to '%2'.")
            .arg(shortCommit, branchName),
        _container
            .tr("Cherry-picking commit %1 to '%2' produced conflicts. "
                "Resolve them on '%2' and commit.")
            .arg(shortCommit, branchName));
    break;
  case GitRepo::GitLabFlowAction::Unknown:
  case GitRepo::GitLabFlowAction::last:
  default:
    LOG_ERROR(Log::ErrorId::Gui, "Unknown GitLab Flow action {}",
              static_cast<int>(action));
    break;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::runGitHubFlowAction(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitHubFlowAction action, const QString &branchName) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command;
  command.workflow = GitRepo::WorkflowType::GitHubFlow;
  command.gitHubFlowAction = action;
  command.branchName = branchName;
  command.mainBranchName = _container.gitHubFlowMainBranchName();
  command.author = _container._userProfileManager.currentProfile().author;
  command.email = _container._userProfileManager.currentProfile().email;

  switch (action) {
  case GitRepo::GitHubFlowAction::StartBranch:
    sendGitFlowCommand(
        guard, command, _container.tr("Failed to start GitHub Flow branch."),
        _container.tr("Successfully started branch '%1'.").arg(branchName));
    break;
  case GitRepo::GitHubFlowAction::UpdateBranch:
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to update branch '%1' from '%2'.")
            .arg(branchName, command.mainBranchName),
        _container.tr("Successfully updated branch '%1' from '%2'.")
            .arg(branchName, command.mainBranchName));
    break;
  case GitRepo::GitHubFlowAction::FinishBranch:
    sendGitFlowCommand(
        guard, command, _container.tr("Failed to finish GitHub Flow branch."),
        _container.tr("Successfully finished branch '%1'.").arg(branchName));
    break;
  case GitRepo::GitHubFlowAction::Unknown:
  case GitRepo::GitHubFlowAction::last:
  default:
    LOG_ERROR(Log::ErrorId::Gui, "Unknown GitHub Flow action {}",
              static_cast<int>(action));
    break;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::runGitFlowNextAction(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowNextAction action, const QString &name,
    const QString &argument) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command;
  command.workflow = GitRepo::WorkflowType::GitFlowNext;
  command.gitFlowNextAction = action;
  command.author = _container._userProfileManager.currentProfile().author;
  command.email = _container._userProfileManager.currentProfile().email;

  switch (action) {
  case GitRepo::GitFlowNextAction::Init:
    command.mainBranchName = name;
    command.developmentBranchName = argument;
    sendGitFlowCommand(
        guard, command, _container.tr("Failed to initialize git-flow-next."),
        _container.tr("Initialized git-flow-next with '%1' and '%2'.")
            .arg(name, argument));
    // The menu is built from the configuration that was just written.
    getWorkflowType(guard);
    break;
  case GitRepo::GitFlowNextAction::Start:
    command.topicType = name;
    command.branchName = argument;
    sendGitFlowCommand(
        guard, command, _container.tr("Failed to start %1 branch.").arg(name),
        _container.tr("Successfully started branch '%1'.").arg(argument));
    break;
  case GitRepo::GitFlowNextAction::Finish:
    command.branchName = name;
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to finish branch '%1'.").arg(name),
        _container.tr("Successfully finished branch '%1'.").arg(name));
    break;
  case GitRepo::GitFlowNextAction::Update:
    command.branchName = name;
    sendGitFlowCommand(
        guard, command,
        _container.tr("Failed to update branch '%1'.").arg(name),
        _container.tr("Successfully updated branch '%1'.").arg(name));
    break;
  case GitRepo::GitFlowNextAction::Unknown:
  case GitRepo::GitFlowNextAction::last:
  default:
    LOG_ERROR(Log::ErrorId::Gui, "Unknown git-flow-next action {}",
              static_cast<int>(action));
    break;
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::applyGitFlowNextWorkflow(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::GitFlowNextWorkflow &workflow) {
  LOG_PERF(0, "Enter");

  HgRepoGitFlowCommand command;
  command.workflow = GitRepo::WorkflowType::GitFlowNext;
  command.gitFlowNextAction = GitRepo::GitFlowNextAction::Apply;
  command.customWorkflow = workflow;

  sendGitFlowCommand(
      guard, command,
      _container.tr("Failed to apply the workflow '%1'.").arg(workflow.name),
      _container.tr("Applied the workflow '%1' to this repository.")
          .arg(workflow.name));
  // The menu is built from the configuration that was just written.
  getWorkflowType(guard);

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::getWorkflowType(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  HgRepoWorkflowCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.write = false;

  EventManager::Event workflowEvent(
      EventManager::EventType::HgRepoManagerWorkflow, eventUuid);
  workflowEvent.addData(command);
  _container._eventManager.send(workflowEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerWorkflowError,
      EventManager::EventType::HgRepoManagerWorkflowResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  // Repositories without a stored workflow use classic Git Flow, which is
  // what the menu shows until a result arrives.
  if (!success ||
      event.type() != EventManager::EventType::HgRepoManagerWorkflowResult) {
    LOG_DEBUG("Failed to read the repository workflow, using Git Flow");
    LOG_PERF(0, "Exit")
    return;
  }

  const auto workflow =
      event.data<HgRepoResult>().value.value<HgRepoWorkflowResult>();
  const GitRepo::WorkflowState state = workflow.state;
  _flowBranchKind = workflow.flowBranchKind;
  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.applyWorkflowState(state); });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::setWorkflowType(
    const QWeakPointer<RepoContainer> &guard,
    const GitRepo::WorkflowType workflow) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  HgRepoWorkflowCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.write = true;
  command.workflow = workflow;

  EventManager::Event workflowEvent(
      EventManager::EventType::HgRepoManagerWorkflow, eventUuid);
  workflowEvent.addData(command);
  _container._eventManager.send(workflowEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerWorkflowError,
      EventManager::EventType::HgRepoManagerWorkflowResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success ||
      event.type() != EventManager::EventType::HgRepoManagerWorkflowResult) {
    const QString message =
        success ? event.data<HgRepoError>().message
                : _container.tr("Failed to save the repository workflow.");
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(message, _container.tr("Error"));
    });

    // Bring the menu back in line with what is stored in the repository.
    getWorkflowType(guard);
    LOG_PERF(0, "Exit")
    return;
  }

  // The result also carries the git-flow-next configuration of the repository.
  const auto stored =
      event.data<HgRepoResult>().value.value<HgRepoWorkflowResult>();
  const GitRepo::WorkflowState state = stored.state;
  _flowBranchKind = stored.flowBranchKind;
  Util::Dispatcher::dispatchToMainThread(
      guard, [=]() { _container.applyWorkflowState(state); });

  LOG_PERF(0, "Exit");
}

bool HgRepoManagerBridgeImpl::openRepo(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const Common::UserProfile profile =
      Common::UserProfileManager::instance().currentProfile();

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoOpenRepoCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.path = _container._path;
  command.author = profile.author;
  command.email = profile.email;

  EventManager::Event openRepoEvent(
      EventManager::EventType::HgRepoManagerOpenRepo, eventUuid);
  openRepoEvent.addData(command);
  _container._eventManager.send(openRepoEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerOpenRepoError,
      EventManager::EventType::HgRepoManagerOpenRepoResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return false;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerOpenRepoResult) {
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

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

void HgRepoManagerBridgeImpl::pull(const QWeakPointer<RepoContainer> &guard,
                                   bool rebase) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoPullCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = _container._activeBranch;
  command.rebase = rebase;

  EventManager::Event pullEvent(EventManager::EventType::HgRepoManagerPull,
                                eventUuid);
  pullEvent.addData(command);
  _container._eventManager.send(pullEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerPullError,
                           EventManager::EventType::HgRepoManagerPullResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerPullResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pulled = commandResult.value.toBool(); !pulled) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(
        rebase ? _container.tr("Pull with rebase completed successfully.")
               : _container.tr("Git repository is up to-date."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::fetchAll(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoFetchAllCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event fetchAllEvent(
      EventManager::EventType::HgRepoManagerFetchAll, eventUuid);
  fetchAllEvent.addData(command);
  _container._eventManager.send(fetchAllEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerFetchAllError,
      EventManager::EventType::HgRepoManagerFetchAllResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerFetchAllResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool fetched = commandResult.value.toBool(); !fetched) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to fetch from remotes."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Fetch completed successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::fetchRemote(
    const QWeakPointer<RepoContainer> &guard, const QString &remoteName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoFetchRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;

  EventManager::Event fetchRemoteEvent(
      EventManager::EventType::HgRepoManagerFetchRemote, eventUuid);
  fetchRemoteEvent.addData(command);
  _container._eventManager.send(fetchRemoteEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerFetchRemoteError,
      EventManager::EventType::HgRepoManagerFetchRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerFetchRemoteResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool fetched = commandResult.value.toBool(); !fetched) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to fetch from remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Fetch completed successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::push(const QWeakPointer<RepoContainer> &guard,
                                   bool force) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoPushCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.branchName = _container._activeBranch;
  command.force = force;

  EventManager::Event pushEvent(EventManager::EventType::HgRepoManagerPush,
                                eventUuid);
  pushEvent.addData(command);
  _container._eventManager.send(pushEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerPushError,
                           EventManager::EventType::HgRepoManagerPushResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerPushResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);
  refreshStatus(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(
        force ? _container.tr("Successfully force-pushed to remote.")
              : _container.tr("Successfully pushed to remote."),
        _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::addRemote(
    const QWeakPointer<RepoContainer> &guard, const QString &remoteName,
    const QString &fetchUrl, const QString &pushUrl) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Adding remote '{}' with fetch URL '{}'", remoteName.toStdString(),
            fetchUrl.toStdString());

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoAddRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remoteName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event addRemoteEvent(
      EventManager::EventType::HgRepoManagerAddRemote, eventUuid);
  addRemoteEvent.addData(command);
  _container._eventManager.send(addRemoteEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerAddRemoteError,
      EventManager::EventType::HgRepoManagerAddRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to add remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerAddRemoteResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Remote added successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::editRemote(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    const QString &remoteName, const QString &fetchUrl,
    const QString &pushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoUpdateRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.newRemoteName = remoteName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event updateRemoteEvent(
      EventManager::EventType::HgRepoManagerUpdateRemote, eventUuid);
  updateRemoteEvent.addData(command);
  _container._eventManager.send(updateRemoteEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerUpdateRemoteError,
      EventManager::EventType::HgRepoManagerUpdateRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to update remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerUpdateRemoteResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Remote updated successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::deleteRemote(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoRemoveRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;

  EventManager::Event removeRemoteEvent(
      EventManager::EventType::HgRepoManagerRemoveRemote, eventUuid);
  removeRemoteEvent.addData(command);
  _container._eventManager.send(removeRemoteEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerRemoveRemoteError,
      EventManager::EventType::HgRepoManagerRemoveRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to delete remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerRemoveRemoteResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Remote deleted successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::setDefaultRemote(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoSetDefaultRemoteCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;

  EventManager::Event setDefaultRemoteEvent(
      EventManager::EventType::HgRepoManagerSetDefaultRemote, eventUuid);
  setDefaultRemoteEvent.addData(command);
  _container._eventManager.send(setDefaultRemoteEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerSetDefaultRemoteError,
      EventManager::EventType::HgRepoManagerSetDefaultRemoteResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to set default remote."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerSetDefaultRemoteResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Default remote set successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::addTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    const QString &targetName, const QString &fetchUrl,
    const QString &pushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoAddTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = targetName;
  command.fetchUrl = fetchUrl;
  command.pushUrl = pushUrl;

  EventManager::Event addTargetEvent(
      EventManager::EventType::HgRepoManagerAddTarget, eventUuid);
  addTargetEvent.addData(command);
  _container._eventManager.send(addTargetEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerAddTargetError,
      EventManager::EventType::HgRepoManagerAddTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to add target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerAddTargetResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.ui->remotesPage->expandRemote(remote.name);
    _container.showToast(_container.tr("Target added successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::editTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex, int targetIndex,
    const QString &newFetchUrl, const QString &newPushUrl) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoEditTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;
  command.newFetchUrl = newFetchUrl;
  command.newPushUrl = newPushUrl;

  EventManager::Event editTargetEvent(
      EventManager::EventType::HgRepoManagerEditTarget, eventUuid);
  editTargetEvent.addData(command);
  _container._eventManager.send(editTargetEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerEditTargetError,
      EventManager::EventType::HgRepoManagerEditTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to edit target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerEditTargetResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Target edited successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::deleteTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    int targetIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoDeleteTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;

  EventManager::Event deleteTargetEvent(
      EventManager::EventType::HgRepoManagerDeleteTarget, eventUuid);
  deleteTargetEvent.addData(command);
  _container._eventManager.send(deleteTargetEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerDeleteTargetError,
      EventManager::EventType::HgRepoManagerDeleteTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to delete target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerDeleteTargetResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Target deleted successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::setDefaultTarget(
    const QWeakPointer<RepoContainer> &guard, int remoteIndex,
    int targetIndex) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 ||
      remoteIndex >= _container.ui->remotesPage->remotes().size()) {
    LOG_DEBUG("Invalid remote index: {}", remoteIndex);
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
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

  HgRepoSetDefaultTargetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.remoteName = remote.name;
  command.targetName = target.name;

  EventManager::Event setDefaultTargetEvent(
      EventManager::EventType::HgRepoManagerSetDefaultTarget, eventUuid);
  setDefaultTargetEvent.addData(command);
  _container._eventManager.send(setDefaultTargetEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerSetDefaultTargetError,
      EventManager::EventType::HgRepoManagerSetDefaultTargetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to set default target."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerSetDefaultTargetResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  getRemotes(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Default target set successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::renameBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &oldName,
    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoRenameBranchCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.oldBranchName = oldName;
  command.newBranchName = newName;

  EventManager::Event renameBranchEvent(
      EventManager::EventType::HgRepoManagerRenameBranch, eventUuid);
  renameBranchEvent.addData(command);
  _container._eventManager.send(renameBranchEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerRenameBranchError,
      EventManager::EventType::HgRepoManagerRenameBranchResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to rename branch"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerRenameBranchResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to rename branch"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully renamed branch."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::renameTag(
    const QWeakPointer<RepoContainer> &guard, const QString &oldName,
    const QString &newName) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoRenameTagCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.oldTagName = oldName;
  command.newTagName = newName;

  EventManager::Event renameTagEvent(
      EventManager::EventType::HgRepoManagerRenameTag, eventUuid);
  renameTagEvent.addData(command);
  _container._eventManager.send(renameTagEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerRenameTagError,
      EventManager::EventType::HgRepoManagerRenameTagResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to rename tag"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerRenameTagResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to rename tag"),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully renamed tag."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::reset(const QWeakPointer<RepoContainer> &guard,
                                    const QString &commit, const int type) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoResetCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.resetType = static_cast<GitRepo::ResetType>(type);

  EventManager::Event resetEvent(EventManager::EventType::HgRepoManagerReset,
                                 eventUuid);
  resetEvent.addData(command);
  _container._eventManager.send(resetEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerResetError,
                           EventManager::EventType::HgRepoManagerResetResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerResetResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to reset to commit."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully reset to commit."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::revert(const QWeakPointer<RepoContainer> &guard,
                                     const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString summaryToRevert =
      commitSummaryFromHistoryModel(_container._historyModel.data(), commit);

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoRevertCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;
  command.message = QString("[%1] %2").arg(
      _container.tr("Revert"),
      summaryToRevert.isEmpty() ? commit : summaryToRevert);

  EventManager::Event revertEvent(EventManager::EventType::HgRepoManagerRevert,
                                  eventUuid);
  revertEvent.addData(command);
  _container._eventManager.send(revertEvent);

  static std::set types = {EventManager::EventType::HgRepoManagerRevertError,
                           EventManager::EventType::HgRepoManagerRevertResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerRevertResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool pushed = commandResult.value.toBool(); !pushed) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to revert commit."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully reverted commit."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::cherryPick(
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

  HgRepoCherryPickCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.commit = commit;
  command.author =
      Common::UserProfileManager::instance().currentProfile().author;
  command.email = Common::UserProfileManager::instance().currentProfile().email;

  EventManager::Event cherryPickEvent(
      EventManager::EventType::HgRepoManagerCherryPick, eventUuid);
  cherryPickEvent.addData(command);
  _container._eventManager.send(cherryPickEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerCherryPickError,
      EventManager::EventType::HgRepoManagerCherryPickResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.showErrorPage(); });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerCherryPickResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool picked = commandResult.value.toBool(); !picked) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(_container.tr("Failed to cherry-pick commit."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Successfully cherry-picked commit."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::refreshStatus(
    const QWeakPointer<RepoContainer> &guard) {
  LOG_PERF(0, "Enter");

  const QString statusEventUuid = Uuid::createUuid();

  const int statusToken = _container._eventManager.startWait();

  HgRepoStatusCommand statusCommand;
  statusCommand.uuid = _container._uuid;
  statusCommand.syncUuid = statusEventUuid;

  EventManager::Event statusEvent(EventManager::EventType::HgRepoManagerStatus,
                                  statusEventUuid);
  statusEvent.addData(statusCommand);
  _container._eventManager.send(statusEvent);

  static std::set statusTypes = {
      EventManager::EventType::HgRepoManagerStatusError,
      EventManager::EventType::HgRepoManagerStatusResult};

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
      EventManager::EventType::HgRepoManagerStatusResult) {
    const auto error = statusResultEvent.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto statusCommandResult = statusResultEvent.data<HgRepoResult>();

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

void HgRepoManagerBridgeImpl::showCommitAndMerge(
    QWeakPointer<RepoContainer> guard) {
  LOG_PERF(0, "Enter");

  QString eventUuid = Uuid::createUuid();

  int token = _container._eventManager.startWait();

  HgRepoGetIsMergeInProgressCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event getIsMergeInProgressEvent(
      EventManager::EventType::HgRepoManagerGetIsMergeInProgress, eventUuid);
  getIsMergeInProgressEvent.addData(command);
  _container._eventManager.send(getIsMergeInProgressEvent);

  std::set types = {
      EventManager::EventType::HgRepoManagerGetIsMergeInProgressError,
      EventManager::EventType::HgRepoManagerGetIsMergeInProgressResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to get merge progress status."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerGetIsMergeInProgressResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  auto commandResult = event.data<HgRepoResult>();

  _container._mergeInProgress = commandResult.value.toBool();

  eventUuid = Uuid::createUuid();

  token = _container._eventManager.startWait();

  HgRepoGetIsRebaseInProgressCommand commandRebase;
  commandRebase.uuid = _container._uuid;
  commandRebase.syncUuid = eventUuid;

  EventManager::Event getIsRebaseInProgressEvent(
      EventManager::EventType::HgRepoManagerGetIsRebaseInProgress, eventUuid);
  getIsRebaseInProgressEvent.addData(commandRebase);
  _container._eventManager.send(getIsRebaseInProgressEvent);

  types = {EventManager::EventType::HgRepoManagerGetIsRebaseInProgressError,
           EventManager::EventType::HgRepoManagerGetIsRebaseInProgressResult};

  const auto &[successRebase, eventRebase] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!successRebase) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("Failed to get rebase progress status."),
          _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (eventRebase.type() !=
      EventManager::EventType::HgRepoManagerGetIsRebaseInProgressResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  commandResult = eventRebase.data<HgRepoResult>();

  _container._rebaseInProgress = commandResult.value.toBool();

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

void HgRepoManagerBridgeImpl::stageFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoStageFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event stageFilesEvent(
      EventManager::EventType::HgRepoManagerStageFiles, eventUuid);
  stageFilesEvent.addData(command);
  _container._eventManager.send(stageFilesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerStageFilesError,
      EventManager::EventType::HgRepoManagerStageFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to stage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() != EventManager::EventType::HgRepoManagerStageFilesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool stageAllSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!stageAllSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to stage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.simulateClickOnFirstHistoryRow();
    _container.resetCommitAndMergeWorkingTreeTreeStageAllButton();
  });

  LOG_PERF(0, "Enter");
}

void HgRepoManagerBridgeImpl::unstageFiles(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(!files.isEmpty());

  if (files.isEmpty()) {
    LOG_PERF(0, "Exit")
    return;
  }

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoUnstageFilesCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.files = files;

  EventManager::Event unstageFilesEvent(
      EventManager::EventType::HgRepoManagerUnstageFiles, eventUuid);
  unstageFilesEvent.addData(command);
  _container._eventManager.send(unstageFilesEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerUnstageFilesError,
      EventManager::EventType::HgRepoManagerUnstageFilesResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to unstage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerUnstageFilesResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const bool unstageAllSuccess = commandResult.value.toBool();

  refreshStatus(guard);

  if (!unstageAllSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to unstage files."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  Util::Dispatcher::dispatchToMainThread(guard, [&]() {
    _container.simulateClickOnFirstHistoryRow();
    _container.resetCommitAndMergeIndexTreeUnstageAllButton();
  });

  LOG_PERF(0, "Exit")
}

void HgRepoManagerBridgeImpl::updateSubmodule(
    const QWeakPointer<RepoContainer> &guard, const QString &submoduleName) {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();

  const int token = _container._eventManager.startWait();

  HgRepoSubmoduleUpdateCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.submoduleName = submoduleName;

  EventManager::Event submoduleUpdateEvent(
      EventManager::EventType::HgRepoManagerSubmoduleUpdate, eventUuid);
  submoduleUpdateEvent.addData(command);
  _container._eventManager.send(submoduleUpdateEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerSubmoduleUpdateError,
      EventManager::EventType::HgRepoManagerSubmoduleUpdateResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();

    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorPage();
      _container.showErrorToast(_container.tr("Failed to update submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  if (event.type() !=
      EventManager::EventType::HgRepoManagerSubmoduleUpdateResult) {
    const auto error = event.data<HgRepoError>();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  const auto commandResult = event.data<HgRepoResult>();

  if (const bool commandSuccess = commandResult.value.toBool();
      !commandSuccess) {
    Util::Dispatcher::dispatchToMainThread(guard, [&]() {
      _container.showErrorToast(_container.tr("Failed to update submodule."),
                                _container.tr("Error"));
    });

    LOG_PERF(0, "Exit")
    return;
  }

  _container.refreshHistory(guard);

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showToast(_container.tr("Submodule updated successfully."),
                         _container.tr("Info"));
  });

  LOG_PERF(0, "Exit")
}

qint64 HgRepoManagerBridgeImpl::getFileSizeFromCommit(const QString &filePath,
                                                      const QString &commit) {
  LOG_PERF(0, "Enter");

  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  HgRepoGetFileSizeFromCommitCommand command;
  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;
  command.filePath = filePath;
  command.commit = commit;

  EventManager::Event getFileSizeEvent(
      EventManager::EventType::HgRepoManagerGetFileSizeFromCommit, eventUuid);
  getFileSizeEvent.addData(command);
  _container._eventManager.send(getFileSizeEvent);

  static std::set types = {
      EventManager::EventType::HgRepoManagerGetFileSizeFromCommitError,
      EventManager::EventType::HgRepoManagerGetFileSizeFromCommitResult};

  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success ||
      event.type() !=
          EventManager::EventType::HgRepoManagerGetFileSizeFromCommitResult) {
    LOG_PERF(0, "Exit");
    return -1;
  }

  const auto commandResult = event.data<HgRepoResult>();
  const auto fileSizeResult = FileSizeResult{commandResult.value.toLongLong()};

  LOG_PERF(0, "Exit");
  return fileSizeResult.fileSize;
}

template <typename Command>
std::optional<HgRepoResult> HgRepoManagerBridgeImpl::sendCommand(
    const QWeakPointer<RepoContainer> &guard, Command command,
    const EventManager::EventType requestType,
    const EventManager::EventType resultType,
    const EventManager::EventType errorType, HgRepoError *error) {
  const QString eventUuid = Uuid::createUuid();
  const int token = _container._eventManager.startWait();

  command.uuid = _container._uuid;
  command.syncUuid = eventUuid;

  EventManager::Event requestEvent(requestType, eventUuid);
  requestEvent.addData(command);
  _container._eventManager.send(requestEvent);

  const std::set<EventManager::EventType> types = {resultType, errorType};
  const auto &[success, event] =
      _container._eventManager.waitForAnswer(token, eventUuid, types);

  if (!success) {
    closeRepo();
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorPage();
      _container.showErrorToast(
          _container.tr("The Mercurial repository doesn't answer."),
          _container.tr("Error"));
    });
    return std::nullopt;
  }

  if (event.type() != resultType) {
    const auto failure = event.data<HgRepoError>();
    if (error) {
      *error = failure;
    } else {
      Util::Dispatcher::dispatchToMainThread(guard, [=]() {
        _container.showErrorToast(failure.message, _container.tr("Error"));
      });
    }
    return std::nullopt;
  }

  return event.data<HgRepoResult>();
}

bool HgRepoManagerBridgeImpl::confirmOnMainThread(
    const QWeakPointer<RepoContainer> &guard, const QString &title,
    const QString &message) {
  const auto locked = guard.lock();
  if (!locked) {
    return false;
  }

  bool confirmed = false;
  QMetaObject::invokeMethod(
      &_container,
      [&]() {
        confirmed = QMessageBox::question(&_container, title, message,
                                          QMessageBox::Yes | QMessageBox::No,
                                          QMessageBox::No) == QMessageBox::Yes;
      },
      Qt::BlockingQueuedConnection);
  return confirmed;
}

void HgRepoManagerBridgeImpl::addFilesToLfs(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  HgRepoAddFilesToLfsCommand command;
  command.files = files;

  HgRepoError error;
  auto added = sendCommand(
      guard, command, EventManager::EventType::HgRepoManagerAddFilesToLfs,
      EventManager::EventType::HgRepoManagerAddFilesToLfsResult,
      EventManager::EventType::HgRepoManagerAddFilesToLfsError, &error);

  // The first file tracked with LFS turns on the lfs extension, which changes
  // what clients need to read the repository.
  if (!added && error.hgError == HgRepo::HgError::LfsNotEnabled) {
    if (!confirmOnMainThread(
            guard, _container.tr("Enable LFS"),
            _container.tr(
                "Tracking files with LFS enables Mercurial's lfs extension "
                "for this repository.\n\nAfter the next commit, clients "
                "without the lfs extension can't read the repository, and "
                "the server must support LFS.\n\nEnable LFS?"))) {
      LOG_PERF(0, "Exit");
      return;
    }

    if (!sendCommand(guard, HgRepoEnableLfsCommand(),
                     EventManager::EventType::HgRepoManagerEnableLfs,
                     EventManager::EventType::HgRepoManagerEnableLfsResult,
                     EventManager::EventType::HgRepoManagerEnableLfsError)) {
      LOG_PERF(0, "Exit");
      return;
    }

    sendCommand(guard, command,
                EventManager::EventType::HgRepoManagerAddFilesToLfs,
                EventManager::EventType::HgRepoManagerAddFilesToLfsResult,
                EventManager::EventType::HgRepoManagerAddFilesToLfsError);
  } else if (!added && !error.message.isEmpty()) {
    Util::Dispatcher::dispatchToMainThread(guard, [=]() {
      _container.showErrorToast(error.message, _container.tr("Error"));
    });
  }

  refreshStatus(guard);
  Util::Dispatcher::dispatchToMainThread(
      guard, [this]() { _container.simulateClickOnFirstHistoryRow(); });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::removeFilesFromLfs(
    const QWeakPointer<RepoContainer> &guard, const QStringList &files) {
  LOG_PERF(0, "Enter");

  HgRepoRemoveFilesFromLfsCommand command;
  command.files = files;
  sendCommand(guard, command,
              EventManager::EventType::HgRepoManagerRemoveFilesFromLfs,
              EventManager::EventType::HgRepoManagerRemoveFilesFromLfsResult,
              EventManager::EventType::HgRepoManagerRemoveFilesFromLfsError);

  refreshStatus(guard);
  Util::Dispatcher::dispatchToMainThread(
      guard, [this]() { _container.simulateClickOnFirstHistoryRow(); });

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::closeBranch(
    const QWeakPointer<RepoContainer> &guard, const QString &branchName) {
  LOG_PERF(0, "Enter");

  const Common::UserProfile profile =
      Common::UserProfileManager::instance().currentProfile();

  HgRepoCloseNamedBranchCommand command;
  command.branchName = branchName;
  command.author = profile.author;
  command.email = profile.email;

  const bool closed =
      sendCommand(guard, command,
                  EventManager::EventType::HgRepoManagerCloseNamedBranch,
                  EventManager::EventType::HgRepoManagerCloseNamedBranchResult,
                  EventManager::EventType::HgRepoManagerCloseNamedBranchError)
          .has_value();

  _container.refreshHistory(guard);
  getLocalBranches();

  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    if (closed) {
      _container.showToast(
          _container.tr("Branch '%1' was closed.").arg(branchName),
          _container.tr("Info"));
    }
  });

  LOG_PERF(0, "Exit");
}

BranchKind HgRepoManagerBridgeImpl::flowBranchKind() const {
  return _flowBranchKind == HgRepo::HgBranchKind::Bookmark
             ? BranchKind::Bookmark
             : BranchKind::NamedBranch;
}

void HgRepoManagerBridgeImpl::setFlowBranchKind(
    const QWeakPointer<RepoContainer> &guard, const BranchKind kind) {
  LOG_PERF(0, "Enter");

  HgRepoWorkflowCommand command;
  command.writeFlowBranchKind = true;
  command.flowBranchKind = kind == BranchKind::Bookmark
                               ? HgRepo::HgBranchKind::Bookmark
                               : HgRepo::HgBranchKind::NamedBranch;

  const auto answer = sendCommand(
      guard, command, EventManager::EventType::HgRepoManagerWorkflow,
      EventManager::EventType::HgRepoManagerWorkflowResult,
      EventManager::EventType::HgRepoManagerWorkflowError);
  if (answer) {
    const auto workflow = answer->value.value<HgRepoWorkflowResult>();
    _flowBranchKind = workflow.flowBranchKind;
    const GitRepo::WorkflowState state = workflow.state;
    Util::Dispatcher::dispatchToMainThread(
        guard, [=]() { _container.applyWorkflowState(state); });
  }

  LOG_PERF(0, "Exit");
}

void HgRepoManagerBridgeImpl::strip(const QWeakPointer<RepoContainer> &guard,
                                    const QString &commit,
                                    const bool keepWorkingCopy,
                                    const bool backup) {
  LOG_PERF(0, "Enter");

  HgRepoStripCommand command;
  command.commit = commit;
  command.keepWorkingCopy = keepWorkingCopy;
  command.backup = backup;

  const bool stripped =
      sendCommand(guard, command, EventManager::EventType::HgRepoManagerStrip,
                  EventManager::EventType::HgRepoManagerStripResult,
                  EventManager::EventType::HgRepoManagerStripError)
          .has_value();

  _container.refreshHistory(guard);
  getLocalBranches();
  getTags();

  const QString backupDir = QDir::toNativeSeparators(
      _container._path + QStringLiteral("/.hg/strip-backup"));
  Util::Dispatcher::dispatchToMainThread(guard, [=]() {
    _container.showSpinnerOverlay(false);
    if (stripped) {
      _container.showToast(
          backup ? _container
                       .tr("The commits were stripped. A backup bundle "
                           "is in %1.")
                       .arg(backupDir)
                 : _container.tr("The commits were stripped."),
          _container.tr("Info"));
    }
  });

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Containers
