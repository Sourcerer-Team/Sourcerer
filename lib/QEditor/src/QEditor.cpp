#include "Qsci/qsciscintilla.h"

#include <tree_sitter/api.h>

extern "C" {
const TSLanguage *tree_sitter_bash(void);
const TSLanguage *tree_sitter_c(void);
const TSLanguage *tree_sitter_c_sharp(void);
const TSLanguage *tree_sitter_cpp(void);
const TSLanguage *tree_sitter_css(void);
const TSLanguage *tree_sitter_dart(void);
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
#include <QSyntaxHighlighter>
#include <QTimer>

#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextFormat>
#include <QTextLayout>

#include <algorithm>
#include <cstring>
#include <vector>

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

QsciLexerDart::QsciLexerDart(QObject *parent) : QsciLexerCPP(parent) {
  setTreeSitterLanguage(QStringLiteral("dart"));
}

// Applies precomputed tree-sitter formats as layout formats, so highlighting
// never modifies the document (no undo entries, no contentsChanged feedback).
class QEditorTreeSitterHighlighter final : public QSyntaxHighlighter {
public:
  using BlockFormats = QVector<QVector<QTextLayout::FormatRange>>;

  explicit QEditorTreeSitterHighlighter(QTextDocument *document)
      : QSyntaxHighlighter(document) {}

  void setBlockFormats(BlockFormats formats) {
    _blockFormats = std::move(formats);
  }
  void clearBlockFormats() { _blockFormats.clear(); }

protected:
  void highlightBlock(const QString &text) override {
    Q_UNUSED(text);

    const int blockNumber = currentBlock().blockNumber();
    if (blockNumber < 0 || blockNumber >= _blockFormats.size()) {
      return;
    }

    for (const auto &range : _blockFormats.at(blockNumber)) {
      setFormat(range.start, range.length, range.format);
    }
  }

private:
  BlockFormats _blockFormats;
};

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
  if (language.compare(QStringLiteral("dart"), Qt::CaseInsensitive) == 0) {
    return tree_sitter_dart();
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
  if (language.compare(QStringLiteral("ocaml_type"), Qt::CaseInsensitive) ==
      0) {
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
  } else if (type == QLatin1String("true") || type == QLatin1String("false") ||
             type == QLatin1String("null") || type == QLatin1String("nil") ||
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
        QStringLiteral("as"),         QStringLiteral("await"),
        QStringLiteral("break"),      QStringLiteral("case"),
        QStringLiteral("catch"),      QStringLiteral("class"),
        QStringLiteral("const"),      QStringLiteral("continue"),
        QStringLiteral("def"),        QStringLiteral("delete"),
        QStringLiteral("do"),         QStringLiteral("else"),
        QStringLiteral("enum"),       QStringLiteral("export"),
        QStringLiteral("extends"),    QStringLiteral("final"),
        QStringLiteral("for"),        QStringLiteral("foreach"),
        QStringLiteral("from"),       QStringLiteral("fun"),
        QStringLiteral("function"),   QStringLiteral("if"),
        QStringLiteral("implements"), QStringLiteral("import"),
        QStringLiteral("include"),    QStringLiteral("in"),
        QStringLiteral("interface"),  QStringLiteral("let"),
        QStringLiteral("match"),      QStringLiteral("module"),
        QStringLiteral("mutable"),    QStringLiteral("namespace"),
        QStringLiteral("new"),        QStringLiteral("newtype"),
        QStringLiteral("of"),         QStringLiteral("package"),
        QStringLiteral("param"),      QStringLiteral("private"),
        QStringLiteral("protected"),  QStringLiteral("public"),
        QStringLiteral("record"),     QStringLiteral("rec"),
        QStringLiteral("return"),     QStringLiteral("sealed"),
        QStringLiteral("static"),     QStringLiteral("struct"),
        QStringLiteral("switch"),     QStringLiteral("template"),
        QStringLiteral("then"),       QStringLiteral("throw"),
        QStringLiteral("throws"),     QStringLiteral("trait"),
        QStringLiteral("try"),        QStringLiteral("type"),
        QStringLiteral("typedef"),    QStringLiteral("unsafe"),
        QStringLiteral("using"),      QStringLiteral("var"),
        QStringLiteral("virtual"),    QStringLiteral("where"),
        QStringLiteral("while"),      QStringLiteral("with"),
        QStringLiteral("yield")};
    if (keywordNodes.contains(type)) {
      format.setForeground(QColor(198, 120, 221));
      format.setFontWeight(QFont::Bold);
    }
  }

  return format;
}

