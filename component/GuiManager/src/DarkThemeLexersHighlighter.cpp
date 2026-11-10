// MIT License
// Copyright (c) 2026 Sourcerer

#include "DarkThemeLexersHighlighter.h"

#include <StylesheetManager.h>

#include <Qsci/qscilexer.h>
#include <Qsci/qscilexerasm.h>
#include <Qsci/qscilexerbash.h>
#include <Qsci/qscilexerbatch.h>
#include <Qsci/qscilexercmake.h>
#include <Qsci/qscilexercpp.h>
#include <Qsci/qscilexercss.h>
#include <Qsci/qscilexercustom.h>
#include <Qsci/qscilexerd.h>
#include <Qsci/qscilexerdiff.h>
#include <Qsci/qscilexerfortran.h>
#include <Qsci/qscilexerfortran77.h>
#include <Qsci/qscilexerhtml.h>
#include <Qsci/qscilexeridl.h>
#include <Qsci/qscilexerjavascript.h>
#include <Qsci/qscilexerjson.h>
#include <Qsci/qscilexerlua.h>
#include <Qsci/qscilexermakefile.h>
#include <Qsci/qscilexermarkdown.h>
#include <Qsci/qscilexermatlab.h>
#include <Qsci/qscilexeroctave.h>
#include <Qsci/qscilexerpascal.h>
#include <Qsci/qscilexerperl.h>
#include <Qsci/qscilexerpostscript.h>
#include <Qsci/qscilexerpov.h>
#include <Qsci/qscilexerproperties.h>
#include <Qsci/qscilexerpython.h>
#include <Qsci/qscilexerruby.h>
#include <Qsci/qscilexerspice.h>
#include <Qsci/qscilexersql.h>
#include <Qsci/qscilexertcl.h>
#include <Qsci/qscilexertex.h>
#include <Qsci/qscilexerverilog.h>
#include <Qsci/qscilexervhdl.h>
#include <Qsci/qscilexerxml.h>
#include <Qsci/qscilexeryaml.h>
#include <Qsci/qsciscintilla.h>

#include <QDebug>
#include <QFont>
#include <QFontInfo>

using namespace GuiManager;

// Static color scheme definitions
const DarkThemeLexersHighlighter::DarkThemeColors
    DarkThemeLexersHighlighter::DEFAULT_DARK_COLORS = {
        // Background and basic colors
        QColor(40, 44, 52),       // background - dark gray
        QColor(171, 178, 191),    // foreground - light gray
        QColor(61, 174, 233),     // selection - blue
        QColor(61, 174, 233, 50), // selectionBackground - semi-transparent blue
        QColor(44, 48, 56),       // currentLine - slightly lighter gray
        QColor(130, 137, 150),    // lineNumbers - medium gray
        QColor(35, 39, 46),       // lineNumbersBackground - darker gray
        QColor(35, 39, 46),       // foldMargin - darker gray
        QColor(35, 39, 46),       // foldMarginBackground - darker gray
        QColor(60, 64, 72),       // indentationGuides - medium dark gray
        QColor(86, 182, 194),     // braceMatch - cyan
        QColor(224, 108, 117),    // braceUnmatch - red
        QColor(44, 48, 56),       // caretLine - slightly lighter gray
        QColor(171, 178, 191),    // caretForeground - light gray

        // Syntax highlighting colors
        QColor(198, 120, 221), // keyword - purple
        QColor(86, 182, 194),  // keyword2 - cyan
        QColor(92, 99, 112),   // comment - dark gray
        QColor(92, 99, 112),   // commentLine - dark gray
        QColor(92, 99, 112),   // commentDoc - dark gray
        QColor(209, 154, 102), // number - orange
        QColor(152, 195, 121), // string - green
        QColor(152, 195, 121), // character - green
        QColor(86, 182, 194),  // operator - cyan
        QColor(171, 178, 191), // identifier - light gray
        QColor(198, 120, 221), // preprocessor - purple
        QColor(224, 108, 117), // unclosedString - red
        QColor(152, 195, 121), // verbatimString - green
        QColor(255, 138, 101), // regex - orange-red
        QColor(152, 195, 121), // doubleQuotedString - green
        QColor(152, 195, 121), // singleQuotedString - green
        QColor(86, 182, 194),  // keywordSet2 - cyan
        QColor(229, 192, 123), // globalClass - yellow
        QColor(97, 175, 239),  // function - blue
        QColor(198, 120, 221), // decorator - purple
        QColor(229, 192, 123), // className - yellow
        QColor(97, 175, 239),  // functionMethodName - blue
        QColor(224, 108, 117), // tag - red
        QColor(209, 154, 102), // attribute - orange
        QColor(152, 195, 121), // value - green
        QColor(86, 182, 194),  // entity - cyan
        QColor(198, 120, 221), // xmlKeyword - purple
        QColor(209, 154, 102), // xmlAttribute - orange
        QColor(97, 175, 239),  // script - blue
        QColor(198, 120, 221), // asp - purple
        QColor(171, 178, 191), // phpDefault - light gray
        QColor(152, 195, 121), // phpDoubleQuotedString - green
        QColor(152, 195, 121), // phpSingleQuotedString - green
        QColor(198, 120, 221), // phpKeyword - purple
        QColor(209, 154, 102), // phpNumber - orange
        QColor(224, 108, 117), // phpVariable - red
        QColor(92, 99, 112),   // phpComment - dark gray
        QColor(92, 99, 112),   // phpCommentLine - dark gray
        QColor(224, 108, 117), // error - red
        QColor(255, 138, 101), // warning - orange-red
        QColor(86, 182, 194),  // info - cyan
        QColor(255, 138, 101), // lineChangedText - orange-red
        QColor(255, 138, 101,
               30),           // lineChangedBackground - translucent orange-red
        QColor(152, 195, 121) // rawString - green
};

const DarkThemeLexersHighlighter::DarkThemeColors
    DarkThemeLexersHighlighter::DEFAULT_LIGHT_COLORS = {
        QColor(255, 255, 255), // background
        QColor(32, 32, 32),    // foreground
        QColor(32, 32, 32),    // selection
        QColor(231, 238, 244), // selectionBackground
        QColor(248, 250, 252), // currentLine
        QColor(96, 96, 96),    // lineNumbers
        QColor(243, 246, 248), // lineNumbersBackground
        QColor(230, 235, 240), // foldMargin
        QColor(243, 246, 248), // foldMarginBackground
        QColor(220, 226, 232), // indentationGuides
        QColor(0, 120, 215),   // braceMatch
        QColor(224, 108, 117), // braceUnmatch
        QColor(248, 250, 252), // caretLine
        QColor(32, 32, 32),    // caretForeground

        QColor(0, 90, 150),    // keyword
        QColor(0, 123, 167),   // keyword2
        QColor(102, 102, 102), // comment
        QColor(102, 102, 102), // commentLine
        QColor(102, 102, 102), // commentDoc
        QColor(181, 103, 0),   // number
        QColor(24, 102, 0),    // string
        QColor(24, 102, 0),    // character
        QColor(32, 32, 32),    // operator_
        QColor(32, 32, 32),    // identifier
        QColor(123, 60, 160),  // preprocessor
        QColor(164, 38, 44),   // unclosedString
        QColor(24, 102, 0),    // verbatimString
        QColor(164, 38, 44),   // regex
        QColor(24, 102, 0),    // doubleQuotedString
        QColor(24, 102, 0),    // singleQuotedString
        QColor(0, 123, 167),   // keywordSet2
        QColor(130, 98, 0),    // globalClass
        QColor(0, 90, 150),    // function_
        QColor(123, 60, 160),  // decorator
        QColor(130, 98, 0),    // className
        QColor(0, 90, 150),    // functionMethodName
        QColor(164, 38, 44),   // tag
        QColor(181, 103, 0),   // attribute
        QColor(24, 102, 0),    // value
        QColor(0, 123, 167),   // entity
        QColor(123, 60, 160),  // xmlKeyword
        QColor(181, 103, 0),   // xmlAttribute
        QColor(0, 90, 150),    // script
        QColor(123, 60, 160),  // asp
        QColor(80, 80, 80),    // phpDefault
        QColor(24, 102, 0),    // phpDoubleQuotedString
        QColor(24, 102, 0),    // phpSingleQuotedString
        QColor(123, 60, 160),  // phpKeyword
        QColor(181, 103, 0),   // phpNumber
        QColor(164, 38, 44),   // phpVariable
        QColor(102, 102, 102), // phpComment
        QColor(102, 102, 102), // phpCommentLine
        QColor(164, 38, 44),   // error
        QColor(181, 103, 0),   // warning
        QColor(0, 123, 167),   // info
        QColor(140, 88, 0),    // lineChangedText - warm brown
        QColor(255, 241, 201), // lineChangedBackground - warm highlight
        QColor(24, 102, 0)     // rawString
};

DarkThemeLexersHighlighter::DarkThemeLexersHighlighter(QObject *parent)
    : QObject(parent), _currentScheme("default"),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  initializeColorSchemes();

  const QString diffFontName = _configManager.getString(
      ConfigManager::ConfigKey::GitRepoMonospaceFontName);
  const qlonglong diffFontSize = _configManager.getLong(
      ConfigManager::ConfigKey::GitRepoMonospaceFontSize);

  // Set default font
  _defaultFont = QFont(diffFontName, diffFontSize);

  QFontInfo info(_defaultFont);

  if (info.family() != _defaultFont.family()) {
    _defaultFont = QFont("Courier New", 10);
  }

  _defaultFont.setStyleHint(QFont::Monospace);
  _defaultFont.setFixedPitch(true);
}

DarkThemeLexersHighlighter::~DarkThemeLexersHighlighter() {}

void DarkThemeLexersHighlighter::initializeColorSchemes() {
  _colorSchemes["default"] = DEFAULT_DARK_COLORS;
  _colorSchemes["light"] = DEFAULT_LIGHT_COLORS;

  // Purple keeps the dark syntax colors and only tints the editor chrome.
  DarkThemeColors purple = DEFAULT_DARK_COLORS;
  purple.background = QColor(43, 39, 53);
  purple.foreground = QColor(224, 221, 232);
  purple.selection = QColor(224, 221, 232);
  purple.selectionBackground = QColor(55, 70, 89);
  purple.currentLine = QColor(56, 51, 66);
  purple.lineNumbers = QColor(138, 134, 150);
  purple.lineNumbersBackground = QColor(36, 33, 46);
  purple.foldMargin = QColor(36, 33, 46);
  purple.foldMarginBackground = QColor(36, 33, 46);
  purple.indentationGuides = QColor(74, 69, 86);
  purple.caretLine = QColor(56, 51, 66);
  purple.caretForeground = QColor(224, 221, 232);
  purple.identifier = QColor(224, 221, 232);
  purple.phpDefault = QColor(224, 221, 232);
  _colorSchemes["purple"] = purple;

  DarkThemeColors blue = DEFAULT_DARK_COLORS;
  blue.background = QColor(33, 45, 67);
  blue.foreground = QColor(213, 222, 234);
  blue.selection = QColor(213, 222, 234);
  blue.selectionBackground = QColor(32, 84, 114);
  blue.currentLine = QColor(36, 50, 71);
  blue.lineNumbers = QColor(127, 143, 166);
  blue.lineNumbersBackground = QColor(23, 33, 49);
  blue.foldMargin = QColor(23, 33, 49);
  blue.foldMarginBackground = QColor(23, 33, 49);
  blue.indentationGuides = QColor(51, 66, 96);
  blue.caretLine = QColor(36, 50, 71);
  blue.caretForeground = QColor(213, 222, 234);
  blue.identifier = QColor(213, 222, 234);
  blue.phpDefault = QColor(213, 222, 234);
  _colorSchemes["blue"] = blue;

  DarkThemeColors monokai = DEFAULT_DARK_COLORS;
  monokai.background = QColor(39, 40, 34);
  monokai.foreground = QColor(248, 248, 242);
  monokai.selection = QColor(248, 248, 242);
  monokai.selectionBackground = QColor(73, 72, 62);
  monokai.currentLine = QColor(62, 61, 50);
  monokai.lineNumbers = QColor(144, 144, 138);
  monokai.lineNumbersBackground = QColor(35, 36, 30);
  monokai.foldMargin = QColor(35, 36, 30);
  monokai.foldMarginBackground = QColor(35, 36, 30);
  monokai.indentationGuides = QColor(73, 72, 62);
  monokai.caretLine = QColor(62, 61, 50);
  monokai.caretForeground = QColor(248, 248, 242);
  monokai.identifier = QColor(248, 248, 242);
  monokai.phpDefault = QColor(248, 248, 242);
  _colorSchemes["monokai"] = monokai;

  DarkThemeColors gruvbox = DEFAULT_DARK_COLORS;
  gruvbox.background = QColor(40, 40, 40);
  gruvbox.foreground = QColor(235, 219, 178);
  gruvbox.selection = QColor(235, 219, 178);
  gruvbox.selectionBackground = QColor(80, 73, 69);
  gruvbox.currentLine = QColor(60, 56, 54);
  gruvbox.lineNumbers = QColor(146, 131, 116);
  gruvbox.lineNumbersBackground = QColor(29, 32, 33);
  gruvbox.foldMargin = QColor(29, 32, 33);
  gruvbox.foldMarginBackground = QColor(29, 32, 33);
  gruvbox.indentationGuides = QColor(80, 73, 69);
  gruvbox.caretLine = QColor(60, 56, 54);
  gruvbox.caretForeground = QColor(235, 219, 178);
  gruvbox.identifier = QColor(235, 219, 178);
  gruvbox.phpDefault = QColor(235, 219, 178);
  _colorSchemes["gruvbox"] = gruvbox;

  DarkThemeColors solarized = DEFAULT_DARK_COLORS;
  solarized.background = QColor(0, 43, 54);
  solarized.foreground = QColor(131, 148, 150);
  solarized.selection = QColor(147, 161, 161);
  solarized.selectionBackground = QColor(14, 74, 90);
  solarized.currentLine = QColor(7, 54, 66);
  solarized.lineNumbers = QColor(88, 110, 117);
  solarized.lineNumbersBackground = QColor(0, 35, 44);
  solarized.foldMargin = QColor(0, 35, 44);
  solarized.foldMarginBackground = QColor(0, 35, 44);
  solarized.indentationGuides = QColor(7, 54, 66);
  solarized.caretLine = QColor(7, 54, 66);
  solarized.caretForeground = QColor(147, 161, 161);
  solarized.identifier = QColor(131, 148, 150);
  solarized.phpDefault = QColor(131, 148, 150);
  _colorSchemes["solarized"] = solarized;

  DarkThemeColors dracula = DEFAULT_DARK_COLORS;
  dracula.background = QColor(40, 42, 54);
  dracula.foreground = QColor(248, 248, 242);
  dracula.selection = QColor(248, 248, 242);
  dracula.selectionBackground = QColor(68, 71, 90);
  dracula.currentLine = QColor(50, 52, 66);
  dracula.lineNumbers = QColor(98, 114, 164);
  dracula.lineNumbersBackground = QColor(33, 34, 44);
  dracula.foldMargin = QColor(33, 34, 44);
  dracula.foldMarginBackground = QColor(33, 34, 44);
  dracula.indentationGuides = QColor(68, 71, 90);
  dracula.caretLine = QColor(50, 52, 66);
  dracula.caretForeground = QColor(248, 248, 242);
  dracula.identifier = QColor(248, 248, 242);
  dracula.phpDefault = QColor(248, 248, 242);
  _colorSchemes["dracula"] = dracula;

  DarkThemeColors ayu = DEFAULT_DARK_COLORS;
  ayu.background = QColor(15, 20, 25);
  ayu.foreground = QColor(230, 225, 207);
  ayu.selection = QColor(230, 225, 207);
  ayu.selectionBackground = QColor(37, 51, 64);
  ayu.currentLine = QColor(20, 25, 31);
  ayu.lineNumbers = QColor(62, 75, 89);
  ayu.lineNumbersBackground = QColor(13, 16, 22);
  ayu.foldMargin = QColor(13, 16, 22);
  ayu.foldMarginBackground = QColor(13, 16, 22);
  ayu.indentationGuides = QColor(30, 40, 50);
  ayu.caretLine = QColor(20, 25, 31);
  ayu.caretForeground = QColor(230, 225, 207);
  ayu.identifier = QColor(230, 225, 207);
  ayu.phpDefault = QColor(230, 225, 207);
  _colorSchemes["ayu"] = ayu;
}

void DarkThemeLexersHighlighter::applyDarkTheme(QsciScintilla *editor) {
  if (!editor) {
    qWarning() << "DarkThemeLexersHighlighter::applyDarkTheme: editor is null";
    return;
  }

  switch (StylesheetManager::StylesheetManager::currentStylesheet()) {
  case StylesheetManager::StylesheetKey::SourcererLight:
    _currentScheme = QStringLiteral("light");
    break;
  case StylesheetManager::StylesheetKey::SourcererPurple:
    _currentScheme = QStringLiteral("purple");
    break;
  case StylesheetManager::StylesheetKey::SourcererBlue:
    _currentScheme = QStringLiteral("blue");
    break;
  case StylesheetManager::StylesheetKey::SourcererMonokai:
    _currentScheme = QStringLiteral("monokai");
    break;
  case StylesheetManager::StylesheetKey::SourcererGruvbox:
    _currentScheme = QStringLiteral("gruvbox");
    break;
  case StylesheetManager::StylesheetKey::SourcererSolarized:
    _currentScheme = QStringLiteral("solarized");
    break;
  case StylesheetManager::StylesheetKey::SourcererDracula:
    _currentScheme = QStringLiteral("dracula");
    break;
  case StylesheetManager::StylesheetKey::SourcererAyu:
    _currentScheme = QStringLiteral("ayu");
    break;
  default:
    _currentScheme = QStringLiteral("default");
    break;
  }

  applyEditorColors(editor);

  QsciLexer *lexer = editor->lexer();
  if (lexer) {
    configureLexer(lexer);
  }
}

