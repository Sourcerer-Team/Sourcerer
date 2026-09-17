#include "Qsci/qsciscintilla.h"

#include <tree_sitter/api.h>

extern "C" {
const TSLanguage *tree_sitter_bash(void);
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_c_sharp(void);
const TSLanguage *tree_sitter_cpp(void);
const TSLanguage *tree_sitter_css(void);
const TSLanguage *tree_sitter_haskell(void);
const TSLanguage *tree_sitter_html(void);
const TSLanguage *tree_sitter_java(void);
const TSLanguage *tree_sitter_javascript(void);
const TSLanguage *tree_sitter_json(void);
const TSLanguage *tree_sitter_ocaml(void);
const TSLanguage *tree_sitter_ocaml_interface(void);
const TSLanguage *tree_sitter_ocaml_type(void);
const TSLanguage *tree_sitter_powershell(void);
const TSLanguage *tree_sitter_python(void);
const TSLanguage *tree_sitter_rust(void);
const TSLanguage *tree_sitter_tsx(void);
const TSLanguage *tree_sitter_typescript(void);
}

#include <QFocusEvent>
#include <QFrame>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QResizeEvent>
#include <QScopedValueRollback>

#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextCharFormat>
#include <QTextFormat>

#include <algorithm>
#include <functional>

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

#define QEDITOR_DEFINE_LEXER(ClassName, LanguageId)                            \
  ClassName::ClassName(QObject *parent) : QsciLexer(parent, LanguageId) {}

QEDITOR_DEFINE_LEXER(QsciLexerCPP, "cpp")
QEDITOR_DEFINE_LEXER(QsciLexerPython, "python")
QEDITOR_DEFINE_LEXER(QsciLexerJavaScript, "javascript")
QEDITOR_DEFINE_LEXER(QsciLexerJava, "java")
QEDITOR_DEFINE_LEXER(QsciLexerTypeScript, "typescript")
QEDITOR_DEFINE_LEXER(QsciLexerTSX, "tsx")
QEDITOR_DEFINE_LEXER(QsciLexerCSharp, "csharp")
QEDITOR_DEFINE_LEXER(QsciLexerRust, "rust")
QEDITOR_DEFINE_LEXER(QsciLexerOCaml, "ocaml")
QEDITOR_DEFINE_LEXER(QsciLexerOCamlInterface, "ocaml_interface")
QEDITOR_DEFINE_LEXER(QsciLexerOCamlType, "ocaml_type")
QEDITOR_DEFINE_LEXER(QsciLexerHaskell, "haskell")
QEDITOR_DEFINE_LEXER(QsciLexerPowerShell, "powershell")
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
    if (_editor && event && event->button() == Qt::LeftButton) {
      const QPoint editorPoint = mapTo(_editor, event->position().toPoint());
      const int line = _editor->lineFromPoint(editorPoint);

      int margin = -1;
      int x = 0;
      for (int i = 0; i < 4; ++i) {
        int width = 0;
        if (i == 0 && _editor->_marginLineNumbers[0]) {
          const int digits =
              QString::number(std::max(1, _editor->blockCount())).size();
          width = _editor->fontMetrics().horizontalAdvance(
                      QString(digits, QChar('9'))) +
                  12;
        } else if (_editor->_marginWidths[i] > 0) {
          width = _editor->_marginWidths[i];
        }

        if (width <= 0) {
          continue;
        }

        if (event->position().x() >= x && event->position().x() < x + width) {
          margin = i;
          break;
        }
        x += width;
      }

      if (margin != -1) {
        emit _editor->marginClicked(margin, line, event->modifiers());
        event->accept();
        return;
      }
    }
    QWidget::mousePressEvent(event);
  }

private:
  QsciScintilla *_editor;
};