enum class StyledLanguage {
  None,
  CFamily,
  Dart,
  Html,
  // Only reached through <script>/<style> injections in HTML documents.
  EmbeddedJavaScript,
  EmbeddedCss
};

StyledLanguage styledLanguageFor(const TSLanguage *language) {
  if (!language) {
    return StyledLanguage::None;
  }
  if (language == tree_sitter_c() || language == tree_sitter_cpp()) {
    return StyledLanguage::CFamily;
  }
  if (language == tree_sitter_dart()) {
    return StyledLanguage::Dart;
  }
  if (language == tree_sitter_html()) {
    return StyledLanguage::Html;
  }
  return StyledLanguage::None;
}

bool typeEquals(const char *type, const char *expected) {
  return std::strcmp(type, expected) == 0;
}

bool isIdentifierToken(const char *type) {
  const auto isStart = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  };
  if (!isStart(type[0])) {
    return false;
  }
  for (const char *c = type + 1; *c; ++c) {
    if (!isStart(*c) && !(*c >= '0' && *c <= '9')) {
      return false;
    }
  }
  return true;
}

bool isOperatorToken(const char *type) {
  if (!type[0]) {
    return false;
  }
  for (const char *c = type; *c; ++c) {
    if (!std::strchr("!%&*+-./:<=>?^|~()[]{};,", *c)) {
      return false;
    }
  }
  return true;
}

int slashCommentStyle(const QByteArray &source, uint32_t startByte) {
  const auto at = [&source](uint32_t index) {
    return index < static_cast<uint32_t>(source.size()) ? source.at(index)
                                                         : '\0';
  };

  const char third = at(startByte + 2);
  if (at(startByte + 1) == '/') {
    const bool doc =
        third == '!' || (third == '/' && at(startByte + 3) != '/');
    return doc ? QsciLexerCPP::CommentDoc : QsciLexerCPP::CommentLine;
  }

  const bool doc = third == '!' || (third == '*' && at(startByte + 3) != '/');
  return doc ? QsciLexerCPP::CommentDoc : QsciLexerCPP::Comment;
}

// Maps a C/C++ tree-sitter node to a QsciLexerCPP style, or -1 when the node
// itself is not highlighted. Literal and comment nodes are styled as a whole,
// so their children don't need to be visited.
int cFamilyStyleForNode(TSNode node, const QByteArray &source, bool &descend) {
  descend = true;

  const char *type = ts_node_type(node);
  if (!type) {
    return -1;
  }

  if (ts_node_is_named(node)) {
    descend = false;
    if (typeEquals(type, "comment")) {
      return slashCommentStyle(source, ts_node_start_byte(node));
    }
    if (typeEquals(type, "string_literal") ||
        typeEquals(type, "system_lib_string")) {
      return QsciLexerCPP::DoubleQuotedString;
    }
    if (typeEquals(type, "char_literal")) {
      return QsciLexerCPP::SingleQuotedString;
    }
    if (typeEquals(type, "raw_string_literal")) {
      return QsciLexerCPP::RawString;
    }
    if (typeEquals(type, "number_literal")) {
      return QsciLexerCPP::Number;
    }
    if (typeEquals(type, "true") || typeEquals(type, "false") ||
        typeEquals(type, "null") || typeEquals(type, "nullptr") ||
        typeEquals(type, "this")) {
      return QsciLexerCPP::Keyword;
    }
    if (typeEquals(type, "primitive_type")) {
      return QsciLexerCPP::KeywordSet2;
    }
    if (typeEquals(type, "type_identifier")) {
      return QsciLexerCPP::GlobalClass;
    }
    if (typeEquals(type, "preproc_directive")) {
      return QsciLexerCPP::PreProcessor;
    }

    descend = true;
    return -1;
  }

  // Anonymous nodes are the grammar's literal tokens: directives, keywords
  // and punctuation.
  if (type[0] == '#' || typeEquals(type, "defined")) {
    return QsciLexerCPP::PreProcessor;
  }
  if (typeEquals(type, "unsigned") || typeEquals(type, "signed") ||
      typeEquals(type, "long") || typeEquals(type, "short")) {
    return QsciLexerCPP::KeywordSet2;
  }
  if (isIdentifierToken(type)) {
    return QsciLexerCPP::Keyword;
  }
  if (isOperatorToken(type)) {
    return QsciLexerCPP::Operator;
  }

  return -1;
}

