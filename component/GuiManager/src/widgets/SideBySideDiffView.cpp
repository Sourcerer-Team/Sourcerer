#include "widgets/SideBySideDiffView.h"

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
    : QWidget(parent), leftEditor(new QsciScintilla(this)),
      rightEditor(new QsciScintilla(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setupEditors();
  syncScrollbars();

  auto *layout = new QHBoxLayout(this);
  layout->addWidget(leftEditor);
  layout->addWidget(rightEditor);
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
                                          const QString &rightContent) {
  LOG_PERF(0, "Enter");

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

  leftEditor->setMarginWidth(0, leftMask);
  rightEditor->setMarginWidth(0, rightMask);

  leftEditor->setText(leftAlignedContent);
  rightEditor->setText(rightAlignedContent);

         // Highlight differing lines
  leftEditor->markerDefine(QsciScintilla::Background, 1);
  rightEditor->markerDefine(QsciScintilla::Background, 1);
  leftEditor->setMarkerBackgroundColor(Qt::yellow, 1);
  rightEditor->setMarkerBackgroundColor(Qt::yellow, 1);

  for (int line : diffLines) {
    leftEditor->markerAdd(line, 1);
    rightEditor->markerAdd(line, 1);
  }

  LOG_PERF(0, "Exit");
}

void SideBySideDiffView::setupEditors() {
  LOG_PERF(0, "Enter");

  auto *lexer = new QsciLexerDiff(this);
  leftEditor->setLexer(lexer);
  rightEditor->setLexer(lexer);

  const QString fontName =
      _configManager.getString(::ConfigManager::ConfigKey::GitRepoDiffFontName);
  const qlonglong fontSize =
      _configManager.getLong(::ConfigManager::ConfigKey::GitRepoDiffFontSize);

  const QFont font(fontName, fontSize);
  leftEditor->setFont(font);
  rightEditor->setFont(font);

  leftEditor->setMarginsBackgroundColor(Qt::lightGray);
  rightEditor->setMarginsBackgroundColor(Qt::lightGray);

  leftEditor->setWrapMode(QsciScintilla::WrapNone);
  rightEditor->setWrapMode(QsciScintilla::WrapNone);

  LOG_PERF(0, "Exit");
}

void SideBySideDiffView::syncScrollbars() {
  LOG_PERF(0, "Enter");

  connect(leftEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          rightEditor->verticalScrollBar(), &QScrollBar::setValue);
  connect(rightEditor->verticalScrollBar(), &QScrollBar::valueChanged,
          leftEditor->verticalScrollBar(), &QScrollBar::setValue);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