void DarkThemeLexersHighlighter::applyEditorColors(QsciScintilla *editor) {
  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set basic editor colors
  editor->setPaper(colors.background);
  editor->setColor(colors.foreground);
  editor->setCaretLineBackgroundColor(colors.caretLine);
  editor->setCaretLineVisible(true);
  editor->setCaretForegroundColor(colors.caretForeground);

  // Set font
  editor->setFont(_defaultFont);

  // Set selection colors
  editor->setSelectionBackgroundColor(colors.selectionBackground);
  editor->setSelectionForegroundColor(colors.selection);

  // Set margin colors
  editor->setMarginLineNumbers(0, true);
  editor->setMarginBackgroundColor(0, colors.lineNumbersBackground);
  editor->setMarginsForegroundColor(colors.lineNumbers);
  editor->setMarginsBackgroundColor(colors.lineNumbersBackground);

  // Set fold margin colors
  editor->setFolding(QsciScintilla::BoxedTreeFoldStyle);
  editor->setFoldMarginColors(colors.foldMargin, colors.foldMarginBackground);

  // Set indentation guides
  editor->setIndentationsUseTabs(false);
  editor->setAutoIndent(true);
  editor->setIndentationGuides(true);
  editor->setIndentationGuidesBackgroundColor(colors.indentationGuides);
  editor->setIndentationGuidesForegroundColor(colors.indentationGuides);

  // Set brace matching colors
  editor->setBraceMatching(QsciScintilla::SloppyBraceMatch);
  editor->setMatchedBraceBackgroundColor(colors.braceMatch);
  editor->setUnmatchedBraceBackgroundColor(colors.braceUnmatch);
}

void DarkThemeLexersHighlighter::configureLexer(QsciLexer *lexer) {
  if (!lexer) {
    qWarning() << "DarkThemeLexersHighlighter::configureLexer: lexer is null";
    return;
  }

  if (dynamic_cast<QsciLexerCPP *>(lexer)) {
    configureCppLexer(lexer);
  } else if (dynamic_cast<QsciLexerPython *>(lexer)) {
    configurePythonLexer(lexer);
  } else if (dynamic_cast<QsciLexerJavaScript *>(lexer)) {
    configureJavaScriptLexer(lexer);
  } else if (dynamic_cast<QsciLexerHTML *>(lexer)) {
    configureHtmlLexer(lexer);
  } else if (dynamic_cast<QsciLexerCSS *>(lexer)) {
    configureCssLexer(lexer);
  } else if (dynamic_cast<QsciLexerXML *>(lexer)) {
    configureXmlLexer(lexer);
  } else if (dynamic_cast<QsciLexerJSON *>(lexer)) {
    configureJsonLexer(lexer);
  } else if (dynamic_cast<QsciLexerSQL *>(lexer)) {
    configureSqlLexer(lexer);
  } else if (dynamic_cast<QsciLexerBash *>(lexer)) {
    configureBashLexer(lexer);
  } else if (dynamic_cast<QsciLexerBatch *>(lexer)) {
    configureBatchLexer(lexer);
  } else if (dynamic_cast<QsciLexerMakefile *>(lexer)) {
    configureMakefileLexer(lexer);
  } else if (dynamic_cast<QsciLexerDiff *>(lexer)) {
    configureDiffLexer(lexer);
  } else if (dynamic_cast<QsciLexerProperties *>(lexer)) {
    configurePropertiesLexer(lexer);
  } else if (dynamic_cast<QsciLexerYAML *>(lexer)) {
    configureYamlLexer(lexer);
  } else if (dynamic_cast<QsciLexerMarkdown *>(lexer)) {
    configureMarkdownLexer(lexer);
  } else if (dynamic_cast<QsciLexerTeX *>(lexer)) {
    configureTexLexer(lexer);
  } else if (dynamic_cast<QsciLexerPostScript *>(lexer)) {
    configurePostScriptLexer(lexer);
  } else if (dynamic_cast<QsciLexerMatlab *>(lexer)) {
    configureMatlabLexer(lexer);
  } else if (dynamic_cast<QsciLexerOctave *>(lexer)) {
    configureOctaveLexer(lexer);
  } else if (dynamic_cast<QsciLexerPerl *>(lexer)) {
    configurePerlLexer(lexer);
  } else if (dynamic_cast<QsciLexerRuby *>(lexer)) {
    configureRubyLexer(lexer);
  } else if (dynamic_cast<QsciLexerFortran *>(lexer)) {
    configureFortranLexer(lexer);
  } else if (dynamic_cast<QsciLexerFortran77 *>(lexer)) {
    configureFortran77Lexer(lexer);
  } else if (dynamic_cast<QsciLexerIDL *>(lexer)) {
    configureIdlLexer(lexer);
  } else if (dynamic_cast<QsciLexerVHDL *>(lexer)) {
    configureVhdlLexer(lexer);
  } else if (dynamic_cast<QsciLexerVerilog *>(lexer)) {
    configureVerilogLexer(lexer);
  } else if (dynamic_cast<QsciLexerTCL *>(lexer)) {
    configureTclLexer(lexer);
  } else if (dynamic_cast<QsciLexerLua *>(lexer)) {
    configureLuaLexer(lexer);
  } else if (dynamic_cast<QsciLexerPascal *>(lexer)) {
    configurePascalLexer(lexer);
  } else if (dynamic_cast<QsciLexerAsm *>(lexer)) {
    configureAsmLexer(lexer);
  } else if (dynamic_cast<QsciLexerCMake *>(lexer)) {
    configureCMakeLexer(lexer);
  } else if (dynamic_cast<QsciLexerD *>(lexer)) {
    configureDLexer(lexer);
  } else if (dynamic_cast<QsciLexerPOV *>(lexer)) {
    configurePov(lexer);
  } else if (dynamic_cast<QsciLexerSpice *>(lexer)) {
    configureSpiceLexer(lexer);
  } else if (dynamic_cast<QsciLexerCustom *>(lexer)) {
    configureCustomLexer(lexer);
  } else {
    const DarkThemeColors &colors = _colorSchemes[_currentScheme];
    lexer->setDefaultPaper(colors.background);
    lexer->setDefaultColor(colors.foreground);
    lexer->setDefaultFont(_defaultFont);
  }
}

void DarkThemeLexersHighlighter::configureCppLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerCPP *cppLexer = dynamic_cast<QsciLexerCPP *>(lexer);
  if (!cppLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  cppLexer->setDefaultPaper(colors.background);
  cppLexer->setDefaultColor(colors.foreground);
  cppLexer->setDefaultFont(_defaultFont);

  // Configure C++ specific styles
  cppLexer->setColor(colors.foreground, QsciLexerCPP::Default);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Default);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::Default);

  // Comments
  cppLexer->setColor(colors.comment, QsciLexerCPP::Comment);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Comment);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::Comment);

  cppLexer->setColor(colors.commentLine, QsciLexerCPP::CommentLine);
  cppLexer->setPaper(colors.background, QsciLexerCPP::CommentLine);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::CommentLine);

  cppLexer->setColor(colors.commentDoc, QsciLexerCPP::CommentDoc);
  cppLexer->setPaper(colors.background, QsciLexerCPP::CommentDoc);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::CommentDoc);

  // Keywords
  cppLexer->setColor(colors.keyword, QsciLexerCPP::Keyword);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  cppLexer->setFont(keywordFont, QsciLexerCPP::Keyword);

  cppLexer->setColor(colors.keyword2, QsciLexerCPP::KeywordSet2);
  cppLexer->setPaper(colors.background, QsciLexerCPP::KeywordSet2);
  cppLexer->setFont(keywordFont, QsciLexerCPP::KeywordSet2);

  // Strings
  cppLexer->setColor(colors.string, QsciLexerCPP::DoubleQuotedString);
  cppLexer->setPaper(colors.background, QsciLexerCPP::DoubleQuotedString);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::DoubleQuotedString);

  cppLexer->setColor(colors.singleQuotedString,
                     QsciLexerCPP::SingleQuotedString);
  cppLexer->setPaper(colors.background, QsciLexerCPP::SingleQuotedString);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::SingleQuotedString);

  cppLexer->setColor(colors.verbatimString, QsciLexerCPP::VerbatimString);
  cppLexer->setPaper(colors.background, QsciLexerCPP::VerbatimString);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::VerbatimString);

  cppLexer->setColor(colors.unclosedString, QsciLexerCPP::UnclosedString);
  cppLexer->setPaper(colors.background, QsciLexerCPP::UnclosedString);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::UnclosedString);

  cppLexer->setColor(colors.rawString, QsciLexerCPP::RawString);
  cppLexer->setPaper(colors.background, QsciLexerCPP::RawString);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::RawString);

  // Numbers
  cppLexer->setColor(colors.number, QsciLexerCPP::Number);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Number);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::Number);

  // Operators
  cppLexer->setColor(colors.operator_, QsciLexerCPP::Operator);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Operator);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::Operator);

  // Identifiers
  cppLexer->setColor(colors.identifier, QsciLexerCPP::Identifier);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Identifier);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::Identifier);

  // Preprocessor
  cppLexer->setColor(colors.preprocessor, QsciLexerCPP::PreProcessor);
  cppLexer->setPaper(colors.background, QsciLexerCPP::PreProcessor);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::PreProcessor);

  // Global classes
  cppLexer->setColor(colors.globalClass, QsciLexerCPP::GlobalClass);
  cppLexer->setPaper(colors.background, QsciLexerCPP::GlobalClass);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::GlobalClass);

  // Regular expressions
  cppLexer->setColor(colors.regex, QsciLexerCPP::Regex);
  cppLexer->setPaper(colors.background, QsciLexerCPP::Regex);
  cppLexer->setFont(_defaultFont, QsciLexerCPP::Regex);
}

void DarkThemeLexersHighlighter::configurePythonLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerPython *pythonLexer = dynamic_cast<QsciLexerPython *>(lexer);
  if (!pythonLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  pythonLexer->setDefaultPaper(colors.background);
  pythonLexer->setDefaultColor(colors.foreground);
  pythonLexer->setDefaultFont(_defaultFont);

  // Configure Python specific styles
  pythonLexer->setColor(colors.foreground, QsciLexerPython::Default);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Default);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::Default);

  // Comments
  pythonLexer->setColor(colors.comment, QsciLexerPython::Comment);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Comment);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::Comment);

  // Keywords
  pythonLexer->setColor(colors.keyword, QsciLexerPython::Keyword);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  pythonLexer->setFont(keywordFont, QsciLexerPython::Keyword);

  // Strings
  pythonLexer->setColor(colors.string, QsciLexerPython::DoubleQuotedString);
  pythonLexer->setPaper(colors.background, QsciLexerPython::DoubleQuotedString);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::DoubleQuotedString);

  pythonLexer->setColor(colors.singleQuotedString,
                        QsciLexerPython::SingleQuotedString);
  pythonLexer->setPaper(colors.background, QsciLexerPython::SingleQuotedString);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::SingleQuotedString);

  pythonLexer->setColor(colors.string,
                        QsciLexerPython::TripleDoubleQuotedString);
  pythonLexer->setPaper(colors.background,
                        QsciLexerPython::TripleDoubleQuotedString);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::TripleDoubleQuotedString);

  pythonLexer->setColor(colors.singleQuotedString,
                        QsciLexerPython::TripleSingleQuotedString);
  pythonLexer->setPaper(colors.background,
                        QsciLexerPython::TripleSingleQuotedString);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::TripleSingleQuotedString);

  pythonLexer->setColor(colors.unclosedString, QsciLexerPython::UnclosedString);
  pythonLexer->setPaper(colors.background, QsciLexerPython::UnclosedString);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::UnclosedString);

  // Numbers
  pythonLexer->setColor(colors.number, QsciLexerPython::Number);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Number);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::Number);

  // Operators
  pythonLexer->setColor(colors.operator_, QsciLexerPython::Operator);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Operator);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::Operator);

  // Identifiers
  pythonLexer->setColor(colors.identifier, QsciLexerPython::Identifier);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Identifier);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::Identifier);

  // Class names
  pythonLexer->setColor(colors.className, QsciLexerPython::ClassName);
  pythonLexer->setPaper(colors.background, QsciLexerPython::ClassName);
  QFont classFont = _defaultFont;
  classFont.setBold(true);
  pythonLexer->setFont(classFont, QsciLexerPython::ClassName);

  // Function/method names
  pythonLexer->setColor(colors.functionMethodName,
                        QsciLexerPython::FunctionMethodName);
  pythonLexer->setPaper(colors.background, QsciLexerPython::FunctionMethodName);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::FunctionMethodName);

  // Decorators
  pythonLexer->setColor(colors.decorator, QsciLexerPython::Decorator);
  pythonLexer->setPaper(colors.background, QsciLexerPython::Decorator);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::Decorator);

  // Highlighted identifiers
  pythonLexer->setColor(colors.keywordSet2,
                        QsciLexerPython::HighlightedIdentifier);
  pythonLexer->setPaper(colors.background,
                        QsciLexerPython::HighlightedIdentifier);
  pythonLexer->setFont(_defaultFont, QsciLexerPython::HighlightedIdentifier);
}

QString DarkThemeLexersHighlighter::getLexerTypeName(QsciLexer *lexer) const {
  if (!lexer)
    return QString();

  return lexer->metaObject()->className();
}

void DarkThemeLexersHighlighter::setColorScheme(const QString &schemeName) {
  if (_colorSchemes.contains(schemeName)) {
    _currentScheme = schemeName;
    emit themeChanged();
  }
}

QStringList DarkThemeLexersHighlighter::availableColorSchemes() const {
  return _colorSchemes.keys();
}

void DarkThemeLexersHighlighter::refreshTheme() {
  emit themeChanged();
}

void DarkThemeLexersHighlighter::configureJavaScriptLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerJavaScript *jsLexer = dynamic_cast<QsciLexerJavaScript *>(lexer);
  if (!jsLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  jsLexer->setDefaultPaper(colors.background);
  jsLexer->setDefaultColor(colors.foreground);
  jsLexer->setDefaultFont(_defaultFont);

  // Configure JavaScript specific styles
  jsLexer->setColor(colors.foreground, QsciLexerJavaScript::Default);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Default);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::Default);

  // Comments
  jsLexer->setColor(colors.comment, QsciLexerJavaScript::Comment);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Comment);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::Comment);

  jsLexer->setColor(colors.commentLine, QsciLexerJavaScript::CommentLine);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::CommentLine);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::CommentLine);

  jsLexer->setColor(colors.commentDoc, QsciLexerJavaScript::CommentDoc);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::CommentDoc);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::CommentDoc);

  // Keywords
  jsLexer->setColor(colors.keyword, QsciLexerJavaScript::Keyword);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  jsLexer->setFont(keywordFont, QsciLexerJavaScript::Keyword);

  jsLexer->setColor(colors.keyword2, QsciLexerJavaScript::KeywordSet2);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::KeywordSet2);
  jsLexer->setFont(keywordFont, QsciLexerJavaScript::KeywordSet2);

  // Strings
  jsLexer->setColor(colors.string, QsciLexerJavaScript::DoubleQuotedString);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::DoubleQuotedString);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::DoubleQuotedString);

  jsLexer->setColor(colors.singleQuotedString,
                    QsciLexerJavaScript::SingleQuotedString);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::SingleQuotedString);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::SingleQuotedString);

  jsLexer->setColor(colors.unclosedString, QsciLexerJavaScript::UnclosedString);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::UnclosedString);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::UnclosedString);

  // Numbers
  jsLexer->setColor(colors.number, QsciLexerJavaScript::Number);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Number);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::Number);

  // Operators
  jsLexer->setColor(colors.operator_, QsciLexerJavaScript::Operator);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Operator);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::Operator);

  // Identifiers
  jsLexer->setColor(colors.identifier, QsciLexerJavaScript::Identifier);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Identifier);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::Identifier);

  // Global classes
  jsLexer->setColor(colors.globalClass, QsciLexerJavaScript::GlobalClass);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::GlobalClass);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::GlobalClass);

  // Regular expressions
  jsLexer->setColor(colors.regex, QsciLexerJavaScript::Regex);
  jsLexer->setPaper(colors.background, QsciLexerJavaScript::Regex);
  jsLexer->setFont(_defaultFont, QsciLexerJavaScript::Regex);
}