// Maps a Dart tree-sitter node to a QsciLexerCPP style (QsciLexerDart shares
// the C++ style set), or -1 when the node itself is not highlighted.
int dartStyleForNode(TSNode node, const QByteArray &source, bool &descend) {
  descend = true;

  const char *type = ts_node_type(node);
  if (!type) {
    return -1;
  }

  if (ts_node_is_named(node)) {
    // Strings are visited so interpolations (${...}) can be reset to the
    // default color and highlighted as code.
    if (typeEquals(type, "string_literal")) {
      return QsciLexerCPP::DoubleQuotedString;
    }
    if (typeEquals(type, "template_substitution")) {
      return QsciLexerCPP::Default;
    }
    if (typeEquals(type, "identifier")) {
      const uint32_t start = ts_node_start_byte(node);
      const bool annotationName =
          start > 0 && start <= static_cast<uint32_t>(source.size()) &&
          source.at(static_cast<qsizetype>(start) - 1) == '@';
      return annotationName ? QsciLexerCPP::PreProcessor : -1;
    }

    descend = false;
    if (typeEquals(type, "comment")) {
      return slashCommentStyle(source, ts_node_start_byte(node));
    }
    if (typeEquals(type, "documentation_comment")) {
      return QsciLexerCPP::CommentDoc;
    }
    if (typeEquals(type, "decimal_integer_literal") ||
        typeEquals(type, "decimal_floating_point_literal") ||
        typeEquals(type, "hex_integer_literal") ||
        typeEquals(type, "symbol_literal")) {
      return QsciLexerCPP::Number;
    }
    if (typeEquals(type, "true") || typeEquals(type, "false") ||
        typeEquals(type, "null_literal") || typeEquals(type, "this") ||
        typeEquals(type, "super") || typeEquals(type, "inferred_type") ||
        typeEquals(type, "void_type") || typeEquals(type, "is_operator")) {
      return QsciLexerCPP::Keyword;
    }
    const size_t typeLength = std::strlen(type);
    const size_t builtinSuffixLength = std::strlen("_builtin");
    if (typeLength > builtinSuffixLength &&
        typeEquals(type + typeLength - builtinSuffixLength, "_builtin")) {
      return QsciLexerCPP::Keyword;
    }
    if (typeEquals(type, "type_identifier")) {
      return QsciLexerCPP::GlobalClass;
    }
    const size_t operatorSuffixLength = std::strlen("_operator");
    if (typeLength > operatorSuffixLength &&
        typeEquals(type + typeLength - operatorSuffixLength, "_operator")) {
      return QsciLexerCPP::Operator;
    }

    descend = true;
    return -1;
  }

  // Every identifier-like anonymous token in the Dart grammar is a (possibly
  // contextual) keyword.
  if (isIdentifierToken(type) || typeEquals(type, "async*") ||
      typeEquals(type, "sync*")) {
    return QsciLexerCPP::Keyword;
  }
  if (typeEquals(type, "@")) {
    return QsciLexerCPP::PreProcessor;
  }
  if (isOperatorToken(type)) {
    return QsciLexerCPP::Operator;
  }

  return -1;
}

// Maps an HTML tree-sitter node to a QsciLexerHTML style, or -1 when the node
// itself is not highlighted. Text and <script>/<style> bodies keep the default
// color.
int htmlStyleForNode(TSNode node, const QByteArray &source, bool &descend) {
  descend = true;

  const char *type = ts_node_type(node);
  if (!type) {
    return -1;
  }

  if (ts_node_is_named(node)) {
    descend = false;
    if (typeEquals(type, "comment")) {
      return QsciLexerHTML::HTMLComment;
    }
    if (typeEquals(type, "doctype") || typeEquals(type, "tag_name")) {
      return QsciLexerHTML::Tag;
    }
    if (typeEquals(type, "erroneous_end_tag_name")) {
      return QsciLexerHTML::UnknownTag;
    }
    if (typeEquals(type, "attribute_name")) {
      return QsciLexerHTML::Attribute;
    }
    if (typeEquals(type, "quoted_attribute_value")) {
      const uint32_t start = ts_node_start_byte(node);
      const bool singleQuoted =
          start < static_cast<uint32_t>(source.size()) &&
          source.at(static_cast<qsizetype>(start)) == '\'';
      return singleQuoted ? QsciLexerHTML::HTMLSingleQuotedString
                          : QsciLexerHTML::HTMLDoubleQuotedString;
    }
    if (typeEquals(type, "attribute_value")) {
      // Only unquoted values get here; quoted ones are styled as a whole.
      const uint32_t start = ts_node_start_byte(node);
      const uint32_t end = std::min(ts_node_end_byte(node),
                                    static_cast<uint32_t>(source.size()));
      bool numeric = start < end;
      for (uint32_t i = start; numeric && i < end; ++i) {
        const char c = source.at(static_cast<qsizetype>(i));
        numeric = (c >= '0' && c <= '9') || c == '.' || c == '%';
      }
      return numeric ? QsciLexerHTML::HTMLNumber
                     : QsciLexerHTML::HTMLDoubleQuotedString;
    }
    if (typeEquals(type, "entity")) {
      return QsciLexerHTML::Entity;
    }

    descend = true;
    return -1;
  }

  // Tag delimiters share the tag color.
  if (typeEquals(type, "<") || typeEquals(type, ">") ||
      typeEquals(type, "</") || typeEquals(type, "/>") ||
      typeEquals(type, "<!")) {
    return QsciLexerHTML::Tag;
  }

  return -1;
}

