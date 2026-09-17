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

#include <limits>

#define THREAD_ID "MergeDiffView"

#define PLUS_MARKER 3
#define PLUS_MARKER_MARGIN_WIDTH 28
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
  int lineIndexBottom = 0;

  enum State { Normal, InOurs, InTheirs } state = Normal;
  MergeDiffRange currentRangeTop;
  MergeDiffRange currentRangeBottom;

  for (const QString &line : lines) {
    if (line.startsWith("<<<<<<<") && state == Normal) {
      state = InOurs;
      currentRangeTop = MergeDiffRange();
      currentRangeTop.leftStart = lineIndexTop; // ours starts after marker
      currentRangeTop.startMarkerLabel = markerLabel(line);
      currentRangeBottom = MergeDiffRange();
      currentRangeBottom.leftStart =
          lineIndexBottom; // ours starts after marker
      currentRangeBottom.startMarkerLabel = currentRangeTop.startMarkerLabel;
    } else if (line.startsWith("=======") && state == InOurs) {
      state = InTheirs;
      currentRangeTop.leftEnd = lineIndexTop; // ours ended before separator
      currentRangeTop.rightStart =
          lineIndexTop; // theirs starts after separator
      currentRangeBottom.leftEnd =
          lineIndexBottom; // ours ended before separator
      currentRangeBottom.rightStart =
          lineIndexBottom; // theirs starts after separator
    } else if (line.startsWith(">>>>>>>") && state == InTheirs) {
      const QString endMarkerLabel = markerLabel(line);

      if (endMarkerLabel == currentRangeTop.startMarkerLabel ||
          (currentRangeTop.startMarkerLabel == UPDATED_UPSTREAM &&
           endMarkerLabel == STASHED_CHANGES)) {
        state = Normal;
        currentRangeTop.rightEnd = lineIndexTop; // theirs ended before marker
        currentRangeTop.endMarkerLabel = endMarkerLabel;
        currentRangeBottom.rightEnd =
            lineIndexBottom; // theirs ended before marker
        currentRangeBottom.endMarkerLabel = currentRangeTop.endMarkerLabel;
        result.rangesTop.append(currentRangeTop);
        result.rangesBottom.append(currentRangeBottom);
      } else {
        result.right += line + "\n";
        result.left += "\n"; // placeholder
        lineIndexTop++;
      }
    } else {
      if (state == Normal) {
        // Non-conflicting line goes to bottom and both panes
        result.left += line + "\n";
        result.right += line + "\n";
        lineIndexTop++;
      } else if (state == InOurs) {
        // Our change: goes to left, empty line in right
        result.left += line + "\n";
        result.right += "\n"; // placeholder
        result.leftMarks0.append(lineIndexTop);
        result.rightMarks1.append(lineIndexTop);
        lineIndexTop++;
      } else if (state == InTheirs) {
        // Their change: goes to right, empty line in left
        result.right += line + "\n";
        result.left += "\n"; // placeholder
        result.rightMarks0.append(lineIndexTop);
        result.leftMarks1.append(lineIndexTop);
        lineIndexTop++;
      }
    }

    lineIndexBottom++;
  }

  result.bottom = lines.join('\n');

  return result;
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
  const bool lightTheme =
      StylesheetManager::StylesheetManager::currentStylesheet() ==
      StylesheetManager::StylesheetKey::SourcererLight;
  const QColor deletedColor =
      lightTheme ? QColor(255, 231, 231) : QColor(0x3b1313);
  const QColor addedColor =
      lightTheme ? QColor(232, 246, 232) : QColor(0x133b13);
  const QColor plusColor =
      lightTheme ? QColor(200, 230, 251) : QColor(0xc8e6c9);
  const QColor rightPlusColor =
      lightTheme ? QColor(223, 236, 250) : QColor(0xbbdefb);
  const QColor marginColor =
      lightTheme ? QColor(243, 246, 248) : QColor(0x23272e);
  const QColor changedColor =
      lightTheme ? QColor(255, 241, 201) : QColor(Qt::darkYellow);

  _leftEditor->setMarginsBackgroundColor(marginColor);
  _rightEditor->setMarginsBackgroundColor(marginColor);
  _bottomEditor->setMarginsBackgroundColor(marginColor);

  _leftEditor->setMarginBackgroundColor(3, marginColor);
  _rightEditor->setMarginBackgroundColor(3, marginColor);
  _bottomEditor->setMarginBackgroundColor(1, marginColor);
  _bottomEditor->setMarginBackgroundColor(2, marginColor);

  _leftEditor->setMarkerBackgroundColor(deletedColor, 1);
  _leftEditor->setMarkerBackgroundColor(addedColor, 2);
  _leftEditor->setMarkerForegroundColor(Qt::black, PLUS_MARKER);
  _leftEditor->setMarkerBackgroundColor(plusColor, PLUS_MARKER);

  _rightEditor->setMarkerBackgroundColor(addedColor, 1);
  _rightEditor->setMarkerBackgroundColor(deletedColor, 2);
  _rightEditor->setMarkerForegroundColor(Qt::black, PLUS_MARKER);
  _rightEditor->setMarkerBackgroundColor(rightPlusColor, PLUS_MARKER);

  _bottomEditor->setMarkerBackgroundColor(changedColor, 1);
  _bottomEditor->setMarkerBackgroundColor(deletedColor, 2);
  _bottomEditor->setMarkerBackgroundColor(addedColor, 3);
  _bottomEditor->setMarkerBackgroundColor(plusColor, 4);
  _bottomEditor->setMarkerBackgroundColor(rightPlusColor, 5);
  _bottomEditor->setMarkerForegroundColor(Qt::black, 4);
  _bottomEditor->setMarkerForegroundColor(Qt::black, 5);

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
  _leftEditor->clear();
  _rightEditor->clear();
  _bottomEditor->clear();
  _leftEditor->markerDeleteAll();
  _rightEditor->markerDeleteAll();
  _bottomEditor->markerDeleteAll();
  _leftMarks->setMarks({}, {});
  _rightMarks->setMarks({}, {});
  _bottomMarks->setMarks({}, {});

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

  for (const auto &range : _content.rangesTop) {
    _leftEditor->markerAdd(
        range.leftStart + (range.leftEnd - range.leftStart) / 2, PLUS_MARKER);
    _rightEditor->markerAdd(range.rightStart +
                                (range.rightEnd - range.rightStart) / 2,
                            PLUS_MARKER);
  }

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
    for (int line = range.leftStart; line <= range.rightEnd; ++line) {
      conflictLines.append(line);
    }
  }

  _bottomMarks->setMarks(conflictLines, QVector<int>());

  setupEditors(_filePath);

  _leftEditor->setTreeSitterHighlightingEnabled(true);
  _rightEditor->setTreeSitterHighlightingEnabled(true);
  _bottomEditor->setTreeSitterHighlightingEnabled(true);

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

  if (margin != 3) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (!(_leftEditor->markersAtLine(line) & (1 << PLUS_MARKER))) {
    LOG_PERF(0, "Exit");
    return;
  }

  int rangeIndex = -1;

  for (int i = 0; i < _content.rangesTop.length(); ++i) {
    const MergeDiffRange &range = _content.rangesTop[i];
    if (line >= range.leftStart && line <= range.leftEnd) {
      rangeIndex = i;
      break;
    }
  }

  if (rangeIndex == -1) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int removedLinesSoFar = removedLinesBeforeRange(rangeIndex);

  const QStringList contentLeftList = _content.left.split('\n');

  QStringList contentBottomList = _content.bottom.split('\n');

  const MergeDiffRange &rangeTop = _content.rangesTop[rangeIndex];

  MergeDiffRange &rangeBottom = _content.rangesBottom[rangeIndex];

  Q_ASSERT(rangeBottom.leftStart - removedLinesSoFar >= 0);

  if (rangeBottom.leftStart - removedLinesSoFar < 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  contentBottomList.remove(rangeBottom.leftStart - removedLinesSoFar,
                           rangeBottom.rightEnd - rangeBottom.leftStart + 1);

  int newLeftStart = std::numeric_limits<int>::max();
  int newLeftEnd = std::numeric_limits<int>::min();

  for (int i = rangeTop.leftStart; i < rangeTop.leftEnd; ++i) {
    Q_ASSERT(i < contentLeftList.length());
    if (i >= contentLeftList.length()) {
      continue;
    }

    const int insertIndex =
        rangeBottom.leftStart - removedLinesSoFar + (i - rangeTop.leftStart);

    if (insertIndex < newLeftStart) {
      newLeftStart = insertIndex;
    }

    if (insertIndex > newLeftEnd) {
      newLeftEnd = insertIndex;
    }

    if (insertIndex > contentBottomList.length()) {
      contentBottomList.resize(insertIndex);
    }

    contentBottomList.insert(insertIndex, contentLeftList[i]);
  }

  const int horizontalScrollbarPosition =
      _bottomEditor->horizontalScrollBar()->value();
  const int verticalScrollbarPosition =
      _bottomEditor->verticalScrollBar()->value();

  _content.bottom = contentBottomList.join('\n');

  _bottomEditor->setText(_content.bottom);

  _bottomEditor->horizontalScrollBar()->setValue(horizontalScrollbarPosition);
  _bottomEditor->verticalScrollBar()->setValue(verticalScrollbarPosition);

  _leftEditor->markerDelete(
      rangeTop.leftStart + (rangeTop.leftEnd - rangeTop.leftStart) / 2, 3);
  _rightEditor->markerDelete(
      rangeTop.rightStart + (rangeTop.rightEnd - rangeTop.rightStart) / 2, 3);

  updateBottomRangesFromIndex(rangeIndex, MergeDiffResolutionType::Left,
                              newLeftStart, newLeftEnd);

  highlightConflictsInBottomEditor();

  LOG_PERF(0, "Exit");
}