void DarkThemeLexersHighlighter::configureHtmlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerHTML *htmlLexer = dynamic_cast<QsciLexerHTML *>(lexer);
  if (!htmlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  htmlLexer->setDefaultPaper(colors.background);
  htmlLexer->setDefaultColor(colors.foreground);
  htmlLexer->setDefaultFont(_defaultFont);

  // Configure HTML specific styles
  htmlLexer->setColor(colors.foreground, QsciLexerHTML::Default);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::Default);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::Default);

  // HTML Tags
  htmlLexer->setColor(colors.tag, QsciLexerHTML::Tag);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::Tag);
  QFont tagFont = _defaultFont;
  tagFont.setBold(true);
  htmlLexer->setFont(tagFont, QsciLexerHTML::Tag);

  htmlLexer->setColor(colors.tag, QsciLexerHTML::UnknownTag);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::UnknownTag);
  htmlLexer->setFont(tagFont, QsciLexerHTML::UnknownTag);

  // HTML Attributes
  htmlLexer->setColor(colors.attribute, QsciLexerHTML::Attribute);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::Attribute);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::Attribute);

  htmlLexer->setColor(colors.attribute, QsciLexerHTML::UnknownAttribute);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::UnknownAttribute);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::UnknownAttribute);

  // HTML Values
  htmlLexer->setColor(colors.value, QsciLexerHTML::HTMLDoubleQuotedString);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::HTMLDoubleQuotedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::HTMLDoubleQuotedString);

  htmlLexer->setColor(colors.value, QsciLexerHTML::HTMLSingleQuotedString);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::HTMLSingleQuotedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::HTMLSingleQuotedString);

  htmlLexer->setColor(colors.number, QsciLexerHTML::HTMLNumber);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::HTMLNumber);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::HTMLNumber);

  // HTML Comments
  htmlLexer->setColor(colors.comment, QsciLexerHTML::HTMLComment);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::HTMLComment);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::HTMLComment);

  // HTML Entities
  htmlLexer->setColor(colors.entity, QsciLexerHTML::Entity);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::Entity);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::Entity);

  // Script sections
  htmlLexer->setColor(colors.script, QsciLexerHTML::Script);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::Script);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::Script);

  // ASP sections
  htmlLexer->setColor(colors.asp, QsciLexerHTML::ASPAtStart);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::ASPAtStart);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::ASPAtStart);

  htmlLexer->setColor(colors.asp, QsciLexerHTML::ASPStart);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::ASPStart);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::ASPStart);

  // CDATA sections
  htmlLexer->setColor(colors.string, QsciLexerHTML::CDATA);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::CDATA);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::CDATA);

  // PHP sections
  htmlLexer->setColor(colors.phpDefault, QsciLexerHTML::PHPStart);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPStart);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPStart);

  htmlLexer->setColor(colors.phpDoubleQuotedString,
                      QsciLexerHTML::PHPDoubleQuotedString);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPDoubleQuotedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPDoubleQuotedString);

  htmlLexer->setColor(colors.phpSingleQuotedString,
                      QsciLexerHTML::PHPSingleQuotedString);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPSingleQuotedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPSingleQuotedString);

  htmlLexer->setColor(colors.phpKeyword, QsciLexerHTML::PHPKeyword);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPKeyword);
  QFont phpKeywordFont = _defaultFont;
  phpKeywordFont.setBold(true);
  htmlLexer->setFont(phpKeywordFont, QsciLexerHTML::PHPKeyword);

  htmlLexer->setColor(colors.phpNumber, QsciLexerHTML::PHPNumber);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPNumber);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPNumber);

  htmlLexer->setColor(colors.phpVariable, QsciLexerHTML::PHPVariable);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPVariable);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPVariable);

  htmlLexer->setColor(colors.phpComment, QsciLexerHTML::PHPComment);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPComment);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPComment);

  htmlLexer->setColor(colors.phpCommentLine, QsciLexerHTML::PHPCommentLine);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::PHPCommentLine);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::PHPCommentLine);

  // JavaScript within HTML
  htmlLexer->setColor(colors.keyword, QsciLexerHTML::JavaScriptKeyword);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::JavaScriptKeyword);
  htmlLexer->setFont(phpKeywordFont, QsciLexerHTML::JavaScriptKeyword);

  htmlLexer->setColor(colors.string,
                      QsciLexerHTML::JavaScriptDoubleQuotedString);
  htmlLexer->setPaper(colors.background,
                      QsciLexerHTML::JavaScriptDoubleQuotedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptDoubleQuotedString);

  htmlLexer->setColor(colors.singleQuotedString,
                      QsciLexerHTML::JavaScriptSingleQuotedString);
  htmlLexer->setPaper(colors.background,
                      QsciLexerHTML::JavaScriptSingleQuotedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptSingleQuotedString);

  htmlLexer->setColor(colors.comment, QsciLexerHTML::JavaScriptComment);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::JavaScriptComment);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptComment);

  htmlLexer->setColor(colors.commentLine, QsciLexerHTML::JavaScriptCommentLine);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::JavaScriptCommentLine);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptCommentLine);

  htmlLexer->setColor(colors.commentDoc, QsciLexerHTML::JavaScriptCommentDoc);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::JavaScriptCommentDoc);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptCommentDoc);

  htmlLexer->setColor(colors.number, QsciLexerHTML::JavaScriptNumber);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::JavaScriptNumber);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptNumber);

  htmlLexer->setColor(colors.unclosedString,
                      QsciLexerHTML::JavaScriptUnclosedString);
  htmlLexer->setPaper(colors.background,
                      QsciLexerHTML::JavaScriptUnclosedString);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptUnclosedString);

  htmlLexer->setColor(colors.regex, QsciLexerHTML::JavaScriptRegex);
  htmlLexer->setPaper(colors.background, QsciLexerHTML::JavaScriptRegex);
  htmlLexer->setFont(_defaultFont, QsciLexerHTML::JavaScriptRegex);
}

void DarkThemeLexersHighlighter::configureCssLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerCSS *cssLexer = dynamic_cast<QsciLexerCSS *>(lexer);
  if (!cssLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  cssLexer->setDefaultPaper(colors.background);
  cssLexer->setDefaultColor(colors.foreground);
  cssLexer->setDefaultFont(_defaultFont);

  // Configure CSS specific styles
  cssLexer->setColor(colors.foreground, QsciLexerCSS::Default);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Default);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::Default);

  // CSS Tags (element selectors)
  cssLexer->setColor(colors.tag, QsciLexerCSS::Tag);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Tag);
  QFont tagFont = _defaultFont;
  tagFont.setBold(true);
  cssLexer->setFont(tagFont, QsciLexerCSS::Tag);

  // CSS Classes
  cssLexer->setColor(colors.className, QsciLexerCSS::ClassSelector);
  cssLexer->setPaper(colors.background, QsciLexerCSS::ClassSelector);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::ClassSelector);

  // CSS IDs
  cssLexer->setColor(colors.function_, QsciLexerCSS::IDSelector);
  cssLexer->setPaper(colors.background, QsciLexerCSS::IDSelector);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::IDSelector);

  // CSS Pseudo-classes and pseudo-elements
  cssLexer->setColor(colors.keyword, QsciLexerCSS::PseudoClass);
  cssLexer->setPaper(colors.background, QsciLexerCSS::PseudoClass);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::PseudoClass);

  cssLexer->setColor(colors.keyword2, QsciLexerCSS::PseudoElement);
  cssLexer->setPaper(colors.background, QsciLexerCSS::PseudoElement);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::PseudoElement);

  // CSS Attributes
  cssLexer->setColor(colors.attribute, QsciLexerCSS::Attribute);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Attribute);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::Attribute);

  // CSS Properties
  cssLexer->setColor(colors.xmlKeyword, QsciLexerCSS::CSS1Property);
  cssLexer->setPaper(colors.background, QsciLexerCSS::CSS1Property);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::CSS1Property);

  cssLexer->setColor(colors.xmlKeyword, QsciLexerCSS::CSS2Property);
  cssLexer->setPaper(colors.background, QsciLexerCSS::CSS2Property);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::CSS2Property);

  cssLexer->setColor(colors.xmlKeyword, QsciLexerCSS::CSS3Property);
  cssLexer->setPaper(colors.background, QsciLexerCSS::CSS3Property);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::CSS3Property);

  cssLexer->setColor(colors.xmlKeyword, QsciLexerCSS::UnknownProperty);
  cssLexer->setPaper(colors.background, QsciLexerCSS::UnknownProperty);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::UnknownProperty);

  // CSS Values
  cssLexer->setColor(colors.value, QsciLexerCSS::Value);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Value);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::Value);

  // CSS Comments
  cssLexer->setColor(colors.comment, QsciLexerCSS::Comment);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Comment);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::Comment);

  // CSS Strings
  cssLexer->setColor(colors.string, QsciLexerCSS::DoubleQuotedString);
  cssLexer->setPaper(colors.background, QsciLexerCSS::DoubleQuotedString);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::DoubleQuotedString);

  cssLexer->setColor(colors.singleQuotedString,
                     QsciLexerCSS::SingleQuotedString);
  cssLexer->setPaper(colors.background, QsciLexerCSS::SingleQuotedString);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::SingleQuotedString);

  // CSS Operators
  cssLexer->setColor(colors.operator_, QsciLexerCSS::Operator);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Operator);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::Operator);

  // CSS Important
  cssLexer->setColor(colors.keyword, QsciLexerCSS::Important);
  cssLexer->setPaper(colors.background, QsciLexerCSS::Important);
  QFont importantFont = _defaultFont;
  importantFont.setBold(true);
  cssLexer->setFont(importantFont, QsciLexerCSS::Important);

  // CSS At-rules
  cssLexer->setColor(colors.preprocessor, QsciLexerCSS::AtRule);
  cssLexer->setPaper(colors.background, QsciLexerCSS::AtRule);
  cssLexer->setFont(_defaultFont, QsciLexerCSS::AtRule);
}

void DarkThemeLexersHighlighter::configureXmlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerXML *xmlLexer = dynamic_cast<QsciLexerXML *>(lexer);
  if (!xmlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  xmlLexer->setDefaultPaper(colors.background);
  xmlLexer->setDefaultColor(colors.foreground);
  xmlLexer->setDefaultFont(_defaultFont);

  // Configure XML specific styles
  xmlLexer->setColor(colors.foreground, QsciLexerXML::Default);
  xmlLexer->setPaper(colors.background, QsciLexerXML::Default);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::Default);

  // XML Tags
  xmlLexer->setColor(colors.tag, QsciLexerXML::Tag);
  xmlLexer->setPaper(colors.background, QsciLexerXML::Tag);
  QFont tagFont = _defaultFont;
  tagFont.setBold(true);
  xmlLexer->setFont(tagFont, QsciLexerXML::Tag);

  xmlLexer->setColor(colors.tag, QsciLexerXML::UnknownTag);
  xmlLexer->setPaper(colors.background, QsciLexerXML::UnknownTag);
  xmlLexer->setFont(tagFont, QsciLexerXML::UnknownTag);

  // XML Attributes
  xmlLexer->setColor(colors.xmlAttribute, QsciLexerXML::Attribute);
  xmlLexer->setPaper(colors.background, QsciLexerXML::Attribute);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::Attribute);

  xmlLexer->setColor(colors.xmlAttribute, QsciLexerXML::UnknownAttribute);
  xmlLexer->setPaper(colors.background, QsciLexerXML::UnknownAttribute);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::UnknownAttribute);

  // XML Values
  xmlLexer->setColor(colors.value, QsciLexerXML::HTMLDoubleQuotedString);
  xmlLexer->setPaper(colors.background, QsciLexerXML::HTMLDoubleQuotedString);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::HTMLDoubleQuotedString);

  xmlLexer->setColor(colors.value, QsciLexerXML::HTMLSingleQuotedString);
  xmlLexer->setPaper(colors.background, QsciLexerXML::HTMLSingleQuotedString);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::HTMLSingleQuotedString);

  xmlLexer->setColor(colors.number, QsciLexerXML::HTMLNumber);
  xmlLexer->setPaper(colors.background, QsciLexerXML::HTMLNumber);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::HTMLNumber);

  // XML Comments
  xmlLexer->setColor(colors.comment, QsciLexerXML::HTMLComment);
  xmlLexer->setPaper(colors.background, QsciLexerXML::HTMLComment);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::HTMLComment);

  // XML Entities
  xmlLexer->setColor(colors.entity, QsciLexerXML::Entity);
  xmlLexer->setPaper(colors.background, QsciLexerXML::Entity);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::Entity);

  // XML Start/End tags
  xmlLexer->setColor(colors.operator_, QsciLexerXML::XMLStart);
  xmlLexer->setPaper(colors.background, QsciLexerXML::XMLStart);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::XMLStart);

  xmlLexer->setColor(colors.operator_, QsciLexerXML::XMLEnd);
  xmlLexer->setPaper(colors.background, QsciLexerXML::XMLEnd);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::XMLEnd);

  // Script sections
  xmlLexer->setColor(colors.script, QsciLexerXML::Script);
  xmlLexer->setPaper(colors.background, QsciLexerXML::Script);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::Script);

  // ASP sections
  xmlLexer->setColor(colors.asp, QsciLexerXML::ASPAtStart);
  xmlLexer->setPaper(colors.background, QsciLexerXML::ASPAtStart);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::ASPAtStart);

  xmlLexer->setColor(colors.asp, QsciLexerXML::ASPStart);
  xmlLexer->setPaper(colors.background, QsciLexerXML::ASPStart);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::ASPStart);

  // CDATA sections
  xmlLexer->setColor(colors.string, QsciLexerXML::CDATA);
  xmlLexer->setPaper(colors.background, QsciLexerXML::CDATA);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::CDATA);

  // XML Other tags
  xmlLexer->setColor(colors.preprocessor, QsciLexerXML::OtherInTag);
  xmlLexer->setPaper(colors.background, QsciLexerXML::OtherInTag);
  xmlLexer->setFont(_defaultFont, QsciLexerXML::OtherInTag);
}

void DarkThemeLexersHighlighter::configureJsonLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerJSON *jsonLexer = dynamic_cast<QsciLexerJSON *>(lexer);
  if (!jsonLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  jsonLexer->setDefaultPaper(colors.background);
  jsonLexer->setDefaultColor(colors.foreground);
  jsonLexer->setDefaultFont(_defaultFont);

  // Configure JSON specific styles
  jsonLexer->setColor(colors.foreground, QsciLexerJSON::Default);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::Default);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::Default);

  // JSON Numbers
  jsonLexer->setColor(colors.number, QsciLexerJSON::Number);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::Number);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::Number);

  // JSON Strings
  jsonLexer->setColor(colors.string, QsciLexerJSON::String);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::String);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::String);

  jsonLexer->setColor(colors.unclosedString, QsciLexerJSON::UnclosedString);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::UnclosedString);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::UnclosedString);

  // JSON Property names (keys)
  jsonLexer->setColor(colors.attribute, QsciLexerJSON::Property);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::Property);
  QFont propertyFont = _defaultFont;
  propertyFont.setBold(true);
  jsonLexer->setFont(propertyFont, QsciLexerJSON::Property);

  // JSON Escape sequences
  jsonLexer->setColor(colors.entity, QsciLexerJSON::EscapeSequence);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::EscapeSequence);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::EscapeSequence);

  // JSON Comments (if supported)
  jsonLexer->setColor(colors.comment, QsciLexerJSON::CommentLine);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::CommentLine);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::CommentLine);

  jsonLexer->setColor(colors.comment, QsciLexerJSON::CommentBlock);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::CommentBlock);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::CommentBlock);

  // JSON Operators (brackets, braces, colons, commas)
  jsonLexer->setColor(colors.operator_, QsciLexerJSON::Operator);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::Operator);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::Operator);

  // JSON IRI (Internationalized Resource Identifier)
  jsonLexer->setColor(colors.value, QsciLexerJSON::IRI);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::IRI);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::IRI);

  // JSON-LD keywords
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);

  // JSON Keywords (true, false, null)
  jsonLexer->setColor(colors.keyword, QsciLexerJSON::Keyword);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::Keyword);
  jsonLexer->setFont(keywordFont, QsciLexerJSON::Keyword);

  // JSON Errors
  jsonLexer->setColor(colors.error, QsciLexerJSON::Error);
  jsonLexer->setPaper(colors.background, QsciLexerJSON::Error);
  jsonLexer->setFont(_defaultFont, QsciLexerJSON::Error);
}