QByteArray nodeBytes(TSNode node, const QByteArray &source) {
  const auto size = static_cast<uint32_t>(source.size());
  const uint32_t start = std::min(ts_node_start_byte(node), size);
  const uint32_t end = std::min(ts_node_end_byte(node), size);
  return start < end ? source.mid(static_cast<qsizetype>(start),
                                  static_cast<qsizetype>(end - start))
                     : QByteArray();
}

bool startsWithSingleQuote(TSNode node, const QByteArray &source) {
  const uint32_t start = ts_node_start_byte(node);
  return start < static_cast<uint32_t>(source.size()) &&
         source.at(static_cast<qsizetype>(start)) == '\'';
}

// Returns the language of an HTML raw_text node based on its <script> or
// <style> element and that element's type attribute.
StyledLanguage embeddedLanguageFor(TSNode rawText, const QByteArray &source) {
  const TSNode element = ts_node_parent(rawText);
  if (ts_node_is_null(element)) {
    return StyledLanguage::None;
  }
  const bool script = typeEquals(ts_node_type(element), "script_element");
  if (!script && !typeEquals(ts_node_type(element), "style_element")) {
    return StyledLanguage::None;
  }

  QByteArray typeValue;
  const TSNode startTag = ts_node_named_child(element, 0);
  if (!ts_node_is_null(startTag) &&
      typeEquals(ts_node_type(startTag), "start_tag")) {
    const uint32_t count = ts_node_named_child_count(startTag);
    for (uint32_t i = 0; i < count; ++i) {
      const TSNode attribute = ts_node_named_child(startTag, i);
      if (!typeEquals(ts_node_type(attribute), "attribute")) {
        continue;
      }
      const TSNode name = ts_node_named_child(attribute, 0);
      if (ts_node_is_null(name) ||
          nodeBytes(name, source).toLower() != QByteArrayLiteral("type")) {
        continue;
      }
      TSNode value = ts_node_named_child(attribute, 1);
      if (!ts_node_is_null(value) &&
          typeEquals(ts_node_type(value), "quoted_attribute_value")) {
        value = ts_node_named_child(value, 0);
      }
      if (!ts_node_is_null(value)) {
        typeValue = nodeBytes(value, source).trimmed().toLower();
      }
      break;
    }
  }

  if (script) {
    const bool javaScript =
        typeValue.isEmpty() || typeValue == QByteArrayLiteral("module") ||
        typeValue.contains("javascript") || typeValue.contains("ecmascript") ||
        typeValue.contains("jsx") || typeValue.contains("babel");
    return javaScript ? StyledLanguage::EmbeddedJavaScript
                      : StyledLanguage::None;
  }
  return typeValue.isEmpty() || typeValue == QByteArrayLiteral("text/css")
             ? StyledLanguage::EmbeddedCss
             : StyledLanguage::None;
}