namespace {

const TSLanguage *treeSitterLanguageForLexer(const QsciLexer *lexer) {
  if (!lexer) {
    return nullptr;
  }

  const QString language = lexer->treeSitterLanguage();
  if (language.compare(QStringLiteral("bash"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_bash();
  }
  if (language.compare(QStringLiteral("c"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_c();
  }
  if (language.compare(QStringLiteral("csharp"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_c_sharp();
  }
  if (language.compare(QStringLiteral("cpp"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_cpp();
  }
  if (language.compare(QStringLiteral("css"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_css();
  }
  if (language.compare(QStringLiteral("haskell"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_haskell();
  }
  if (language.compare(QStringLiteral("html"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_html();
  }
  if (language.compare(QStringLiteral("java"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_java();
  }
  if (language.compare(QStringLiteral("javascript"), Qt::CaseInsensitive) ==
      0) {
    return tree_sitter_javascript();
  }
  if (language.compare(QStringLiteral("json"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_json();
  }
  if (language.compare(QStringLiteral("ocaml"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_ocaml();
  }
  if (language.compare(QStringLiteral("ocaml_interface"),
                       Qt::CaseInsensitive) == 0) {
    return tree_sitter_ocaml_interface();
  }
  if (language.compare(QStringLiteral("ocaml_type"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_ocaml_type();
  }
  if (language.compare(QStringLiteral("powershell"), Qt::CaseInsensitive) ==
      0) {
    return tree_sitter_powershell();
  }
  if (language.compare(QStringLiteral("python"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_python();
  }
  if (language.compare(QStringLiteral("rust"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_rust();
  }
  if (language.compare(QStringLiteral("tsx"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_tsx();
  }
  if (language.compare(QStringLiteral("typescript"), Qt::CaseInsensitive) ==
      0) {
    return tree_sitter_typescript();
  }

  return nullptr;
}

QTextCharFormat formatForTreeSitterNode(const char *nodeType) {
  QTextCharFormat format;
  const QString type = QString::fromUtf8(nodeType ? nodeType : "").toLower();

  if (type.contains(QStringLiteral("comment"))) {
    format.setForeground(QColor(92, 99, 112));
    format.setFontItalic(true);
  } else if (type.contains(QStringLiteral("string")) ||
             type.contains(QStringLiteral("heredoc")) ||
             type.contains(QStringLiteral("template_string")) ||
             type.contains(QStringLiteral("raw_string"))) {
    format.setForeground(QColor(152, 195, 121));
  } else if (type.contains(QStringLiteral("number")) ||
             type.contains(QStringLiteral("integer")) ||
             type.contains(QStringLiteral("float"))) {
    format.setForeground(QColor(209, 154, 102));
  } else if (type == QLatin1String("true") ||
             type == QLatin1String("false") ||
             type == QLatin1String("null") ||
             type == QLatin1String("nil") ||
             type == QLatin1String("none") ||
             type == QLatin1String("undefined")) {
    format.setForeground(QColor(198, 120, 221));
    format.setFontWeight(QFont::Bold);
  } else if (type.contains(QStringLiteral("escape"))) {
    format.setForeground(QColor(86, 182, 194));
  } else if (type.startsWith(QStringLiteral("preproc")) ||
             type == QLatin1String("directive") ||
             type == QLatin1String("decorator")) {
    format.setForeground(QColor(97, 175, 239));
    format.setFontWeight(QFont::Bold);
  } else {
    static const QSet<QString> keywordNodes = {
        QStringLiteral("as"),        QStringLiteral("await"),
        QStringLiteral("break"),     QStringLiteral("case"),
        QStringLiteral("catch"),     QStringLiteral("class"),
        QStringLiteral("const"),     QStringLiteral("continue"),
        QStringLiteral("def"),       QStringLiteral("delete"),
        QStringLiteral("do"),        QStringLiteral("else"),
        QStringLiteral("enum"),      QStringLiteral("export"),
        QStringLiteral("extends"),   QStringLiteral("final"),
        QStringLiteral("for"),       QStringLiteral("foreach"),
        QStringLiteral("from"),      QStringLiteral("fun"),
        QStringLiteral("function"),  QStringLiteral("if"),
        QStringLiteral("implements"), QStringLiteral("import"),
        QStringLiteral("include"),    QStringLiteral("in"),
        QStringLiteral("interface"),  QStringLiteral("let"),
        QStringLiteral("match"),     QStringLiteral("module"),
        QStringLiteral("mutable"),   QStringLiteral("namespace"),
        QStringLiteral("new"),       QStringLiteral("newtype"),
        QStringLiteral("of"),        QStringLiteral("package"),
        QStringLiteral("param"),     QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("record"),    QStringLiteral("rec"),
        QStringLiteral("return"),    QStringLiteral("sealed"),
        QStringLiteral("static"),    QStringLiteral("struct"),
        QStringLiteral("switch"),    QStringLiteral("template"),
        QStringLiteral("then"),      QStringLiteral("throw"),
        QStringLiteral("throws"),    QStringLiteral("trait"),
        QStringLiteral("try"),       QStringLiteral("type"),
        QStringLiteral("typedef"),   QStringLiteral("unsafe"),
        QStringLiteral("using"),     QStringLiteral("var"),
        QStringLiteral("virtual"),   QStringLiteral("where"),
        QStringLiteral("while"),     QStringLiteral("with"),
        QStringLiteral("yield")};
    if (keywordNodes.contains(type)) {
      format.setForeground(QColor(198, 120, 221));
      format.setFontWeight(QFont::Bold);
    }
  }

  return format;
}

} // namespace

QsciScintilla::QsciScintilla(QWidget *parent)
    : QPlainTextEdit(parent), _lexer(nullptr), _caretLineVisible(false),
      _indentationsUseTabs(false), _autoIndent(false),
      _indentationGuides(false), _foldStyle(NoFoldStyle),
      _braceMatch(NoBraceMatch), _lineNumberArea(nullptr),
      _treeSitterLanguage(nullptr), _updatingTreeSitterHighlighting(false),
      _treeSitterHighlightingEnabled(true), _parser(nullptr), _tree(nullptr) {
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
  connect(document(), &QTextDocument::contentsChanged, this,
          &QsciScintilla::updateTreeSitterHighlighting);
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
  _marginTexts.clear();

  QTextCursor cursor(document());
  cursor.beginEditBlock();
  cursor.select(QTextCursor::Document);
  cursor.removeSelectedText();
  cursor.insertText(text);
  cursor.endEditBlock();

  refreshCurrentLineHighlight();
}

void QsciScintilla::updateTreeSitterHighlighting() {
  if (_updatingTreeSitterHighlighting || !_treeSitterHighlightingEnabled) {
    return;
  }
  const QScopedValueRollback<bool> updateGuard(
      _updatingTreeSitterHighlighting, true);

  if (_tree) {
    ts_tree_delete(_tree);
    _tree = nullptr;
  }

  QTextCharFormat defaultFormat;
  defaultFormat.setForeground(_color.isValid() ? _color
                                                 : palette().text().color());
  defaultFormat.setFontWeight(QFont::Normal);
  defaultFormat.setFontItalic(false);
  QTextCursor documentCursor(document());
  documentCursor.select(QTextCursor::Document);
  documentCursor.setCharFormat(defaultFormat);

  if (!_parser || !_treeSitterLanguage) {
    return;
  }

  const QByteArray source = toPlainText().toUtf8();
  _tree = ts_parser_parse_string(_parser, nullptr, source.constData(),
                                  static_cast<uint32_t>(source.size()));
  if (!_tree) {
    return;
  }

  const int documentLength = document()->characterCount() - 1;
  const auto utf8ByteToUtf16 = [&source](uint32_t byteOffset) {
    const int boundedOffset = std::min(
        static_cast<int>(byteOffset), static_cast<int>(source.size()));
    return QString::fromUtf8(source.constData(), boundedOffset).size();
  };

  std::function<void(TSNode)> applyNode = [&](TSNode node) {
    const char *nodeType = ts_node_type(node);
    QTextCharFormat format = formatForTreeSitterNode(nodeType);

    if (format.hasProperty(QTextFormat::ForegroundBrush) &&
        format.foreground().color().isValid()) {
      const TSNode parent = ts_node_parent(node);
      if (QString::fromUtf8(nodeType ? nodeType : "") ==
              QLatin1String("string") && !ts_node_is_null(parent) &&
          QString::fromUtf8(ts_node_type(parent)) == QLatin1String("pair") &&
          ts_node_eq(ts_node_named_child(parent, 0), node)) {
        format.setForeground(QColor(97, 175, 239));
        format.setFontWeight(QFont::Bold);
      }

      const int start = utf8ByteToUtf16(ts_node_start_byte(node));
      const int end = utf8ByteToUtf16(ts_node_end_byte(node));
      if (start < end && start < documentLength) {
        QTextCursor cursor(document());
        cursor.setPosition(start);
        cursor.setPosition(std::min(end, documentLength),
                           QTextCursor::KeepAnchor);
        cursor.setCharFormat(format);
      }
    }

    const uint32_t childCount = ts_node_child_count(node);
    for (uint32_t index = 0; index < childCount; ++index) {
      applyNode(ts_node_child(node, index));
    }
  };

  applyNode(ts_tree_root_node(_tree));
}

QString QsciScintilla::text() const {
  return toPlainText();
}

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

void QsciScintilla::setTreeSitterHighlightingEnabled(bool enabled) {
  if (_treeSitterHighlightingEnabled == enabled) {
    return;
  }

  _treeSitterHighlightingEnabled = enabled;
  if (_treeSitterHighlightingEnabled) {
    updateTreeSitterHighlighting();
  }
}

void QsciScintilla::setLexer(QsciLexer *lexer) {
  if (_lexer == lexer) {
    return;
  }

  if (_lexer) {
    disconnect(_lexer, nullptr, this, nullptr);
  }

  _lexer = lexer;
  _treeSitterLanguage = treeSitterLanguageForLexer(_lexer);
  if (_treeSitterLanguage && _parser) {
    ts_parser_set_language(_parser, _treeSitterLanguage);
  }

  if (_lexer) {
    connect(_lexer, &QsciLexer::changed, this,
            &QsciScintilla::applyCurrentPalette);
    connect(_lexer, &QsciLexer::changed, this,
            &QsciScintilla::updateTreeSitterHighlighting);
  }

  applyCurrentPalette();
  updateTreeSitterHighlighting();
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
  palette.setColor(QPalette::Text,
                   _color.isValid() ? _color : palette.color(QPalette::Text));
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

void QsciScintilla::setMarginText(int line, int margin, const QString &text) {
  if (line < 0 || margin < 0 || margin >= 4) {
    return;
  }

  _marginTexts[line][margin] = text;
  if (_lineNumberArea) {
    _lineNumberArea->update();
  }
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

void QsciScintilla::setFolding(FoldStyle style) {
  _foldStyle = style;
}

void QsciScintilla::setFoldMarginColors(const QColor &foreground,
                                        const QColor &background) {
  _foldMarginForeground = foreground;
  _foldMarginBackground = background;
}

void QsciScintilla::setIndentationsUseTabs(bool useTabs) {
  _indentationsUseTabs = useTabs;
}

void QsciScintilla::setAutoIndent(bool enabled) {
  _autoIndent = enabled;
}

void QsciScintilla::setIndentationGuides(bool enabled) {
  _indentationGuides = enabled;
}

void QsciScintilla::setIndentationGuidesBackgroundColor(const QColor &color) {
  _indentGuidesBackground = color;
}

void QsciScintilla::setIndentationGuidesForegroundColor(const QColor &color) {
  _indentGuidesForeground = color;
}

void QsciScintilla::setBraceMatching(BraceMatch style) {
  _braceMatch = style;
}

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
  for (int i = 0; i < 4; ++i) {
    if (i == 0 && _marginLineNumbers[0]) {
      width +=
          fontMetrics().horizontalAdvance(QString(digits, QChar('9'))) + 12;
    } else if (_marginWidths[i] > 0) {
      width += _marginWidths[i];
    }
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
  painter.fillRect(event->rect(), _marginsBackground.isValid()
                                      ? _marginsBackground
                                      : palette().window().color());

  const QColor textColor = _marginsForeground.isValid()
                               ? _marginsForeground
                               : palette().text().color();
  const QColor markerColor = textColor;

  const int digits = QString::number(std::max(1, blockCount())).size();
  int marginWidths[4] = {0, 0, 0, 0};
  int marginOffsets[4] = {0, 0, 0, 0};
  int x = 0;
  for (int i = 0; i < 4; ++i) {
    if (i == 0 && _marginLineNumbers[0]) {
      marginWidths[i] =
          fontMetrics().horizontalAdvance(QString(digits, QChar('9'))) + 12;
    } else if (_marginWidths[i] > 0) {
      marginWidths[i] = _marginWidths[i];
    }
    marginOffsets[i] = x;
    x += marginWidths[i];
  }

  if (x == 0) {
    return;
  }

  QTextBlock block = firstVisibleBlock();
  int blockNumber = block.blockNumber();
  int top = static_cast<int>(
      blockBoundingGeometry(block).translated(contentOffset()).top());
  int bottom = top + static_cast<int>(blockBoundingRect(block).height());
  const int lineHeight = fontMetrics().height();

  while (block.isValid() && top <= event->rect().bottom()) {
    if (block.isVisible() && bottom >= event->rect().top()) {
      const int markers = markersAtLine(blockNumber);

      for (int margin = 0; margin < 4; ++margin) {
        const int marginWidth = marginWidths[margin];
        if (marginWidth <= 0) {
          continue;
        }

        const QRect marginRect(marginOffsets[margin], top, marginWidth,
                               lineHeight);
        painter.fillRect(marginRect,
                         _marginBackgroundColors.value(
                             margin, _marginsBackground.isValid()
                                         ? _marginsBackground
                                         : palette().window().color()));

        const auto lineMargins = _marginTexts.constFind(blockNumber);
        if (lineMargins != _marginTexts.constEnd()) {
          const auto textIt = lineMargins->constFind(margin);
          if (textIt != lineMargins->constEnd()) {
            if (!textIt->isEmpty()) {
              const Qt::Alignment alignment =
                  margin == 0 ? (Qt::AlignRight | Qt::AlignVCenter)
                              : (Qt::AlignLeft | Qt::AlignVCenter);
              painter.setPen(textColor);
              painter.drawText(marginRect.adjusted(4, 0, -4, 0), alignment,
                               *textIt);
            }
            continue;
          }
        }

        if (margin == 0 && _marginLineNumbers[0]) {
          const QString number = QString::number(blockNumber + 1);
          painter.setPen(textColor);
          painter.drawText(marginRect.adjusted(0, 0, -4, 0),
                           Qt::AlignRight | Qt::AlignVCenter, number);
          continue;
        }

        const int mask = _marginMarkerMasks[margin];
        const int visibleMarkers = markers & mask;
        if (visibleMarkers == 0) {
          continue;
        }

        int markerIndex = -1;
        for (int candidate : _markers.value(blockNumber)) {
          if (visibleMarkers & (1 << candidate)) {
            markerIndex = candidate;
            break;
          }
        }

        if (markerIndex < 0) {
          continue;
        }

        const int symbol = _markerSymbols.value(markerIndex, Background);
        const QRect symbolRect = marginRect.adjusted(3, 3, -3, -3);
        const QColor fillColor =
            _markerBackgroundColors.value(markerIndex, markerColor);
        const QColor strokeColor =
            _markerForegroundColors.value(markerIndex, markerColor);

        painter.setRenderHint(QPainter::Antialiasing, true);
        switch (symbol) {
        case BoxedPlus: {
          painter.setPen(QPen(strokeColor, 1.5));
          painter.setBrush(fillColor);
          painter.drawRoundedRect(symbolRect, 2, 2);
          painter.drawLine(symbolRect.center().x(), symbolRect.top() + 3,
                           symbolRect.center().x(), symbolRect.bottom() - 3);
          painter.drawLine(symbolRect.left() + 3, symbolRect.center().y(),
                           symbolRect.right() - 3, symbolRect.center().y());
          break;
        }
        case Background:
        default: {
          painter.fillRect(symbolRect, fillColor);
          painter.setPen(strokeColor);
          painter.drawRect(symbolRect);
          break;
        }
        }
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
    _lineNumberArea->update(0, rect.y(), _lineNumberArea->width(),
                            rect.height());
  }

  if (rect.contains(viewport()->rect())) {
    updateLineNumberAreaWidth(0);
  }
}

int QsciScintilla::lineFromPoint(const QPoint &point) const {
  QTextCursor cursor = cursorForPosition(point);
  return cursor.block().blockNumber();
}