void DarkThemeLexersHighlighter::configureSqlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerSQL *sqlLexer = dynamic_cast<QsciLexerSQL *>(lexer);
  if (!sqlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  sqlLexer->setDefaultPaper(colors.background);
  sqlLexer->setDefaultColor(colors.foreground);
  sqlLexer->setDefaultFont(_defaultFont);

  // Configure SQL specific styles
  sqlLexer->setColor(colors.foreground, QsciLexerSQL::Default);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::Default);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::Default);

  // SQL Comments
  sqlLexer->setColor(colors.comment, QsciLexerSQL::Comment);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::Comment);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::Comment);

  sqlLexer->setColor(colors.commentLine, QsciLexerSQL::CommentLine);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::CommentLine);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::CommentLine);

  sqlLexer->setColor(colors.commentDoc, QsciLexerSQL::CommentDoc);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::CommentDoc);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::CommentDoc);

  // SQL Keywords
  sqlLexer->setColor(colors.keyword, QsciLexerSQL::Keyword);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  sqlLexer->setFont(keywordFont, QsciLexerSQL::Keyword);

  // SQL Numbers
  sqlLexer->setColor(colors.number, QsciLexerSQL::Number);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::Number);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::Number);

  // SQL Strings
  sqlLexer->setColor(colors.string, QsciLexerSQL::DoubleQuotedString);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::DoubleQuotedString);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::DoubleQuotedString);

  sqlLexer->setColor(colors.singleQuotedString,
                     QsciLexerSQL::SingleQuotedString);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::SingleQuotedString);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::SingleQuotedString);

  // SQL Operators
  sqlLexer->setColor(colors.operator_, QsciLexerSQL::Operator);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::Operator);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::Operator);

  // SQL Identifiers
  sqlLexer->setColor(colors.identifier, QsciLexerSQL::Identifier);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::Identifier);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::Identifier);

  // SQL Quoted identifiers
  sqlLexer->setColor(colors.value, QsciLexerSQL::QuotedIdentifier);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::QuotedIdentifier);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::QuotedIdentifier);

  // SQL Quoted operators
  sqlLexer->setColor(colors.operator_, QsciLexerSQL::QuotedOperator);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::QuotedOperator);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::QuotedOperator);

  // SQL Plus keywords
  sqlLexer->setColor(colors.keyword2, QsciLexerSQL::PlusKeyword);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::PlusKeyword);
  sqlLexer->setFont(keywordFont, QsciLexerSQL::PlusKeyword);

  // SQL Plus prompts
  sqlLexer->setColor(colors.preprocessor, QsciLexerSQL::PlusPrompt);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::PlusPrompt);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::PlusPrompt);

  // SQL Plus comments
  sqlLexer->setColor(colors.comment, QsciLexerSQL::PlusComment);
  sqlLexer->setPaper(colors.background, QsciLexerSQL::PlusComment);
  sqlLexer->setFont(_defaultFont, QsciLexerSQL::PlusComment);
}

void DarkThemeLexersHighlighter::configureBashLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerBash *bashLexer = dynamic_cast<QsciLexerBash *>(lexer);
  if (!bashLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  bashLexer->setDefaultPaper(colors.background);
  bashLexer->setDefaultColor(colors.foreground);
  bashLexer->setDefaultFont(_defaultFont);

  // Configure Bash specific styles
  bashLexer->setColor(colors.foreground, QsciLexerBash::Default);
  bashLexer->setPaper(colors.background, QsciLexerBash::Default);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Default);

  // Bash Comments
  bashLexer->setColor(colors.comment, QsciLexerBash::Comment);
  bashLexer->setPaper(colors.background, QsciLexerBash::Comment);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Comment);

  // Bash Numbers
  bashLexer->setColor(colors.number, QsciLexerBash::Number);
  bashLexer->setPaper(colors.background, QsciLexerBash::Number);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Number);

  // Bash Keywords
  bashLexer->setColor(colors.keyword, QsciLexerBash::Keyword);
  bashLexer->setPaper(colors.background, QsciLexerBash::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  bashLexer->setFont(keywordFont, QsciLexerBash::Keyword);

  // Bash Strings
  bashLexer->setColor(colors.string, QsciLexerBash::DoubleQuotedString);
  bashLexer->setPaper(colors.background, QsciLexerBash::DoubleQuotedString);
  bashLexer->setFont(_defaultFont, QsciLexerBash::DoubleQuotedString);

  bashLexer->setColor(colors.singleQuotedString,
                      QsciLexerBash::SingleQuotedString);
  bashLexer->setPaper(colors.background, QsciLexerBash::SingleQuotedString);
  bashLexer->setFont(_defaultFont, QsciLexerBash::SingleQuotedString);

  // Bash Operators
  bashLexer->setColor(colors.operator_, QsciLexerBash::Operator);
  bashLexer->setPaper(colors.background, QsciLexerBash::Operator);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Operator);

  // Bash Identifiers
  bashLexer->setColor(colors.identifier, QsciLexerBash::Identifier);
  bashLexer->setPaper(colors.background, QsciLexerBash::Identifier);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Identifier);

  // Bash Scalars (variables)
  bashLexer->setColor(colors.phpVariable, QsciLexerBash::Scalar);
  bashLexer->setPaper(colors.background, QsciLexerBash::Scalar);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Scalar);

  // Bash Parameter expansion
  bashLexer->setColor(colors.entity, QsciLexerBash::ParameterExpansion);
  bashLexer->setPaper(colors.background, QsciLexerBash::ParameterExpansion);
  bashLexer->setFont(_defaultFont, QsciLexerBash::ParameterExpansion);

  // Bash Backticks (command substitution)
  bashLexer->setColor(colors.verbatimString, QsciLexerBash::Backticks);
  bashLexer->setPaper(colors.background, QsciLexerBash::Backticks);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Backticks);

  // Bash Here documents
  bashLexer->setColor(colors.commentDoc, QsciLexerBash::HereDocumentDelimiter);
  bashLexer->setPaper(colors.background, QsciLexerBash::HereDocumentDelimiter);
  bashLexer->setFont(_defaultFont, QsciLexerBash::HereDocumentDelimiter);

  bashLexer->setColor(colors.string, QsciLexerBash::SingleQuotedHereDocument);
  bashLexer->setPaper(colors.background,
                      QsciLexerBash::SingleQuotedHereDocument);
  bashLexer->setFont(_defaultFont, QsciLexerBash::SingleQuotedHereDocument);

  // Bash Error
  bashLexer->setColor(colors.error, QsciLexerBash::Error);
  bashLexer->setPaper(colors.background, QsciLexerBash::Error);
  bashLexer->setFont(_defaultFont, QsciLexerBash::Error);
}

void DarkThemeLexersHighlighter::configureBatchLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerBatch *batchLexer = dynamic_cast<QsciLexerBatch *>(lexer);
  if (!batchLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  batchLexer->setDefaultPaper(colors.background);
  batchLexer->setDefaultColor(colors.foreground);
  batchLexer->setDefaultFont(_defaultFont);

  // Configure Batch specific styles
  batchLexer->setColor(colors.foreground, QsciLexerBatch::Default);
  batchLexer->setPaper(colors.background, QsciLexerBatch::Default);
  batchLexer->setFont(_defaultFont, QsciLexerBatch::Default);

  // Batch Comments
  batchLexer->setColor(colors.comment, QsciLexerBatch::Comment);
  batchLexer->setPaper(colors.background, QsciLexerBatch::Comment);
  batchLexer->setFont(_defaultFont, QsciLexerBatch::Comment);

  // Batch Keywords (commands)
  batchLexer->setColor(colors.keyword, QsciLexerBatch::Keyword);
  batchLexer->setPaper(colors.background, QsciLexerBatch::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  batchLexer->setFont(keywordFont, QsciLexerBatch::Keyword);

  // Batch Labels
  batchLexer->setColor(colors.function_, QsciLexerBatch::Label);
  batchLexer->setPaper(colors.background, QsciLexerBatch::Label);
  QFont labelFont = _defaultFont;
  labelFont.setBold(true);
  batchLexer->setFont(labelFont, QsciLexerBatch::Label);

  // Batch Hide command prefix (@)
  batchLexer->setColor(colors.operator_, QsciLexerBatch::HideCommandChar);
  batchLexer->setPaper(colors.background, QsciLexerBatch::HideCommandChar);
  batchLexer->setFont(_defaultFont, QsciLexerBatch::HideCommandChar);

  // Batch External commands
  batchLexer->setColor(colors.globalClass, QsciLexerBatch::ExternalCommand);
  batchLexer->setPaper(colors.background, QsciLexerBatch::ExternalCommand);
  batchLexer->setFont(_defaultFont, QsciLexerBatch::ExternalCommand);

  // Batch Variables
  batchLexer->setColor(colors.phpVariable, QsciLexerBatch::Variable);
  batchLexer->setPaper(colors.background, QsciLexerBatch::Variable);
  batchLexer->setFont(_defaultFont, QsciLexerBatch::Variable);

  // Batch Operators
  batchLexer->setColor(colors.operator_, QsciLexerBatch::Operator);
  batchLexer->setPaper(colors.background, QsciLexerBatch::Operator);
  batchLexer->setFont(_defaultFont, QsciLexerBatch::Operator);
}

void DarkThemeLexersHighlighter::configureMakefileLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerMakefile *makefileLexer = dynamic_cast<QsciLexerMakefile *>(lexer);
  if (!makefileLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  makefileLexer->setDefaultPaper(colors.background);
  makefileLexer->setDefaultColor(colors.foreground);
  makefileLexer->setDefaultFont(_defaultFont);

  // Configure Makefile specific styles
  makefileLexer->setColor(colors.foreground, QsciLexerMakefile::Default);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Default);
  makefileLexer->setFont(_defaultFont, QsciLexerMakefile::Default);

  // Makefile Comments
  makefileLexer->setColor(colors.comment, QsciLexerMakefile::Comment);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Comment);
  makefileLexer->setFont(_defaultFont, QsciLexerMakefile::Comment);

  // Makefile Preprocessor (include, ifdef, etc.)
  makefileLexer->setColor(colors.preprocessor, QsciLexerMakefile::Preprocessor);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Preprocessor);
  QFont preprocessorFont = _defaultFont;
  preprocessorFont.setBold(true);
  makefileLexer->setFont(preprocessorFont, QsciLexerMakefile::Preprocessor);

  // Makefile Variables
  makefileLexer->setColor(colors.phpVariable, QsciLexerMakefile::Variable);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Variable);
  makefileLexer->setFont(_defaultFont, QsciLexerMakefile::Variable);

  // Makefile Operators (= := += ?= etc.)
  makefileLexer->setColor(colors.operator_, QsciLexerMakefile::Operator);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Operator);
  makefileLexer->setFont(_defaultFont, QsciLexerMakefile::Operator);

  // Makefile Targets
  makefileLexer->setColor(colors.function_, QsciLexerMakefile::Target);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Target);
  QFont targetFont = _defaultFont;
  targetFont.setBold(true);
  makefileLexer->setFont(targetFont, QsciLexerMakefile::Target);

  // Makefile Error
  makefileLexer->setColor(colors.error, QsciLexerMakefile::Error);
  makefileLexer->setPaper(colors.background, QsciLexerMakefile::Error);
  makefileLexer->setFont(_defaultFont, QsciLexerMakefile::Error);
}

void DarkThemeLexersHighlighter::configureDiffLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerDiff *diffLexer = dynamic_cast<QsciLexerDiff *>(lexer);
  if (!diffLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  diffLexer->setDefaultPaper(colors.background);
  diffLexer->setDefaultColor(colors.foreground);
  diffLexer->setDefaultFont(_defaultFont);

  // Configure Diff specific styles
  diffLexer->setColor(colors.foreground, QsciLexerDiff::Default);
  diffLexer->setPaper(colors.background, QsciLexerDiff::Default);
  diffLexer->setFont(_defaultFont, QsciLexerDiff::Default);

  // Diff Comments (diff header information)
  diffLexer->setColor(colors.comment, QsciLexerDiff::Comment);
  diffLexer->setPaper(colors.background, QsciLexerDiff::Comment);
  diffLexer->setFont(_defaultFont, QsciLexerDiff::Comment);

  // Diff Command (diff command line)
  diffLexer->setColor(colors.keyword, QsciLexerDiff::Command);
  diffLexer->setPaper(colors.background, QsciLexerDiff::Command);
  QFont commandFont = _defaultFont;
  commandFont.setBold(true);
  diffLexer->setFont(commandFont, QsciLexerDiff::Command);

  // Diff Header (file headers)
  diffLexer->setColor(colors.preprocessor, QsciLexerDiff::Header);
  diffLexer->setPaper(colors.background, QsciLexerDiff::Header);
  QFont headerFont = _defaultFont;
  headerFont.setBold(true);
  diffLexer->setFont(headerFont, QsciLexerDiff::Header);

  // Diff Position (line number information)
  diffLexer->setColor(colors.info, QsciLexerDiff::Position);
  diffLexer->setPaper(colors.background, QsciLexerDiff::Position);
  diffLexer->setFont(_defaultFont, QsciLexerDiff::Position);

  // Diff Line Removed (deletions)
  diffLexer->setColor(colors.error, QsciLexerDiff::LineRemoved);
  diffLexer->setPaper(colors.background, QsciLexerDiff::LineRemoved);
  diffLexer->setFont(_defaultFont, QsciLexerDiff::LineRemoved);

  // Diff Line Added (additions)
  diffLexer->setColor(colors.string, QsciLexerDiff::LineAdded);
  diffLexer->setPaper(colors.background, QsciLexerDiff::LineAdded);
  diffLexer->setFont(_defaultFont, QsciLexerDiff::LineAdded);

  // Diff Line Changed (modifications)
  diffLexer->setColor(colors.lineChangedText, QsciLexerDiff::LineChanged);
  diffLexer->setPaper(colors.lineChangedBackground, QsciLexerDiff::LineChanged);
  diffLexer->setFont(_defaultFont, QsciLexerDiff::LineChanged);
}

void DarkThemeLexersHighlighter::configurePropertiesLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerProperties *propertiesLexer =
      dynamic_cast<QsciLexerProperties *>(lexer);
  if (!propertiesLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  propertiesLexer->setDefaultPaper(colors.background);
  propertiesLexer->setDefaultColor(colors.foreground);
  propertiesLexer->setDefaultFont(_defaultFont);

  // Configure Properties specific styles
  propertiesLexer->setColor(colors.foreground, QsciLexerProperties::Default);
  propertiesLexer->setPaper(colors.background, QsciLexerProperties::Default);
  propertiesLexer->setFont(_defaultFont, QsciLexerProperties::Default);

  // Properties Comments
  propertiesLexer->setColor(colors.comment, QsciLexerProperties::Comment);
  propertiesLexer->setPaper(colors.background, QsciLexerProperties::Comment);
  propertiesLexer->setFont(_defaultFont, QsciLexerProperties::Comment);

  // Properties Section (group headers in brackets)
  propertiesLexer->setColor(colors.keyword, QsciLexerProperties::Section);
  propertiesLexer->setPaper(colors.background, QsciLexerProperties::Section);
  QFont sectionFont = _defaultFont;
  sectionFont.setBold(true);
  propertiesLexer->setFont(sectionFont, QsciLexerProperties::Section);

  // Properties Key (property names)
  propertiesLexer->setColor(colors.attribute, QsciLexerProperties::Key);
  propertiesLexer->setPaper(colors.background, QsciLexerProperties::Key);
  QFont keyFont = _defaultFont;
  keyFont.setBold(true);
  propertiesLexer->setFont(keyFont, QsciLexerProperties::Key);

  // Properties Assignment operator (= or :)
  propertiesLexer->setColor(colors.operator_, QsciLexerProperties::Assignment);
  propertiesLexer->setPaper(colors.background, QsciLexerProperties::Assignment);
  propertiesLexer->setFont(_defaultFont, QsciLexerProperties::Assignment);

  // Properties Value
  propertiesLexer->setColor(colors.value, QsciLexerProperties::DefaultValue);
  propertiesLexer->setPaper(colors.background,
                            QsciLexerProperties::DefaultValue);
  propertiesLexer->setFont(_defaultFont, QsciLexerProperties::DefaultValue);
}

void DarkThemeLexersHighlighter::configureYamlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerYAML *yamlLexer = dynamic_cast<QsciLexerYAML *>(lexer);
  if (!yamlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  yamlLexer->setDefaultPaper(colors.background);
  yamlLexer->setDefaultColor(colors.foreground);
  yamlLexer->setDefaultFont(_defaultFont);

  // Configure YAML specific styles
  yamlLexer->setColor(colors.foreground, QsciLexerYAML::Default);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Default);
  yamlLexer->setFont(_defaultFont, QsciLexerYAML::Default);

  // YAML Comments
  yamlLexer->setColor(colors.comment, QsciLexerYAML::Comment);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Comment);
  yamlLexer->setFont(_defaultFont, QsciLexerYAML::Comment);

  // YAML Identifier (keys)
  yamlLexer->setColor(colors.attribute, QsciLexerYAML::Identifier);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Identifier);
  QFont identifierFont = _defaultFont;
  identifierFont.setBold(true);
  yamlLexer->setFont(identifierFont, QsciLexerYAML::Identifier);

  // YAML Keyword (true, false, null, etc.)
  yamlLexer->setColor(colors.keyword, QsciLexerYAML::Keyword);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  yamlLexer->setFont(keywordFont, QsciLexerYAML::Keyword);

  // YAML Number
  yamlLexer->setColor(colors.number, QsciLexerYAML::Number);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Number);
  yamlLexer->setFont(_defaultFont, QsciLexerYAML::Number);

  // YAML Reference (anchors and aliases)
  yamlLexer->setColor(colors.entity, QsciLexerYAML::Reference);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Reference);
  yamlLexer->setFont(_defaultFont, QsciLexerYAML::Reference);

  // YAML Document delimiter (---)
  yamlLexer->setColor(colors.preprocessor, QsciLexerYAML::DocumentDelimiter);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::DocumentDelimiter);
  QFont delimiterFont = _defaultFont;
  delimiterFont.setBold(true);
  yamlLexer->setFont(delimiterFont, QsciLexerYAML::DocumentDelimiter);

  // YAML Operator (: - | > etc.)
  yamlLexer->setColor(colors.operator_, QsciLexerYAML::Operator);
  yamlLexer->setPaper(colors.background, QsciLexerYAML::Operator);
  yamlLexer->setFont(_defaultFont, QsciLexerYAML::Operator);
}

