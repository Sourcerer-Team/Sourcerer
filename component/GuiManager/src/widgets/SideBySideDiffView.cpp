#include "widgets/SideBySideDiffView.h"
#include "DarkThemeLexersHighlighter.h"
#include "QScintillaLexerFactory.h"

#include <Log.h>
#include <StylesheetManager.h>

#include <Qsci/qscilexerdiff.h>

#include <QEvent>
#include <QHBoxLayout>
#include <QPainter>
#include <QScrollBar>
#include <QStyleOptionSlider>

#define THREAD_ID "SideBySideDiffView"

namespace GuiManager::Widgets {

// ==================== ScrollbarMarksWidget ====================

ScrollbarMarksWidget::ScrollbarMarksWidget(QScrollBar *scrollBar,
                                           QsciScintilla *editor,
                                           QWidget *parent)
    : QWidget(parent), _scrollBar(scrollBar), _editor(editor) {
  setAttribute(Qt::WA_TransparentForMouseEvents);
  setAttribute(Qt::WA_TranslucentBackground);
  setAutoFillBackground(false);

  // Install event filter to track scrollbar geometry changes
  _scrollBar->installEventFilter(this);

  updateGeometry();
}

void ScrollbarMarksWidget::setMarks(const QVector<int> &deletedLines,
                                    const QVector<int> &addedLines) {
  _deletedLines = deletedLines;
  _addedLines = addedLines;
  update();
}

void ScrollbarMarksWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  // Don't draw marks if scrollbar is not visible
  if (!_scrollBar || !_scrollBar->isVisible()) {
    return;
  }

  const int totalLines = _editor->lines();
  if (totalLines <= 0) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);

  const int markerWidth = 4;
  const int markerHeight = 2;

  // Calculate available drawing area (exclude buttons)
  QStyleOptionSlider opt;
  opt.initFrom(_scrollBar);
  opt.orientation = _scrollBar->orientation();
  opt.minimum = _scrollBar->minimum();
  opt.maximum = _scrollBar->maximum();
  opt.sliderPosition = _scrollBar->value();
  opt.sliderValue = _scrollBar->value();
  opt.singleStep = _scrollBar->singleStep();
  opt.pageStep = _scrollBar->pageStep();

  QRect grooveRect = _scrollBar->style()->subControlRect(
      QStyle::CC_ScrollBar, &opt, QStyle::SC_ScrollBarGroove, _scrollBar);

  const int grooveTop = grooveRect.top();
  const int grooveHeight = grooveRect.height();

  if (grooveHeight <= 0) {
    return;
  }

  // Draw at the right edge of the groove area
  const int markerX = grooveRect.right() - markerWidth;

  // Draw deleted lines (red)
  painter.setBrush(QColor(220, 50, 47)); // Red
  painter.setPen(Qt::NoPen);
  for (int lineNum : _deletedLines) {
    if (lineNum < 0 || lineNum >= totalLines) {
      continue;
    }
    const int yPos =
        grooveTop + (lineNum * grooveHeight / totalLines) - markerHeight / 2;
    painter.drawRect(markerX, yPos, markerWidth, markerHeight);
  }

  // Draw added lines (green)
  painter.setBrush(QColor(133, 153, 0)); // Green
  for (int lineNum : _addedLines) {
    if (lineNum < 0 || lineNum >= totalLines) {
      continue;
    }
    const int yPos =
        grooveTop + (lineNum * grooveHeight / totalLines) - markerHeight / 2;
    painter.drawRect(markerX, yPos, markerWidth, markerHeight);
  }
}

bool ScrollbarMarksWidget::eventFilter(QObject *obj, QEvent *event) {
  if (obj == _scrollBar) {
    if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
      updateGeometry();
    } else if (event->type() == QEvent::Show) {
      // Show marks when scrollbar becomes visible
      show();
    } else if (event->type() == QEvent::Hide) {
      // Hide marks when scrollbar is hidden
      hide();
    }
  }
  return QWidget::eventFilter(obj, event);
}

void ScrollbarMarksWidget::updateGeometry() {
  if (!_scrollBar) {
    return;
  }

  // Position the overlay directly on top of the scrollbar
  const QPoint scrollBarPos = _scrollBar->mapTo(parentWidget(), QPoint(0, 0));
  setGeometry(scrollBarPos.x(), scrollBarPos.y(), _scrollBar->width(),
              _scrollBar->height());
  raise(); // Ensure it's on top
}