// Maps a JavaScript tree-sitter node to a QsciLexerHTML JavaScript style, or -1
// when the node itself is not highlighted.
int javaScriptStyleForNode(TSNode node, const QByteArray &source,
                           bool &descend) {
  descend = true;

  const char *type = ts_node_type(node);
  if (!type) {
    return -1;
  }

  if (ts_node_is_named(node)) {
    // Template strings are visited so ${...} can be reset to the default
    // color and highlighted as code.
    if (typeEquals(type, "template_string")) {
      return QsciLexerHTML::JavaScriptDoubleQuotedString;
    }
    if (typeEquals(type, "template_substitution")) {
      return QsciLexerHTML::Default;
    }

    descend = false;
    if (typeEquals(type, "comment")) {
      switch (slashCommentStyle(source, ts_node_start_byte(node))) {
      case QsciLexerCPP::CommentLine:
        return QsciLexerHTML::JavaScriptCommentLine;
      case QsciLexerCPP::CommentDoc:
        return QsciLexerHTML::JavaScriptCommentDoc;
      default:
        return QsciLexerHTML::JavaScriptComment;
      }
    }
    if (typeEquals(type, "html_comment")) {
      return QsciLexerHTML::JavaScriptComment;
    }
    if (typeEquals(type, "hash_bang_line")) {
      return QsciLexerHTML::JavaScriptCommentLine;
    }
    if (typeEquals(type, "string")) {
      return startsWithSingleQuote(node, source)
                 ? QsciLexerHTML::JavaScriptSingleQuotedString
                 : QsciLexerHTML::JavaScriptDoubleQuotedString;
    }
    if (typeEquals(type, "number")) {
      return QsciLexerHTML::JavaScriptNumber;
    }
    if (typeEquals(type, "regex")) {
      return QsciLexerHTML::JavaScriptRegex;
    }
    if (typeEquals(type, "true") || typeEquals(type, "false") ||
        typeEquals(type, "null") || typeEquals(type, "undefined") ||
        typeEquals(type, "this") || typeEquals(type, "super")) {
      return QsciLexerHTML::JavaScriptKeyword;
    }

    descend = true;
    return -1;
  }

  // Every identifier-like anonymous token in the JavaScript grammar is a
  // (possibly contextual) keyword.
  if (isIdentifierToken(type)) {
    return QsciLexerHTML::JavaScriptKeyword;
  }

  return -1;
}

// Maps a CSS tree-sitter node to a QsciLexerHTML style, or -1 when the node
// itself is not highlighted.
int cssStyleForNode(TSNode node, const QByteArray &source, bool &descend) {
  descend = true;

  const char *type = ts_node_type(node);
  if (!type) {
    return -1;
  }

  if (ts_node_is_named(node)) {
    descend = false;
    if (typeEquals(type, "comment") || typeEquals(type, "js_comment")) {
      return QsciLexerHTML::HTMLComment;
    }
    if (typeEquals(type, "tag_name") || typeEquals(type, "nesting_selector") ||
        typeEquals(type, "universal_selector")) {
      return QsciLexerHTML::Tag;
    }
    if (typeEquals(type, "class_name") || typeEquals(type, "id_name") ||
        typeEquals(type, "attribute_name")) {
      return QsciLexerHTML::Attribute;
    }
    if (typeEquals(type, "property_name") ||
        typeEquals(type, "feature_name")) {
      return QsciLexerHTML::Entity;
    }
    if (typeEquals(type, "string_value")) {
      return startsWithSingleQuote(node, source)
                 ? QsciLexerHTML::HTMLSingleQuotedString
                 : QsciLexerHTML::HTMLDoubleQuotedString;
    }
    if (typeEquals(type, "integer_value") || typeEquals(type, "float_value") ||
        typeEquals(type, "color_value")) {
      return QsciLexerHTML::HTMLNumber;
    }
    if (typeEquals(type, "at_keyword") || typeEquals(type, "important")) {
      return QsciLexerHTML::JavaScriptKeyword;
    }
    if (typeEquals(type, "function_name")) {
      return QsciLexerHTML::Script;
    }

    descend = true;
    return -1;
  }

  // At-rules (@media, @import, ...) and media query operators.
  if (type[0] == '@' || typeEquals(type, "and") || typeEquals(type, "or") ||
      typeEquals(type, "not") || typeEquals(type, "only")) {
    return QsciLexerHTML::JavaScriptKeyword;
  }

  return -1;
}

// Uses the color configured on the lexer for the style (theme aware) and falls
// back to the given dark palette color when the lexer has not been themed.
QTextCharFormat lexerStyleFormat(const QsciLexer *lexer, int style,
                                 const QColor &fallbackColor,
                                 bool fallbackBold, bool italic) {
  QTextCharFormat format;
  const QColor lexerColor = lexer ? lexer->color(style) : QColor();
  if (lexerColor.isValid()) {
    const QFont lexerFont = lexer->font(style);
    format.setForeground(lexerColor);
    format.setFontWeight(lexerFont.bold() ? QFont::Bold : QFont::Normal);
    format.setFontItalic(italic || lexerFont.italic());
  } else {
    format.setForeground(fallbackColor);
    format.setFontWeight(fallbackBold ? QFont::Bold : QFont::Normal);
    format.setFontItalic(italic);
  }

  return format;
}