void DarkThemeLexersHighlighter::configureMarkdownLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerMarkdown *markdownLexer = dynamic_cast<QsciLexerMarkdown *>(lexer);
  if (!markdownLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  markdownLexer->setDefaultPaper(colors.background);
  markdownLexer->setDefaultColor(colors.foreground);
  markdownLexer->setDefaultFont(_defaultFont);

  // Configure Markdown specific styles
  markdownLexer->setColor(colors.foreground, QsciLexerMarkdown::Default);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::Default);
  markdownLexer->setFont(_defaultFont, QsciLexerMarkdown::Default);

  // Markdown Special characters (*, _, etc.)
  markdownLexer->setColor(colors.operator_, QsciLexerMarkdown::Special);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::Special);
  markdownLexer->setFont(_defaultFont, QsciLexerMarkdown::Special);

  QFont headerFont = _defaultFont;
  headerFont.setBold(true);

  // Markdown Prechar (leading characters)
  markdownLexer->setColor(colors.operator_, QsciLexerMarkdown::Prechar);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::Prechar);
  markdownLexer->setFont(_defaultFont, QsciLexerMarkdown::Prechar);

  // Markdown Unordered list (- * +)
  QFont listFont = _defaultFont;
  listFont.setBold(true);

  // Markdown Blockquote (>)
  markdownLexer->setColor(colors.comment, QsciLexerMarkdown::BlockQuote);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::BlockQuote);
  QFont quoteFont = _defaultFont;
  quoteFont.setItalic(true);
  markdownLexer->setFont(quoteFont, QsciLexerMarkdown::BlockQuote);

  // Markdown Strike out (~~text~~)
  markdownLexer->setColor(colors.comment, QsciLexerMarkdown::StrikeOut);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::StrikeOut);
  QFont strikeFont = _defaultFont;
  strikeFont.setStrikeOut(true);
  markdownLexer->setFont(strikeFont, QsciLexerMarkdown::StrikeOut);

  // Markdown Horizontal rule (---)
  markdownLexer->setColor(colors.preprocessor,
                          QsciLexerMarkdown::HorizontalRule);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::HorizontalRule);
  markdownLexer->setFont(_defaultFont, QsciLexerMarkdown::HorizontalRule);

  // Markdown Links ([text](url))
  markdownLexer->setColor(colors.function_, QsciLexerMarkdown::Link);
  markdownLexer->setPaper(colors.background, QsciLexerMarkdown::Link);
  markdownLexer->setFont(_defaultFont, QsciLexerMarkdown::Link);

  // Markdown Code span (`code`)
  QFont codeFont = _defaultFont;
  codeFont.setFixedPitch(true);

  // Markdown Code block (```)
  markdownLexer->setColor(colors.string, QsciLexerMarkdown::CodeBlock);
  markdownLexer->setPaper(QColor(colors.string.red(), colors.string.green(),
                                 colors.string.blue(), 20),
                          QsciLexerMarkdown::CodeBlock);
  markdownLexer->setFont(codeFont, QsciLexerMarkdown::CodeBlock);
}

void DarkThemeLexersHighlighter::configureTexLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerTeX *texLexer = dynamic_cast<QsciLexerTeX *>(lexer);
  if (!texLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  texLexer->setDefaultPaper(colors.background);
  texLexer->setDefaultColor(colors.foreground);
  texLexer->setDefaultFont(_defaultFont);

  // Configure TeX specific styles
  texLexer->setColor(colors.foreground, QsciLexerTeX::Default);
  texLexer->setPaper(colors.background, QsciLexerTeX::Default);
  texLexer->setFont(_defaultFont, QsciLexerTeX::Default);

  // TeX Special characters (\)
  texLexer->setColor(colors.operator_, QsciLexerTeX::Special);
  texLexer->setPaper(colors.background, QsciLexerTeX::Special);
  texLexer->setFont(_defaultFont, QsciLexerTeX::Special);

  // TeX Groups ({ })
  texLexer->setColor(colors.operator_, QsciLexerTeX::Group);
  texLexer->setPaper(colors.background, QsciLexerTeX::Group);
  texLexer->setFont(_defaultFont, QsciLexerTeX::Group);

  // TeX Symbols (mathematical symbols)
  texLexer->setColor(colors.entity, QsciLexerTeX::Symbol);
  texLexer->setPaper(colors.background, QsciLexerTeX::Symbol);
  texLexer->setFont(_defaultFont, QsciLexerTeX::Symbol);

  // TeX Commands (\command)
  texLexer->setColor(colors.keyword, QsciLexerTeX::Command);
  texLexer->setPaper(colors.background, QsciLexerTeX::Command);
  QFont commandFont = _defaultFont;
  commandFont.setBold(true);
  texLexer->setFont(commandFont, QsciLexerTeX::Command);

  // TeX Text (regular text content)
  texLexer->setColor(colors.foreground, QsciLexerTeX::Text);
  texLexer->setPaper(colors.background, QsciLexerTeX::Text);
  texLexer->setFont(_defaultFont, QsciLexerTeX::Text);
}

void DarkThemeLexersHighlighter::configurePostScriptLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerPostScript *psLexer = dynamic_cast<QsciLexerPostScript *>(lexer);
  if (!psLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  psLexer->setDefaultPaper(colors.background);
  psLexer->setDefaultColor(colors.foreground);
  psLexer->setDefaultFont(_defaultFont);

  // Configure PostScript specific styles
  psLexer->setColor(colors.foreground, QsciLexerPostScript::Default);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Default);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Default);

  // PostScript Comments
  psLexer->setColor(colors.comment, QsciLexerPostScript::Comment);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Comment);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Comment);

  // PostScript DSC Comments (Document Structuring Conventions)
  psLexer->setColor(colors.commentDoc, QsciLexerPostScript::DSCComment);
  psLexer->setPaper(colors.background, QsciLexerPostScript::DSCComment);
  QFont dscFont = _defaultFont;
  dscFont.setBold(true);
  psLexer->setFont(dscFont, QsciLexerPostScript::DSCComment);

  // PostScript Numbers
  psLexer->setColor(colors.number, QsciLexerPostScript::Number);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Number);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Number);

  // PostScript Names (identifiers)
  psLexer->setColor(colors.identifier, QsciLexerPostScript::Name);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Name);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Name);

  // PostScript Keywords (operators)
  psLexer->setColor(colors.keyword, QsciLexerPostScript::Keyword);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  psLexer->setFont(keywordFont, QsciLexerPostScript::Keyword);

  // PostScript Literal strings
  psLexer->setColor(colors.string, QsciLexerPostScript::Literal);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Literal);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Literal);

  // PostScript Array/Dictionary parentheses
  psLexer->setColor(colors.operator_, QsciLexerPostScript::ArrayParenthesis);
  psLexer->setPaper(colors.background, QsciLexerPostScript::ArrayParenthesis);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::ArrayParenthesis);

  psLexer->setColor(colors.operator_,
                    QsciLexerPostScript::DictionaryParenthesis);
  psLexer->setPaper(colors.background,
                    QsciLexerPostScript::DictionaryParenthesis);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::DictionaryParenthesis);

  // PostScript Procedure parentheses
  psLexer->setColor(colors.operator_,
                    QsciLexerPostScript::ProcedureParenthesis);
  psLexer->setPaper(colors.background,
                    QsciLexerPostScript::ProcedureParenthesis);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::ProcedureParenthesis);

  // PostScript Text (strings)
  psLexer->setColor(colors.string, QsciLexerPostScript::Text);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Text);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Text);

  // PostScript Hex strings
  psLexer->setColor(colors.verbatimString, QsciLexerPostScript::HexString);
  psLexer->setPaper(colors.background, QsciLexerPostScript::HexString);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::HexString);

  // PostScript Base85 strings
  psLexer->setColor(colors.verbatimString, QsciLexerPostScript::Base85String);
  psLexer->setPaper(colors.background, QsciLexerPostScript::Base85String);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::Base85String);

  // PostScript Bad string character
  psLexer->setColor(colors.error, QsciLexerPostScript::BadStringCharacter);
  psLexer->setPaper(colors.background, QsciLexerPostScript::BadStringCharacter);
  psLexer->setFont(_defaultFont, QsciLexerPostScript::BadStringCharacter);
}

void DarkThemeLexersHighlighter::configureMatlabLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerMatlab *matlabLexer = dynamic_cast<QsciLexerMatlab *>(lexer);
  if (!matlabLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  matlabLexer->setDefaultPaper(colors.background);
  matlabLexer->setDefaultColor(colors.foreground);
  matlabLexer->setDefaultFont(_defaultFont);

  // Configure MATLAB specific styles
  matlabLexer->setColor(colors.foreground, QsciLexerMatlab::Default);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Default);
  matlabLexer->setFont(_defaultFont, QsciLexerMatlab::Default);

  // MATLAB Comments
  matlabLexer->setColor(colors.comment, QsciLexerMatlab::Comment);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Comment);
  matlabLexer->setFont(_defaultFont, QsciLexerMatlab::Comment);

  // MATLAB Commands (built-in functions)
  matlabLexer->setColor(colors.function_, QsciLexerMatlab::Command);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Command);
  QFont commandFont = _defaultFont;
  commandFont.setBold(true);
  matlabLexer->setFont(commandFont, QsciLexerMatlab::Command);

  // MATLAB Numbers
  matlabLexer->setColor(colors.number, QsciLexerMatlab::Number);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Number);
  matlabLexer->setFont(_defaultFont, QsciLexerMatlab::Number);

  // MATLAB Keywords
  matlabLexer->setColor(colors.keyword, QsciLexerMatlab::Keyword);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  matlabLexer->setFont(keywordFont, QsciLexerMatlab::Keyword);

  // MATLAB Operators
  matlabLexer->setColor(colors.operator_, QsciLexerMatlab::Operator);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Operator);
  matlabLexer->setFont(_defaultFont, QsciLexerMatlab::Operator);

  // MATLAB Identifiers (variables)
  matlabLexer->setColor(colors.identifier, QsciLexerMatlab::Identifier);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::Identifier);
  matlabLexer->setFont(_defaultFont, QsciLexerMatlab::Identifier);

  // MATLAB Double-quoted strings
  matlabLexer->setColor(colors.doubleQuotedString,
                        QsciLexerMatlab::DoubleQuotedString);
  matlabLexer->setPaper(colors.background, QsciLexerMatlab::DoubleQuotedString);
  matlabLexer->setFont(_defaultFont, QsciLexerMatlab::DoubleQuotedString);
}

void DarkThemeLexersHighlighter::configureOctaveLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerOctave *octaveLexer = dynamic_cast<QsciLexerOctave *>(lexer);
  if (!octaveLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  octaveLexer->setDefaultPaper(colors.background);
  octaveLexer->setDefaultColor(colors.foreground);
  octaveLexer->setDefaultFont(_defaultFont);

  // Configure Octave specific styles
  octaveLexer->setColor(colors.foreground, QsciLexerOctave::Default);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Default);
  octaveLexer->setFont(_defaultFont, QsciLexerOctave::Default);

  // Octave Comments
  octaveLexer->setColor(colors.comment, QsciLexerOctave::Comment);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Comment);
  octaveLexer->setFont(_defaultFont, QsciLexerOctave::Comment);

  // Octave Commands (built-in functions)
  octaveLexer->setColor(colors.function_, QsciLexerOctave::Command);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Command);
  QFont commandFont = _defaultFont;
  commandFont.setBold(true);
  octaveLexer->setFont(commandFont, QsciLexerOctave::Command);

  // Octave Numbers
  octaveLexer->setColor(colors.number, QsciLexerOctave::Number);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Number);
  octaveLexer->setFont(_defaultFont, QsciLexerOctave::Number);

  // Octave Keywords
  octaveLexer->setColor(colors.keyword, QsciLexerOctave::Keyword);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  octaveLexer->setFont(keywordFont, QsciLexerOctave::Keyword);

  // Octave Operators
  octaveLexer->setColor(colors.operator_, QsciLexerOctave::Operator);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Operator);
  octaveLexer->setFont(_defaultFont, QsciLexerOctave::Operator);

  // Octave Identifiers (variables)
  octaveLexer->setColor(colors.identifier, QsciLexerOctave::Identifier);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::Identifier);
  octaveLexer->setFont(_defaultFont, QsciLexerOctave::Identifier);

  // Octave Double-quoted strings
  octaveLexer->setColor(colors.doubleQuotedString,
                        QsciLexerOctave::DoubleQuotedString);
  octaveLexer->setPaper(colors.background, QsciLexerOctave::DoubleQuotedString);
  octaveLexer->setFont(_defaultFont, QsciLexerOctave::DoubleQuotedString);
}

void DarkThemeLexersHighlighter::configurePerlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerPerl *perlLexer = dynamic_cast<QsciLexerPerl *>(lexer);
  if (!perlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  perlLexer->setDefaultPaper(colors.background);
  perlLexer->setDefaultColor(colors.foreground);
  perlLexer->setDefaultFont(_defaultFont);

  // Configure Perl specific styles
  perlLexer->setColor(colors.foreground, QsciLexerPerl::Default);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Default);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Default);

  // Perl Error
  perlLexer->setColor(colors.error, QsciLexerPerl::Error);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Error);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Error);

  // Perl Comments
  perlLexer->setColor(colors.comment, QsciLexerPerl::Comment);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Comment);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Comment);

  // Perl POD (Plain Old Documentation)
  perlLexer->setColor(colors.commentDoc, QsciLexerPerl::POD);
  perlLexer->setPaper(colors.background, QsciLexerPerl::POD);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::POD);

  // Perl Numbers
  perlLexer->setColor(colors.number, QsciLexerPerl::Number);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Number);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Number);

  // Perl Keywords
  perlLexer->setColor(colors.keyword, QsciLexerPerl::Keyword);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  perlLexer->setFont(keywordFont, QsciLexerPerl::Keyword);

  // Perl Strings
  perlLexer->setColor(colors.string, QsciLexerPerl::DoubleQuotedString);
  perlLexer->setPaper(colors.background, QsciLexerPerl::DoubleQuotedString);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::DoubleQuotedString);

  perlLexer->setColor(colors.singleQuotedString,
                      QsciLexerPerl::SingleQuotedString);
  perlLexer->setPaper(colors.background, QsciLexerPerl::SingleQuotedString);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::SingleQuotedString);

  // Perl Operators
  perlLexer->setColor(colors.operator_, QsciLexerPerl::Operator);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Operator);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Operator);

  // Perl Identifiers
  perlLexer->setColor(colors.identifier, QsciLexerPerl::Identifier);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Identifier);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Identifier);

  // Perl Scalars (variables)
  perlLexer->setColor(colors.phpVariable, QsciLexerPerl::Scalar);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Scalar);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Scalar);

  // Perl Arrays
  perlLexer->setColor(colors.globalClass, QsciLexerPerl::Array);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Array);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Array);

  // Perl Hashes
  perlLexer->setColor(colors.className, QsciLexerPerl::Hash);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Hash);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Hash);

  // Perl Symbol table
  perlLexer->setColor(colors.entity, QsciLexerPerl::SymbolTable);
  perlLexer->setPaper(colors.background, QsciLexerPerl::SymbolTable);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::SymbolTable);

  // Perl Regular expressions
  perlLexer->setColor(colors.regex, QsciLexerPerl::Regex);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Regex);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Regex);

  // Perl Substitution
  perlLexer->setColor(colors.regex, QsciLexerPerl::Substitution);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Substitution);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Substitution);

  // Perl Backticks (command execution)
  perlLexer->setColor(colors.verbatimString, QsciLexerPerl::Backticks);
  perlLexer->setPaper(colors.background, QsciLexerPerl::Backticks);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::Backticks);

  // Perl Data section
  perlLexer->setColor(colors.commentDoc, QsciLexerPerl::DataSection);
  perlLexer->setPaper(colors.background, QsciLexerPerl::DataSection);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::DataSection);

  // Perl Here document delimiter
  perlLexer->setColor(colors.preprocessor,
                      QsciLexerPerl::HereDocumentDelimiter);
  perlLexer->setPaper(colors.background, QsciLexerPerl::HereDocumentDelimiter);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::HereDocumentDelimiter);

  // Perl Single quoted here document
  perlLexer->setColor(colors.singleQuotedString,
                      QsciLexerPerl::SingleQuotedHereDocument);
  perlLexer->setPaper(colors.background,
                      QsciLexerPerl::SingleQuotedHereDocument);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::SingleQuotedHereDocument);

  // Perl Double quoted here document
  perlLexer->setColor(colors.string, QsciLexerPerl::DoubleQuotedHereDocument);
  perlLexer->setPaper(colors.background,
                      QsciLexerPerl::DoubleQuotedHereDocument);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::DoubleQuotedHereDocument);

  // Perl Backtick here document
  perlLexer->setColor(colors.verbatimString,
                      QsciLexerPerl::BacktickHereDocument);
  perlLexer->setPaper(colors.background, QsciLexerPerl::BacktickHereDocument);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::BacktickHereDocument);

  // Perl Quoted string (q, qq, qw, qr, qx)
  perlLexer->setColor(colors.string, QsciLexerPerl::QuotedStringQ);
  perlLexer->setPaper(colors.background, QsciLexerPerl::QuotedStringQ);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::QuotedStringQ);

  perlLexer->setColor(colors.string, QsciLexerPerl::QuotedStringQQ);
  perlLexer->setPaper(colors.background, QsciLexerPerl::QuotedStringQQ);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::QuotedStringQQ);

  perlLexer->setColor(colors.verbatimString, QsciLexerPerl::QuotedStringQX);
  perlLexer->setPaper(colors.background, QsciLexerPerl::QuotedStringQX);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::QuotedStringQX);

  perlLexer->setColor(colors.regex, QsciLexerPerl::QuotedStringQR);
  perlLexer->setPaper(colors.background, QsciLexerPerl::QuotedStringQR);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::QuotedStringQR);

  perlLexer->setColor(colors.verbatimString, QsciLexerPerl::QuotedStringQW);
  perlLexer->setPaper(colors.background, QsciLexerPerl::QuotedStringQW);
  perlLexer->setFont(_defaultFont, QsciLexerPerl::QuotedStringQW);
}

