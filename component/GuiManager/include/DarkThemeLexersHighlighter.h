#pragma once

#include <ConfigManager.h>

#include <QColor>
#include <QFont>
#include <QHash>
#include <QObject>

class QsciScintilla;
class QsciLexer;

namespace GuiManager {

class DarkThemeLexersHighlighter : public QObject {
  Q_OBJECT

public:
  explicit DarkThemeLexersHighlighter(QObject *parent = nullptr);
  ~DarkThemeLexersHighlighter();

  // Main configuration methods
  void applyDarkTheme(QsciScintilla *editor);
  void configureLexer(QsciLexer *lexer);

  // Color scheme management
  void setColorScheme(const QString &schemeName);
  QStringList availableColorSchemes() const;

  // Individual lexer configuration methods
  void configureCppLexer(QsciLexer *lexer);
  void configurePythonLexer(QsciLexer *lexer);
  void configureJavaScriptLexer(QsciLexer *lexer);
  void configureHtmlLexer(QsciLexer *lexer);
  void configureCssLexer(QsciLexer *lexer);
  void configureXmlLexer(QsciLexer *lexer);
  void configureJsonLexer(QsciLexer *lexer);
  void configureSqlLexer(QsciLexer *lexer);
  void configureBashLexer(QsciLexer *lexer);
  void configureBatchLexer(QsciLexer *lexer);
  void configureMakefileLexer(QsciLexer *lexer);
  void configureDiffLexer(QsciLexer *lexer);
  void configurePropertiesLexer(QsciLexer *lexer);
  void configureYamlLexer(QsciLexer *lexer);
  void configureMarkdownLexer(QsciLexer *lexer);
  void configureTexLexer(QsciLexer *lexer);
  void configurePostScriptLexer(QsciLexer *lexer);
  void configureMatlabLexer(QsciLexer *lexer);
  void configureOctaveLexer(QsciLexer *lexer);
  void configurePerlLexer(QsciLexer *lexer);
  void configureRubyLexer(QsciLexer *lexer);
  void configureFortranLexer(QsciLexer *lexer);
  void configureFortran77Lexer(QsciLexer *lexer);
  void configureIdlLexer(QsciLexer *lexer);
  void configureVhdlLexer(QsciLexer *lexer);
  void configureVerilogLexer(QsciLexer *lexer);
  void configureTclLexer(QsciLexer *lexer);
  void configureLuaLexer(QsciLexer *lexer);
  void configurePascalLexer(QsciLexer *lexer);
  void configureAsmLexer(QsciLexer *lexer);
  void configureCMakeLexer(QsciLexer *lexer);
  void configureDLexer(QsciLexer *lexer);
  void configurePov(QsciLexer *lexer);
  void configureSpiceLexer(QsciLexer *lexer);
  void configureCustomLexer(QsciLexer *lexer);

public slots:
  void refreshTheme();

signals:
  void themeChanged();

private:
  // Color definitions for dark theme
  struct DarkThemeColors {
    QColor background;
    QColor foreground;
    QColor selection;
    QColor selectionBackground;
    QColor currentLine;
    QColor lineNumbers;
    QColor lineNumbersBackground;
    QColor foldMargin;
    QColor foldMarginBackground;
    QColor indentationGuides;
    QColor braceMatch;
    QColor braceUnmatch;
    QColor caretLine;
    QColor caretForeground;

    // Syntax highlighting colors
    QColor keyword;
    QColor keyword2;
    QColor comment;
    QColor commentLine;
    QColor commentDoc;
    QColor number;
    QColor string;
    QColor character;
    QColor operator_;
    QColor identifier;
    QColor preprocessor;
    QColor unclosedString;
    QColor verbatimString;
    QColor regex;
    QColor doubleQuotedString;
    QColor singleQuotedString;
    QColor keywordSet2;
    QColor globalClass;
    QColor function_;
    QColor decorator;
    QColor className;
    QColor functionMethodName;
    QColor tag;
    QColor attribute;
    QColor value;
    QColor entity;
    QColor xmlKeyword;
    QColor xmlAttribute;
    QColor script;
    QColor asp;
    QColor phpDefault;
    QColor phpDoubleQuotedString;
    QColor phpSingleQuotedString;
    QColor phpKeyword;
    QColor phpNumber;
    QColor phpVariable;
    QColor phpComment;
    QColor phpCommentLine;
    QColor error;
    QColor warning;
    QColor info;
    QColor rawString;
  };

         // Helper methods
  void initializeColorSchemes();
  void applyEditorColors(QsciScintilla *editor);
  void setLexerColors(QsciLexer *lexer, const DarkThemeColors &colors);
  void configureLexerByType(QsciLexer *lexer);
  QString getLexerTypeName(QsciLexer *lexer) const;

  // Member variables
  QHash<QString, DarkThemeColors> _colorSchemes;
  QString _currentScheme;
  QFont _defaultFont;

  ConfigManager::ConfigManager &_configManager;

  // Constants
  static const DarkThemeColors DEFAULT_DARK_COLORS;
  static const DarkThemeColors MONOKAI_COLORS;
  static const DarkThemeColors DRACULA_COLORS;
  static const DarkThemeColors SOLARIZED_DARK_COLORS;
};

} // namespace GuiManager
