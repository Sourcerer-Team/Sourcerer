#pragma once

#include "qsciglobal.h"

#include <QColor>
#include <QFont>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class QSCI_EXPORT QsciLexer : public QObject {
  Q_OBJECT

public:
  explicit QsciLexer(QObject *parent = nullptr,
                     const QString &treeSitterLanguage = QString());
  ~QsciLexer() override;

  void setDefaultPaper(const QColor &color);
  void setDefaultColor(const QColor &color);
  void setDefaultFont(const QFont &font);

  void setColor(const QColor &color, int style = 0);
  void setPaper(const QColor &color, int style = 0);
  void setFont(const QFont &font, int style = 0);

  QColor defaultPaper() const { return _defaultPaper; }
  QColor defaultColor() const { return _defaultColor; }
  QFont defaultFont() const { return _defaultFont; }

  QColor color(int style = 0) const;
  QColor paper(int style = 0) const;
  QFont font(int style = 0) const;

  QString treeSitterLanguage() const { return _treeSitterLanguage; }

signals:
  void changed();

protected:
  void setTreeSitterLanguage(const QString &language);

private:
  QString _treeSitterLanguage;
  QColor _defaultPaper;
  QColor _defaultColor;
  QFont _defaultFont;
  QHash<int, QColor> _styleColors;
  QHash<int, QColor> _stylePapers;
  QHash<int, QFont> _styleFonts;
};

#define QEDITOR_DECLARE_LEXER(ClassName, LanguageId, ...)                      \
  class QSCI_EXPORT ClassName : public QsciLexer {                             \
  public:                                                                      \
    enum Style { __VA_ARGS__ };                                                \
    explicit ClassName(QObject *parent = nullptr);                             \
  };

QEDITOR_DECLARE_LEXER(QsciLexerCPP, "cpp", Default = 0, Comment, CommentLine,
                      CommentDoc, Keyword, KeywordSet2, DoubleQuotedString,
                      SingleQuotedString, VerbatimString, UnclosedString,
                      RawString, Number, Operator, Identifier, PreProcessor,
                      GlobalClass, Regex)

QEDITOR_DECLARE_LEXER(QsciLexerPython, "python", Default = 0, Comment, Keyword,
                      DoubleQuotedString, SingleQuotedString,
                      TripleDoubleQuotedString, TripleSingleQuotedString,
                      UnclosedString, Number, Operator, Identifier, ClassName,
                      FunctionMethodName, Decorator, HighlightedIdentifier)

QEDITOR_DECLARE_LEXER(QsciLexerJavaScript, "javascript", Default = 0, Comment,
                      CommentLine, CommentDoc, Keyword, KeywordSet2,
                      DoubleQuotedString, SingleQuotedString, UnclosedString,
                      Number, Operator, Identifier, GlobalClass, Regex)

QEDITOR_DECLARE_LEXER(QsciLexerJava, "java", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerTypeScript, "typescript", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerTSX, "tsx", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerCSharp, "csharp", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerRust, "rust", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerOCaml, "ocaml", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerOCamlInterface, "ocaml_interface", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerOCamlType, "ocaml_type", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerHaskell, "haskell", Default = 0)
QEDITOR_DECLARE_LEXER(QsciLexerPowerShell, "powershell", Default = 0)

QEDITOR_DECLARE_LEXER(QsciLexerHTML, "html", Default = 0, Tag, UnknownTag,
                      Attribute, UnknownAttribute, HTMLDoubleQuotedString,
                      HTMLSingleQuotedString, HTMLNumber, HTMLComment, Entity,
                      Script, ASPAtStart, ASPStart, CDATA, PHPStart,
                      PHPDoubleQuotedString, PHPSingleQuotedString, PHPKeyword,
                      PHPNumber, PHPVariable, PHPComment, PHPCommentLine,
                      JavaScriptKeyword, JavaScriptDoubleQuotedString,
                      JavaScriptSingleQuotedString, JavaScriptComment,
                      JavaScriptCommentLine, JavaScriptCommentDoc,
                      JavaScriptNumber, JavaScriptUnclosedString,
                      JavaScriptRegex)

QEDITOR_DECLARE_LEXER(QsciLexerCSS, "css", Default = 0, Tag, ClassSelector,
                      IDSelector, PseudoClass, PseudoElement, Attribute,
                      CSS1Property, CSS2Property, CSS3Property, UnknownProperty,
                      Value, Comment, DoubleQuotedString, SingleQuotedString,
                      Operator, Important, AtRule)