void DarkThemeLexersHighlighter::configureRubyLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerRuby *rubyLexer = dynamic_cast<QsciLexerRuby *>(lexer);
  if (!rubyLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  rubyLexer->setDefaultPaper(colors.background);
  rubyLexer->setDefaultColor(colors.foreground);
  rubyLexer->setDefaultFont(_defaultFont);

  // Configure Ruby specific styles
  rubyLexer->setColor(colors.foreground, QsciLexerRuby::Default);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Default);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Default);

  // Ruby Error
  rubyLexer->setColor(colors.error, QsciLexerRuby::Error);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Error);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Error);

  // Ruby Comments
  rubyLexer->setColor(colors.comment, QsciLexerRuby::Comment);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Comment);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Comment);

  // Ruby POD (Plain Old Documentation)
  rubyLexer->setColor(colors.commentDoc, QsciLexerRuby::POD);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::POD);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::POD);

  // Ruby Numbers
  rubyLexer->setColor(colors.number, QsciLexerRuby::Number);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Number);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Number);

  // Ruby Keywords
  rubyLexer->setColor(colors.keyword, QsciLexerRuby::Keyword);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  rubyLexer->setFont(keywordFont, QsciLexerRuby::Keyword);

  // Ruby Strings
  rubyLexer->setColor(colors.string, QsciLexerRuby::DoubleQuotedString);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::DoubleQuotedString);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::DoubleQuotedString);

  rubyLexer->setColor(colors.singleQuotedString,
                      QsciLexerRuby::SingleQuotedString);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::SingleQuotedString);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::SingleQuotedString);

  // Ruby Class names
  rubyLexer->setColor(colors.className, QsciLexerRuby::ClassName);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::ClassName);
  QFont classFont = _defaultFont;
  classFont.setBold(true);
  rubyLexer->setFont(classFont, QsciLexerRuby::ClassName);

  // Ruby Function/method names
  rubyLexer->setColor(colors.functionMethodName,
                      QsciLexerRuby::FunctionMethodName);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::FunctionMethodName);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::FunctionMethodName);

  // Ruby Operators
  rubyLexer->setColor(colors.operator_, QsciLexerRuby::Operator);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Operator);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Operator);

  // Ruby Identifiers
  rubyLexer->setColor(colors.identifier, QsciLexerRuby::Identifier);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Identifier);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Identifier);

  // Ruby Regular expressions
  rubyLexer->setColor(colors.regex, QsciLexerRuby::Regex);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Regex);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Regex);

  // Ruby Global variables
  rubyLexer->setColor(colors.globalClass, QsciLexerRuby::Global);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Global);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Global);

  // Ruby Symbols
  rubyLexer->setColor(colors.entity, QsciLexerRuby::Symbol);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Symbol);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Symbol);

  // Ruby Module names
  rubyLexer->setColor(colors.globalClass, QsciLexerRuby::ModuleName);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::ModuleName);
  rubyLexer->setFont(classFont, QsciLexerRuby::ModuleName);

  // Ruby Instance variables
  rubyLexer->setColor(colors.phpVariable, QsciLexerRuby::InstanceVariable);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::InstanceVariable);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::InstanceVariable);

  // Ruby Class variables
  rubyLexer->setColor(colors.phpVariable, QsciLexerRuby::ClassVariable);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::ClassVariable);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::ClassVariable);

  // Ruby Backticks (command execution)
  rubyLexer->setColor(colors.verbatimString, QsciLexerRuby::Backticks);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Backticks);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Backticks);

  // Ruby Data section
  rubyLexer->setColor(colors.commentDoc, QsciLexerRuby::DataSection);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::DataSection);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::DataSection);

  // Ruby Here document delimiter
  rubyLexer->setColor(colors.preprocessor,
                      QsciLexerRuby::HereDocumentDelimiter);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::HereDocumentDelimiter);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::HereDocumentDelimiter);

  // Ruby Percent strings (%, %Q, %q, %w, %W, %r, %x)
  rubyLexer->setColor(colors.string, QsciLexerRuby::PercentStringq);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::PercentStringq);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::PercentStringq);

  rubyLexer->setColor(colors.string, QsciLexerRuby::PercentStringQ);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::PercentStringQ);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::PercentStringQ);

  rubyLexer->setColor(colors.verbatimString, QsciLexerRuby::PercentStringx);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::PercentStringx);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::PercentStringx);

  rubyLexer->setColor(colors.regex, QsciLexerRuby::PercentStringr);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::PercentStringr);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::PercentStringr);

  rubyLexer->setColor(colors.verbatimString, QsciLexerRuby::PercentStringw);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::PercentStringw);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::PercentStringw);

  // Ruby Demoted keywords
  rubyLexer->setColor(colors.keyword2, QsciLexerRuby::DemotedKeyword);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::DemotedKeyword);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::DemotedKeyword);

  // Ruby stdin, stdout, stderr
  rubyLexer->setColor(colors.globalClass, QsciLexerRuby::Stdin);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Stdin);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Stdin);

  rubyLexer->setColor(colors.globalClass, QsciLexerRuby::Stdout);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Stdout);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Stdout);

  rubyLexer->setColor(colors.globalClass, QsciLexerRuby::Stderr);
  rubyLexer->setPaper(colors.background, QsciLexerRuby::Stderr);
  rubyLexer->setFont(_defaultFont, QsciLexerRuby::Stderr);
}

void DarkThemeLexersHighlighter::configureFortranLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerFortran *fortranLexer = dynamic_cast<QsciLexerFortran *>(lexer);
  if (!fortranLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  fortranLexer->setDefaultPaper(colors.background);
  fortranLexer->setDefaultColor(colors.foreground);
  fortranLexer->setDefaultFont(_defaultFont);

  // Configure Fortran specific styles
  fortranLexer->setColor(colors.foreground, QsciLexerFortran::Default);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Default);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Default);

  // Fortran Comments
  fortranLexer->setColor(colors.comment, QsciLexerFortran::Comment);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Comment);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Comment);

  // Fortran Numbers
  fortranLexer->setColor(colors.number, QsciLexerFortran::Number);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Number);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Number);

  // Fortran Strings
  fortranLexer->setColor(colors.string, QsciLexerFortran::SingleQuotedString);
  fortranLexer->setPaper(colors.background,
                         QsciLexerFortran::SingleQuotedString);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::SingleQuotedString);

  fortranLexer->setColor(colors.doubleQuotedString,
                         QsciLexerFortran::DoubleQuotedString);
  fortranLexer->setPaper(colors.background,
                         QsciLexerFortran::DoubleQuotedString);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::DoubleQuotedString);

  // Fortran Unclosed strings
  fortranLexer->setColor(colors.unclosedString,
                         QsciLexerFortran::UnclosedString);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::UnclosedString);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::UnclosedString);

  // Fortran Operators
  fortranLexer->setColor(colors.operator_, QsciLexerFortran::Operator);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Operator);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Operator);

  // Fortran Identifiers
  fortranLexer->setColor(colors.identifier, QsciLexerFortran::Identifier);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Identifier);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Identifier);

  // Fortran Keywords
  fortranLexer->setColor(colors.keyword, QsciLexerFortran::Keyword);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  fortranLexer->setFont(keywordFont, QsciLexerFortran::Keyword);

  // Fortran Intrinsic functions
  fortranLexer->setColor(colors.function_, QsciLexerFortran::IntrinsicFunction);
  fortranLexer->setPaper(colors.background,
                         QsciLexerFortran::IntrinsicFunction);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  fortranLexer->setFont(functionFont, QsciLexerFortran::IntrinsicFunction);

  // Fortran Extended intrinsic functions
  fortranLexer->setColor(colors.function_, QsciLexerFortran::ExtendedFunction);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::ExtendedFunction);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::ExtendedFunction);

  // Fortran Preprocessor
  fortranLexer->setColor(colors.preprocessor, QsciLexerFortran::PreProcessor);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::PreProcessor);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::PreProcessor);

  // Fortran Labels
  fortranLexer->setColor(colors.globalClass, QsciLexerFortran::Label);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Label);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Label);

  // Fortran Continuation
  fortranLexer->setColor(colors.preprocessor, QsciLexerFortran::Continuation);
  fortranLexer->setPaper(colors.background, QsciLexerFortran::Continuation);
  fortranLexer->setFont(_defaultFont, QsciLexerFortran::Continuation);
}

void DarkThemeLexersHighlighter::configureFortran77Lexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerFortran77 *fortran77Lexer =
      dynamic_cast<QsciLexerFortran77 *>(lexer);
  if (!fortran77Lexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  fortran77Lexer->setDefaultPaper(colors.background);
  fortran77Lexer->setDefaultColor(colors.foreground);
  fortran77Lexer->setDefaultFont(_defaultFont);

  // Configure Fortran77 specific styles
  fortran77Lexer->setColor(colors.foreground, QsciLexerFortran77::Default);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Default);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Default);

  // Fortran77 Comments
  fortran77Lexer->setColor(colors.comment, QsciLexerFortran77::Comment);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Comment);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Comment);

  // Fortran77 Numbers
  fortran77Lexer->setColor(colors.number, QsciLexerFortran77::Number);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Number);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Number);

  // Fortran77 Strings
  fortran77Lexer->setColor(colors.string,
                           QsciLexerFortran77::SingleQuotedString);
  fortran77Lexer->setPaper(colors.background,
                           QsciLexerFortran77::SingleQuotedString);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::SingleQuotedString);

  fortran77Lexer->setColor(colors.doubleQuotedString,
                           QsciLexerFortran77::DoubleQuotedString);
  fortran77Lexer->setPaper(colors.background,
                           QsciLexerFortran77::DoubleQuotedString);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::DoubleQuotedString);

  // Fortran77 Unclosed strings
  fortran77Lexer->setColor(colors.unclosedString,
                           QsciLexerFortran77::UnclosedString);
  fortran77Lexer->setPaper(colors.background,
                           QsciLexerFortran77::UnclosedString);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::UnclosedString);

  // Fortran77 Operators
  fortran77Lexer->setColor(colors.operator_, QsciLexerFortran77::Operator);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Operator);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Operator);

  // Fortran77 Identifiers
  fortran77Lexer->setColor(colors.identifier, QsciLexerFortran77::Identifier);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Identifier);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Identifier);

  // Fortran77 Keywords
  fortran77Lexer->setColor(colors.keyword, QsciLexerFortran77::Keyword);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  fortran77Lexer->setFont(keywordFont, QsciLexerFortran77::Keyword);

  // Fortran77 Intrinsic functions
  fortran77Lexer->setColor(colors.function_,
                           QsciLexerFortran77::IntrinsicFunction);
  fortran77Lexer->setPaper(colors.background,
                           QsciLexerFortran77::IntrinsicFunction);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  fortran77Lexer->setFont(functionFont, QsciLexerFortran77::IntrinsicFunction);

  // Fortran77 Extended intrinsic functions
  fortran77Lexer->setColor(colors.function_,
                           QsciLexerFortran77::ExtendedFunction);
  fortran77Lexer->setPaper(colors.background,
                           QsciLexerFortran77::ExtendedFunction);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::ExtendedFunction);

  // Fortran77 Preprocessor
  fortran77Lexer->setColor(colors.preprocessor,
                           QsciLexerFortran77::PreProcessor);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::PreProcessor);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::PreProcessor);

  // Fortran77 Labels
  fortran77Lexer->setColor(colors.globalClass, QsciLexerFortran77::Label);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Label);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Label);

  // Fortran77 Continuation
  fortran77Lexer->setColor(colors.preprocessor,
                           QsciLexerFortran77::Continuation);
  fortran77Lexer->setPaper(colors.background, QsciLexerFortran77::Continuation);
  fortran77Lexer->setFont(_defaultFont, QsciLexerFortran77::Continuation);
}

void DarkThemeLexersHighlighter::configureIdlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerIDL *idlLexer = dynamic_cast<QsciLexerIDL *>(lexer);
  if (!idlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  idlLexer->setDefaultPaper(colors.background);
  idlLexer->setDefaultColor(colors.foreground);
  idlLexer->setDefaultFont(_defaultFont);

  // Configure IDL specific styles
  idlLexer->setColor(colors.foreground, QsciLexerIDL::Default);
  idlLexer->setPaper(colors.background, QsciLexerIDL::Default);
  idlLexer->setFont(_defaultFont, QsciLexerIDL::Default);

  // IDL Comments
  idlLexer->setColor(colors.comment, QsciLexerIDL::Comment);
  idlLexer->setPaper(colors.background, QsciLexerIDL::Comment);
  idlLexer->setFont(_defaultFont, QsciLexerIDL::Comment);

  // IDL Numbers
  idlLexer->setColor(colors.number, QsciLexerIDL::Number);
  idlLexer->setPaper(colors.background, QsciLexerIDL::Number);
  idlLexer->setFont(_defaultFont, QsciLexerIDL::Number);

  // IDL Operators
  idlLexer->setColor(colors.operator_, QsciLexerIDL::Operator);
  idlLexer->setPaper(colors.background, QsciLexerIDL::Operator);
  idlLexer->setFont(_defaultFont, QsciLexerIDL::Operator);

  // IDL Identifiers
  idlLexer->setColor(colors.identifier, QsciLexerIDL::Identifier);
  idlLexer->setPaper(colors.background, QsciLexerIDL::Identifier);
  idlLexer->setFont(_defaultFont, QsciLexerIDL::Identifier);

  // IDL Unclosed strings
  idlLexer->setColor(colors.unclosedString, QsciLexerIDL::UnclosedString);
  idlLexer->setPaper(colors.background, QsciLexerIDL::UnclosedString);
  idlLexer->setFont(_defaultFont, QsciLexerIDL::UnclosedString);

  // IDL Keywords
  idlLexer->setColor(colors.keyword, QsciLexerIDL::Keyword);
  idlLexer->setPaper(colors.background, QsciLexerIDL::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  idlLexer->setFont(keywordFont, QsciLexerIDL::Keyword);
}

void DarkThemeLexersHighlighter::configureVhdlLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerVHDL *vhdlLexer = dynamic_cast<QsciLexerVHDL *>(lexer);
  if (!vhdlLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  vhdlLexer->setDefaultPaper(colors.background);
  vhdlLexer->setDefaultColor(colors.foreground);
  vhdlLexer->setDefaultFont(_defaultFont);

  // Configure VHDL specific styles
  vhdlLexer->setColor(colors.foreground, QsciLexerVHDL::Default);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Default);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::Default);

  // VHDL Comments
  vhdlLexer->setColor(colors.comment, QsciLexerVHDL::Comment);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Comment);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::Comment);

  // VHDL Comment line
  vhdlLexer->setColor(colors.commentLine, QsciLexerVHDL::CommentLine);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::CommentLine);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::CommentLine);

  // VHDL Numbers
  vhdlLexer->setColor(colors.number, QsciLexerVHDL::Number);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Number);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::Number);

  // VHDL Strings
  vhdlLexer->setColor(colors.string, QsciLexerVHDL::String);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::String);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::String);

  // VHDL Operators
  vhdlLexer->setColor(colors.operator_, QsciLexerVHDL::Operator);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Operator);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::Operator);

  // VHDL Identifiers
  vhdlLexer->setColor(colors.identifier, QsciLexerVHDL::Identifier);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Identifier);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::Identifier);

  // VHDL Unclosed strings
  vhdlLexer->setColor(colors.unclosedString, QsciLexerVHDL::UnclosedString);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::UnclosedString);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::UnclosedString);

  // VHDL Keywords
  vhdlLexer->setColor(colors.keyword, QsciLexerVHDL::Keyword);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  vhdlLexer->setFont(keywordFont, QsciLexerVHDL::Keyword);

  // VHDL Standard operator
  vhdlLexer->setColor(colors.function_, QsciLexerVHDL::StandardOperator);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::StandardOperator);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::StandardOperator);

  // VHDL Attributes
  vhdlLexer->setColor(colors.attribute, QsciLexerVHDL::Attribute);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::Attribute);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::Attribute);

  // VHDL Functions
  vhdlLexer->setColor(colors.function_, QsciLexerVHDL::StandardFunction);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::StandardFunction);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  vhdlLexer->setFont(functionFont, QsciLexerVHDL::StandardFunction);

  // VHDL Packages
  vhdlLexer->setColor(colors.globalClass, QsciLexerVHDL::StandardPackage);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::StandardPackage);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::StandardPackage);

  // VHDL Types
  vhdlLexer->setColor(colors.className, QsciLexerVHDL::StandardType);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::StandardType);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::StandardType);

  // VHDL User defined types
  vhdlLexer->setColor(colors.className, QsciLexerVHDL::KeywordSet7);
  vhdlLexer->setPaper(colors.background, QsciLexerVHDL::KeywordSet7);
  vhdlLexer->setFont(_defaultFont, QsciLexerVHDL::KeywordSet7);
}

