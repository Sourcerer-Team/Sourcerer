#include "widgets/MergeDiffView.h"
#include "DarkThemeLexersHighlighter.h"
#include "QScintillaLexerFactory.h"
#include "widgets/SideBySideDiffView.h"

#include <Log.h>
#include <StylesheetManager.h>

#include <Qsci/qscilexerdiff.h>

#include <QEvent>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QVBoxLayout>

#include <algorithm>

#define THREAD_ID "MergeDiffView"

// Markers in the top editors: take the side into the merge result, or append
// it when the other side was taken already.
#define TAKE_MARKER 3
#define APPEND_MARKER 4
// Markers in the bottom editor, in margin 1 (left) and margin 2 (right).
#define BOTTOM_TAKE_LEFT_MARKER 4
#define BOTTOM_TAKE_RIGHT_MARKER 5
#define BOTTOM_APPEND_LEFT_MARKER 6
#define BOTTOM_APPEND_RIGHT_MARKER 7
#define ACTION_MARKER_MARGIN_WIDTH 28
#define TOP_ACTION_MARKERS ((1 << TAKE_MARKER) | (1 << APPEND_MARKER))
#define BOTTOM_LEFT_ACTION_MARKERS                                             \
  ((1 << BOTTOM_TAKE_LEFT_MARKER) | (1 << BOTTOM_APPEND_LEFT_MARKER))
#define BOTTOM_RIGHT_ACTION_MARKERS                                            \
  ((1 << BOTTOM_TAKE_RIGHT_MARKER) | (1 << BOTTOM_APPEND_RIGHT_MARKER))
#define STASHED_CHANGES "Stashed changes"
#define UPDATED_UPSTREAM "Updated upstream"

