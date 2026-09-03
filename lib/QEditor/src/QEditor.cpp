#include "Qsci/qsciscintilla.h"

#include <tree_sitter/api.h>

#include <QFocusEvent>
#include <QFrame>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QScrollBar>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextEdit>
#include <QTextFormat>
#include <QVBoxLayout>
#include <algorithm>

QsciLexer::QsciLexer(QObject *parent, const QString &treeSitterLanguage)
    : QObject(parent), _treeSitterLanguage(treeSitterLanguage) {}

QsciLexer::~QsciLexer() = default;

void QsciLexer::setTreeSitterLanguage(const QString &language) {
  if (_treeSitterLanguage == language) {
    return;
  }

  _treeSitterLanguage = language;
  emit changed();
}

void QsciLexer::setDefaultPaper(const QColor &color) {
  _defaultPaper = color;
  emit changed();
}

void QsciLexer::setDefaultColor(const QColor &color) {
  _defaultColor = color;
  emit changed();
}

void QsciLexer::setDefaultFont(const QFont &font) {
  _defaultFont = font;
  emit changed();
}

void QsciLexer::setColor(const QColor &color, int style) {
  _styleColors[style] = color;
  emit changed();
}

void QsciLexer::setPaper(const QColor &color, int style) {
  _stylePapers[style] = color;
  emit changed();
}

void QsciLexer::setFont(const QFont &font, int style) {
  _styleFonts[style] = font;
  emit changed();
}

QColor QsciLexer::color(int style) const {
  return _styleColors.value(style, _defaultColor);
}

QColor QsciLexer::paper(int style) const {
  return _stylePapers.value(style, _defaultPaper);
}

QFont QsciLexer::font(int style) const {
  return _styleFonts.value(style, _defaultFont);
}

#define QEDITOR_DEFINE_LEXER(ClassName, LanguageId)                           \
  ClassName::ClassName(QObject *parent) : QsciLexer(parent, LanguageId) {}

QEDITOR_DEFINE_LEXER(QsciLexerCPP, "cpp")
QEDITOR_DEFINE_LEXER(QsciLexerPython, "python")
QEDITOR_DEFINE_LEXER(QsciLexerJavaScript, "javascript")
QEDITOR_DEFINE_LEXER(QsciLexerHTML, "html")
QEDITOR_DEFINE_LEXER(QsciLexerCSS, "css")
QEDITOR_DEFINE_LEXER(QsciLexerXML, "xml")
QEDITOR_DEFINE_LEXER(QsciLexerJSON, "json")
QEDITOR_DEFINE_LEXER(QsciLexerSQL, "sql")
QEDITOR_DEFINE_LEXER(QsciLexerBash, "bash")
QEDITOR_DEFINE_LEXER(QsciLexerBatch, "batch")
QEDITOR_DEFINE_LEXER(QsciLexerMakefile, "makefile")
QEDITOR_DEFINE_LEXER(QsciLexerDiff, "diff")
QEDITOR_DEFINE_LEXER(QsciLexerProperties, "properties")
QEDITOR_DEFINE_LEXER(QsciLexerYAML, "yaml")
QEDITOR_DEFINE_LEXER(QsciLexerMarkdown, "markdown")
QEDITOR_DEFINE_LEXER(QsciLexerTeX, "tex")
QEDITOR_DEFINE_LEXER(QsciLexerPostScript, "postscript")
QEDITOR_DEFINE_LEXER(QsciLexerMatlab, "matlab")
QEDITOR_DEFINE_LEXER(QsciLexerOctave, "octave")
QEDITOR_DEFINE_LEXER(QsciLexerPerl, "perl")
QEDITOR_DEFINE_LEXER(QsciLexerRuby, "ruby")
QEDITOR_DEFINE_LEXER(QsciLexerFortran, "fortran")
QEDITOR_DEFINE_LEXER(QsciLexerFortran77, "fortran77")
QEDITOR_DEFINE_LEXER(QsciLexerIDL, "idl")
QEDITOR_DEFINE_LEXER(QsciLexerVHDL, "vhdl")
QEDITOR_DEFINE_LEXER(QsciLexerVerilog, "verilog")
QEDITOR_DEFINE_LEXER(QsciLexerTCL, "tcl")
QEDITOR_DEFINE_LEXER(QsciLexerLua, "lua")
QEDITOR_DEFINE_LEXER(QsciLexerPascal, "pascal")
QEDITOR_DEFINE_LEXER(QsciLexerAsm, "asm")
QEDITOR_DEFINE_LEXER(QsciLexerMASM, "masm")
QEDITOR_DEFINE_LEXER(QsciLexerNASM, "nasm")
QEDITOR_DEFINE_LEXER(QsciLexerCMake, "cmake")
QEDITOR_DEFINE_LEXER(QsciLexerD, "d")
QEDITOR_DEFINE_LEXER(QsciLexerPOV, "pov")
QEDITOR_DEFINE_LEXER(QsciLexerSpice, "spice")
QEDITOR_DEFINE_LEXER(QsciLexerCustom, "custom")