void DarkThemeLexersHighlighter::configureVerilogLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerVerilog *verilogLexer = dynamic_cast<QsciLexerVerilog *>(lexer);
  if (!verilogLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  verilogLexer->setDefaultPaper(colors.background);
  verilogLexer->setDefaultColor(colors.foreground);
  verilogLexer->setDefaultFont(_defaultFont);

  // Configure Verilog specific styles
  verilogLexer->setColor(colors.foreground, QsciLexerVerilog::Default);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Default);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::Default);

  // Verilog Comments
  verilogLexer->setColor(colors.comment, QsciLexerVerilog::Comment);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Comment);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::Comment);

  // Verilog Comment line
  verilogLexer->setColor(colors.commentLine, QsciLexerVerilog::CommentLine);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::CommentLine);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::CommentLine);

  // Verilog Comment bang (special comments)
  verilogLexer->setColor(colors.commentDoc, QsciLexerVerilog::CommentBang);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::CommentBang);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::CommentBang);

  // Verilog Numbers
  verilogLexer->setColor(colors.number, QsciLexerVerilog::Number);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Number);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::Number);

  // Verilog Keywords
  verilogLexer->setColor(colors.keyword, QsciLexerVerilog::Keyword);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  verilogLexer->setFont(keywordFont, QsciLexerVerilog::Keyword);

  // Verilog Strings
  verilogLexer->setColor(colors.string, QsciLexerVerilog::String);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::String);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::String);

  // Verilog Keywords set 2 (system tasks and functions)
  verilogLexer->setColor(colors.keyword2, QsciLexerVerilog::KeywordSet2);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::KeywordSet2);
  verilogLexer->setFont(keywordFont, QsciLexerVerilog::KeywordSet2);

  // Verilog System tasks
  verilogLexer->setColor(colors.function_, QsciLexerVerilog::SystemTask);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::SystemTask);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::SystemTask);

  // Verilog Preprocessor
  verilogLexer->setColor(colors.preprocessor, QsciLexerVerilog::Preprocessor);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Preprocessor);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::Preprocessor);

  // Verilog Operators
  verilogLexer->setColor(colors.operator_, QsciLexerVerilog::Operator);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Operator);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::Operator);

  // Verilog Identifiers
  verilogLexer->setColor(colors.identifier, QsciLexerVerilog::Identifier);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::Identifier);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::Identifier);

  // Verilog Unclosed strings
  verilogLexer->setColor(colors.unclosedString,
                         QsciLexerVerilog::UnclosedString);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::UnclosedString);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::UnclosedString);

  // Verilog User keywords (user-defined types and modules)
  verilogLexer->setColor(colors.globalClass, QsciLexerVerilog::UserKeywordSet);
  verilogLexer->setPaper(colors.background, QsciLexerVerilog::UserKeywordSet);
  verilogLexer->setFont(_defaultFont, QsciLexerVerilog::UserKeywordSet);
}

void DarkThemeLexersHighlighter::configureTclLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerTCL *tclLexer = dynamic_cast<QsciLexerTCL *>(lexer);
  if (!tclLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  tclLexer->setDefaultPaper(colors.background);
  tclLexer->setDefaultColor(colors.foreground);
  tclLexer->setDefaultFont(_defaultFont);

  // Configure Tcl specific styles
  tclLexer->setColor(colors.foreground, QsciLexerTCL::Default);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Default);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Default);

  // Tcl Comments
  tclLexer->setColor(colors.comment, QsciLexerTCL::Comment);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Comment);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Comment);

  // Tcl Comment line
  tclLexer->setColor(colors.commentLine, QsciLexerTCL::CommentLine);
  tclLexer->setPaper(colors.background, QsciLexerTCL::CommentLine);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::CommentLine);

  // Tcl Comment box
  tclLexer->setColor(colors.commentDoc, QsciLexerTCL::CommentBox);
  tclLexer->setPaper(colors.background, QsciLexerTCL::CommentBox);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::CommentBox);

  // Tcl Comment block
  tclLexer->setColor(colors.commentDoc, QsciLexerTCL::CommentBlock);
  tclLexer->setPaper(colors.background, QsciLexerTCL::CommentBlock);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::CommentBlock);

  // Tcl Numbers
  tclLexer->setColor(colors.number, QsciLexerTCL::Number);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Number);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Number);

  // Tcl Keywords
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);

  // Tcl Strings
  tclLexer->setColor(colors.string, QsciLexerTCL::QuotedString);
  tclLexer->setPaper(colors.background, QsciLexerTCL::QuotedString);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::QuotedString);

  // Tcl Operators
  tclLexer->setColor(colors.operator_, QsciLexerTCL::Operator);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Operator);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Operator);

  // Tcl Identifiers
  tclLexer->setColor(colors.identifier, QsciLexerTCL::Identifier);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Identifier);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Identifier);

  // Tcl Substitution
  tclLexer->setColor(colors.entity, QsciLexerTCL::Substitution);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Substitution);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Substitution);

  // Tcl Substitution in braces
  tclLexer->setColor(colors.entity, QsciLexerTCL::SubstitutionBrace);
  tclLexer->setPaper(colors.background, QsciLexerTCL::SubstitutionBrace);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::SubstitutionBrace);

  // Tcl Modifier
  tclLexer->setColor(colors.preprocessor, QsciLexerTCL::Modifier);
  tclLexer->setPaper(colors.background, QsciLexerTCL::Modifier);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::Modifier);

  // Tcl Expand keyword
  tclLexer->setColor(colors.keyword2, QsciLexerTCL::ExpandKeyword);
  tclLexer->setPaper(colors.background, QsciLexerTCL::ExpandKeyword);
  tclLexer->setFont(keywordFont, QsciLexerTCL::ExpandKeyword);

  // Tcl TCL keywords
  tclLexer->setColor(colors.function_, QsciLexerTCL::TCLKeyword);
  tclLexer->setPaper(colors.background, QsciLexerTCL::TCLKeyword);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::TCLKeyword);

  // Tcl ITcl keywords
  tclLexer->setColor(colors.className, QsciLexerTCL::ITCLKeyword);
  tclLexer->setPaper(colors.background, QsciLexerTCL::ITCLKeyword);
  tclLexer->setFont(_defaultFont, QsciLexerTCL::ITCLKeyword);
}

void DarkThemeLexersHighlighter::configureLuaLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerLua *luaLexer = dynamic_cast<QsciLexerLua *>(lexer);
  if (!luaLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  luaLexer->setDefaultPaper(colors.background);
  luaLexer->setDefaultColor(colors.foreground);
  luaLexer->setDefaultFont(_defaultFont);

  // Configure Lua specific styles
  luaLexer->setColor(colors.foreground, QsciLexerLua::Default);
  luaLexer->setPaper(colors.background, QsciLexerLua::Default);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Default);

  // Lua Comments
  luaLexer->setColor(colors.comment, QsciLexerLua::Comment);
  luaLexer->setPaper(colors.background, QsciLexerLua::Comment);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Comment);

  // Lua Line comments
  luaLexer->setColor(colors.commentLine, QsciLexerLua::LineComment);
  luaLexer->setPaper(colors.background, QsciLexerLua::LineComment);
  luaLexer->setFont(_defaultFont, QsciLexerLua::LineComment);

  // Lua Numbers
  luaLexer->setColor(colors.number, QsciLexerLua::Number);
  luaLexer->setPaper(colors.background, QsciLexerLua::Number);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Number);

  // Lua Keywords
  luaLexer->setColor(colors.keyword, QsciLexerLua::Keyword);
  luaLexer->setPaper(colors.background, QsciLexerLua::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  luaLexer->setFont(keywordFont, QsciLexerLua::Keyword);

  // Lua Strings
  luaLexer->setColor(colors.string, QsciLexerLua::String);
  luaLexer->setPaper(colors.background, QsciLexerLua::String);
  luaLexer->setFont(_defaultFont, QsciLexerLua::String);

  // Lua Character (single character strings)
  luaLexer->setColor(colors.character, QsciLexerLua::Character);
  luaLexer->setPaper(colors.background, QsciLexerLua::Character);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Character);

  // Lua Literal strings
  luaLexer->setColor(colors.verbatimString, QsciLexerLua::LiteralString);
  luaLexer->setPaper(colors.background, QsciLexerLua::LiteralString);
  luaLexer->setFont(_defaultFont, QsciLexerLua::LiteralString);

  // Lua Preprocessor
  luaLexer->setColor(colors.preprocessor, QsciLexerLua::Preprocessor);
  luaLexer->setPaper(colors.background, QsciLexerLua::Preprocessor);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Preprocessor);

  // Lua Operators
  luaLexer->setColor(colors.operator_, QsciLexerLua::Operator);
  luaLexer->setPaper(colors.background, QsciLexerLua::Operator);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Operator);

  // Lua Identifiers
  luaLexer->setColor(colors.identifier, QsciLexerLua::Identifier);
  luaLexer->setPaper(colors.background, QsciLexerLua::Identifier);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Identifier);

  // Lua Unclosed strings
  luaLexer->setColor(colors.unclosedString, QsciLexerLua::UnclosedString);
  luaLexer->setPaper(colors.background, QsciLexerLua::UnclosedString);
  luaLexer->setFont(_defaultFont, QsciLexerLua::UnclosedString);

  // Lua Basic functions
  luaLexer->setColor(colors.function_, QsciLexerLua::BasicFunctions);
  luaLexer->setPaper(colors.background, QsciLexerLua::BasicFunctions);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  luaLexer->setFont(functionFont, QsciLexerLua::BasicFunctions);

  // Lua String, table & math functions
  luaLexer->setColor(colors.function_, QsciLexerLua::StringTableMathsFunctions);
  luaLexer->setPaper(colors.background,
                     QsciLexerLua::StringTableMathsFunctions);
  luaLexer->setFont(_defaultFont, QsciLexerLua::StringTableMathsFunctions);

  // Lua Coroutines, IO & system facilities
  luaLexer->setColor(colors.globalClass,
                     QsciLexerLua::CoroutinesIOSystemFacilities);
  luaLexer->setPaper(colors.background,
                     QsciLexerLua::CoroutinesIOSystemFacilities);
  luaLexer->setFont(_defaultFont, QsciLexerLua::CoroutinesIOSystemFacilities);

  // Lua Keywords set 5-8 (user defined)
  luaLexer->setColor(colors.keywordSet2, QsciLexerLua::KeywordSet5);
  luaLexer->setPaper(colors.background, QsciLexerLua::KeywordSet5);
  luaLexer->setFont(_defaultFont, QsciLexerLua::KeywordSet5);

  luaLexer->setColor(colors.keywordSet2, QsciLexerLua::KeywordSet6);
  luaLexer->setPaper(colors.background, QsciLexerLua::KeywordSet6);
  luaLexer->setFont(_defaultFont, QsciLexerLua::KeywordSet6);

  luaLexer->setColor(colors.keywordSet2, QsciLexerLua::KeywordSet7);
  luaLexer->setPaper(colors.background, QsciLexerLua::KeywordSet7);
  luaLexer->setFont(_defaultFont, QsciLexerLua::KeywordSet7);

  luaLexer->setColor(colors.keywordSet2, QsciLexerLua::KeywordSet8);
  luaLexer->setPaper(colors.background, QsciLexerLua::KeywordSet8);
  luaLexer->setFont(_defaultFont, QsciLexerLua::KeywordSet8);

  // Lua Labels
  luaLexer->setColor(colors.preprocessor, QsciLexerLua::Label);
  luaLexer->setPaper(colors.background, QsciLexerLua::Label);
  luaLexer->setFont(_defaultFont, QsciLexerLua::Label);
}

void DarkThemeLexersHighlighter::configurePascalLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerPascal *pascalLexer = dynamic_cast<QsciLexerPascal *>(lexer);
  if (!pascalLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  pascalLexer->setDefaultPaper(colors.background);
  pascalLexer->setDefaultColor(colors.foreground);
  pascalLexer->setDefaultFont(_defaultFont);

  // Configure Pascal specific styles
  pascalLexer->setColor(colors.foreground, QsciLexerPascal::Default);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Default);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::Default);

  // Pascal Identifiers
  pascalLexer->setColor(colors.identifier, QsciLexerPascal::Identifier);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Identifier);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::Identifier);

  // Pascal Comments
  pascalLexer->setColor(colors.comment, QsciLexerPascal::Comment);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Comment);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::Comment);

  // Pascal Comment lines
  pascalLexer->setColor(colors.commentLine, QsciLexerPascal::CommentLine);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::CommentLine);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::CommentLine);

  // Pascal Comment parenthesis
  pascalLexer->setColor(colors.comment, QsciLexerPascal::CommentParenthesis);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::CommentParenthesis);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::CommentParenthesis);

  // Pascal Preprocessor
  pascalLexer->setColor(colors.preprocessor, QsciLexerPascal::PreProcessor);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::PreProcessor);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::PreProcessor);

  // Pascal Preprocessor parenthesis
  pascalLexer->setColor(colors.preprocessor,
                        QsciLexerPascal::PreProcessorParenthesis);
  pascalLexer->setPaper(colors.background,
                        QsciLexerPascal::PreProcessorParenthesis);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::PreProcessorParenthesis);

  // Pascal Numbers
  pascalLexer->setColor(colors.number, QsciLexerPascal::Number);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Number);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::Number);

  // Pascal Hex numbers
  pascalLexer->setColor(colors.number, QsciLexerPascal::HexNumber);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::HexNumber);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::HexNumber);

  // Pascal Keywords
  pascalLexer->setColor(colors.keyword, QsciLexerPascal::Keyword);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  pascalLexer->setFont(keywordFont, QsciLexerPascal::Keyword);

  // Pascal Single quoted strings
  pascalLexer->setColor(colors.singleQuotedString,
                        QsciLexerPascal::SingleQuotedString);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::SingleQuotedString);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::SingleQuotedString);

  // Pascal Operators
  pascalLexer->setColor(colors.operator_, QsciLexerPascal::Operator);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Operator);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::Operator);

  // Pascal Asm block
  pascalLexer->setColor(colors.preprocessor, QsciLexerPascal::Asm);
  pascalLexer->setPaper(colors.background, QsciLexerPascal::Asm);
  pascalLexer->setFont(_defaultFont, QsciLexerPascal::Asm);
}

void DarkThemeLexersHighlighter::configureAsmLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerAsm *asmLexer = dynamic_cast<QsciLexerAsm *>(lexer);
  if (!asmLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  asmLexer->setDefaultPaper(colors.background);
  asmLexer->setDefaultColor(colors.foreground);
  asmLexer->setDefaultFont(_defaultFont);

  // Configure Assembly specific styles
  asmLexer->setColor(colors.foreground, QsciLexerAsm::Default);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Default);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::Default);

  // Assembly Comments
  asmLexer->setColor(colors.comment, QsciLexerAsm::Comment);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Comment);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::Comment);

  // Assembly Numbers
  asmLexer->setColor(colors.number, QsciLexerAsm::Number);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Number);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::Number);

  // Assembly Operators
  asmLexer->setColor(colors.operator_, QsciLexerAsm::Operator);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Operator);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::Operator);

  // Assembly Identifiers
  asmLexer->setColor(colors.identifier, QsciLexerAsm::Identifier);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Identifier);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::Identifier);

  // Assembly CPU Instructions
  asmLexer->setColor(colors.keyword, QsciLexerAsm::CPUInstruction);
  asmLexer->setPaper(colors.background, QsciLexerAsm::CPUInstruction);
  QFont instructionFont = _defaultFont;
  instructionFont.setBold(true);
  asmLexer->setFont(instructionFont, QsciLexerAsm::CPUInstruction);

  // Assembly Registers
  asmLexer->setColor(colors.globalClass, QsciLexerAsm::Register);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Register);
  QFont registerFont = _defaultFont;
  registerFont.setBold(true);
  asmLexer->setFont(registerFont, QsciLexerAsm::Register);

  // Assembly Directives
  asmLexer->setColor(colors.preprocessor, QsciLexerAsm::Directive);
  asmLexer->setPaper(colors.background, QsciLexerAsm::Directive);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::Directive);

  // Assembly Directive operands
  asmLexer->setColor(colors.preprocessor, QsciLexerAsm::DirectiveOperand);
  asmLexer->setPaper(colors.background, QsciLexerAsm::DirectiveOperand);
  asmLexer->setFont(_defaultFont, QsciLexerAsm::DirectiveOperand);
}