namespace GuiManager::Widgets {

MergeDiffView::MergeDiffView(QWidget *parent)
    : QWidget(parent), _leftEditor(new QsciScintilla(this)),
      _rightEditor(new QsciScintilla(this)),
      _bottomEditor(new QsciScintilla(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  auto *horizontalLayout = new QHBoxLayout();
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->addWidget(_leftEditor);
  horizontalLayout->addWidget(_rightEditor);

  auto *verticalLayout = new QVBoxLayout();
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  verticalLayout->addLayout(horizontalLayout);
  verticalLayout->addWidget(_bottomEditor);

  setLayout(verticalLayout);

  connect(_leftEditor, &QsciScintilla::marginClicked, this,
          &MergeDiffView::leftMarginClicked);
  connect(_rightEditor, &QsciScintilla::marginClicked, this,
          &MergeDiffView::rightMarginClicked);
  connect(_bottomEditor, &QsciScintilla::marginClicked, this,
          &MergeDiffView::bottomMarginClicked);

  syncScrollbars();

  // Create scrollbar marks widgets
  _leftMarks = new ScrollbarMarksWidget(_leftEditor->verticalScrollBar(),
                                        _leftEditor, this);
  _rightMarks = new ScrollbarMarksWidget(_rightEditor->verticalScrollBar(),
                                         _rightEditor, this);
  _bottomMarks = new ScrollbarMarksWidget(_bottomEditor->verticalScrollBar(),
                                          _bottomEditor, this);

  // Initially hide marks, they will show when scrollbars become visible
  _leftMarks->hide();
  _rightMarks->hide();
  _bottomMarks->hide();

  LOG_PERF(0, "Exit");
}

static QString markerLabel(const QString &line) {
  return line.mid(7).trimmed();
}

static MergeDiffContent
parseContentWithMarkers(const QString &contentWithMarkers) {
  MergeDiffContent result;

  result.windowsFile = contentWithMarkers.split("\r\n").length() ==
                       contentWithMarkers.split("\n").length();

  QStringList lines =
      QString(contentWithMarkers).replace("\r\n", "\n").split('\n');
  int lineIndexTop = 0;

  enum State { Normal, InOurs, InBase, InTheirs } state = Normal;
  MergeDiffRange currentRangeTop;
  MergeDiffConflict currentConflict;
  QStringList currentCommon;
  QStringList pendingConflictLines;

  for (const QString &line : lines) {
    if (state != Normal) {
      pendingConflictLines.append(line);
    }

    if (line.startsWith("<<<<<<<") && state == Normal) {
      state = InOurs;
      currentRangeTop = MergeDiffRange();
      currentRangeTop.leftStart = lineIndexTop; // ours starts after marker
      currentRangeTop.startMarkerLabel = markerLabel(line);
      currentConflict = MergeDiffConflict();
      currentConflict.startMarkerLine = line;
      pendingConflictLines = {line};
    } else if (line.startsWith("|||||||") && state == InOurs) {
      // Subversion diff3 style: base section, not shown in either pane.
      state = InBase;
      currentRangeTop.leftEnd = lineIndexTop;
    } else if (line.startsWith("=======") &&
               (state == InOurs || state == InBase)) {
      state = InTheirs;
      currentRangeTop.leftEnd = lineIndexTop; // ours ended before separator
      currentRangeTop.rightStart =
          lineIndexTop; // theirs starts after separator
      currentConflict.separatorLine = line;
    } else if (line.startsWith(">>>>>>>") && state == InTheirs) {
      const QString endMarkerLabel = markerLabel(line);

      // Subversion labels start markers ".working"/".mine" and end markers
      // ".merge-right.rN" or ".rN", so any end label is accepted.
      const bool subversionMarkers =
          currentRangeTop.startMarkerLabel.startsWith(".");

      if (endMarkerLabel == currentRangeTop.startMarkerLabel ||
          subversionMarkers ||
          (currentRangeTop.startMarkerLabel == UPDATED_UPSTREAM &&
           endMarkerLabel == STASHED_CHANGES)) {
        state = Normal;
        currentRangeTop.rightEnd = lineIndexTop; // theirs ended before marker
        currentRangeTop.endMarkerLabel = endMarkerLabel;
        currentConflict.endMarkerLine = line;
        result.rangesTop.append(currentRangeTop);
        result.rangesBottom.append(MergeDiffRange());
        result.rangesBottom.last().startMarkerLabel =
            currentRangeTop.startMarkerLabel;
        result.rangesBottom.last().endMarkerLabel = endMarkerLabel;
        result.conflicts.append(currentConflict);
        result.commonChunks.append(currentCommon);
        currentCommon.clear();
        pendingConflictLines.clear();
      } else {
        result.right += line + "\n";
        result.left += "\n"; // placeholder
        currentConflict.rightLines.append(line);
        lineIndexTop++;
      }
    } else {
      if (state == Normal) {
        // Non-conflicting line goes to bottom and both panes
        result.left += line + "\n";
        result.right += line + "\n";
        currentCommon.append(line);
        lineIndexTop++;
      } else if (state == InOurs) {
        // Our change: goes to left, empty line in right
        result.left += line + "\n";
        result.right += "\n"; // placeholder
        result.leftMarks0.append(lineIndexTop);
        result.rightMarks1.append(lineIndexTop);
        currentConflict.leftLines.append(line);
        lineIndexTop++;
      } else if (state == InTheirs) {
        // Their change: goes to right, empty line in left
        result.right += line + "\n";
        result.left += "\n"; // placeholder
        result.rightMarks0.append(lineIndexTop);
        result.leftMarks1.append(lineIndexTop);
        currentConflict.rightLines.append(line);
        lineIndexTop++;
      }
    }
  }

  // An unterminated conflict is kept verbatim in the merged content.
  currentCommon.append(pendingConflictLines);
  result.commonChunks.append(currentCommon);

  return result;
}

static bool includesLeft(MergeDiffResolutionType resolution) {
  return resolution == MergeDiffResolutionType::Left ||
         resolution == MergeDiffResolutionType::LeftThenRight ||
         resolution == MergeDiffResolutionType::RightThenLeft;
}

static bool includesRight(MergeDiffResolutionType resolution) {
  return resolution == MergeDiffResolutionType::Right ||
         resolution == MergeDiffResolutionType::LeftThenRight ||
         resolution == MergeDiffResolutionType::RightThenLeft;
}

// Choosing the other side of an already resolved conflict appends it, so
// both changes end up in the merged content in the order they were chosen.
static MergeDiffResolutionType
combineResolution(MergeDiffResolutionType current,
                  MergeDiffResolutionType side) {
  switch (current) {
  case MergeDiffResolutionType::Unknown:
    return side;
  case MergeDiffResolutionType::Left:
    return side == MergeDiffResolutionType::Right
               ? MergeDiffResolutionType::LeftThenRight
               : current;
  case MergeDiffResolutionType::Right:
    return side == MergeDiffResolutionType::Left
               ? MergeDiffResolutionType::RightThenLeft
               : current;
  default:
    return current;
  }
}

// Rebuilds the merged content from the common chunks and the conflicts
// according to their resolutions, and updates the bottom ranges.
static void buildBottomContent(MergeDiffContent &content) {
  QStringList lines;

  const auto appendLines = [&lines](const QStringList &source, int &start,
                                    int &end) {
    if (source.isEmpty()) {
      start = -1;
      end = -1;
      return;
    }

    start = lines.length();
    lines.append(source);
    end = lines.length() - 1;
  };

  for (int i = 0; i < content.conflicts.length(); ++i) {
    lines.append(content.commonChunks[i]);

    const MergeDiffConflict &conflict = content.conflicts[i];
    MergeDiffRange &range = content.rangesBottom[i];
    const int blockStart = lines.length();

    switch (range.resolution) {
    case MergeDiffResolutionType::Unknown:
      range.leftStart = lines.length();
      lines.append(conflict.startMarkerLine);
      lines.append(conflict.leftLines);
      range.leftEnd = lines.length();
      range.rightStart = lines.length();
      lines.append(conflict.separatorLine);
      lines.append(conflict.rightLines);
      range.rightEnd = lines.length();
      lines.append(conflict.endMarkerLine);
      break;
    case MergeDiffResolutionType::Left:
      appendLines(conflict.leftLines, range.leftStart, range.leftEnd);
      range.rightStart = range.rightEnd = -1;
      break;
    case MergeDiffResolutionType::Right:
      range.leftStart = range.leftEnd = -1;
      appendLines(conflict.rightLines, range.rightStart, range.rightEnd);
      break;
    case MergeDiffResolutionType::LeftThenRight:
      appendLines(conflict.leftLines, range.leftStart, range.leftEnd);
      appendLines(conflict.rightLines, range.rightStart, range.rightEnd);
      break;
    case MergeDiffResolutionType::RightThenLeft:
      appendLines(conflict.rightLines, range.rightStart, range.rightEnd);
      appendLines(conflict.leftLines, range.leftStart, range.leftEnd);
      break;
    default:
      break;
    }

    const bool blockEmpty = lines.length() == blockStart;
    range.blockStart = blockStart;
    range.blockEnd = blockEmpty ? -1 : lines.length() - 1;
  }

  lines.append(content.commonChunks.last());

  content.bottom = lines.join('\n');
}

void MergeDiffView::changeEvent(QEvent *event) {
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

void MergeDiffView::refreshScintillaTheme() {
  const auto theme = StylesheetManager::StylesheetManager::currentStylesheet();
  const bool lightTheme =
      theme == StylesheetManager::StylesheetKey::SourcererLight;
  const bool purpleTheme =
      theme == StylesheetManager::StylesheetKey::SourcererPurple;
  const bool blueTheme =
      theme == StylesheetManager::StylesheetKey::SourcererBlue;
  const bool monokaiTheme =
      theme == StylesheetManager::StylesheetKey::SourcererMonokai;
  const bool gruvboxTheme =
      theme == StylesheetManager::StylesheetKey::SourcererGruvbox;
  const bool solarizedTheme =
      theme == StylesheetManager::StylesheetKey::SourcererSolarized;
  const bool draculaTheme =
      theme == StylesheetManager::StylesheetKey::SourcererDracula;
  const bool ayuTheme = theme == StylesheetManager::StylesheetKey::SourcererAyu;
  const QColor deletedColor =
      lightTheme ? QColor(255, 231, 231) : QColor(0x4a2327);
  const QColor addedColor =
      lightTheme ? QColor(232, 246, 232) : QColor(0x1f3d2b);
  const QColor actionColor = lightTheme ? QColor(0x0969da) : QColor(0x2f81f7);
  const QColor actionGlyphColor = Qt::white;
  const QColor marginColor = lightTheme       ? QColor(243, 246, 248)
                             : purpleTheme    ? QColor(0x2b2735)
                             : blueTheme      ? QColor(0x1a2435)
                             : monokaiTheme   ? QColor(0x2f3029)
                             : gruvboxTheme   ? QColor(0x1d2021)
                             : solarizedTheme ? QColor(0x00232c)
                             : draculaTheme   ? QColor(0x21222c)
                             : ayuTheme       ? QColor(0x0d1016)
                                              : QColor(0x23272e);
  const QColor changedColor =
      lightTheme ? QColor(255, 241, 201) : QColor(0x4d4021);

  _leftEditor->setMarginsBackgroundColor(marginColor);
  _rightEditor->setMarginsBackgroundColor(marginColor);
  _bottomEditor->setMarginsBackgroundColor(marginColor);

  _leftEditor->setMarginBackgroundColor(3, marginColor);
  _rightEditor->setMarginBackgroundColor(3, marginColor);
  _bottomEditor->setMarginBackgroundColor(1, marginColor);
  _bottomEditor->setMarginBackgroundColor(2, marginColor);

  _leftEditor->setMarkerBackgroundColor(deletedColor, 1);
  _leftEditor->setMarkerBackgroundColor(addedColor, 2);

  _rightEditor->setMarkerBackgroundColor(addedColor, 1);
  _rightEditor->setMarkerBackgroundColor(deletedColor, 2);

  _bottomEditor->setMarkerBackgroundColor(changedColor, 1);
  _bottomEditor->setMarkerBackgroundColor(deletedColor, 2);
  _bottomEditor->setMarkerBackgroundColor(addedColor, 3);

  for (auto *editor : {_leftEditor, _rightEditor}) {
    for (const int marker : {TAKE_MARKER, APPEND_MARKER}) {
      editor->setMarkerBackgroundColor(actionColor, marker);
      editor->setMarkerForegroundColor(actionGlyphColor, marker);
    }
  }

  for (const int marker :
       {BOTTOM_TAKE_LEFT_MARKER, BOTTOM_TAKE_RIGHT_MARKER,
        BOTTOM_APPEND_LEFT_MARKER, BOTTOM_APPEND_RIGHT_MARKER}) {
    _bottomEditor->setMarkerBackgroundColor(actionColor, marker);
    _bottomEditor->setMarkerForegroundColor(actionGlyphColor, marker);
  }

  DarkThemeLexersHighlighter highlighter;
  const auto editors = findChildren<QsciScintilla *>();

  for (auto *editor : editors) {
    highlighter.applyDarkTheme(editor);
  }
}

void MergeDiffView::clearContent() {
  LOG_PERF(0, "Enter");

  _leftEditor->setLexer(nullptr);
  _rightEditor->setLexer(nullptr);
  _bottomEditor->setLexer(nullptr);
  delete _lexer;
  _lexer = nullptr;
  _leftEditor->clear();
  _rightEditor->clear();
  _bottomEditor->clear();
  _leftEditor->markerDeleteAll();
  _rightEditor->markerDeleteAll();
  _bottomEditor->markerDeleteAll();
  _leftMarks->setMarks({}, {});
  _rightMarks->setMarks({}, {});
  _bottomMarks->setMarks({}, {});

  _currentConflictIndex = -1;
  emit currentConflictChanged(0, 0);

  LOG_PERF(0, "Exit");
}

void MergeDiffView::loadAndHighlight(const QString &contentWithMarkers,
                                     const QString &filePath) {
  LOG_PERF(0, "Enter");

  clearContent();

  _leftEditor->setTreeSitterHighlightingEnabled(false);
  _rightEditor->setTreeSitterHighlightingEnabled(false);
  _bottomEditor->setTreeSitterHighlightingEnabled(false);

  _filePath = filePath;

  _content = parseContentWithMarkers(contentWithMarkers);
  buildBottomContent(_content);

  const QStringList leftRaw =
      QString(_content.left).replace("\r\n", "\n").split('\n');
  const QStringList rightRaw =
      QString(_content.right).replace("\r\n", "\n").split('\n');

  const int leftLineCount = _content.left.length() + 1;
  const int rightLineCount = _content.right.length() + 1;
  const int leftWidth = QString::number(leftLineCount).length();
  const int rightWidth = QString::number(rightLineCount).length();
  const QString leftMask =
      QString::number(0).rightJustified(leftWidth + 1, '0');
  const QString rightMask =
      QString::number(0).rightJustified(rightWidth + 1, '0');
  const QString bottomMask =
      QString::number(0).rightJustified(leftWidth + rightWidth + 1, '0');

  _leftEditor->setMarginWidth(0, leftMask);
  _rightEditor->setMarginWidth(0, rightMask);
  _bottomEditor->setMarginWidth(0, bottomMask);

  _leftEditor->setText(_content.left);
  _rightEditor->setText(_content.right);
  _bottomEditor->setText(_content.bottom);

  // Reapply the theme after setting text so the editor foreground colors
  // are refreshed with the loaded document contents.
  // refreshScintillaTheme();

  for (const int line : _content.leftMarks0) {
    _leftEditor->markerAdd(line, 1);
  }

  for (const int line : _content.leftMarks1) {
    _leftEditor->markerAdd(line, 2);
  }

  for (const int line : _content.rightMarks0) {
    _rightEditor->markerAdd(line, 1);
  }

  for (const int line : _content.rightMarks1) {
    _rightEditor->markerAdd(line, 2);
  }

  updateTopPlusMarkers();
  highlightConflictsInBottomEditor();

  // Update scrollbar marks
  // leftMarks0 = deleted lines (red) on left editor
  // leftMarks1 = added lines (green) on left editor
  // rightMarks0 = added lines (green) on right editor
  // rightMarks1 = deleted lines (red) on right editor
  _leftMarks->setMarks(_content.leftMarks0, _content.leftMarks1);
  _rightMarks->setMarks(_content.rightMarks1, _content.rightMarks0);

  // For bottom editor, show all conflict ranges as markers
  QVector<int> conflictLines;
  for (const auto &range : _content.rangesBottom) {
    for (int line = range.blockStart; line <= range.blockEnd; ++line) {
      conflictLines.append(line);
    }
  }

  _bottomMarks->setMarks(conflictLines, QVector<int>());

  setupEditors(_filePath);

  _leftEditor->setTreeSitterHighlightingEnabled(true);
  _rightEditor->setTreeSitterHighlightingEnabled(true);
  _bottomEditor->setTreeSitterHighlightingEnabled(true);

  _leftEditor->verticalScrollBar()->triggerAction(
      QAbstractSlider::SliderToMinimum);

  // Start at the first conflict, so it is the current one.
  scrollToConflict(0);

  LOG_PERF(0, "Exit");
}

bool MergeDiffView::areConflictsResolved() const {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return conflictsBeforeIndex(_content.rangesTop.length() - 1) == 0;
}

void MergeDiffView::leftMarginClicked(int margin, int line,
                                      Qt::KeyboardModifiers state) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(state);

  if (margin != 3) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (!(_leftEditor->markersAtLine(line) & TOP_ACTION_MARKERS)) {
    LOG_PERF(0, "Exit");
    return;
  }

  for (int i = 0; i < _content.rangesTop.length(); ++i) {
    const MergeDiffRange &range = _content.rangesTop[i];
    if (line >= range.leftStart && line <= range.leftEnd) {
      applyConflictChoice(i, MergeDiffResolutionType::Left);
      break;
    }
  }

  LOG_PERF(0, "Exit");
}

void MergeDiffView::rightMarginClicked(int margin, int line,
                                       Qt::KeyboardModifiers state) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(state);

  if (margin != 3) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (!(_rightEditor->markersAtLine(line) & TOP_ACTION_MARKERS)) {
    LOG_PERF(0, "Exit");
    return;
  }

  for (int i = 0; i < _content.rangesTop.length(); ++i) {
    const MergeDiffRange &range = _content.rangesTop[i];
    if (line >= range.rightStart && line <= range.rightEnd) {
      applyConflictChoice(i, MergeDiffResolutionType::Right);
      break;
    }
  }

  LOG_PERF(0, "Exit");
}

void MergeDiffView::applyConflictChoice(int rangeIndex,
                                        MergeDiffResolutionType side) {
  LOG_PERF(0, "Enter");

  if (rangeIndex < 0 || rangeIndex >= _content.rangesBottom.length()) {
    LOG_PERF(0, "Exit");
    return;
  }

  MergeDiffRange &rangeBottom = _content.rangesBottom[rangeIndex];
  const MergeDiffResolutionType resolution =
      combineResolution(rangeBottom.resolution, side);

  if (resolution == rangeBottom.resolution) {
    LOG_PERF(0, "Exit");
    return;
  }

  rangeBottom.resolution = resolution;
  setCurrentConflictIndex(rangeIndex);
  buildBottomContent(_content);

  const int horizontalScrollbarPosition =
      _bottomEditor->horizontalScrollBar()->value();
  const int verticalScrollbarPosition =
      _bottomEditor->verticalScrollBar()->value();

  _bottomEditor->setText(_content.bottom);
  _bottomEditor->horizontalScrollBar()->setValue(horizontalScrollbarPosition);
  _bottomEditor->verticalScrollBar()->setValue(verticalScrollbarPosition);

  updateTopPlusMarkers();
  highlightConflictsInBottomEditor();

  LOG_PERF(0, "Exit");
}

void MergeDiffView::goToNextConflict() {
  LOG_PERF(0, "Enter");

  if (_content.rangesTop.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  scrollToConflict((std::min)(_currentConflictIndex + 1,
                              int(_content.rangesTop.length()) - 1));

  LOG_PERF(0, "Exit");
}

void MergeDiffView::goToPreviousConflict() {
  LOG_PERF(0, "Enter");

  if (_content.rangesTop.isEmpty()) {
    LOG_PERF(0, "Exit");
    return;
  }

  scrollToConflict((std::max)(_currentConflictIndex - 1, 0));

  LOG_PERF(0, "Exit");
}

void MergeDiffView::scrollToConflict(int rangeIndex) {
  LOG_PERF(0, "Enter");

  if (rangeIndex < 0 || rangeIndex >= _content.rangesTop.length()) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Lines of context shown above the conflict.
  constexpr int contextLines = 3;

  setCurrentConflictIndex(rangeIndex);

  const int topLine =
      (std::max)(0, _content.rangesTop[rangeIndex].leftStart - contextLines);
  const int bottomLine = (std::max)(
      0, _content.rangesBottom[rangeIndex].blockStart - contextLines);

  // The top editors are kept in sync and drag the bottom editor along, so
  // the bottom editor, whose lines differ, is positioned last.
  _leftEditor->verticalScrollBar()->setValue(topLine);
  _rightEditor->verticalScrollBar()->setValue(topLine);
  _bottomEditor->verticalScrollBar()->setValue(bottomLine);

  LOG_PERF(0, "Exit");
}

void MergeDiffView::setCurrentConflictIndex(int rangeIndex) {
  _currentConflictIndex = rangeIndex;
  emit currentConflictChanged(rangeIndex + 1, int(_content.rangesTop.length()));
}

void MergeDiffView::bottomMarginClicked(int margin, int line,
                                        Qt::KeyboardModifiers state) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(state);

  if (margin != 1 && margin != 2) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int markers =
      margin == 1 ? BOTTOM_LEFT_ACTION_MARKERS : BOTTOM_RIGHT_ACTION_MARKERS;

  if (!(_bottomEditor->markersAtLine(line) & markers)) {
    LOG_PERF(0, "Exit");
    return;
  }

  for (int i = 0; i < _content.rangesBottom.length(); ++i) {
    const MergeDiffRange &rangeBottom = _content.rangesBottom[i];

    if (line >= rangeBottom.blockStart && line <= rangeBottom.blockEnd) {
      applyConflictChoice(i, margin == 1 ? MergeDiffResolutionType::Left
                                         : MergeDiffResolutionType::Right);
      break;
    }
  }

  LOG_PERF(0, "Exit");
}

int MergeDiffView::conflictsBeforeIndex(int index) const {
  LOG_PERF(0, "Enter");

  int unresolvedCount = 0;

  for (int i = 0; i < _content.rangesBottom.length() && i <= index; ++i) {
    const MergeDiffRange &rangeBottom = _content.rangesBottom[i];
    if (rangeBottom.resolution == MergeDiffResolutionType::Unknown) {
      ++unresolvedCount;
    }
  }

  LOG_PERF(0, "Exit");

  return unresolvedCount;
}

void MergeDiffView::highlightConflictsInBottomEditor() {
  LOG_PERF(0, "Enter");

  _bottomEditor->markerDeleteAll();

  for (const MergeDiffRange &rangeBottom : _content.rangesBottom) {
    if (rangeBottom.resolution == MergeDiffResolutionType::Unknown) {
      _bottomEditor->markerAdd(rangeBottom.leftStart, 1);
      _bottomEditor->markerAdd(rangeBottom.rightStart, 1);
      _bottomEditor->markerAdd(rangeBottom.rightEnd, 1);
      _bottomEditor->markerAdd(rangeBottom.leftStart, BOTTOM_TAKE_LEFT_MARKER);
      _bottomEditor->markerAdd(rangeBottom.rightEnd, BOTTOM_TAKE_RIGHT_MARKER);

      for (int j = rangeBottom.leftStart + 1; j < rangeBottom.leftEnd; ++j) {
        _bottomEditor->markerAdd(j, 2);
      }

      for (int j = rangeBottom.rightStart + 1; j < rangeBottom.rightEnd; ++j) {
        _bottomEditor->markerAdd(j, 3);
      }

      continue;
    }

    if (rangeBottom.leftStart >= 0) {
      for (int j = rangeBottom.leftStart; j <= rangeBottom.leftEnd; ++j) {
        _bottomEditor->markerAdd(j, 2);
      }
    }

    if (rangeBottom.rightStart >= 0) {
      for (int j = rangeBottom.rightStart; j <= rangeBottom.rightEnd; ++j) {
        _bottomEditor->markerAdd(j, 3);
      }
    }

    // Offer appending the side that was not chosen yet.
    if (rangeBottom.blockEnd >= 0) {
      if (!includesLeft(rangeBottom.resolution)) {
        _bottomEditor->markerAdd(rangeBottom.blockEnd,
                                 BOTTOM_APPEND_LEFT_MARKER);
      }

      if (!includesRight(rangeBottom.resolution)) {
        _bottomEditor->markerAdd(rangeBottom.blockEnd,
                                 BOTTOM_APPEND_RIGHT_MARKER);
      }
    }
  }

  LOG_PERF(0, "Exit");
}

void MergeDiffView::updateTopPlusMarkers() {
  LOG_PERF(0, "Enter");

  for (int i = 0; i < _content.rangesTop.length(); ++i) {
    const MergeDiffRange &rangeTop = _content.rangesTop[i];
    const MergeDiffResolutionType resolution =
        _content.rangesBottom[i].resolution;
    const int leftLine =
        rangeTop.leftStart + (rangeTop.leftEnd - rangeTop.leftStart) / 2;
    const int rightLine =
        rangeTop.rightStart + (rangeTop.rightEnd - rangeTop.rightStart) / 2;

    const bool resolved = resolution != MergeDiffResolutionType::Unknown;
    const int actionMarker = resolved ? APPEND_MARKER : TAKE_MARKER;

    for (const int marker : {TAKE_MARKER, APPEND_MARKER}) {
      _leftEditor->markerDelete(leftLine, marker);
      _rightEditor->markerDelete(rightLine, marker);
    }

    if (!includesLeft(resolution)) {
      _leftEditor->markerAdd(leftLine, actionMarker);
    }

    if (!includesRight(resolution)) {
      _rightEditor->markerAdd(rightLine, actionMarker);
    }
  }

  LOG_PERF(0, "Exit");
}

void MergeDiffView::setupEditors(const QString &filePath) {
  LOG_PERF(0, "Enter");

  _leftEditor->setReadOnly(true);
  _rightEditor->setReadOnly(true);
  _bottomEditor->setReadOnly(true);

  // The lexer has to be attached before theming so its styles get the
  // current theme colors.
  _lexer = QScintillaLexerFactory::createDiffViewerLexerForFile(filePath, this);
  _leftEditor->setLexer(_lexer);
  _rightEditor->setLexer(_lexer);
  _bottomEditor->setLexer(_lexer);

  _leftEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _leftEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginType(3, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginWidth(1, 0);
  _leftEditor->setMarginWidth(2, 0);
  _leftEditor->setMarginWidth(3, ACTION_MARKER_MARGIN_WIDTH);
  _leftEditor->setMarginBackgroundColor(3, QColor(0x23272e));
  _leftEditor->setMarginMarkerMask(3, TOP_ACTION_MARKERS);
  _leftEditor->setMarginSensitivity(3, true);

  _rightEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _rightEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginType(3, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginWidth(1, 0);
  _rightEditor->setMarginWidth(2, 0);
  _rightEditor->setMarginWidth(3, ACTION_MARKER_MARGIN_WIDTH);
  _rightEditor->setMarginBackgroundColor(3, QColor(0x23272e));
  _rightEditor->setMarginMarkerMask(3, TOP_ACTION_MARKERS);
  _rightEditor->setMarginSensitivity(3, true);

  _leftEditor->markerDefine(QsciScintilla::Background, 1);
  _leftEditor->markerDefine(QsciScintilla::Background, 2);
  _leftEditor->markerDefine(QsciScintilla::BoxedArrowDown, TAKE_MARKER);
  _leftEditor->markerDefine(QsciScintilla::BoxedPlus, APPEND_MARKER);
  _leftEditor->setMarkerToolTip(tr("Use our change (left) in the merge result"),
                                TAKE_MARKER);
  _leftEditor->setMarkerToolTip(
      tr("Also add our change (left) to the merge result, after theirs"),
      APPEND_MARKER);

  _rightEditor->markerDefine(QsciScintilla::Background, 1);
  _rightEditor->markerDefine(QsciScintilla::Background, 2);
  _rightEditor->markerDefine(QsciScintilla::BoxedArrowDown, TAKE_MARKER);
  _rightEditor->markerDefine(QsciScintilla::BoxedPlus, APPEND_MARKER);
  _rightEditor->setMarkerToolTip(
      tr("Use their change (right) in the merge result"), TAKE_MARKER);
  _rightEditor->setMarkerToolTip(
      tr("Also add their change (right) to the merge result, after ours"),
      APPEND_MARKER);

  _bottomEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _bottomEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _bottomEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _bottomEditor->setMarginWidth(1, ACTION_MARKER_MARGIN_WIDTH);
  _bottomEditor->setMarginWidth(2, ACTION_MARKER_MARGIN_WIDTH);
  //_bottomEditor->setMarginBackgroundColor(1, QColor(0x23272e));
  //_bottomEditor->setMarginBackgroundColor(2, QColor(0x23272e));
  _bottomEditor->setMarginMarkerMask(1, BOTTOM_LEFT_ACTION_MARKERS);
  _bottomEditor->setMarginMarkerMask(2, BOTTOM_RIGHT_ACTION_MARKERS);
  _bottomEditor->setMarginSensitivity(1, true);
  _bottomEditor->setMarginSensitivity(2, true);
  _bottomEditor->markerDefine(QsciScintilla::Background, 1);
  _bottomEditor->markerDefine(QsciScintilla::Background, 2);
  _bottomEditor->markerDefine(QsciScintilla::Background, 3);
  _bottomEditor->markerDefine(QsciScintilla::BoxedCheck,
                              BOTTOM_TAKE_LEFT_MARKER);
  _bottomEditor->markerDefine(QsciScintilla::BoxedCheck,
                              BOTTOM_TAKE_RIGHT_MARKER);
  _bottomEditor->markerDefine(QsciScintilla::BoxedPlus,
                              BOTTOM_APPEND_LEFT_MARKER);
  _bottomEditor->markerDefine(QsciScintilla::BoxedPlus,
                              BOTTOM_APPEND_RIGHT_MARKER);
  _bottomEditor->setMarkerToolTip(tr("Resolve with our change (left)"),
                                  BOTTOM_TAKE_LEFT_MARKER);
  _bottomEditor->setMarkerToolTip(tr("Resolve with their change (right)"),
                                  BOTTOM_TAKE_RIGHT_MARKER);
  _bottomEditor->setMarkerToolTip(tr("Also add our change (left) below"),
                                  BOTTOM_APPEND_LEFT_MARKER);
  _bottomEditor->setMarkerToolTip(tr("Also add their change (right) below"),
                                  BOTTOM_APPEND_RIGHT_MARKER);

  _leftEditor->setWrapMode(QsciScintilla::WrapNone);
  _rightEditor->setWrapMode(QsciScintilla::WrapNone);
  _bottomEditor->setWrapMode(QsciScintilla::WrapNone);

  _leftEditor->setFolding(QsciScintilla::NoFoldStyle);
  _rightEditor->setFolding(QsciScintilla::NoFoldStyle);
  _rightEditor->setFolding(QsciScintilla::NoFoldStyle);

  refreshScintillaTheme();

  DarkThemeLexersHighlighter highligheter;
  highligheter.applyDarkTheme(_leftEditor);
  highligheter.applyDarkTheme(_rightEditor);
  highligheter.applyDarkTheme(_bottomEditor);

  LOG_PERF(0, "Exit");
}

void MergeDiffView::syncScrollbars() {
  LOG_PERF(0, "Enter");

  connect(_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _rightEditor->verticalScrollBar(), &QScrollBar::setValue);
  connect(_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _bottomEditor->verticalScrollBar(), &QScrollBar::setValue);

  connect(_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _leftEditor->verticalScrollBar(), &QScrollBar::setValue);
  connect(_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _bottomEditor->verticalScrollBar(), &QScrollBar::setValue);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