QTextCharFormat formatForHtmlStyle(const QsciLexer *lexer, int style,
                                   const QColor &defaultColor) {
  switch (style) {
  case QsciLexerHTML::Default:
    return lexerStyleFormat(lexer, style, defaultColor, false, false);
  case QsciLexerHTML::Tag:
  case QsciLexerHTML::UnknownTag:
    return lexerStyleFormat(lexer, style, QColor(224, 108, 117), true, false);
  case QsciLexerHTML::Attribute:
    return lexerStyleFormat(lexer, style, QColor(209, 154, 102), false, false);
  case QsciLexerHTML::HTMLDoubleQuotedString:
  case QsciLexerHTML::HTMLSingleQuotedString:
  case QsciLexerHTML::JavaScriptDoubleQuotedString:
  case QsciLexerHTML::JavaScriptSingleQuotedString:
    return lexerStyleFormat(lexer, style, QColor(152, 195, 121), false, false);
  case QsciLexerHTML::HTMLNumber:
  case QsciLexerHTML::JavaScriptNumber:
    return lexerStyleFormat(lexer, style, QColor(209, 154, 102), false, false);
  case QsciLexerHTML::HTMLComment:
  case QsciLexerHTML::JavaScriptComment:
  case QsciLexerHTML::JavaScriptCommentLine:
  case QsciLexerHTML::JavaScriptCommentDoc:
    return lexerStyleFormat(lexer, style, QColor(92, 99, 112), false, true);
  case QsciLexerHTML::Entity:
    return lexerStyleFormat(lexer, style, QColor(86, 182, 194), false, false);
  case QsciLexerHTML::Script:
    return lexerStyleFormat(lexer, style, QColor(97, 175, 239), false, false);
  case QsciLexerHTML::JavaScriptKeyword:
    return lexerStyleFormat(lexer, style, QColor(198, 120, 221), true, false);
  case QsciLexerHTML::JavaScriptRegex:
    return lexerStyleFormat(lexer, style, QColor(255, 138, 101), false, false);
  default:
    return {};
  }
}

// Uses the colors configured on the lexer (theme aware) and falls back to a
// dark palette when the lexer has not been themed.
QTextCharFormat formatForCppStyle(const QsciLexer *lexer, int style,
                                  const QColor &defaultColor) {
  QColor fallbackColor;
  bool fallbackBold = false;
  bool italic = false;

  switch (style) {
  case QsciLexerCPP::Default:
    fallbackColor = defaultColor;
    break;
  case QsciLexerCPP::Comment:
  case QsciLexerCPP::CommentLine:
  case QsciLexerCPP::CommentDoc:
    fallbackColor = QColor(92, 99, 112);
    italic = true;
    break;
  case QsciLexerCPP::Keyword:
    fallbackColor = QColor(198, 120, 221);
    fallbackBold = true;
    break;
  case QsciLexerCPP::KeywordSet2:
  case QsciLexerCPP::Operator:
    fallbackColor = QColor(86, 182, 194);
    break;
  case QsciLexerCPP::DoubleQuotedString:
  case QsciLexerCPP::SingleQuotedString:
  case QsciLexerCPP::RawString:
    fallbackColor = QColor(152, 195, 121);
    break;
  case QsciLexerCPP::Number:
    fallbackColor = QColor(209, 154, 102);
    break;
  case QsciLexerCPP::PreProcessor:
    fallbackColor = QColor(97, 175, 239);
    break;
  case QsciLexerCPP::GlobalClass:
    fallbackColor = QColor(229, 192, 123);
    break;
  default:
    return {};
  }

  return lexerStyleFormat(lexer, style, fallbackColor, fallbackBold, italic);
}

} // namespace