void DarkThemeLexersHighlighter::configureCMakeLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerCMake *cmakeLexer = dynamic_cast<QsciLexerCMake *>(lexer);
  if (!cmakeLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  cmakeLexer->setDefaultPaper(colors.background);
  cmakeLexer->setDefaultColor(colors.foreground);
  cmakeLexer->setDefaultFont(_defaultFont);

  // Configure CMake specific styles
  cmakeLexer->setColor(colors.foreground, QsciLexerCMake::Default);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::Default);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::Default);

  // CMake Comments
  cmakeLexer->setColor(colors.comment, QsciLexerCMake::Comment);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::Comment);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::Comment);

  // CMake Strings
  cmakeLexer->setColor(colors.string, QsciLexerCMake::String);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::String);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::String);

  // CMake Left quoted strings
  cmakeLexer->setColor(colors.string, QsciLexerCMake::StringLeftQuote);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::StringLeftQuote);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::StringLeftQuote);

  // CMake Right quoted strings
  cmakeLexer->setColor(colors.string, QsciLexerCMake::StringRightQuote);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::StringRightQuote);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::StringRightQuote);

  // CMake Functions
  cmakeLexer->setColor(colors.function_, QsciLexerCMake::Function);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::Function);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  cmakeLexer->setFont(functionFont, QsciLexerCMake::Function);

  // CMake Variables
  cmakeLexer->setColor(colors.globalClass, QsciLexerCMake::Variable);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::Variable);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::Variable);

  // CMake Labels
  cmakeLexer->setColor(colors.preprocessor, QsciLexerCMake::Label);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::Label);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::Label);

  // CMake Keywords defined by user
  cmakeLexer->setColor(colors.keywordSet2, QsciLexerCMake::KeywordSet3);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::KeywordSet3);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::KeywordSet3);

  // CMake WHILE/ENDWHILE blocks
  cmakeLexer->setColor(colors.keyword, QsciLexerCMake::BlockWhile);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::BlockWhile);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  cmakeLexer->setFont(keywordFont, QsciLexerCMake::BlockWhile);

  // CMake FOREACH/ENDFOREACH blocks
  cmakeLexer->setColor(colors.keyword, QsciLexerCMake::BlockForeach);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::BlockForeach);
  cmakeLexer->setFont(keywordFont, QsciLexerCMake::BlockForeach);

  // CMake IF/ENDIF blocks
  cmakeLexer->setColor(colors.keyword, QsciLexerCMake::BlockIf);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::BlockIf);
  cmakeLexer->setFont(keywordFont, QsciLexerCMake::BlockIf);

  // CMake MACRO/ENDMACRO blocks
  cmakeLexer->setColor(colors.keyword2, QsciLexerCMake::BlockMacro);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::BlockMacro);
  cmakeLexer->setFont(keywordFont, QsciLexerCMake::BlockMacro);

  // CMake Numbers
  cmakeLexer->setColor(colors.number, QsciLexerCMake::Number);
  cmakeLexer->setPaper(colors.background, QsciLexerCMake::Number);
  cmakeLexer->setFont(_defaultFont, QsciLexerCMake::Number);
}

void DarkThemeLexersHighlighter::configureDLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerD *dLexer = dynamic_cast<QsciLexerD *>(lexer);
  if (!dLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  dLexer->setDefaultPaper(colors.background);
  dLexer->setDefaultColor(colors.foreground);
  dLexer->setDefaultFont(_defaultFont);

  // Configure D specific styles
  dLexer->setColor(colors.foreground, QsciLexerD::Default);
  dLexer->setPaper(colors.background, QsciLexerD::Default);
  dLexer->setFont(_defaultFont, QsciLexerD::Default);

  // D Comments
  dLexer->setColor(colors.comment, QsciLexerD::Comment);
  dLexer->setPaper(colors.background, QsciLexerD::Comment);
  dLexer->setFont(_defaultFont, QsciLexerD::Comment);

  // D Line comments
  dLexer->setColor(colors.commentLine, QsciLexerD::CommentLine);
  dLexer->setPaper(colors.background, QsciLexerD::CommentLine);
  dLexer->setFont(_defaultFont, QsciLexerD::CommentLine);

  // D Doc comments
  dLexer->setColor(colors.commentDoc, QsciLexerD::CommentDoc);
  dLexer->setPaper(colors.background, QsciLexerD::CommentDoc);
  dLexer->setFont(_defaultFont, QsciLexerD::CommentDoc);

  // D Nested comments
  dLexer->setColor(colors.comment, QsciLexerD::CommentNested);
  dLexer->setPaper(colors.background, QsciLexerD::CommentNested);
  dLexer->setFont(_defaultFont, QsciLexerD::CommentNested);

  // D Numbers
  dLexer->setColor(colors.number, QsciLexerD::Number);
  dLexer->setPaper(colors.background, QsciLexerD::Number);
  dLexer->setFont(_defaultFont, QsciLexerD::Number);

  // D Keywords
  dLexer->setColor(colors.keyword, QsciLexerD::Keyword);
  dLexer->setPaper(colors.background, QsciLexerD::Keyword);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  dLexer->setFont(keywordFont, QsciLexerD::Keyword);

  // D Secondary keywords
  dLexer->setColor(colors.keyword2, QsciLexerD::KeywordSecondary);
  dLexer->setPaper(colors.background, QsciLexerD::KeywordSecondary);
  dLexer->setFont(keywordFont, QsciLexerD::KeywordSecondary);

  // D Doc keywords
  dLexer->setColor(colors.commentDoc, QsciLexerD::KeywordDoc);
  dLexer->setPaper(colors.background, QsciLexerD::KeywordDoc);
  dLexer->setFont(keywordFont, QsciLexerD::KeywordDoc);

  // D Type definitions
  dLexer->setColor(colors.globalClass, QsciLexerD::Typedefs);
  dLexer->setPaper(colors.background, QsciLexerD::Typedefs);
  dLexer->setFont(keywordFont, QsciLexerD::Typedefs);

  // D Strings
  dLexer->setColor(colors.string, QsciLexerD::String);
  dLexer->setPaper(colors.background, QsciLexerD::String);
  dLexer->setFont(_defaultFont, QsciLexerD::String);

  // D Unclosed strings
  dLexer->setColor(colors.unclosedString, QsciLexerD::UnclosedString);
  dLexer->setPaper(colors.background, QsciLexerD::UnclosedString);
  dLexer->setFont(_defaultFont, QsciLexerD::UnclosedString);

  // D Characters
  dLexer->setColor(colors.character, QsciLexerD::Character);
  dLexer->setPaper(colors.background, QsciLexerD::Character);
  dLexer->setFont(_defaultFont, QsciLexerD::Character);

  // D Operators
  dLexer->setColor(colors.operator_, QsciLexerD::Operator);
  dLexer->setPaper(colors.background, QsciLexerD::Operator);
  dLexer->setFont(_defaultFont, QsciLexerD::Operator);

  // D Identifiers
  dLexer->setColor(colors.identifier, QsciLexerD::Identifier);
  dLexer->setPaper(colors.background, QsciLexerD::Identifier);
  dLexer->setFont(_defaultFont, QsciLexerD::Identifier);

  // D Doc line comments
  dLexer->setColor(colors.commentDoc, QsciLexerD::CommentLineDoc);
  dLexer->setPaper(colors.background, QsciLexerD::CommentLineDoc);
  dLexer->setFont(_defaultFont, QsciLexerD::CommentLineDoc);

  // D Keyword set 5-7 (user defined)
  dLexer->setColor(colors.keywordSet2, QsciLexerD::KeywordSet5);
  dLexer->setPaper(colors.background, QsciLexerD::KeywordSet5);
  dLexer->setFont(_defaultFont, QsciLexerD::KeywordSet5);

  dLexer->setColor(colors.keywordSet2, QsciLexerD::KeywordSet6);
  dLexer->setPaper(colors.background, QsciLexerD::KeywordSet6);
  dLexer->setFont(_defaultFont, QsciLexerD::KeywordSet6);

  dLexer->setColor(colors.keywordSet2, QsciLexerD::KeywordSet7);
  dLexer->setPaper(colors.background, QsciLexerD::KeywordSet7);
  dLexer->setFont(_defaultFont, QsciLexerD::KeywordSet7);
}

void DarkThemeLexersHighlighter::configurePov(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerPOV *povLexer = dynamic_cast<QsciLexerPOV *>(lexer);
  if (!povLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  povLexer->setDefaultPaper(colors.background);
  povLexer->setDefaultColor(colors.foreground);
  povLexer->setDefaultFont(_defaultFont);

  // Configure POV-Ray specific styles
  povLexer->setColor(colors.foreground, QsciLexerPOV::Default);
  povLexer->setPaper(colors.background, QsciLexerPOV::Default);
  povLexer->setFont(_defaultFont, QsciLexerPOV::Default);

  // POV Comments
  povLexer->setColor(colors.comment, QsciLexerPOV::Comment);
  povLexer->setPaper(colors.background, QsciLexerPOV::Comment);
  povLexer->setFont(_defaultFont, QsciLexerPOV::Comment);

  // POV Comment line
  povLexer->setColor(colors.commentLine, QsciLexerPOV::CommentLine);
  povLexer->setPaper(colors.background, QsciLexerPOV::CommentLine);
  povLexer->setFont(_defaultFont, QsciLexerPOV::CommentLine);

  // POV Numbers
  povLexer->setColor(colors.number, QsciLexerPOV::Number);
  povLexer->setPaper(colors.background, QsciLexerPOV::Number);
  povLexer->setFont(_defaultFont, QsciLexerPOV::Number);

  // POV Operators
  povLexer->setColor(colors.operator_, QsciLexerPOV::Operator);
  povLexer->setPaper(colors.background, QsciLexerPOV::Operator);
  povLexer->setFont(_defaultFont, QsciLexerPOV::Operator);

  // POV Identifiers
  povLexer->setColor(colors.identifier, QsciLexerPOV::Identifier);
  povLexer->setPaper(colors.background, QsciLexerPOV::Identifier);
  povLexer->setFont(_defaultFont, QsciLexerPOV::Identifier);

  // POV Strings
  povLexer->setColor(colors.string, QsciLexerPOV::String);
  povLexer->setPaper(colors.background, QsciLexerPOV::String);
  povLexer->setFont(_defaultFont, QsciLexerPOV::String);

  // POV Unclosed strings
  povLexer->setColor(colors.unclosedString, QsciLexerPOV::UnclosedString);
  povLexer->setPaper(colors.background, QsciLexerPOV::UnclosedString);
  povLexer->setFont(_defaultFont, QsciLexerPOV::UnclosedString);

  // POV Directives
  povLexer->setColor(colors.preprocessor, QsciLexerPOV::Directive);
  povLexer->setPaper(colors.background, QsciLexerPOV::Directive);
  povLexer->setFont(_defaultFont, QsciLexerPOV::Directive);

  // POV Bad directives
  povLexer->setColor(colors.error, QsciLexerPOV::BadDirective);
  povLexer->setPaper(colors.background, QsciLexerPOV::BadDirective);
  povLexer->setFont(_defaultFont, QsciLexerPOV::BadDirective);

  // POV Objects & CSG
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);

  // POV Predefined identifiers
  povLexer->setColor(colors.globalClass, QsciLexerPOV::PredefinedIdentifiers);
  povLexer->setPaper(colors.background, QsciLexerPOV::PredefinedIdentifiers);
  povLexer->setFont(_defaultFont, QsciLexerPOV::PredefinedIdentifiers);

  // POV Predefined functions
  povLexer->setColor(colors.function_, QsciLexerPOV::PredefinedFunctions);
  povLexer->setPaper(colors.background, QsciLexerPOV::PredefinedFunctions);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  povLexer->setFont(functionFont, QsciLexerPOV::PredefinedFunctions);

  // POV User defined 1-3
  povLexer->setColor(colors.keywordSet2, QsciLexerPOV::KeywordSet6);
  povLexer->setPaper(colors.background, QsciLexerPOV::KeywordSet6);
  povLexer->setFont(_defaultFont, QsciLexerPOV::KeywordSet6);

  povLexer->setColor(colors.keywordSet2, QsciLexerPOV::KeywordSet7);
  povLexer->setPaper(colors.background, QsciLexerPOV::KeywordSet7);
  povLexer->setFont(_defaultFont, QsciLexerPOV::KeywordSet7);

  povLexer->setColor(colors.keywordSet2, QsciLexerPOV::KeywordSet8);
  povLexer->setPaper(colors.background, QsciLexerPOV::KeywordSet8);
  povLexer->setFont(_defaultFont, QsciLexerPOV::KeywordSet8);
}

void DarkThemeLexersHighlighter::configureSpiceLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  QsciLexerSpice *spiceLexer = dynamic_cast<QsciLexerSpice *>(lexer);
  if (!spiceLexer)
    return;

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font
  spiceLexer->setDefaultPaper(colors.background);
  spiceLexer->setDefaultColor(colors.foreground);
  spiceLexer->setDefaultFont(_defaultFont);

  // Configure SPICE specific styles
  spiceLexer->setColor(colors.foreground, QsciLexerSpice::Default);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Default);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Default);

  // SPICE Identifiers
  spiceLexer->setColor(colors.identifier, QsciLexerSpice::Identifier);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Identifier);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Identifier);

  // SPICE Commands
  spiceLexer->setColor(colors.keyword, QsciLexerSpice::Command);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Command);
  QFont keywordFont = _defaultFont;
  keywordFont.setBold(true);
  spiceLexer->setFont(keywordFont, QsciLexerSpice::Command);

  // SPICE Functions
  spiceLexer->setColor(colors.function_, QsciLexerSpice::Function);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Function);
  QFont functionFont = _defaultFont;
  functionFont.setBold(true);
  spiceLexer->setFont(functionFont, QsciLexerSpice::Function);

  // SPICE Parameters
  spiceLexer->setColor(colors.globalClass, QsciLexerSpice::Parameter);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Parameter);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Parameter);

  // SPICE Numbers
  spiceLexer->setColor(colors.number, QsciLexerSpice::Number);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Number);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Number);

  // SPICE Delimiters
  spiceLexer->setColor(colors.operator_, QsciLexerSpice::Delimiter);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Delimiter);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Delimiter);

  // SPICE Values (like voltage/current values)
  spiceLexer->setColor(colors.number, QsciLexerSpice::Value);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Value);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Value);

  // SPICE Comments
  spiceLexer->setColor(colors.comment, QsciLexerSpice::Comment);
  spiceLexer->setPaper(colors.background, QsciLexerSpice::Comment);
  spiceLexer->setFont(_defaultFont, QsciLexerSpice::Comment);
}

void DarkThemeLexersHighlighter::configureCustomLexer(QsciLexer *lexer) {
  if (!lexer)
    return;

  // Note: QsciLexerCustom is a base class that may be subclassed
  // This configuration provides a generic dark theme for custom lexers

  const DarkThemeColors &colors = _colorSchemes[_currentScheme];

  // Set default colors and font for any custom lexer
  lexer->setDefaultPaper(colors.background);
  lexer->setDefaultColor(colors.foreground);
  lexer->setDefaultFont(_defaultFont);

  // Try to cast to specific custom lexer types if they exist
  // This is a flexible approach for handling various custom lexer
  // implementations

  // Check if it's a QsciLexerCustom instance
  QsciLexerCustom *customLexer = dynamic_cast<QsciLexerCustom *>(lexer);
  if (customLexer) {
    // Configure basic styles for custom lexer
    // Note: The exact style constants depend on the custom lexer implementation

    QFont keywordFont = _defaultFont;
    keywordFont.setBold(true);

    // Apply basic color scheme
    for (int style = 0; style < 32; ++style) {
      switch (style % 8) {
      case 0: // Default/Text
        customLexer->setColor(colors.foreground, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      case 1: // Keywords
        customLexer->setColor(colors.keyword, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(keywordFont, style);
        break;
      case 2: // Comments
        customLexer->setColor(colors.comment, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      case 3: // Strings
        customLexer->setColor(colors.string, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      case 4: // Numbers
        customLexer->setColor(colors.number, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      case 5: // Operators
        customLexer->setColor(colors.operator_, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      case 6: // Identifiers
        customLexer->setColor(colors.identifier, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      case 7: // Preprocessor/Special
        customLexer->setColor(colors.preprocessor, style);
        customLexer->setPaper(colors.background, style);
        customLexer->setFont(_defaultFont, style);
        break;
      }
    }
    return;
  }

  // For unknown custom lexers, apply a minimal color scheme
  // This ensures that even completely custom lexers get basic dark theme colors

  // Try to set common style IDs that most lexers use
  QColor textColor = colors.foreground;
  QColor backColor = colors.background;

  // Apply colors to common style indices
  for (int i = 0; i <= 15; ++i) {
    lexer->setColor(textColor, i);
    lexer->setPaper(backColor, i);
    lexer->setFont(_defaultFont, i);
  }

  // Set some common semantic colors if the lexer supports them
  // These are educated guesses for common style numbers
  lexer->setColor(colors.keyword, 1);   // Often keywords
  lexer->setColor(colors.comment, 2);   // Often comments
  lexer->setColor(colors.string, 3);    // Often strings
  lexer->setColor(colors.number, 4);    // Often numbers
  lexer->setColor(colors.operator_, 5); // Often operators

  // Set bold font for keywords (style 1)
  QFont boldFont = _defaultFont;
  boldFont.setBold(true);
  lexer->setFont(boldFont, 1);
}
