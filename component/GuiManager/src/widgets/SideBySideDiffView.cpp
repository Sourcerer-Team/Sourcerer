#include "widgets/SideBySideDiffView.h"
#include "DarkThemeLexersHighlighter.h"
#include "QScintillaLexerFactory.h"

#include <Log.h>

#include <Qsci/qscilexerdiff.h>

#include <QHBoxLayout>
#include <QScrollBar>

#define THREAD_ID "SideBySideDiffView"

namespace GuiManager::Widgets {

struct AlignedLine {
  QString left;
  QString right;
  bool isDifferent;
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

static QList<AlignedLine> compareFiles(const QStringList &leftLines,
                                       const QStringList &rightLines) {
  LOG_PERF(0, "Enter");

  QList<AlignedLine> result;
  int maxLines = qMax(leftLines.size(), rightLines.size());

  for (int i = 0; i < maxLines; ++i) {
    QString left = i < leftLines.size() ? leftLines[i] : "";
    QString right = i < rightLines.size() ? rightLines[i] : "";
    bool diff = (left != right);
    result.append({left, right, diff});
  }

  LOG_PERF(0, "Exit");

  return result;
}

void SideBySideDiffView::loadAndHighlight(const QString &leftContent,
                                          const QString &rightContent,
                                          const QString &extension) {
  LOG_PERF(0, "Enter");

  setupEditors(extension);

  const QStringList leftRaw =
      QString(leftContent).replace("\r\n", "\n").split('\n');
  const QStringList rightRaw =
      QString(rightContent).replace("\r\n", "\n").split('\n');

  const auto aligned = compareFiles(leftRaw, rightRaw);

  QStringList leftAligned, rightAligned;
  QList<int> diffLines;

  for (int i = 0; i < aligned.size(); ++i) {
    leftAligned << aligned[i].left;
    rightAligned << aligned[i].right;
    if (aligned[i].isDifferent)
      diffLines << i;
  }

  const QString leftAlignedContent = leftAligned.join("\n");
  const QString rightAlignedContent = rightAligned.join("\n");

  const int leftLineCount = leftAligned.length() + 1;
  const int rightLineCount = rightAligned.length() + 1;
  const int leftWidth = QString::number(leftLineCount).length();
  const int rightWidth = QString::number(rightLineCount).length();
  const QString leftMask =
      QString::number(0).rightJustified(leftWidth + 1, '0');
  const QString rightMask =
      QString::number(0).rightJustified(rightWidth + 1, '0');

  _leftEditor->setMarginWidth(0, leftMask);
  _rightEditor->setMarginWidth(0, rightMask);

  _leftEditor->setText(leftAlignedContent);
  _rightEditor->setText(rightAlignedContent);

         // Highlight differing lines
  _leftEditor->markerDefine(QsciScintilla::Background, 1);
  _rightEditor->markerDefine(QsciScintilla::Background, 1);
  _leftEditor->setMarkerBackgroundColor(Qt::yellow, 1);
  _rightEditor->setMarkerBackgroundColor(Qt::yellow, 1);

  for (int line : diffLines) {
    _leftEditor->markerAdd(line, 1);
    _rightEditor->markerAdd(line, 1);
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