#undef QEDITOR_DEFINE_LEXER

class QLineNumberArea final : public QWidget {
public:
  explicit QLineNumberArea(QsciScintilla *editor)
      : QWidget(editor), _editor(editor) {}

  QSize sizeHint() const override {
    return QSize(_editor ? _editor->lineNumberAreaWidth() : 0, 0);
  }

protected:
  void paintEvent(QPaintEvent *event) override {
    if (_editor) {
      _editor->lineNumberAreaPaintEvent(event);
    }
  }

  void mousePressEvent(QMouseEvent *event) override {
    if (_editor && event) {
      const QPoint editorPoint = mapTo(_editor, event->position().toPoint());
      const int line = _editor->lineFromPoint(editorPoint);
      emit _editor->marginClicked(0, line, event->modifiers());
    }
    QWidget::mousePressEvent(event);
  }

private:
  QsciScintilla *_editor;
};

class SimpleSyntaxHighlighter final : public QSyntaxHighlighter {
public:
  explicit SimpleSyntaxHighlighter(QTextDocument *document,
                                   QsciScintilla *editor)
      : QSyntaxHighlighter(document), _editor(editor) {}

  void setEditor(QsciScintilla *editor) {
    _editor = editor;
    rehighlight();
  }

protected:
  void highlightBlock(const QString &text) override {
    const QsciLexer *lexer = _editor ? _editor->lexer() : nullptr;
    const QString language = lexer ? lexer->treeSitterLanguage().toLower()
                                   : QString();

    if (language == QLatin1String("diff")) {
      highlightDiff(text);
      return;
    }

    highlightGeneric(text, language);
  }

private:
  static QTextCharFormat makeFormat(const QColor &foreground,
                                    bool bold = false,
                                    bool italic = false) {
    QTextCharFormat format;
    format.setForeground(foreground);
    format.setFontWeight(bold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(italic);
    return format;
  }

  void applyRules(const QString &text,
                  const QList<QPair<QRegularExpression, QTextCharFormat>> &rules) {
    for (const auto &rule : rules) {
      auto matchIterator = rule.first.globalMatch(text);
      while (matchIterator.hasNext()) {
        const QRegularExpressionMatch match = matchIterator.next();
        if (match.hasMatch()) {
          setFormat(match.capturedStart(), match.capturedLength(), rule.second);
        }
      }
    }
  }

  void highlightDiff(const QString &text) {
    const QString trimmed = text.trimmed();
    if (trimmed.startsWith(QLatin1String("diff ")) ||
        trimmed.startsWith(QLatin1String("index ")) ||
        trimmed.startsWith(QLatin1String("--- ")) ||
        trimmed.startsWith(QLatin1String("+++ ")) ||
        trimmed.startsWith(QLatin1String("@@"))) {
      setFormat(0, text.length(), makeFormat(QColor(97, 175, 239), true));
      return;
    }

    if (text.startsWith(QLatin1Char('+')) && !text.startsWith(QLatin1String("+++"))) {
      setFormat(0, text.length(), makeFormat(QColor(152, 195, 121)));
      return;
    }

    if (text.startsWith(QLatin1Char('-')) && !text.startsWith(QLatin1String("---"))) {
      setFormat(0, text.length(), makeFormat(QColor(224, 108, 117)));
      return;
    }

    if (text.startsWith(QLatin1String("@@"))) {
      setFormat(0, text.length(), makeFormat(QColor(198, 120, 221), true));
      return;
    }

    const QList<QPair<QRegularExpression, QTextCharFormat>> rules = {
        {QRegularExpression(QStringLiteral("\\b(?:true|false|null|nil|none|yes|no)\\b"),
                            QRegularExpression::CaseInsensitiveOption),
         makeFormat(QColor(198, 120, 221), true)},
        {QRegularExpression(QStringLiteral("\\b(?:if|else|for|while|switch|case|return|class|struct|enum|fn|function|def|import|from|include|package|public|private|protected|const|let|var|new|delete|try|catch|throw|throws|async|await|yield)\\b"),
                            QRegularExpression::CaseInsensitiveOption),
         makeFormat(QColor(198, 120, 221), true)},
        {QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b"),
                            QRegularExpression::CaseInsensitiveOption),
         makeFormat(QColor(209, 154, 102))},
        {QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'")),
         makeFormat(QColor(152, 195, 121))},
        {QRegularExpression(QStringLiteral("//[^\\n]*|#[^\\n]*|--[^\\n]*|;[^\\n]*")),
         makeFormat(QColor(92, 99, 112), false, true)},
    };
    applyRules(text, rules);
  }