QEDITOR_DECLARE_LEXER(QsciLexerXML, "xml", Default = 0, Tag, UnknownTag,
                      Attribute, UnknownAttribute, HTMLDoubleQuotedString,
                      HTMLSingleQuotedString, HTMLNumber, HTMLComment, Entity,
                      XMLStart, XMLEnd, Script, ASPAtStart, ASPStart, CDATA,
                      OtherInTag)

QEDITOR_DECLARE_LEXER(QsciLexerJSON, "json", Default = 0, Number, String,
                      UnclosedString, Property, EscapeSequence, CommentLine,
                      CommentBlock, Operator, IRI, Keyword, Error)

QEDITOR_DECLARE_LEXER(QsciLexerSQL, "sql", Default = 0, Comment, CommentLine,
                      CommentDoc, Keyword, Number, DoubleQuotedString,
                      SingleQuotedString, Operator, Identifier,
                      QuotedIdentifier, QuotedOperator, PlusKeyword, PlusPrompt,
                      PlusComment)

QEDITOR_DECLARE_LEXER(QsciLexerBash, "bash", Default = 0, Comment, Number,
                      Keyword, DoubleQuotedString, SingleQuotedString, Operator,
                      Identifier, Scalar, ParameterExpansion, Backticks,
                      HereDocumentDelimiter, SingleQuotedHereDocument, Error)

QEDITOR_DECLARE_LEXER(QsciLexerBatch, "batch", Default = 0, Comment, Keyword,
                      Label, HideCommandChar, ExternalCommand, Variable,
                      Operator)

QEDITOR_DECLARE_LEXER(QsciLexerMakefile, "makefile", Default = 0, Comment,
                      Preprocessor, Variable, Operator, Target, Error)

QEDITOR_DECLARE_LEXER(QsciLexerDiff, "diff", Default = 0, Comment, Command,
                      Header, Position, LineRemoved, LineAdded, LineChanged)

QEDITOR_DECLARE_LEXER(QsciLexerProperties, "properties", Default = 0, Comment,
                      Section, Key, Assignment, DefaultValue)

QEDITOR_DECLARE_LEXER(QsciLexerYAML, "yaml", Default = 0, Comment, Identifier,
                      Keyword, Number, Reference, DocumentDelimiter, Operator)

QEDITOR_DECLARE_LEXER(QsciLexerMarkdown, "markdown", Default = 0, Special,
                      Prechar, BlockQuote, StrikeOut, HorizontalRule, Link,
                      CodeBlock)

QEDITOR_DECLARE_LEXER(QsciLexerTeX, "tex", Default = 0, Special, Group, Symbol,
                      Command, Text)

QEDITOR_DECLARE_LEXER(QsciLexerPostScript, "postscript", Default = 0, Comment,
                      DSCComment, Number, Name, Keyword, Literal,
                      ArrayParenthesis, DictionaryParenthesis,
                      ProcedureParenthesis, Text, HexString, Base85String,
                      BadStringCharacter)

QEDITOR_DECLARE_LEXER(QsciLexerMatlab, "matlab", Default = 0, Comment, Command,
                      Number, Keyword, Operator, Identifier, DoubleQuotedString)

QEDITOR_DECLARE_LEXER(QsciLexerOctave, "octave", Default = 0, Comment, Command,
                      Number, Keyword, Operator, Identifier, DoubleQuotedString)

QEDITOR_DECLARE_LEXER(QsciLexerPerl, "perl", Default = 0, Error, Comment, POD,
                      Number, Keyword, DoubleQuotedString, SingleQuotedString,
                      Operator, Identifier, Scalar, Array, Hash, SymbolTable,
                      Regex, Substitution, Backticks, DataSection,
                      HereDocumentDelimiter, SingleQuotedHereDocument,
                      DoubleQuotedHereDocument, BacktickHereDocument,
                      QuotedStringQ, QuotedStringQQ, QuotedStringQX,
                      QuotedStringQR, QuotedStringQW)

QEDITOR_DECLARE_LEXER(QsciLexerRuby, "ruby", Default = 0, Error, Comment, POD,
                      Number, Keyword, DoubleQuotedString, SingleQuotedString,
                      ClassName, FunctionMethodName, Operator, Identifier,
                      Regex, Global, Symbol, ModuleName, InstanceVariable,
                      ClassVariable, Backticks, DataSection,
                      HereDocumentDelimiter, PercentStringq, PercentStringQ,
                      PercentStringx, PercentStringr, PercentStringw,
                      DemotedKeyword, Stdin, Stdout, Stderr)

