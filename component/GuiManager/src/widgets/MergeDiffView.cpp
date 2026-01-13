#include "widgets/MergeDiffView.h"
#include "DarkThemeLexersHighlighter.h"
#include "QScintillaLexerFactory.h"

#include <Log.h>

#include <Qsci/qscilexerdiff.h>

#include <QHBoxLayout>
#include <QScrollBar>

#define THREAD_ID "MergeDiffView"

namespace GuiManager::Widgets {

struct Range {
  int leftStart = -1;
  int leftEnd = -1;
  int rightStart = -1;
  int rightEnd = -1;
};

struct Content {
  QString left;
  QString right;
  QString bottom;
  QVector<int> leftMarks0;
  QVector<int> leftMarks1;
  QVector<int> rightMarks0;
  QVector<int> rightMarks1;
  QVector<Range> ranges;
};

MergeDiffView::MergeDiffView(QWidget *parent)
    : QWidget(parent), _leftEditor(new QsciScintilla(this)),
      _rightEditor(new QsciScintilla(this)),
      _bottomEditor(new QsciScintilla(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  syncScrollbars();

  auto *horizontalLayout = new QHBoxLayout();
  horizontalLayout->addWidget(_leftEditor);
  horizontalLayout->addWidget(_rightEditor);

  auto *verticalLayout = new QVBoxLayout();
  verticalLayout->addLayout(horizontalLayout);
  verticalLayout->addWidget(_bottomEditor);

  setLayout(verticalLayout);

  LOG_PERF(0, "Exit");
}

static Content parseContentWithMarkers(const QString &contentWithMarkers) {
  Content result;

  QStringList lines =
      QString(contentWithMarkers).replace("\r\n", "\n").split('\n');
  int lineIndex = 0;

  enum State { Normal, InOurs, InTheirs } state = Normal;
  Range currentRange;

  for (const QString &line : lines) {
    if (line.startsWith("<<<<<<<")) {
      state = InOurs;
      currentRange = Range();
      currentRange.leftStart = lineIndex; // ours starts after marker
    } else if (line.startsWith("=======")) {
      state = InTheirs;
      currentRange.leftEnd = lineIndex;    // ours ended before separator
      currentRange.rightStart = lineIndex; // theirs starts after separator
    } else if (line.startsWith(">>>>>>>")) {
      state = Normal;
      currentRange.rightEnd = lineIndex; // theirs ended before marker
      result.ranges.append(currentRange);
    } else {
      if (state == Normal) {
        // Non-conflicting line goes to bottom and both panes
        result.bottom += line + "\n";
        result.left += line + "\n";
        result.right += line + "\n";
        lineIndex++;
      } else if (state == InOurs) {
        // Our change: goes to left, empty line in right
        result.left += line + "\n";
        result.right += "\n"; // placeholder
        result.leftMarks0.append(lineIndex);
        result.rightMarks1.append(lineIndex);
        lineIndex++;
      } else if (state == InTheirs) {
        // Their change: goes to right, empty line in left
        result.right += line + "\n";
        result.left += "\n"; // placeholder
        result.rightMarks0.append(lineIndex);
        result.leftMarks1.append(lineIndex);
        lineIndex++;
      }
    }
  }

  return result;
}

void MergeDiffView::loadAndHighlight(const QString &contentWithMarkers,
                                     const QString &extension) {
  LOG_PERF(0, "Enter");

  setupEditors(extension);

  const Content content = parseContentWithMarkers(contentWithMarkers);

  const QStringList leftRaw =
      QString(content.left).replace("\r\n", "\n").split('\n');
  const QStringList rightRaw =
      QString(content.right).replace("\r\n", "\n").split('\n');

  const int leftLineCount = content.left.length() + 1;
  const int rightLineCount = content.right.length() + 1;
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

  _leftEditor->setText(content.left);
  _rightEditor->setText(content.right);
  _bottomEditor->setText(content.bottom);

  for (const int line : content.leftMarks0) {
    _leftEditor->markerAdd(line, 1);
  }

  for (const int line : content.leftMarks1) {
    _leftEditor->markerAdd(line, 2);
  }

  for (const int line : content.rightMarks0) {
    _rightEditor->markerAdd(line, 1);
  }

  for (const int line : content.rightMarks1) {
    _rightEditor->markerAdd(line, 2);
  }

  for (const auto &range : content.ranges) {
    _leftEditor->markerAdd(
        range.leftStart + (range.leftEnd - range.leftStart) / 2, 3);
    _rightEditor->markerAdd(
        range.rightStart + (range.rightEnd - range.rightStart) / 2, 3);
  }

  LOG_PERF(0, "Exit");
}

void MergeDiffView::setupEditors(const QString &extension) {
  LOG_PERF(0, "Enter");

  const QScintillaLexerFactory factory;

  auto *lexer = factory.createLexerForExtension(extension, this);

  _leftEditor->setLexer(lexer);
  _rightEditor->setLexer(lexer);
  _bottomEditor->setLexer(lexer);

  _leftEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _leftEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginType(3, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginWidth(1, 0);
  _leftEditor->setMarginWidth(2, 0);
  _leftEditor->setMarginWidth(3, 16);
  _leftEditor->setMarginMarkerMask(3, 1 << 3);

  _rightEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _rightEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginType(2, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginType(3, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginWidth(1, 0);
  _rightEditor->setMarginWidth(2, 0);
  _rightEditor->setMarginWidth(3, 16);
  _rightEditor->setMarginMarkerMask(3, 1 << 3);

  _leftEditor->markerDefine(QsciScintilla::Background, 1);
  _leftEditor->markerDefine(QsciScintilla::Background, 2);
  _leftEditor->markerDefine(QsciScintilla::SC_MARK_PLUS, 3);

  _rightEditor->markerDefine(QsciScintilla::Background, 1);
  _rightEditor->markerDefine(QsciScintilla::Background, 2);
  _rightEditor->markerDefine(QsciScintilla::SC_MARK_PLUS, 3);

  _leftEditor->setMarkerBackgroundColor(QColor(0x3b1313), 1);
  _leftEditor->setMarkerBackgroundColor(QColor(0x133b13), 2);
  _leftEditor->setMarkerForegroundColor(Qt::white, 3);
  _leftEditor->setMarkerBackgroundColor(Qt::darkGreen, 3);

  _rightEditor->setMarkerBackgroundColor(QColor(0x133b13), 1);
  _rightEditor->setMarkerBackgroundColor(QColor(0x3b1313), 2);
  _rightEditor->setMarkerForegroundColor(Qt::white, 3);
  _rightEditor->setMarkerBackgroundColor(Qt::darkGreen, 3);

  _bottomEditor->setMarginType(0, QsciScintilla::NumberMargin);

  _leftEditor->setWrapMode(QsciScintilla::WrapNone);
  _rightEditor->setWrapMode(QsciScintilla::WrapNone);
  _bottomEditor->setWrapMode(QsciScintilla::WrapNone);

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

  connect(_bottomEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _leftEditor->verticalScrollBar(), &QScrollBar::setValue);
  connect(_bottomEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _rightEditor->verticalScrollBar(), &QScrollBar::setValue);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