  void highlightGeneric(const QString &text, const QString &language) {
    QList<QPair<QRegularExpression, QTextCharFormat>> rules;

    if (language == QLatin1String("markdown")) {
      rules.append({QRegularExpression(QStringLiteral("^#{1,6}\\s.*")),
                    makeFormat(QColor(198, 120, 221), true)});
      rules.append({QRegularExpression(QStringLiteral("^>\\s.*")),
                    makeFormat(QColor(92, 99, 112), false, true)});
      rules.append({QRegularExpression(QStringLiteral("`[^`]+`")),
                    makeFormat(QColor(152, 195, 121))});
    } else if (language == QLatin1String("json") ||
               language == QLatin1String("yaml") ||
               language == QLatin1String("properties")) {
      rules.append({QRegularExpression(QStringLiteral("^\\s*[A-Za-z0-9_.-]+(?=\\s*[:=])")),
                    makeFormat(QColor(97, 175, 239), true)});
      rules.append({QRegularExpression(QStringLiteral("\\b(?:true|false|null|yes|no)\\b"),
                                       QRegularExpression::CaseInsensitiveOption),
                    makeFormat(QColor(198, 120, 221), true)});
    } else if (language == QLatin1String("xml") ||
               language == QLatin1String("html")) {
      rules.append({QRegularExpression(QStringLiteral("<[^>]+>")),
                    makeFormat(QColor(97, 175, 239), true)});
      rules.append({QRegularExpression(QStringLiteral("\\b[A-Za-z_:][A-Za-z0-9_:\\-.]*=")),
                    makeFormat(QColor(209, 154, 102))});
    }

    const QList<QPair<QRegularExpression, QTextCharFormat>> commonRules = {
        {QRegularExpression(QStringLiteral("\\b(?:if|else|for|while|switch|case|break|continue|return|class|struct|enum|namespace|import|from|package|def|function|fn|let|var|const|public|private|protected|static|final|new|delete|try|catch|throw|throws|async|await|yield)\\b"),
                            QRegularExpression::CaseInsensitiveOption),
         makeFormat(QColor(198, 120, 221), true)},
        {QRegularExpression(QStringLiteral("\\b\\d+(?:\\.\\d+)?\\b")),
         makeFormat(QColor(209, 154, 102))},
        {QRegularExpression(QStringLiteral("\"(?:\\\\.|[^\"\\\\])*\"|'(?:\\\\.|[^'\\\\])*'")),
         makeFormat(QColor(152, 195, 121))},
        {QRegularExpression(QStringLiteral("//[^\\n]*|#[^\\n]*|--[^\\n]*|;[^\\n]*")),
         makeFormat(QColor(92, 99, 112), false, true)},
        {QRegularExpression(QStringLiteral("/\\*.*\\*/")),
         makeFormat(QColor(92, 99, 112), false, true)},
    };

    applyRules(text, rules);
    applyRules(text, commonRules);
  }

