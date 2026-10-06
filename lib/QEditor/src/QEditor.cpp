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
#include <initializer_list>
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

#define QEDITOR_DEFINE_DERIVED_LEXER(ClassName, BaseClass, LanguageId)        \
  ClassName::ClassName(QObject *parent) : BaseClass(parent) {                  \
    setTreeSitterLanguage(QStringLiteral(LanguageId));                         \
  }

QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerDart, QsciLexerCPP, "dart")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerJava, QsciLexerCPP, "java")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerCSharp, QsciLexerCPP, "csharp")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerRust, QsciLexerCPP, "rust")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerOCaml, QsciLexerCPP, "ocaml")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerOCamlInterface, QsciLexerCPP,
                             "ocaml_interface")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerOCamlType, QsciLexerCPP, "ocaml_type")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerHaskell, QsciLexerCPP, "haskell")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerPowerShell, QsciLexerCPP, "powershell")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerTypeScript, QsciLexerJavaScript,
                             "typescript")
QEDITOR_DEFINE_DERIVED_LEXER(QsciLexerTSX, QsciLexerJavaScript, "tsx")

#undef QEDITOR_DEFINE_DERIVED_LEXER

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

// Semantic kinds produced by the tree-sitter classifiers. Each lexer style set
// maps them onto its own style ids, so the colors configured on the lexer
// (theme) are used.
enum class Token {
  None,
  Default,
  Comment,
  CommentLine,
  CommentDoc,
  String,
  SingleQuotedString,
  TripleQuotedString,
  RawString,
  Number,
  Keyword,
  BuiltinType,
  Type,
  Operator,
  Preprocessor,
  Regex,
  Escape,
  FunctionName,
  Variable,
  Property,
  Tag,
  UnknownTag,
  Attribute,
  Entity,
  ClassSelector,
  IdSelector,
  PseudoClass,
  PseudoElement,
  Value,
  Important,
  AtRule
};

enum class StyledLanguage {
  None,
  CFamily,
  Dart,
  Html,
  Css,
  JavaScript,
  Python,
  Bash,
  Json,
  Java,
  CSharp,
  Rust,
  OCaml,
  Haskell,
  PowerShell,
  // Only reached through <script>/<style> injections in HTML documents.
  EmbeddedJavaScript,
  EmbeddedCss
};

// The lexer style ids a language is highlighted with.
enum class StyleSet {
  Cpp,
  JavaScript,
  Python,
  Bash,
  Json,
  Css,
  // QsciLexerHTML ids for markup and embedded CSS.
  Html,
  // QsciLexerHTML ids for embedded JavaScript.
  HtmlScript
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
  if (language == tree_sitter_css()) {
    return StyledLanguage::Css;
  }
  if (language == tree_sitter_javascript() ||
      language == tree_sitter_typescript() || language == tree_sitter_tsx()) {
    return StyledLanguage::JavaScript;
  }
  if (language == tree_sitter_python()) {
    return StyledLanguage::Python;
  }
  if (language == tree_sitter_bash()) {
    return StyledLanguage::Bash;
  }
  if (language == tree_sitter_json()) {
    return StyledLanguage::Json;
  }
  if (language == tree_sitter_java()) {
    return StyledLanguage::Java;
  }
  if (language == tree_sitter_c_sharp()) {
    return StyledLanguage::CSharp;
  }
  if (language == tree_sitter_rust()) {
    return StyledLanguage::Rust;
  }
  if (language == tree_sitter_ocaml() ||
      language == tree_sitter_ocaml_interface() ||
      language == tree_sitter_ocaml_type()) {
    return StyledLanguage::OCaml;
  }
  if (language == tree_sitter_haskell()) {
    return StyledLanguage::Haskell;
  }
  if (language == tree_sitter_powershell()) {
    return StyledLanguage::PowerShell;
  }
  return StyledLanguage::None;
}

// Must match the lexer created for the language (see qscilexer.h): languages
// without a style set of their own use the C++ one.
StyleSet styleSetFor(StyledLanguage language) {
  switch (language) {
  case StyledLanguage::JavaScript:
    return StyleSet::JavaScript;
  case StyledLanguage::Python:
    return StyleSet::Python;
  case StyledLanguage::Bash:
    return StyleSet::Bash;
  case StyledLanguage::Json:
    return StyleSet::Json;
  case StyledLanguage::Css:
    return StyleSet::Css;
  case StyledLanguage::Html:
  case StyledLanguage::EmbeddedCss:
    return StyleSet::Html;
  case StyledLanguage::EmbeddedJavaScript:
    return StyleSet::HtmlScript;
  case StyledLanguage::None:
  case StyledLanguage::CFamily:
  case StyledLanguage::Dart:
  case StyledLanguage::Java:
  case StyledLanguage::CSharp:
  case StyledLanguage::Rust:
  case StyledLanguage::OCaml:
  case StyledLanguage::Haskell:
  case StyledLanguage::PowerShell:
    break;
  }
  return StyleSet::Cpp;
}

