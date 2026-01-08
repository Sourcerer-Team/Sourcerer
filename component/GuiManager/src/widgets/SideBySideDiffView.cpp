#include "widgets/SideBySideDiffView.h"
#include "DarkThemeLexersHighlighter.h"
#include "QScintillaLexerFactory.h"

#include <Log.h>

#include <Qsci/qscilexerdiff.h>

#include <QHBoxLayout>
#include <QScrollBar>

#define THREAD_ID "SideBySideDiffView"

namespace GuiManager::Widgets {

struct Content {
  QString left;
  QString right;
  QVector<int> leftMarks0;
  QVector<int> leftMarks1;
  QVector<int> rightMarks0;
  QVector<int> rightMarks1;
};

SideBySideDiffView::SideBySideDiffView(QWidget *parent)
    : QWidget(parent), _leftEditor(new QsciScintilla(this)),
      _rightEditor(new QsciScintilla(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  syncScrollbars();

  auto *layout = new QHBoxLayout(this);
  layout->addWidget(_leftEditor);
  layout->addWidget(_rightEditor);
  setLayout(layout);

  LOG_PERF(0, "Exit");
}

Content createContent(const QString &leftContent,
                      const GitRepo::FilesDiffDelta &rightDelta) {
  LOG_PERF(0, "Enter");

  QStringList leftLines =
      QString(leftContent).replace("\r\n", "\n").split('\n');

  QStringList alignedLeft;
  QStringList alignedRight;

  QVector<int> changedLeft0;
  QVector<int> changedLeft1;
  QVector<int> changedRight0;
  QVector<int> changedRight1;

  int currentLeftIndex = 0; // track position in original file

  for (const auto &hunk : rightDelta.hunks) {
    int hunkStart = hunk.oldStart - 1;

    // --- Copy unchanged lines before hunk ---
    while (currentLeftIndex < hunkStart &&
           currentLeftIndex < leftLines.size()) {
      alignedLeft.append(leftLines[currentLeftIndex]);
      alignedRight.append(leftLines[currentLeftIndex]);
      currentLeftIndex++;
    }

    // --- Apply hunk ---
    int leftIndex = hunk.oldStart - 1;
    int rightIndex = hunk.newStart - 1;

    for (const auto &line : hunk.lines) {
      if (line.origin == QChar('-')) {
        // Removed from left
        if (leftIndex < leftLines.size()) {
          alignedLeft.append(leftLines[leftIndex]);
        } else {
          alignedLeft.append(""); // safety
        }
        changedLeft0.append(alignedLeft.length() - 1);
        alignedRight.append(""); // mark deletion on right
        changedRight1.append(alignedLeft.length() - 1);
        leftIndex++;
        // rightIndex++;
        currentLeftIndex++;
      } else if (line.origin == QChar('+')) {
        // Added to right
        alignedLeft.append(""); // placeholder empty line on left
        changedLeft1.append(alignedLeft.length() - 1);
        alignedRight.append(line.content);
        changedRight0.append(alignedRight.length() - 1);
        // leftIndex++;
        rightIndex++;
        // currentLeftIndex++;
      } else {
        // Context line
        if (leftIndex < leftLines.size()) {
          alignedLeft.append(leftLines[leftIndex]);
          alignedRight.append(leftLines[leftIndex]);
        }
        leftIndex++;
        rightIndex++;
        currentLeftIndex++;
      }
    }
  }

  // --- Copy remaining lines after last hunk ---
  while (currentLeftIndex < leftLines.size()) {
    alignedLeft.append(leftLines[currentLeftIndex]);
    alignedRight.append(leftLines[currentLeftIndex]);
    currentLeftIndex++;
  }

  Content content;
  content.left = alignedLeft.join('\n');
  content.right = alignedRight.join('\n');
  content.leftMarks0 = changedLeft0;
  content.leftMarks1 = changedLeft1;
  content.rightMarks0 = changedRight0;
  content.rightMarks1 = changedRight1;

  LOG_PERF(0, "Exit");

  return content;
}

void SideBySideDiffView::loadAndHighlight(
    const QString &leftContent, const GitRepo::FilesDiffDelta &rightDelta,
    const QString &extension) {
  LOG_PERF(0, "Enter");

  setupEditors(extension);

  const Content content = createContent(leftContent, rightDelta);

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

  _leftEditor->setMarginWidth(0, leftMask);
  _rightEditor->setMarginWidth(0, rightMask);

  _leftEditor->setText(content.left);
  _rightEditor->setText(content.right);

  // Highlight differing lines
  _leftEditor->markerDefine(QsciScintilla::Background, 1);
  _leftEditor->markerDefine(QsciScintilla::Background, 2);
  _rightEditor->markerDefine(QsciScintilla::Background, 1);
  _rightEditor->markerDefine(QsciScintilla::Background, 2);
  _leftEditor->setMarkerBackgroundColor(QColor(0xc6efce), 1);
  _leftEditor->setMarkerBackgroundColor(QColor(0xffc7ce), 2);
  _rightEditor->setMarkerBackgroundColor(QColor(0xffc7ce), 1);
  _rightEditor->setMarkerBackgroundColor(QColor(0xc6efce), 2);

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

  LOG_PERF(0, "Exit");
}

void SideBySideDiffView::setupEditors(const QString &extension) {
  LOG_PERF(0, "Enter");

  const QScintillaLexerFactory factory;

  auto *lexer = factory.createLexerForExtension(extension, this);

  _leftEditor->setLexer(lexer);
  _rightEditor->setLexer(lexer);

  _leftEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _leftEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _leftEditor->setMarginWidth(1, 2);

  _rightEditor->setMarginType(0, QsciScintilla::NumberMargin);
  _rightEditor->setMarginType(1, QsciScintilla::SymbolMargin);
  _rightEditor->setMarginWidth(1, 2);

  _leftEditor->setWrapMode(QsciScintilla::WrapNone);
  _rightEditor->setWrapMode(QsciScintilla::WrapNone);

  DarkThemeLexersHighlighter highligheter;
  highligheter.applyDarkTheme(_leftEditor);
  highligheter.applyDarkTheme(_rightEditor);

  LOG_PERF(0, "Exit");
}

void SideBySideDiffView::syncScrollbars() {
  LOG_PERF(0, "Enter");

  connect(_leftEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _rightEditor->verticalScrollBar(), &QScrollBar::setValue);
  connect(_rightEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          _leftEditor->verticalScrollBar(), &QScrollBar::setValue);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