  QsciScintilla *_editor;
};

QsciScintilla::QsciScintilla(QWidget *parent)
    : QPlainTextEdit(parent), _lexer(nullptr), _caretLineVisible(false),
      _indentationsUseTabs(false), _autoIndent(false), _indentationGuides(false),
      _foldStyle(NoFoldStyle), _braceMatch(NoBraceMatch), _lineNumberArea(nullptr),
      _syntaxHighlighter(nullptr), _parser(nullptr), _tree(nullptr) {
  for (int i = 0; i < 4; ++i) {
    _marginLineNumbers[i] = false;
    _marginSensitivity[i] = false;
    _marginTypes[i] = NumberMargin;
    _marginMarkerMasks[i] = 0;
    _marginWidths[i] = 0;
  }
  setFrameShape(QFrame::NoFrame);
  setTabStopDistance(fontMetrics().horizontalAdvance(QStringLiteral("    ")));
  _lineNumberArea = new QLineNumberArea(this);
  connect(this, &QPlainTextEdit::blockCountChanged, this,
          &QsciScintilla::updateLineNumberAreaWidth);
  connect(this, &QPlainTextEdit::updateRequest, this,
          &QsciScintilla::updateLineNumberArea);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &QsciScintilla::refreshCurrentLineHighlight);
  updateLineNumberAreaWidth(0);
  _parser = ts_parser_new();
}

QsciScintilla::~QsciScintilla() {
  if (_tree) {
    ts_tree_delete(_tree);
    _tree = nullptr;
  }
  if (_parser) {
    ts_parser_delete(_parser);
    _parser = nullptr;
  }
}

void QsciScintilla::setText(const QString &text) {
  setPlainText(text);
  if (_syntaxHighlighter) {
    _syntaxHighlighter->rehighlight();
  }
  refreshCurrentLineHighlight();
}

QString QsciScintilla::text() const { return toPlainText(); }

QString QsciScintilla::text(int line) const {
  const QTextBlock block = document()->findBlockByNumber(line);
  if (!block.isValid()) {
    return {};
  }
  return block.text();
}

int QsciScintilla::lines() const {
  const QString currentText = toPlainText();
  if (currentText.isEmpty()) {
    return 1;
  }
  return currentText.count(QChar('\n')) + 1;
}

void QsciScintilla::setLexer(QsciLexer *lexer) {
  if (_lexer == lexer) {
    return;
  }

  if (_lexer) {
    disconnect(_lexer, nullptr, this, nullptr);
  }

  _lexer = lexer;
  if (_lexer) {
    connect(_lexer, &QsciLexer::changed, this, &QsciScintilla::applyCurrentPalette);
  }
  if (!_syntaxHighlighter) {
    _syntaxHighlighter = new SimpleSyntaxHighlighter(document(), this);
  } else {
    _syntaxHighlighter->setParent(this);
  }
  _syntaxHighlighter->setEditor(this);
  applyCurrentPalette();
  refreshCurrentLineHighlight();
}

void QsciScintilla::setPaper(const QColor &color) {
  _paper = color;
  applyCurrentPalette();
}

void QsciScintilla::setColor(const QColor &color) {
  _color = color;
  applyCurrentPalette();
}