QEDITOR_DECLARE_LEXER(QsciLexerFortran, "fortran", Default = 0, Comment, Number,
                      SingleQuotedString, DoubleQuotedString, UnclosedString,
                      Operator, Identifier, Keyword, IntrinsicFunction,
                      ExtendedFunction, PreProcessor, Label, Continuation)

QEDITOR_DECLARE_LEXER(QsciLexerFortran77, "fortran77", Default = 0, Comment,
                      CommentLine, Number, SingleQuotedString,
                      DoubleQuotedString, UnclosedString, Operator, Identifier,
                      Keyword, IntrinsicFunction, ExtendedFunction,
                      PreProcessor, Label, Continuation)

QEDITOR_DECLARE_LEXER(QsciLexerIDL, "idl", Default = 0, Comment, Number,
                      Operator, Identifier, UnclosedString, Keyword)

QEDITOR_DECLARE_LEXER(QsciLexerVHDL, "vhdl", Default = 0, Comment, CommentLine,
                      Number, String, Operator, Identifier, UnclosedString,
                      Keyword, StandardOperator, Attribute, StandardFunction,
                      StandardPackage, StandardType, KeywordSet7)

QEDITOR_DECLARE_LEXER(QsciLexerVerilog, "verilog", Default = 0, Comment,
                      CommentLine, CommentBang, Number, Keyword, String,
                      KeywordSet2, SystemTask, Preprocessor, Operator,
                      Identifier, UnclosedString, UserKeywordSet)

QEDITOR_DECLARE_LEXER(QsciLexerTCL, "tcl", Default = 0, Comment, CommentLine,
                      CommentBox, CommentBlock, Number, QuotedString, Operator,
                      Identifier, Substitution, SubstitutionBrace, Modifier,
                      ExpandKeyword, TCLKeyword, ITCLKeyword)

QEDITOR_DECLARE_LEXER(QsciLexerLua, "lua", Default = 0, Comment, LineComment,
                      Number, Keyword, String, Character, LiteralString,
                      Preprocessor, Operator, Identifier, UnclosedString,
                      BasicFunctions, StringTableMathsFunctions,
                      CoroutinesIOSystemFacilities, KeywordSet5, KeywordSet6,
                      KeywordSet7, KeywordSet8, Label)

QEDITOR_DECLARE_LEXER(QsciLexerPascal, "pascal", Default = 0, Identifier,
                      Comment, CommentLine, CommentParenthesis, PreProcessor,
                      PreProcessorParenthesis, Number, HexNumber, Keyword,
                      SingleQuotedString, Operator, Asm)

QEDITOR_DECLARE_LEXER(QsciLexerAsm, "asm", Default = 0, Comment, Number,
                      Operator, Identifier, CPUInstruction, Register, Directive,
                      DirectiveOperand)

QEDITOR_DECLARE_LEXER(QsciLexerMASM, "masm", Default = 0, Comment, Number,
                      Operator, Identifier, CPUInstruction, Register, Directive,
                      DirectiveOperand)

QEDITOR_DECLARE_LEXER(QsciLexerNASM, "nasm", Default = 0, Comment, Number,
                      Operator, Identifier, CPUInstruction, Register, Directive,
                      DirectiveOperand)

QEDITOR_DECLARE_LEXER(QsciLexerCMake, "cmake", Default = 0, Comment, String,
                      StringLeftQuote, StringRightQuote, Function, Variable,
                      Label, KeywordSet3, BlockWhile, BlockForeach, BlockIf,
                      BlockMacro, Number)

QEDITOR_DECLARE_LEXER(QsciLexerD, "d", Default = 0, Comment, CommentLine,
                      CommentDoc, CommentNested, Number, Keyword,
                      KeywordSecondary, KeywordDoc, Typedefs, String,
                      UnclosedString, Character, Operator, Identifier,
                      CommentLineDoc, KeywordSet5, KeywordSet6, KeywordSet7)

QEDITOR_DECLARE_LEXER(QsciLexerPOV, "pov", Default = 0, Comment, CommentLine,
                      Number, Operator, Identifier, String, UnclosedString,
                      Directive, BadDirective, PredefinedIdentifiers,
                      PredefinedFunctions, KeywordSet6, KeywordSet7,
                      KeywordSet8)

QEDITOR_DECLARE_LEXER(QsciLexerSpice, "spice", Default = 0, Identifier, Command,
                      Function, Parameter, Number, Delimiter, Value, Comment)

QEDITOR_DECLARE_LEXER(QsciLexerCustom, "custom", Default = 0)

#undef QEDITOR_DECLARE_LEXER