void MergeDiffView::rightMarginClicked(int margin, int line,
                                       Qt::KeyboardModifiers state) {
  LOG_PERF(0, "Enter");

  if (margin != 3) {
    LOG_PERF(0, "Exit");
    return;
  }

  if (!(_rightEditor->markersAtLine(line) & (1 << PLUS_MARKER))) {
    LOG_PERF(0, "Exit");
    return;
  }

  int rangeIndex = -1;

  for (int i = 0; i < _content.rangesTop.length(); ++i) {
    const MergeDiffRange &range = _content.rangesTop[i];
    if (line >= range.rightStart && line <= range.rightEnd) {
      rangeIndex = i;
      break;
    }
  }

  if (rangeIndex == -1) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int removedLinesSoFar = removedLinesBeforeRange(rangeIndex);

  const QStringList contentRightList = _content.right.split('\n');

  QStringList contentBottomList = _content.bottom.split('\n');

  const MergeDiffRange &rangeTop = _content.rangesTop[rangeIndex];

  MergeDiffRange &rangeBottom = _content.rangesBottom[rangeIndex];

  Q_ASSERT(rangeBottom.leftStart - removedLinesSoFar >= 0);

  if (rangeBottom.leftStart - removedLinesSoFar < 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  contentBottomList.remove(rangeBottom.leftStart - removedLinesSoFar,
                           rangeBottom.rightEnd - rangeBottom.leftStart + 1);

  int newRightStart = std::numeric_limits<int>::max();
  int newRightEnd = std::numeric_limits<int>::min();

  for (int i = rangeTop.rightStart; i < rangeTop.rightEnd; ++i) {
    Q_ASSERT(i < contentRightList.length());
    if (i >= contentRightList.length()) {
      continue;
    }

    const int insertIndex =
        rangeBottom.leftStart - removedLinesSoFar + (i - rangeTop.rightStart);

    if (insertIndex < newRightStart) {
      newRightStart = insertIndex;
    }

    if (insertIndex > newRightEnd) {
      newRightEnd = insertIndex;
    }

    if (insertIndex > contentBottomList.length()) {
      contentBottomList.resize(insertIndex);
    }

    contentBottomList.insert(insertIndex, contentRightList[i]);
  }

  const int horizontalScrollbarPosition =
      _bottomEditor->horizontalScrollBar()->value();
  const int verticalScrollbarPosition =
      _bottomEditor->verticalScrollBar()->value();

  _content.bottom = contentBottomList.join('\n');

  _bottomEditor->setText(_content.bottom);

  _bottomEditor->horizontalScrollBar()->setValue(horizontalScrollbarPosition);
  _bottomEditor->verticalScrollBar()->setValue(verticalScrollbarPosition);

  _leftEditor->markerDelete(
      rangeTop.leftStart + (rangeTop.leftEnd - rangeTop.leftStart) / 2, 3);
  _rightEditor->markerDelete(
      rangeTop.rightStart + (rangeTop.rightEnd - rangeTop.rightStart) / 2, 3);

  updateBottomRangesFromIndex(rangeIndex, MergeDiffResolutionType::Right,
                              newRightStart, newRightEnd);

  highlightConflictsInBottomEditor();

  LOG_PERF(0, "Exit");
}

void MergeDiffView::applyConflictChoice(int rangeIndex,
                                        MergeDiffResolutionType resolution) {
  LOG_PERF(0, "Enter");

  if (rangeIndex < 0 || rangeIndex >= _content.rangesBottom.length()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const MergeDiffRange &rangeTop = _content.rangesTop[rangeIndex];
  MergeDiffRange &rangeBottom = _content.rangesBottom[rangeIndex];
  const int removedLinesSoFar = removedLinesBeforeRange(rangeIndex);

  const QStringList contentLeftList = _content.left.split('\n');
  const QStringList contentRightList = _content.right.split('\n');
  QStringList contentBottomList = _content.bottom.split('\n');

  Q_ASSERT(rangeBottom.leftStart - removedLinesSoFar >= 0);
  if (rangeBottom.leftStart - removedLinesSoFar < 0) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int removeIndex = rangeBottom.leftStart - removedLinesSoFar;
  contentBottomList.remove(removeIndex,
                           rangeBottom.rightEnd - rangeBottom.leftStart + 1);

  int newStart = std::numeric_limits<int>::max();
  int newEnd = std::numeric_limits<int>::min();

  const QStringList &sourceList = resolution == MergeDiffResolutionType::Left
                                      ? contentLeftList
                                      : contentRightList;
  const int sourceStart = resolution == MergeDiffResolutionType::Left
                              ? rangeTop.leftStart
                              : rangeTop.rightStart;
  const int sourceEnd = resolution == MergeDiffResolutionType::Left
                            ? rangeTop.leftEnd
                            : rangeTop.rightEnd;

  for (int i = sourceStart; i < sourceEnd; ++i) {
    Q_ASSERT(i < sourceList.length());
    if (i >= sourceList.length()) {
      continue;
    }

    const int insertIndex = removeIndex + (i - sourceStart);
    if (insertIndex < newStart) {
      newStart = insertIndex;
    }
    if (insertIndex > newEnd) {
      newEnd = insertIndex;
    }
    if (insertIndex > contentBottomList.length()) {
      contentBottomList.resize(insertIndex);
    }
    contentBottomList.insert(insertIndex, sourceList[i]);
  }

  const int horizontalScrollbarPosition =
      _bottomEditor->horizontalScrollBar()->value();
  const int verticalScrollbarPosition =
      _bottomEditor->verticalScrollBar()->value();

  _content.bottom = contentBottomList.join('\n');
  _bottomEditor->setText(_content.bottom);
  _bottomEditor->horizontalScrollBar()->setValue(horizontalScrollbarPosition);
  _bottomEditor->verticalScrollBar()->setValue(verticalScrollbarPosition);

  _leftEditor->markerDelete(
      rangeTop.leftStart + (rangeTop.leftEnd - rangeTop.leftStart) / 2, 3);
  _rightEditor->markerDelete(
      rangeTop.rightStart + (rangeTop.rightEnd - rangeTop.rightStart) / 2, 3);

  updateBottomRangesFromIndex(rangeIndex, resolution, newStart, newEnd);
  highlightConflictsInBottomEditor();

  LOG_PERF(0, "Exit");
}

void MergeDiffView::bottomMarginClicked(int margin, int line,
                                        Qt::KeyboardModifiers state) {
  LOG_PERF(0, "Enter");
  Q_UNUSED(state);

  if (margin != 1 && margin != 2) {
    LOG_PERF(0, "Exit");
    return;
  }

  int rangeIndex = -1;
  for (int i = 0; i < _content.rangesBottom.length(); ++i) {
    const MergeDiffRange &rangeBottom = _content.rangesBottom[i];
    const int removedLinesSoFar = removedLinesBeforeRange(i);
    const int startLine = rangeBottom.leftStart - removedLinesSoFar;
    const int endLine = rangeBottom.rightEnd - removedLinesSoFar;

    if (margin == 1 && line == startLine &&
        (_bottomEditor->markersAtLine(line) & (1 << 4))) {
      rangeIndex = i;
      break;
    }

    if (margin == 2 && line == endLine &&
        (_bottomEditor->markersAtLine(line) & (1 << 5))) {
      rangeIndex = i;
      break;
    }
  }

  if (rangeIndex == -1) {
    LOG_PERF(0, "Exit");
    return;
  }

  applyConflictChoice(rangeIndex, margin == 1 ? MergeDiffResolutionType::Left
                                              : MergeDiffResolutionType::Right);

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

  for (int i = 0; i < _content.rangesBottom.length(); ++i) {
    const MergeDiffRange &rangeBottom = _content.rangesBottom[i];

    switch (rangeBottom.resolution) {
    case MergeDiffResolutionType::Unknown: {
      const int removedLinesSoFar = removedLinesBeforeRange(i);
      const int startLine = rangeBottom.leftStart - removedLinesSoFar;
      const int separatorLine = rangeBottom.rightStart - removedLinesSoFar;
      const int endLine = rangeBottom.rightEnd - removedLinesSoFar;

      _bottomEditor->markerAdd(startLine, 1);
      _bottomEditor->markerAdd(separatorLine, 1);
      _bottomEditor->markerAdd(endLine, 1);
      _bottomEditor->markerAdd(startLine, 4);
      _bottomEditor->markerAdd(endLine, 5);

      for (int j = rangeBottom.leftStart + 1; j < rangeBottom.leftEnd; ++j) {
        _bottomEditor->markerAdd(j - removedLinesSoFar, 2);
      }

      for (int j = rangeBottom.rightStart + 1; j < rangeBottom.rightEnd; ++j) {
        _bottomEditor->markerAdd(j - removedLinesSoFar, 3);
      }

      break;
    }
    case MergeDiffResolutionType::Left: {
      for (int j = rangeBottom.leftStart; j <= rangeBottom.leftEnd; ++j) {
        _bottomEditor->markerAdd(j, 2);
      }

      break;
    }
    case MergeDiffResolutionType::Right: {
      for (int j = rangeBottom.rightStart; j <= rangeBottom.rightEnd; ++j) {
        _bottomEditor->markerAdd(j, 3);
      }

      break;
    }
    default: {
      break;
    }
    }
  }

  LOG_PERF(0, "Exit");
}

int MergeDiffView::removedLinesBeforeRange(int rangeIndex) const {
  LOG_PERF(0, "Enter");

  int removedLinesSoFar = 0;

  for (int j = 0; j < _content.rangesBottom.length() && j < rangeIndex; ++j) {
    const MergeDiffRange &rangeBottom = _content.rangesBottom[j];

    if (rangeBottom.resolution == MergeDiffResolutionType::Left ||
        rangeBottom.resolution == MergeDiffResolutionType::Right) {
      removedLinesSoFar += rangeBottom.removedCount;
    }
  }

  LOG_PERF(0, "Exit");

  return removedLinesSoFar;
}

void MergeDiffView::setupEditors(const QString &filePath) {
  LOG_PERF(0, "Enter");

  _leftEditor->setReadOnly(true);
  _rightEditor->setReadOnly(true);
  _bottomEditor->setReadOnly(true);

  /*
  const QScintillaLexerFactory factory;

  auto *lexer = factory.createLexerForFile(filePath, this);

  _leftEditor->setLexer(lexer);
  _rightEditor->setLexer(lexer);
  _bottomEditor->setLexer(lexer);
*/
  _leftEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _leftEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginType(3, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginWidth(1, 0);
  _leftEditor->setMarginWidth(2, 0);
  _leftEditor->setMarginWidth(3, PLUS_MARKER_MARGIN_WIDTH);
  _leftEditor->setMarginBackgroundColor(3, QColor(0x23272e));
  _leftEditor->setMarginMarkerMask(3, 1 << PLUS_MARKER);
  _leftEditor->setMarginSensitivity(3, true);

  _rightEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _rightEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginType(3, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginWidth(1, 0);
  _rightEditor->setMarginWidth(2, 0);
  _rightEditor->setMarginWidth(3, PLUS_MARKER_MARGIN_WIDTH);
  _rightEditor->setMarginBackgroundColor(3, QColor(0x23272e));
  _rightEditor->setMarginMarkerMask(3, 1 << PLUS_MARKER);
  _rightEditor->setMarginSensitivity(3, true);

  _leftEditor->markerDefine(QsciScintilla::Background, 1);
  _leftEditor->markerDefine(QsciScintilla::Background, 2);
  _leftEditor->markerDefine(QsciScintilla::BoxedPlus, PLUS_MARKER);

  _rightEditor->markerDefine(QsciScintilla::Background, 1);
  _rightEditor->markerDefine(QsciScintilla::Background, 2);
  _rightEditor->markerDefine(QsciScintilla::BoxedPlus, PLUS_MARKER);

  refreshScintillaTheme();

  _bottomEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _bottomEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _bottomEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _bottomEditor->setMarginWidth(1, PLUS_MARKER_MARGIN_WIDTH);
  _bottomEditor->setMarginWidth(2, PLUS_MARKER_MARGIN_WIDTH);
  _bottomEditor->setMarginBackgroundColor(1, QColor(0x23272e));
  _bottomEditor->setMarginBackgroundColor(2, QColor(0x23272e));
  _bottomEditor->setMarginMarkerMask(1, 1 << 4);
  _bottomEditor->setMarginMarkerMask(2, 1 << 5);
  _bottomEditor->setMarginSensitivity(1, true);
  _bottomEditor->setMarginSensitivity(2, true);
  _bottomEditor->markerDefine(QsciScintilla::Background, 1);
  _bottomEditor->markerDefine(QsciScintilla::Background, 2);
  _bottomEditor->markerDefine(QsciScintilla::Background, 3);
  _bottomEditor->markerDefine(QsciScintilla::BoxedPlus, 4);
  _bottomEditor->markerDefine(QsciScintilla::BoxedPlus, 5);

  _leftEditor->setWrapMode(QsciScintilla::WrapNone);
  _rightEditor->setWrapMode(QsciScintilla::WrapNone);
  _bottomEditor->setWrapMode(QsciScintilla::WrapNone);

  _leftEditor->setFolding(QsciScintilla::NoFoldStyle);
  _rightEditor->setFolding(QsciScintilla::NoFoldStyle);
  _rightEditor->setFolding(QsciScintilla::NoFoldStyle);

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

void MergeDiffView::updateBottomRangesFromIndex(
    int index, MergeDiffResolutionType resolution, int newStart, int newEnd) {
  LOG_PERF(0, "Enter");
  if (index < 0 || index >= _content.rangesBottom.size()) {
    LOG_PERF(0, "Exit");
    return;
  }

  MergeDiffRange &rangeBottom = _content.rangesBottom[index];

  rangeBottom.resolution = resolution;

  rangeBottom.removedCount =
      rangeBottom.rightEnd - rangeBottom.leftStart - (newEnd - newStart);

  if (resolution == MergeDiffResolutionType::Left) {
    rangeBottom.leftStart = newStart;
    rangeBottom.leftEnd = newEnd;
    rangeBottom.rightStart = -1;
    rangeBottom.rightEnd = -1;
  } else if (resolution == MergeDiffResolutionType::Right) {
    rangeBottom.rightStart = newStart;
    rangeBottom.rightEnd = newEnd;
    rangeBottom.leftStart = -1;
    rangeBottom.leftEnd = -1;
  } else {
    LOG_PERF(0, "Exit");
    return;
  }

  for (int i = index + 1; i < _content.rangesBottom.size(); ++i) {
    MergeDiffRange &nextRange = _content.rangesBottom[i];
    const int removedLinesSoFar = removedLinesBeforeRange(i);

    if (nextRange.resolution == MergeDiffResolutionType::Left) {
      nextRange.leftStart -= removedLinesSoFar;
      nextRange.leftEnd -= removedLinesSoFar;
    } else if (nextRange.resolution == MergeDiffResolutionType::Right) {
      nextRange.rightStart -= removedLinesSoFar;
      nextRange.rightEnd -= removedLinesSoFar;
    }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