void QsciScintilla::setCaretLineBackgroundColor(const QColor &color) {
  _caretLineBackground = color;
  refreshCurrentLineHighlight();
}

void QsciScintilla::setCaretLineVisible(bool visible) {
  _caretLineVisible = visible;
  refreshCurrentLineHighlight();
}

void QsciScintilla::setCaretForegroundColor(const QColor &color) {
  _caretForeground = color;
  QPalette palette = this->palette();
  palette.setColor(QPalette::Text, _color.isValid() ? _color : palette.color(QPalette::Text));
  setPalette(palette);
}

void QsciScintilla::setSelectionBackgroundColor(const QColor &color) {
  _selectionBackground = color;
  QPalette palette = this->palette();
  palette.setColor(QPalette::Highlight, color);
  setPalette(palette);
}

void QsciScintilla::setSelectionForegroundColor(const QColor &color) {
  _selectionForeground = color;
  QPalette palette = this->palette();
  palette.setColor(QPalette::HighlightedText, color);
  setPalette(palette);
}

void QsciScintilla::setMarginLineNumbers(int margin, bool enabled) {
  if (margin < 0 || margin >= 4) {
    return;
  }
  _marginLineNumbers[margin] = enabled;
  updateLineNumberAreaWidth(0);
}

void QsciScintilla::setMarginBackgroundColor(int margin, const QColor &color) {
  _marginBackgroundColors[margin] = color;
}

void QsciScintilla::setMarginsBackgroundColor(const QColor &color) {
  _marginsBackground = color;
  applyCurrentPalette();
}

void QsciScintilla::setMarginsForegroundColor(const QColor &color) {
  _marginsForeground = color;
  applyCurrentPalette();
}

void QsciScintilla::setMarginWidth(int margin, const QString &text) {
  if (margin < 0 || margin >= 4) {
    return;
  }
  const int width = fontMetrics().horizontalAdvance(text) + 8;
  _marginWidths[margin] = width;
  updateViewportMargins();
}

void QsciScintilla::setMarginWidth(int margin, int width) {
  if (margin < 0 || margin >= 4) {
    return;
  }
  _marginWidths[margin] = width;
  updateViewportMargins();
  updateLineNumberAreaWidth(0);
}

void QsciScintilla::setMarginType(int margin, MarginType type) {
  if (margin < 0 || margin >= 4) {
    return;
  }
  _marginTypes[margin] = static_cast<int>(type);
}

void QsciScintilla::setMarginSensitivity(int margin, bool sensitivity) {
  if (margin < 0 || margin >= 4) {
    return;
  }
  _marginSensitivity[margin] = sensitivity;
}

void QsciScintilla::setMarginMarkerMask(int margin, int mask) {
  if (margin < 0 || margin >= 4) {
    return;
  }
  _marginMarkerMasks[margin] = mask;
}

void QsciScintilla::markerDefine(MarkerSymbol symbol, int marker) {
  _markerSymbols[marker] = static_cast<int>(symbol);
}

void QsciScintilla::markerDeleteAll() {
  _markers.clear();
  viewport()->update();
  if (_lineNumberArea) {
    _lineNumberArea->update();
  }
  refreshCurrentLineHighlight();
}

void QsciScintilla::setWrapMode(WrapMode mode) {
  setLineWrapMode(mode == WrapWord ? QPlainTextEdit::WidgetWidth
                                   : QPlainTextEdit::NoWrap);
}

void QsciScintilla::setFolding(FoldStyle style) { _foldStyle = style; }

void QsciScintilla::setFoldMarginColors(const QColor &foreground,
                                        const QColor &background) {
  _foldMarginForeground = foreground;
  _foldMarginBackground = background;
}

void QsciScintilla::setIndentationsUseTabs(bool useTabs) {
  _indentationsUseTabs = useTabs;
}

void QsciScintilla::setAutoIndent(bool enabled) { _autoIndent = enabled; }