// A node as seen by the classifiers, with the context of its position in the
// tree. Strings are never null; missing values are empty.
struct NodeContext {
  TSNode node;
  const char *type;
  const char *field;
  const char *parent;
  const char *grandparent;
  bool named;
};

bool typeEquals(const char *type, const char *expected) {
  return std::strcmp(type, expected) == 0;
}

bool typeIn(const char *type, std::initializer_list<const char *> types) {
  for (const char *candidate : types) {
    if (typeEquals(type, candidate)) {
      return true;
    }
  }
  return false;
}

bool typeContains(const char *type, const char *part) {
  return std::strstr(type, part) != nullptr;
}

bool typeEndsWith(const char *type, const char *suffix) {
  const size_t typeLength = std::strlen(type);
  const size_t suffixLength = std::strlen(suffix);
  return typeLength >= suffixLength &&
         typeEquals(type + typeLength - suffixLength, suffix);
}

bool isAsciiLetter(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isAsciiDigit(char c) { return c >= '0' && c <= '9'; }

bool isIdentifierToken(const char *type) {
  if (!isAsciiLetter(type[0]) && type[0] != '_') {
    return false;
  }
  for (const char *c = type + 1; *c; ++c) {
    if (!isAsciiLetter(*c) && !isAsciiDigit(*c) && *c != '_') {
      return false;
    }
  }
  return true;
}

// Anonymous word tokens are keywords, including multi-word ones such as
// "is not" or "macro_rules!". Single letters are excluded: they are
// expansion operators (bash) or quasi-quoter names (Haskell).
bool isKeywordToken(const char *type) {
  if ((!isAsciiLetter(type[0]) && type[0] != '_') || !type[1]) {
    return false;
  }
  for (const char *c = type + 1; *c; ++c) {
    if (!isAsciiLetter(*c) && !isAsciiDigit(*c) && !std::strchr("_ -!", *c)) {
      return false;
    }
  }
  return true;
}

bool isTokenOf(const char *type, const char *characters) {
  if (!type[0]) {
    return false;
  }
  for (const char *c = type; *c; ++c) {
    if (!std::strchr(characters, *c)) {
      return false;
    }
  }
  return true;
}

bool isOperatorToken(const char *type) {
  return isTokenOf(type, "!%&*+-./:<=>?^|~()[]{};,");
}

char byteAt(const QByteArray &source, uint32_t index) {
  return index < static_cast<uint32_t>(source.size())
             ? source.at(static_cast<qsizetype>(index))
             : '\0';
}

bool startsWithSingleQuote(TSNode node, const QByteArray &source) {
  return byteAt(source, ts_node_start_byte(node)) == '\'';
}

Token commentTokenAt(const QByteArray &source, uint32_t start) {
  const char first = byteAt(source, start);
  const char second = byteAt(source, start + 1);
  const char third = byteAt(source, start + 2);
  const char fourth = byteAt(source, start + 3);

  if (first == '/' && second == '/') {
    const bool doc = third == '!' || (third == '/' && fourth != '/');
    return doc ? Token::CommentDoc : Token::CommentLine;
  }
  if (first == '/' && second == '*') {
    const bool doc = third == '!' || (third == '*' && fourth != '/');
    return doc ? Token::CommentDoc : Token::Comment;
  }
  if (first == '(' && second == '*') {
    return third == '*' && fourth != ')' ? Token::CommentDoc : Token::Comment;
  }
  if (first == '#' || (first == '-' && second == '-')) {
    return Token::CommentLine;
  }
  return Token::Comment;
}

Token stringTokenAt(const char *type, const QByteArray &source,
                    uint32_t start) {
  if (typeEquals(type, "raw_string_literal")) {
    return Token::RawString;
  }

  // Skip prefixes such as r, b, f, u, @ and $.
  uint32_t quoteIndex = start;
  while (isAsciiLetter(byteAt(source, quoteIndex)) ||
         byteAt(source, quoteIndex) == '@' ||
         byteAt(source, quoteIndex) == '$') {
    ++quoteIndex;
  }
  const char quote = byteAt(source, quoteIndex);
  if (quote != '"' && quote != '\'') {
    return Token::String;
  }
  if (byteAt(source, quoteIndex + 1) == quote &&
      byteAt(source, quoteIndex + 2) == quote) {
    return Token::TripleQuotedString;
  }
  return quote == '\'' ? Token::SingleQuotedString : Token::String;
}

// Classifies a C/C++ node. Literal and comment nodes are styled as a whole,
// so their children don't need to be visited.
Token cFamilyTokenForNode(const NodeContext &context, const QByteArray &source,
                          bool &descend) {
  descend = true;
  const char *type = context.type;

  if (context.named) {
    descend = false;
    if (typeEquals(type, "comment")) {
      return commentTokenAt(source, ts_node_start_byte(context.node));
    }
    if (typeEquals(type, "string_literal") ||
        typeEquals(type, "system_lib_string")) {
      return Token::String;
    }
    if (typeEquals(type, "char_literal")) {
      return Token::SingleQuotedString;
    }
    if (typeEquals(type, "raw_string_literal")) {
      return Token::RawString;
    }
    if (typeEquals(type, "number_literal")) {
      return Token::Number;
    }
    if (typeIn(type, {"true", "false", "null", "nullptr", "this"})) {
      return Token::Keyword;
    }
    if (typeEquals(type, "primitive_type")) {
      return Token::BuiltinType;
    }
    if (typeEquals(type, "type_identifier")) {
      return Token::Type;
    }
    if (typeEquals(type, "preproc_directive")) {
      return Token::Preprocessor;
    }

    descend = true;
    return Token::None;
  }

  // Anonymous nodes are the grammar's literal tokens: directives, keywords
  // and punctuation.
  if (type[0] == '#' || typeEquals(type, "defined")) {
    return Token::Preprocessor;
  }
  if (typeIn(type, {"unsigned", "signed", "long", "short"})) {
    return Token::BuiltinType;
  }
  if (isIdentifierToken(type)) {
    return Token::Keyword;
  }
  if (isOperatorToken(type)) {
    return Token::Operator;
  }

  return Token::None;
}

// Classifies a Dart node.
Token dartTokenForNode(const NodeContext &context, const QByteArray &source,
                       bool &descend) {
  descend = true;
  const char *type = context.type;

  if (context.named) {
    // Strings are visited so interpolations (${...}) can be reset to the
    // default color and highlighted as code.
    if (typeEquals(type, "string_literal")) {
      return Token::String;
    }
    if (typeEquals(type, "template_substitution")) {
      return Token::Default;
    }
    if (typeEquals(type, "identifier")) {
      const uint32_t start = ts_node_start_byte(context.node);
      const bool annotationName = start > 0 && byteAt(source, start - 1) == '@';
      return annotationName ? Token::Preprocessor : Token::None;
    }

    descend = false;
    if (typeEquals(type, "comment")) {
      return commentTokenAt(source, ts_node_start_byte(context.node));
    }
    if (typeEquals(type, "documentation_comment")) {
      return Token::CommentDoc;
    }
    if (typeIn(type, {"decimal_integer_literal", "decimal_floating_point_literal",
                      "hex_integer_literal", "symbol_literal"})) {
      return Token::Number;
    }
    if (typeIn(type, {"true", "false", "null_literal", "this", "super",
                      "inferred_type", "void_type", "is_operator"}) ||
        typeEndsWith(type, "_builtin")) {
      return Token::Keyword;
    }
    if (typeEquals(type, "type_identifier")) {
      return Token::Type;
    }
    if (typeEndsWith(type, "_operator")) {
      return Token::Operator;
    }

    descend = true;
    return Token::None;
  }

  // Every identifier-like anonymous token in the Dart grammar is a (possibly
  // contextual) keyword.
  if (isIdentifierToken(type) || typeIn(type, {"async*", "sync*"})) {
    return Token::Keyword;
  }
  if (typeEquals(type, "@")) {
    return Token::Preprocessor;
  }
  if (isOperatorToken(type)) {
    return Token::Operator;
  }

  return Token::None;
}

// Classifies an HTML node. Text and <script>/<style> bodies keep the default
// color.
Token htmlTokenForNode(const NodeContext &context, const QByteArray &source,
                       bool &descend) {
  descend = true;
  const char *type = context.type;

  if (context.named) {
    descend = false;
    if (typeEquals(type, "comment")) {
      return Token::Comment;
    }
    if (typeEquals(type, "doctype") || typeEquals(type, "tag_name")) {
      return Token::Tag;
    }
    if (typeEquals(type, "erroneous_end_tag_name")) {
      return Token::UnknownTag;
    }
    if (typeEquals(type, "attribute_name")) {
      return Token::Attribute;
    }
    if (typeEquals(type, "quoted_attribute_value")) {
      return startsWithSingleQuote(context.node, source)
                 ? Token::SingleQuotedString
                 : Token::String;
    }
    if (typeEquals(type, "attribute_value")) {
      // Only unquoted values get here; quoted ones are styled as a whole.
      const uint32_t start = ts_node_start_byte(context.node);
      const uint32_t end = std::min(ts_node_end_byte(context.node),
                                    static_cast<uint32_t>(source.size()));
      bool numeric = start < end;
      for (uint32_t i = start; numeric && i < end; ++i) {
        const char c = byteAt(source, i);
        numeric = isAsciiDigit(c) || c == '.' || c == '%';
      }
      return numeric ? Token::Number : Token::String;
    }
    if (typeEquals(type, "entity")) {
      return Token::Entity;
    }

    descend = true;
    return Token::None;
  }

  // Tag delimiters share the tag color.
  if (typeIn(type, {"<", ">", "</", "/>", "<!"})) {
    return Token::Tag;
  }

  return Token::None;
}

// Classifies a CSS node, standalone or embedded in HTML.
Token cssTokenForNode(const NodeContext &context, const QByteArray &source,
                      bool &descend) {
  descend = true;
  const char *type = context.type;

  if (context.named) {
    descend = false;
    if (typeEquals(type, "comment") || typeEquals(type, "js_comment")) {
      return Token::Comment;
    }
    if (typeEquals(type, "tag_name")) {
      return typeEquals(context.parent, "pseudo_element_selector")
                 ? Token::PseudoElement
                 : Token::Tag;
    }
    if (typeEquals(type, "nesting_selector") ||
        typeEquals(type, "universal_selector")) {
      return Token::Tag;
    }
    if (typeEquals(type, "class_name")) {
      return typeEquals(context.parent, "pseudo_class_selector")
                 ? Token::PseudoClass
                 : Token::ClassSelector;
    }
    if (typeEquals(type, "id_name")) {
      return Token::IdSelector;
    }
    if (typeEquals(type, "attribute_name")) {
      return Token::Attribute;
    }
    if (typeEquals(type, "property_name") ||
        typeEquals(type, "feature_name")) {
      return Token::Property;
    }
    if (typeEquals(type, "string_value")) {
      return startsWithSingleQuote(context.node, source)
                 ? Token::SingleQuotedString
                 : Token::String;
    }
    if (typeIn(type, {"integer_value", "float_value", "color_value"})) {
      return Token::Number;
    }
    if (typeEquals(type, "plain_value")) {
      return Token::Value;
    }
    if (typeEquals(type, "at_keyword")) {
      return Token::AtRule;
    }
    if (typeEquals(type, "important")) {
      return Token::Important;
    }
    if (typeEquals(type, "function_name")) {
      return Token::FunctionName;
    }

    descend = true;
    return Token::None;
  }

  // At-rules (@media, @import, ...) and media query operators.
  if (type[0] == '@') {
    return Token::AtRule;
  }
  if (typeIn(type, {"and", "or", "not", "only"})) {
    return Token::Keyword;
  }
  if (isOperatorToken(type)) {
    return Token::Operator;
  }

  return Token::None;
}

bool isIdentifierNode(const char *type) {
  return typeIn(type, {"identifier", "type_identifier", "simple_name"});
}

// Classifies a node of the remaining grammars (Java, C#, Rust, OCaml,
// Haskell, PowerShell, Python, Bash, JSON and JavaScript/TypeScript), which
// share most of their node naming conventions.
Token genericTokenForNode(StyledLanguage language, const NodeContext &context,
                          const QByteArray &source, bool &descend) {
  descend = true;
  const char *type = context.type;
  const char *field = context.field;
  const char *parent = context.parent;

  if (!context.named) {
    // C# preprocessor directives (#if, #region, ...).
    if (type[0] == '#' && isAsciiLetter(type[1])) {
      return Token::Preprocessor;
    }
    if ((typeEquals(type, "@") &&
         typeIn(parent, {"decorator", "annotation", "marker_annotation"})) ||
        (typeEquals(type, "!") && typeEquals(parent, "macro_invocation"))) {
      return Token::Preprocessor;
    }
    // PowerShell operators (-eq, -and, ...).
    if (type[0] == '-' && isAsciiLetter(type[1])) {
      return Token::Operator;
    }
    // Java's @interface.
    if (type[0] == '@' && isAsciiLetter(type[1])) {
      return Token::Keyword;
    }
    if (isKeywordToken(type)) {
      return Token::Keyword;
    }
    if (isTokenOf(type, "!%&*+-./:<=>?^|~()[]{};,\\@$#")) {
      return Token::Operator;
    }
    return Token::None;
  }

  const uint32_t start = ts_node_start_byte(context.node);

  if (typeContains(type, "comment") ||
      typeIn(type, {"shebang", "hash_bang_line", "shebang_directive"})) {
    descend = false;
    return commentTokenAt(source, start);
  }
  if (typeEquals(type, "haddock")) {
    descend = false;
    return Token::CommentDoc;
  }

  // Decorator, annotation, attribute and macro names.
  if (typeEquals(parent, "decorator")) {
    if (typeIn(type, {"call", "call_expression"})) {
      return Token::None;
    }
    descend = false;
    return Token::Preprocessor;
  }
  if ((typeEquals(field, "function") &&
       typeIn(parent, {"call", "call_expression"}) &&
       typeEquals(context.grandparent, "decorator")) ||
      (typeEquals(field, "name") &&
       (typeIn(parent, {"annotation", "marker_annotation"}) ||
        (language == StyledLanguage::CSharp &&
         typeEquals(parent, "attribute")))) ||
      (typeEquals(field, "macro") && typeEquals(parent, "macro_invocation"))) {
    descend = false;
    return Token::Preprocessor;
  }

  // Language specific nodes.
  switch (language) {
  case StyledLanguage::Rust:
    if (typeIn(type, {"attribute_item", "inner_attribute_item"})) {
      descend = false;
      return Token::Preprocessor;
    }
    break;
  case StyledLanguage::Haskell:
    if (typeIn(type, {"pragma", "cpp"})) {
      descend = false;
      return Token::Preprocessor;
    }
    if (typeIn(type, {"operator", "constructor_operator"})) {
      descend = false;
      return Token::Operator;
    }
    if (typeEquals(type, "name")) {
      descend = false;
      return Token::Type;
    }
    break;
  case StyledLanguage::OCaml:
    if (typeIn(type, {"attribute", "item_attribute", "floating_attribute",
                      "directive", "toplevel_directive",
                      "line_number_directive"})) {
      descend = false;
      return Token::Preprocessor;
    }
    if (typeEndsWith(type, "_operator")) {
      descend = false;
      return Token::Operator;
    }
    break;
  case StyledLanguage::PowerShell:
    if (typeEquals(type, "attribute")) {
      descend = false;
      return Token::Preprocessor;
    }
    if (typeIn(type, {"variable", "braced_variable"})) {
      descend = false;
      return Token::Variable;
    }
    if (typeEquals(type, "command_parameter")) {
      descend = false;
      return Token::Operator;
    }
    if (typeEquals(type, "function_name")) {
      descend = false;
      return Token::FunctionName;
    }
    break;
  case StyledLanguage::Bash:
    if (typeIn(type, {"simple_expansion", "expansion", "variable_name"})) {
      descend = false;
      return Token::Variable;
    }
    if (typeEquals(type, "test_operator")) {
      descend = false;
      return Token::Operator;
    }
    if (typeIn(type, {"heredoc_start", "heredoc_end"})) {
      descend = false;
      return Token::Preprocessor;
    }
    break;
  case StyledLanguage::Python:
    if (typeEquals(type, "type")) {
      descend = false;
      return Token::Type;
    }
    break;
  case StyledLanguage::Json:
    if (typeEquals(field, "key")) {
      descend = false;
      return Token::Property;
    }
    break;
  default:
    break;
  }

  // Characters, strings and their contents. Strings are visited so
  // interpolations can be reset to the default color and highlighted as code.
  if (typeIn(type, {"char_literal", "character_literal", "char", "character"})) {
    descend = false;
    return Token::SingleQuotedString;
  }
  if (typeEquals(type, "string") || typeEndsWith(type, "_string") ||
      typeEndsWith(type, "string_literal") ||
      typeIn(type, {"interpolated_string_expression", "heredoc_body",
                    "quasiquote_body"})) {
    return stringTokenAt(type, source, start);
  }
  if (typeIn(type, {"interpolation", "template_substitution",
                    "string_interpolation", "command_substitution",
                    "arithmetic_expansion", "sub_expression"})) {
    return Token::Default;
  }
  if (typeIn(type, {"escape_sequence", "escape_interpolation"})) {
    descend = false;
    return Token::Escape;
  }
  if (typeEquals(type, "regex")) {
    descend = false;
    return Token::Regex;
  }

  if (((typeContains(type, "number") || typeContains(type, "integer") ||
        typeContains(type, "float") || typeContains(type, "real_literal")) &&
       !typeContains(type, "type")) ||
      typeEquals(type, "file_descriptor")) {
    descend = false;
    return Token::Number;
  }

  if (typeIn(type, {"true", "false", "null", "nil", "none", "undefined", "this",
                    "self", "super", "crate", "null_literal", "boolean_literal",
                    "boolean", "unit", "this_type", "mutable_specifier",
                    "implicit_type"})) {
    descend = false;
    return Token::Keyword;
  }
  if (typeIn(type, {"primitive_type", "predefined_type", "integral_type",
                    "floating_point_type", "boolean_type", "void_type"})) {
    descend = false;
    return Token::BuiltinType;
  }
  if (typeIn(type, {"type_identifier", "constructor", "constructor_name",
                    "type_constructor", "module_name", "module_type_name",
                    "class_name", "class_type_name", "module_id",
                    "type_variable", "type_name", "type_literal"})) {
    descend = false;
    return Token::Type;
  }

  if (isIdentifierNode(type)) {
    if (typeEquals(field, "type") || typeEquals(parent, "generic_name") ||
        // PowerShell class and enum names have no field.
        (typeEquals(type, "simple_name") &&
         typeIn(parent, {"class_statement", "enum_statement"})) ||
        (typeEquals(field, "name") &&
         typeIn(parent,
                {"class_declaration", "class", "abstract_class_declaration",
                 "struct_declaration", "interface_declaration",
                 "enum_declaration", "record_declaration",
                 "delegate_declaration", "class_definition"}))) {
      descend = false;
      return Token::Type;
    }
    if (typeEquals(field, "name") &&
        typeIn(parent,
               {"function_definition", "function_declaration",
                "generator_function_declaration", "method_declaration",
                "method_definition", "function_item", "function_signature_item",
                "local_function_statement"})) {
      descend = false;
      return Token::FunctionName;
    }
  }

  return Token::None;
}

Token tokenForNode(StyledLanguage language, const NodeContext &context,
                   const QByteArray &source, bool &descend) {
  switch (language) {
  case StyledLanguage::None:
    descend = true;
    return Token::None;
  case StyledLanguage::CFamily:
    return cFamilyTokenForNode(context, source, descend);
  case StyledLanguage::Dart:
    return dartTokenForNode(context, source, descend);
  case StyledLanguage::Html:
    return htmlTokenForNode(context, source, descend);
  case StyledLanguage::Css:
  case StyledLanguage::EmbeddedCss:
    return cssTokenForNode(context, source, descend);
  case StyledLanguage::EmbeddedJavaScript:
    return genericTokenForNode(StyledLanguage::JavaScript, context, source,
                               descend);
  case StyledLanguage::JavaScript:
  case StyledLanguage::Python:
  case StyledLanguage::Bash:
  case StyledLanguage::Json:
  case StyledLanguage::Java:
  case StyledLanguage::CSharp:
  case StyledLanguage::Rust:
  case StyledLanguage::OCaml:
  case StyledLanguage::Haskell:
  case StyledLanguage::PowerShell:
    break;
  }
  return genericTokenForNode(language, context, source, descend);
}

QByteArray nodeBytes(TSNode node, const QByteArray &source) {
  const auto size = static_cast<uint32_t>(source.size());
  const uint32_t start = std::min(ts_node_start_byte(node), size);
  const uint32_t end = std::min(ts_node_end_byte(node), size);
  return start < end ? source.mid(static_cast<qsizetype>(start),
                                  static_cast<qsizetype>(end - start))
                     : QByteArray();
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

// Style ids for each token; -1 leaves the token unstyled (it keeps the color
// of the enclosing node).
int cppStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerCPP::Default;
  case Token::Comment:
    return QsciLexerCPP::Comment;
  case Token::CommentLine:
    return QsciLexerCPP::CommentLine;
  case Token::CommentDoc:
    return QsciLexerCPP::CommentDoc;
  case Token::String:
    return QsciLexerCPP::DoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerCPP::SingleQuotedString;
  case Token::TripleQuotedString:
    return QsciLexerCPP::VerbatimString;
  case Token::RawString:
    return QsciLexerCPP::RawString;
  case Token::Number:
    return QsciLexerCPP::Number;
  case Token::Keyword:
    return QsciLexerCPP::Keyword;
  case Token::BuiltinType:
  case Token::Variable:
    return QsciLexerCPP::KeywordSet2;
  case Token::Type:
    return QsciLexerCPP::GlobalClass;
  case Token::Operator:
  case Token::Escape:
    return QsciLexerCPP::Operator;
  case Token::Preprocessor:
    return QsciLexerCPP::PreProcessor;
  case Token::Regex:
    return QsciLexerCPP::Regex;
  default:
    return -1;
  }
}

int javaScriptStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerJavaScript::Default;
  case Token::Comment:
    return QsciLexerJavaScript::Comment;
  case Token::CommentLine:
    return QsciLexerJavaScript::CommentLine;
  case Token::CommentDoc:
    return QsciLexerJavaScript::CommentDoc;
  case Token::String:
  case Token::TripleQuotedString:
  case Token::RawString:
    return QsciLexerJavaScript::DoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerJavaScript::SingleQuotedString;
  case Token::Number:
    return QsciLexerJavaScript::Number;
  case Token::Keyword:
    return QsciLexerJavaScript::Keyword;
  case Token::BuiltinType:
  case Token::Preprocessor:
    return QsciLexerJavaScript::KeywordSet2;
  case Token::Type:
    return QsciLexerJavaScript::GlobalClass;
  case Token::Operator:
  case Token::Escape:
    return QsciLexerJavaScript::Operator;
  case Token::Regex:
    return QsciLexerJavaScript::Regex;
  default:
    return -1;
  }
}

int pythonStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerPython::Default;
  case Token::Comment:
  case Token::CommentLine:
  case Token::CommentDoc:
    return QsciLexerPython::Comment;
  case Token::String:
  case Token::RawString:
    return QsciLexerPython::DoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerPython::SingleQuotedString;
  case Token::TripleQuotedString:
    return QsciLexerPython::TripleDoubleQuotedString;
  case Token::Number:
    return QsciLexerPython::Number;
  case Token::Keyword:
    return QsciLexerPython::Keyword;
  case Token::Type:
    return QsciLexerPython::ClassName;
  case Token::FunctionName:
    return QsciLexerPython::FunctionMethodName;
  case Token::Preprocessor:
    return QsciLexerPython::Decorator;
  case Token::Operator:
  case Token::Escape:
    return QsciLexerPython::Operator;
  default:
    return -1;
  }
}

int bashStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerBash::Default;
  case Token::Comment:
  case Token::CommentLine:
  case Token::CommentDoc:
    return QsciLexerBash::Comment;
  case Token::String:
  case Token::TripleQuotedString:
  case Token::RawString:
    return QsciLexerBash::DoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerBash::SingleQuotedString;
  case Token::Number:
    return QsciLexerBash::Number;
  case Token::Keyword:
    return QsciLexerBash::Keyword;
  case Token::Operator:
  case Token::Escape:
    return QsciLexerBash::Operator;
  case Token::Variable:
    return QsciLexerBash::Scalar;
  case Token::Preprocessor:
    return QsciLexerBash::HereDocumentDelimiter;
  default:
    return -1;
  }
}

int jsonStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerJSON::Default;
  case Token::CommentLine:
    return QsciLexerJSON::CommentLine;
  case Token::Comment:
  case Token::CommentDoc:
    return QsciLexerJSON::CommentBlock;
  case Token::String:
  case Token::SingleQuotedString:
  case Token::TripleQuotedString:
  case Token::RawString:
    return QsciLexerJSON::String;
  case Token::Number:
    return QsciLexerJSON::Number;
  case Token::Keyword:
    return QsciLexerJSON::Keyword;
  case Token::Operator:
    return QsciLexerJSON::Operator;
  case Token::Property:
    return QsciLexerJSON::Property;
  case Token::Escape:
    return QsciLexerJSON::EscapeSequence;
  default:
    return -1;
  }
}

int cssStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerCSS::Default;
  case Token::Comment:
  case Token::CommentLine:
  case Token::CommentDoc:
    return QsciLexerCSS::Comment;
  case Token::Tag:
    return QsciLexerCSS::Tag;
  case Token::ClassSelector:
    return QsciLexerCSS::ClassSelector;
  case Token::IdSelector:
    return QsciLexerCSS::IDSelector;
  case Token::PseudoClass:
    return QsciLexerCSS::PseudoClass;
  case Token::PseudoElement:
    return QsciLexerCSS::PseudoElement;
  case Token::Attribute:
    return QsciLexerCSS::Attribute;
  case Token::Property:
    return QsciLexerCSS::CSS1Property;
  case Token::Number:
  case Token::Value:
    return QsciLexerCSS::Value;
  case Token::String:
    return QsciLexerCSS::DoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerCSS::SingleQuotedString;
  case Token::Operator:
    return QsciLexerCSS::Operator;
  case Token::Important:
    return QsciLexerCSS::Important;
  case Token::Keyword:
  case Token::AtRule:
    return QsciLexerCSS::AtRule;
  default:
    return -1;
  }
}

int htmlStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerHTML::Default;
  case Token::Comment:
  case Token::CommentLine:
  case Token::CommentDoc:
    return QsciLexerHTML::HTMLComment;
  case Token::String:
  case Token::TripleQuotedString:
  case Token::RawString:
    return QsciLexerHTML::HTMLDoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerHTML::HTMLSingleQuotedString;
  case Token::Number:
    return QsciLexerHTML::HTMLNumber;
  case Token::Tag:
  case Token::PseudoElement:
    return QsciLexerHTML::Tag;
  case Token::UnknownTag:
    return QsciLexerHTML::UnknownTag;
  case Token::Attribute:
  case Token::ClassSelector:
  case Token::IdSelector:
  case Token::PseudoClass:
    return QsciLexerHTML::Attribute;
  case Token::Property:
  case Token::Entity:
    return QsciLexerHTML::Entity;
  case Token::FunctionName:
    return QsciLexerHTML::Script;
  case Token::Keyword:
  case Token::Important:
  case Token::AtRule:
    return QsciLexerHTML::JavaScriptKeyword;
  default:
    return -1;
  }
}