// ==================== SideBySideDiffView ====================

SideBySideDiffView::SideBySideDiffView(QWidget *parent)
    : QWidget(parent), _leftEditor(new QsciScintilla(this)),
      _rightEditor(new QsciScintilla(this)),
      _configManager(::ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  syncScrollbars();

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(_leftEditor);
  layout->addWidget(_rightEditor);
  setLayout(layout);

  // Create scrollbar marks widgets
  _leftMarks = new ScrollbarMarksWidget(_leftEditor->verticalScrollBar(),
                                        _leftEditor, this);
  _rightMarks = new ScrollbarMarksWidget(_rightEditor->verticalScrollBar(),
                                         _rightEditor, this);

  // Initially hide marks, they will show when scrollbars become visible
  _leftMarks->hide();
  _rightMarks->hide();

  LOG_PERF(0, "Exit");
}

static SideBySideDiffContent
createContent(const QString &leftContent,
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
    int leftIndex = hunk.oldStart > 0 ? hunk.oldStart - 1 : 0;
    int rightIndex = hunk.newStart > 0 ? hunk.newStart - 1 : 0;

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

  SideBySideDiffContent content;
  content.left = alignedLeft.join('\n');
  content.right = alignedRight.join('\n');
  content.leftMarks0 = changedLeft0;
  content.leftMarks1 = changedLeft1;
  content.rightMarks0 = changedRight0;
  content.rightMarks1 = changedRight1;

  LOG_PERF(0, "Exit");

  return content;
}

void SideBySideDiffView::changeEvent(QEvent *event) {
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

void SideBySideDiffView::refreshScintillaTheme() {
  DarkThemeLexersHighlighter highlighter;
  const auto editors = findChildren<QsciScintilla *>();

  for (auto *editor : editors) {
    highlighter.applyDarkTheme(editor);
  }
}

void SideBySideDiffView::clearContent() {
  LOG_PERF(0, "Enter");

  _leftEditor->clear();
  _rightEditor->clear();
  _leftEditor->markerDeleteAll();
  _rightEditor->markerDeleteAll();
  _leftMarks->setMarks({}, {});
  _rightMarks->setMarks({}, {});

  LOG_PERF(0, "Exit");
}

void SideBySideDiffView::loadAndHighlight(
    const QString &leftContent, const GitRepo::FilesDiffDelta &rightDelta,
    const QString &extension) {
  LOG_PERF(0, "Enter");

  setupEditors(extension);

  const SideBySideDiffContent content = createContent(leftContent, rightDelta);

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

  const bool lightTheme =
      StylesheetManager::StylesheetManager::currentStylesheet() ==
      StylesheetManager::StylesheetKey::SourcererLight;
  const QColor deletedColor =
      lightTheme ? QColor(255, 231, 231) : QColor(0x3b1313);
  const QColor addedColor =
      lightTheme ? QColor(232, 246, 232) : QColor(0x133b13);

  _leftEditor->setMarkerBackgroundColor(deletedColor, 1);
  _leftEditor->setMarkerBackgroundColor(addedColor, 2);
  _rightEditor->setMarkerBackgroundColor(addedColor, 1);
  _rightEditor->setMarkerBackgroundColor(deletedColor, 2);

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

  // Update scrollbar marks
  // leftMarks0 = deleted lines (red) on left editor
  // leftMarks1 = added lines (green) on left editor
  // rightMarks0 = added lines (green) on right editor
  // rightMarks1 = deleted lines (red) on right editor
  _leftMarks->setMarks(content.leftMarks0, content.leftMarks1);
  _rightMarks->setMarks(content.rightMarks1, content.rightMarks0);

  LOG_PERF(0, "Exit");
}

void SideBySideDiffView::setupEditors(const QString &extension) {
  LOG_PERF(0, "Enter");

  _leftEditor->setReadOnly(true);
  _rightEditor->setReadOnly(true);

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

  _leftEditor->setFolding(QsciScintilla::NoFoldStyle);
  _rightEditor->setFolding(QsciScintilla::NoFoldStyle);

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