void QsciScintilla::setIndentationGuides(bool enabled) {
  _indentationGuides = enabled;
}

void QsciScintilla::setIndentationGuidesBackgroundColor(const QColor &color) {
  _indentGuidesBackground = color;
}

void QsciScintilla::setIndentationGuidesForegroundColor(const QColor &color) {
  _indentGuidesForeground = color;
}

void QsciScintilla::setBraceMatching(BraceMatch style) { _braceMatch = style; }

void QsciScintilla::setMatchedBraceBackgroundColor(const QColor &color) {
  _matchedBraceBackground = color;
}

void QsciScintilla::setUnmatchedBraceBackgroundColor(const QColor &color) {
  _unmatchedBraceBackground = color;
}

void QsciScintilla::setMarkerBackgroundColor(const QColor &color, int marker) {
  _markerBackgroundColors[marker] = color;
  refreshCurrentLineHighlight();
}

void QsciScintilla::setMarkerForegroundColor(const QColor &color, int marker) {
  _markerForegroundColors[marker] = color;
  refreshCurrentLineHighlight();
}

int QsciScintilla::markerAdd(int line, int marker) {
  _markers[line].insert(marker);
  viewport()->update();
  if (_lineNumberArea) {
    _lineNumberArea->update();
  }
  refreshCurrentLineHighlight();
  return marker;
}

void QsciScintilla::markerDelete(int line, int marker) {
  auto it = _markers.find(line);
  if (it == _markers.end()) {
    return;
  }
  it->remove(marker);
  if (it->isEmpty()) {
    _markers.erase(it);
  }
  viewport()->update();
  if (_lineNumberArea) {
    _lineNumberArea->update();
  }
  refreshCurrentLineHighlight();
}

int QsciScintilla::markersAtLine(int line) const {
  int mask = 0;
  const auto it = _markers.constFind(line);
  if (it == _markers.constEnd()) {
    return mask;
  }
  for (int marker : *it) {
    mask |= (1 << marker);
  }
  return mask;
}

void QsciScintilla::mousePressEvent(QMouseEvent *event) {
  if (event && event->button() == Qt::LeftButton) {
    const int gutterWidth = lineNumberAreaWidth();
    if (event->position().x() <= gutterWidth) {
      emit marginClicked(3, lineFromPoint(event->position().toPoint()),
                         event->modifiers());
      event->accept();
      return;
    }
  }

  QPlainTextEdit::mousePressEvent(event);
}

void QsciScintilla::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);
  const QRect cr = contentsRect();
  const int width = lineNumberAreaWidth();
  if (_lineNumberArea) {
    _lineNumberArea->setGeometry(cr.left(), cr.top(), width, cr.height());
  }
  updateViewportMargins();
  updateLineNumberAreaWidth(0);
  refreshCurrentLineHighlight();
}

void QsciScintilla::focusInEvent(QFocusEvent *event) {
  QPlainTextEdit::focusInEvent(event);
  refreshCurrentLineHighlight();
}

void QsciScintilla::refreshCurrentLineHighlight() {
  if (_lineNumberArea) {
    _lineNumberArea->update();
  }

  QList<QTextEdit::ExtraSelection> extraSelections;

  const auto markerKeys = _markers.keys();
  for (int line : markerKeys) {
    const auto markerSet = _markers.value(line);
    if (markerSet.isEmpty()) {
      continue;
    }

    int marker = -1;
    for (int candidate : markerSet) {
      if (marker == -1 || candidate < marker) {
        marker = candidate;
      }
    }

    QColor background = _markerBackgroundColors.value(marker);
    if (!background.isValid()) {
      background = palette().alternateBase().color();
    }
    if (!background.isValid()) {
      continue;
    }

    const QTextBlock block = document()->findBlockByNumber(line);
    if (!block.isValid()) {
      continue;
    }

    QTextEdit::ExtraSelection selection;
    QTextCursor cursor(document());
    cursor.setPosition(block.position());
    cursor.clearSelection();
    selection.cursor = cursor;
    selection.format.setBackground(background);
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    extraSelections.append(selection);
  }

  if (_caretLineVisible) {
    QTextEdit::ExtraSelection selection;
    selection.format.setBackground(_caretLineBackground.isValid()
                                       ? _caretLineBackground
                                       : QColor(0, 0, 0, 0));
    selection.format.setProperty(QTextFormat::FullWidthSelection, true);
    selection.cursor = textCursor();
    selection.cursor.clearSelection();
    extraSelections.append(selection);
  }

  setExtraSelections(extraSelections);
}