int htmlScriptStyleFor(Token token) {
  switch (token) {
  case Token::Default:
    return QsciLexerHTML::Default;
  case Token::Comment:
    return QsciLexerHTML::JavaScriptComment;
  case Token::CommentLine:
    return QsciLexerHTML::JavaScriptCommentLine;
  case Token::CommentDoc:
    return QsciLexerHTML::JavaScriptCommentDoc;
  case Token::String:
  case Token::TripleQuotedString:
  case Token::RawString:
    return QsciLexerHTML::JavaScriptDoubleQuotedString;
  case Token::SingleQuotedString:
    return QsciLexerHTML::JavaScriptSingleQuotedString;
  case Token::Number:
    return QsciLexerHTML::JavaScriptNumber;
  case Token::Keyword:
    return QsciLexerHTML::JavaScriptKeyword;
  case Token::Regex:
    return QsciLexerHTML::JavaScriptRegex;
  default:
    return -1;
  }
}

int styleFor(StyleSet styleSet, Token token) {
  switch (styleSet) {
  case StyleSet::Cpp:
    return cppStyleFor(token);
  case StyleSet::JavaScript:
    return javaScriptStyleFor(token);
  case StyleSet::Python:
    return pythonStyleFor(token);
  case StyleSet::Bash:
    return bashStyleFor(token);
  case StyleSet::Json:
    return jsonStyleFor(token);
  case StyleSet::Css:
    return cssStyleFor(token);
  case StyleSet::Html:
    return htmlStyleFor(token);
  case StyleSet::HtmlScript:
    return htmlScriptStyleFor(token);
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

QTextCharFormat formatForToken(const QsciLexer *lexer, StyleSet styleSet,
                               Token token, const QColor &defaultColor) {
  const int style = styleFor(styleSet, token);
  if (style < 0) {
    return {};
  }

  QColor fallbackColor;
  bool fallbackBold = false;
  bool italic = false;
  switch (token) {
  case Token::None:
    return {};
  case Token::Default:
    fallbackColor = defaultColor;
    break;
  case Token::Comment:
  case Token::CommentLine:
  case Token::CommentDoc:
    fallbackColor = QColor(92, 99, 112);
    italic = true;
    break;
  case Token::Keyword:
  case Token::Important:
    fallbackColor = QColor(198, 120, 221);
    fallbackBold = true;
    break;
  case Token::PseudoClass:
  case Token::AtRule:
    fallbackColor = QColor(198, 120, 221);
    break;
  case Token::String:
  case Token::SingleQuotedString:
  case Token::TripleQuotedString:
  case Token::RawString:
  case Token::Value:
    fallbackColor = QColor(152, 195, 121);
    break;
  case Token::Number:
  case Token::Attribute:
    fallbackColor = QColor(209, 154, 102);
    break;
  case Token::BuiltinType:
  case Token::Operator:
  case Token::Escape:
  case Token::Property:
  case Token::Entity:
  case Token::PseudoElement:
    fallbackColor = QColor(86, 182, 194);
    break;
  case Token::Preprocessor:
  case Token::FunctionName:
  case Token::IdSelector:
    fallbackColor = QColor(97, 175, 239);
    break;
  case Token::Type:
  case Token::ClassSelector:
    fallbackColor = QColor(229, 192, 123);
    break;
  case Token::Regex:
    fallbackColor = QColor(255, 138, 101);
    break;
  case Token::Tag:
  case Token::UnknownTag:
    fallbackColor = QColor(224, 108, 117);
    fallbackBold = true;
    break;
  case Token::Variable:
    fallbackColor = QColor(224, 108, 117);
    break;
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

  // Base-class destructors delete child objects (the timer, and lexers that
  // are parented to this editor) and the document; their signals must not
  // reach slots of this already-destroyed subclass.
  if (_lexer) {
    disconnect(_lexer, nullptr, this, nullptr);
    _lexer = nullptr;

  }

  _treeSitterLanguage = nullptr;
  disconnect(document(), nullptr, this, nullptr);

  disconnect(this, nullptr, this, nullptr);
  _highlightTimer->stop();
  disconnect(_highlightTimer, nullptr, this, nullptr);


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
  // Keyed by style set and token; embedded languages use their own sets.
  QHash<int, QTextCharFormat> tokenFormats;

  const auto formatFor = [&](StyleSet styleSet, Token token) {
    const int key = static_cast<int>(styleSet) * 256 + static_cast<int>(token);
    auto it = tokenFormats.find(key);
    if (it == tokenFormats.end()) {
      it = tokenFormats.insert(
          key, formatForToken(_lexer, styleSet, token, defaultColor));
    }
    return it.value();
  };

  struct Injection {
    StyledLanguage language;
    TSRange range;
  };
  std::vector<Injection> injections;

  // Pre-order walk: parents are added before children, so child ranges take
  // precedence when they overlap.
  const auto walk = [&](TSNode root, StyledLanguage language) {
    const StyleSet styleSet = styleSetFor(language);
    // Types of the current node's ancestors, innermost last.
    std::vector<const char *> ancestors;
    TSTreeCursor treeCursor = ts_tree_cursor_new(root);
    for (;;) {
      const TSNode node = ts_tree_cursor_current_node(&treeCursor);
      const char *type = ts_node_type(node);
      const char *field = ts_tree_cursor_current_field_name(&treeCursor);
      const size_t depth = ancestors.size();
      const NodeContext context{node,
                                type ? type : "",
                                field ? field : "",
                                depth > 0 ? ancestors[depth - 1] : "",
                                depth > 1 ? ancestors[depth - 2] : "",
                                ts_node_is_named(node)};

      bool descend = true;
      const Token token = tokenForNode(language, context, source, descend);
      if (token != Token::None) {
        const QTextCharFormat format = formatFor(styleSet, token);
        if (format.hasProperty(QTextFormat::ForegroundBrush) &&
            format.foreground().color().isValid()) {
          addRange(utf8ByteToUtf16(ts_node_start_byte(node)),
                   utf8ByteToUtf16(ts_node_end_byte(node)), format);
        }
      }

      if (language == StyledLanguage::Html &&
          typeEquals(context.type, "raw_text")) {
        const StyledLanguage embedded = embeddedLanguageFor(node, source);
        if (embedded != StyledLanguage::None) {
          injections.push_back(
              {embedded,
               {ts_node_start_point(node), ts_node_end_point(node),
                ts_node_start_byte(node), ts_node_end_byte(node)}});
        }
      }

      if (descend && ts_tree_cursor_goto_first_child(&treeCursor)) {
        ancestors.push_back(context.type);
        continue;
      }

      bool finished = false;
      while (!ts_tree_cursor_goto_next_sibling(&treeCursor)) {
        if (!ts_tree_cursor_goto_parent(&treeCursor)) {
          finished = true;
          break;
        }
        ancestors.pop_back();
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