QsciScintilla::QsciScintilla(QWidget *parent)
    : QPlainTextEdit(parent), _lexer(nullptr), _caretLineVisible(false),
      _indentationsUseTabs(false), _autoIndent(false),
      _indentationGuides(false), _foldStyle(NoFoldStyle),
      _braceMatch(NoBraceMatch), _lineNumberArea(nullptr),
      _treeSitterLanguage(nullptr), _updatingTreeSitterHighlighting(false),
      _treeSitterHighlightingEnabled(true), _parser(nullptr), _tree(nullptr),
      _highlighter(nullptr), _highlightTimer(new QTimer(this)) {
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
  _highlightTimer->setSingleShot(true);
  _highlightTimer->setInterval(0);
  connect(_highlightTimer, &QTimer::timeout, this,
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

void QsciScintilla::scheduleTreeSitterHighlighting() {
  // Lexer configuration emits changed() once per style; coalesce them into a
  // single re-highlight.
  _highlightTimer->start();
}

void QsciScintilla::updateTreeSitterHighlighting() {
  if (_updatingTreeSitterHighlighting) {
    return;
  }
  const QScopedValueRollback<bool> updateGuard(_updatingTreeSitterHighlighting,
                                               true);

  _highlightTimer->stop();

  if (_tree) {
    ts_tree_delete(_tree);
    _tree = nullptr;
  }

  if (!_treeSitterHighlightingEnabled) {
    // Drop formats so blocks changed while disabled are not painted with
    // stale ranges.
    if (_highlighter) {
      _highlighter->clearBlockFormats();
    }
    return;
  }

  if (!_parser || !_treeSitterLanguage) {
    if (_highlighter) {
      _highlighter->clearBlockFormats();
      _highlighter->rehighlight();
    }
    return;
  }

  if (!_highlighter) {
    _highlighter = new QEditorTreeSitterHighlighter(document());
  }

  const QByteArray source = toPlainText().toUtf8();
  _tree = ts_parser_parse_string(_parser, nullptr, source.constData(),
                                 static_cast<uint32_t>(source.size()));
  if (!_tree) {
    _highlighter->clearBlockFormats();
    _highlighter->rehighlight();
    return;
  }

  const int documentLength = document()->characterCount() - 1;

  // tree-sitter reports UTF-8 byte offsets, the document uses UTF-16.
  std::vector<int> utf16Offsets(static_cast<size_t>(source.size()) + 1, 0);
  int utf16Offset = 0;
  for (int byteIndex = 0; byteIndex < source.size(); ++byteIndex) {
    const auto byte = static_cast<unsigned char>(source.at(byteIndex));
    utf16Offsets[static_cast<size_t>(byteIndex)] = utf16Offset;
    if ((byte & 0xC0) != 0x80) {
      utf16Offset += byte >= 0xF0 ? 2 : 1;
    }
  }
  utf16Offsets[static_cast<size_t>(source.size())] = utf16Offset;

  const auto utf8ByteToUtf16 = [&utf16Offsets](uint32_t byteOffset) {
    const size_t index =
        std::min(static_cast<size_t>(byteOffset), utf16Offsets.size() - 1);
    return utf16Offsets[index];
  };

  std::vector<int> blockStarts;
  blockStarts.reserve(static_cast<size_t>(document()->blockCount()));
  for (QTextBlock block = document()->begin(); block.isValid();
       block = block.next()) {
    blockStarts.push_back(block.position());
  }
  const int blockCount = static_cast<int>(blockStarts.size());

  QEditorTreeSitterHighlighter::BlockFormats blockFormats(blockCount);

  const auto addRange = [&](int start, int end,
                            const QTextCharFormat &format) {
    start = std::max(0, start);
    end = std::min(end, documentLength);
    if (start >= end || blockCount == 0) {
      return;
    }

    int block = static_cast<int>(std::upper_bound(blockStarts.begin(),
                                                  blockStarts.end(), start) -
                                 blockStarts.begin()) -
                1;
    block = std::max(block, 0);

    // Multi-line nodes (block comments, raw strings) are split per block.
    for (; block < blockCount && blockStarts[block] < end; ++block) {
      const int blockStart = blockStarts[block];
      const int blockEnd = block + 1 < blockCount
                               ? blockStarts[block + 1] - 1
                               : documentLength;
      const int rangeStart = std::max(start, blockStart);
      const int rangeEnd = std::min(end, blockEnd);
      if (rangeStart < rangeEnd) {
        QTextLayout::FormatRange range;
        range.start = rangeStart - blockStart;
        range.length = rangeEnd - rangeStart;
        range.format = format;
        blockFormats[block].append(range);
      }
    }
  };

  const StyledLanguage documentLanguage =
      styledLanguageFor(_treeSitterLanguage);
  const QColor defaultColor =
      _color.isValid() ? _color : palette().text().color();
  // A document uses either the QsciLexerCPP or the QsciLexerHTML style ids
  // (HTML shares its ids with the embedded languages), so caching by id is
  // unambiguous.
  const bool cppStyles = documentLanguage == StyledLanguage::CFamily ||
                         documentLanguage == StyledLanguage::Dart;
  QHash<int, QTextCharFormat> styleFormats;

  const auto styledFormat = [&](int style) -> QTextCharFormat {
    if (style < 0) {
      return QTextCharFormat();
    }
    auto it = styleFormats.find(style);
    if (it == styleFormats.end()) {
      it = styleFormats.insert(
          style, cppStyles
                     ? formatForCppStyle(_lexer, style, defaultColor)
                     : formatForHtmlStyle(_lexer, style, defaultColor));
    }
    return it.value();
  };

  const auto formatForNode = [&](StyledLanguage language, TSNode node,
                                 bool &descend) -> QTextCharFormat {
    switch (language) {
    case StyledLanguage::CFamily:
      return styledFormat(cFamilyStyleForNode(node, source, descend));
    case StyledLanguage::Dart:
      return styledFormat(dartStyleForNode(node, source, descend));
    case StyledLanguage::Html:
      return styledFormat(htmlStyleForNode(node, source, descend));
    case StyledLanguage::EmbeddedJavaScript:
      return styledFormat(javaScriptStyleForNode(node, source, descend));
    case StyledLanguage::EmbeddedCss:
      return styledFormat(cssStyleForNode(node, source, descend));
    case StyledLanguage::None:
      break;
    }

    descend = true;
    const char *nodeType = ts_node_type(node);
    QTextCharFormat format = formatForTreeSitterNode(nodeType);
    if (format.hasProperty(QTextFormat::ForegroundBrush)) {
      const TSNode parent = ts_node_parent(node);
      if (nodeType && typeEquals(nodeType, "string") &&
          !ts_node_is_null(parent) &&
          typeEquals(ts_node_type(parent), "pair") &&
          ts_node_eq(ts_node_named_child(parent, 0), node)) {
        format.setForeground(QColor(97, 175, 239));
        format.setFontWeight(QFont::Bold);
      }
    }
    return format;
  };

  struct Injection {
    StyledLanguage language;
    TSRange range;
  };
  std::vector<Injection> injections;

  // Pre-order walk: parents are added before children, so child ranges take
  // precedence when they overlap.
  const auto walk = [&](TSNode root, StyledLanguage language) {
    TSTreeCursor treeCursor = ts_tree_cursor_new(root);
    for (;;) {
      const TSNode node = ts_tree_cursor_current_node(&treeCursor);
      bool descend = true;
      const QTextCharFormat format = formatForNode(language, node, descend);
      if (format.hasProperty(QTextFormat::ForegroundBrush) &&
          format.foreground().color().isValid()) {
        addRange(utf8ByteToUtf16(ts_node_start_byte(node)),
                 utf8ByteToUtf16(ts_node_end_byte(node)), format);
      }

      if (language == StyledLanguage::Html &&
          typeEquals(ts_node_type(node), "raw_text")) {
        const StyledLanguage embedded = embeddedLanguageFor(node, source);
        if (embedded != StyledLanguage::None) {
          injections.push_back(
              {embedded,
               {ts_node_start_point(node), ts_node_end_point(node),
                ts_node_start_byte(node), ts_node_end_byte(node)}});
        }
      }

      if (descend && ts_tree_cursor_goto_first_child(&treeCursor)) {
        continue;
      }

      bool finished = false;
      while (!ts_tree_cursor_goto_next_sibling(&treeCursor)) {
        if (!ts_tree_cursor_goto_parent(&treeCursor)) {
          finished = true;
          break;
        }
      }
      if (finished) {
        break;
      }
    }
    ts_tree_cursor_delete(&treeCursor);
  };

  walk(ts_tree_root_node(_tree), documentLanguage);

  // <script> and <style> bodies are parsed separately with their own grammar,
  // restricted to the body's range. Their ranges are added after the HTML
  // ranges, so they take precedence.
  if (!injections.empty()) {
    TSParser *injectionParser = ts_parser_new();
    for (const Injection &injection : injections) {
      const TSLanguage *language =
          injection.language == StyledLanguage::EmbeddedJavaScript
              ? tree_sitter_javascript()
              : tree_sitter_css();
      if (!ts_parser_set_language(injectionParser, language) ||
          !ts_parser_set_included_ranges(injectionParser, &injection.range,
                                         1)) {
        continue;
      }
      TSTree *injectionTree = ts_parser_parse_string(
          injectionParser, nullptr, source.constData(),
          static_cast<uint32_t>(source.size()));
      if (!injectionTree) {
        continue;
      }
      walk(ts_tree_root_node(injectionTree), injection.language);
      ts_tree_delete(injectionTree);
    }
    ts_parser_delete(injectionParser);
  }

  _highlighter->setBlockFormats(std::move(blockFormats));
  _highlighter->rehighlight();
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
            &QsciScintilla::scheduleTreeSitterHighlighting);
    connect(_lexer, &QObject::destroyed, this, [this]() {
      _lexer = nullptr;
      _treeSitterLanguage = nullptr;
      scheduleTreeSitterHighlighting();
    });
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