void QsciScintilla::applyCurrentPalette() {
  if (_lineNumberArea) {
    _lineNumberArea->update();
  }

  QPalette palette = this->palette();
  if (_paper.isValid()) {
    palette.setColor(QPalette::Base, _paper);
    palette.setColor(QPalette::Window, _paper);
  }
  if (_color.isValid()) {
    palette.setColor(QPalette::Text, _color);
  }
  if (_selectionBackground.isValid()) {
    palette.setColor(QPalette::Highlight, _selectionBackground);
  }
  if (_selectionForeground.isValid()) {
    palette.setColor(QPalette::HighlightedText, _selectionForeground);
  }
  if (_marginsBackground.isValid()) {
    palette.setColor(QPalette::Button, _marginsBackground);
  }
  if (_marginsForeground.isValid()) {
    palette.setColor(QPalette::ButtonText, _marginsForeground);
  }
  setPalette(palette);
  viewport()->setPalette(palette);
  refreshCurrentLineHighlight();
}

void QsciScintilla::updateViewportMargins() {
  const int left = lineNumberAreaWidth();
  setViewportMargins(left, 0, 0, 0);
}

int QsciScintilla::lineNumberAreaWidth() const {
  int width = 0;
  const int digits = QString::number(std::max(1, blockCount())).size();
  if (_marginLineNumbers[0]) {
    const int digitWidth =
        fontMetrics().horizontalAdvance(QString(digits, QChar('9'))) + 12;
    width = digitWidth;
  }

  if (width == 0) {
    width = 14;
  }

  return width;
}

void QsciScintilla::lineNumberAreaPaintEvent(QPaintEvent *event) {
  if (!_lineNumberArea || !event) {
    return;
  }

  QPainter painter(_lineNumberArea);
  painter.fillRect(event->rect(),
                   _marginsBackground.isValid() ? _marginsBackground
                                                : palette().window().color());

  const QColor textColor =
      _marginsForeground.isValid() ? _marginsForeground : palette().text().color();

  const int areaWidth = _lineNumberArea->width();
  const int numberRightEdge = areaWidth - 4;

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());
  const int lineHeight = fontMetrics().height();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      if (_marginLineNumbers[0]) {
        const QString number = QString::number(blockNumber + 1);
        painter.setPen(textColor);
        painter.drawText(0, top, numberRightEdge, lineHeight, Qt::AlignRight,
                         number);
      }
    }

    block = block.next();
    ++blockNumber;
    top = bottom;
    bottom = top + static_cast<int>(blockBoundingRect(block).height());
  }
}

void QsciScintilla::updateLineNumberAreaWidth(int) {
  const int width = lineNumberAreaWidth();
  if (_lineNumberArea) {
    _lineNumberArea->setFixedWidth(width);
    _lineNumberArea->setVisible(width > 0);
  }
  setViewportMargins(width, 0, 0, 0);
}

void QsciScintilla::updateLineNumberArea(const QRect &rect, int dy) {
  if (!_lineNumberArea) {
    return;
  }

  if (dy != 0) {
    _lineNumberArea->scroll(0, dy);
  } else {
    _lineNumberArea->update(0, rect.y(), _lineNumberArea->width(), rect.height());
  }

  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

int QsciScintilla::lineFromPoint(const QPoint &point) const {
  QTextCursor cursor = cursorForPosition(point);
  return cursor.block().blockNumber();
}
