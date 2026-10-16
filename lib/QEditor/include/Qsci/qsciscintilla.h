#pragma once

#include "qscilexer.h"

#include <QColor>
#include <QHash>
#include <QPlainTextEdit>
#include <QSet>
#include <QString>
#include <QVector>

struct TSParser;
struct TSLanguage;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;
class QFocusEvent;
class QTimer;
class QWidget;
class QEditorTreeSitterHighlighter;

class QSCI_EXPORT QsciScintilla : public QPlainTextEdit {
  Q_OBJECT

  friend class QLineNumberArea;

public:
  enum BraceMatch { NoBraceMatch = 0, SloppyBraceMatch };
  enum FoldStyle { NoFoldStyle = 0, BoxedTreeFoldStyle };
  enum WrapMode { WrapNone = 0, WrapWord };
  enum MarginType { NumberMargin = 0, SymbolMargin };
  enum MarkerSymbol { Background = 0, BoxedPlus, BoxedArrowDown, BoxedCheck };

  explicit QsciScintilla(QWidget *parent = nullptr);
  ~QsciScintilla() override;

  void setText(const QString &text);
  QString text() const;
  QString text(int line) const;
  int lines() const;

  void setLexer(QsciLexer *lexer);
  QsciLexer *lexer() const { return _lexer; }

  void setPaper(const QColor &color);
  void setColor(const QColor &color);
  void setCaretLineBackgroundColor(const QColor &color);
  void setCaretLineVisible(bool visible);
  void setCaretForegroundColor(const QColor &color);
  void setSelectionBackgroundColor(const QColor &color);
  void setSelectionForegroundColor(const QColor &color);
  void setMarginLineNumbers(int margin, bool enabled);
  void setMarginBackgroundColor(int margin, const QColor &color);
  void setMarginsBackgroundColor(const QColor &color);
  void setMarginsForegroundColor(const QColor &color);
  void setMarginWidth(int margin, const QString &text);
  void setMarginWidth(int margin, int width);
  void setMarginText(int line, int margin, const QString &text);
  void setMarginTextColor(int margin, const QColor &color);
  void setMarginToolTip(int line, int margin, const QString &toolTip);
  void setMarginType(int margin, MarginType type);
  void setMarginSensitivity(int margin, bool sensitivity);
  void setMarginMarkerMask(int margin, int mask);
  void markerDefine(MarkerSymbol symbol, int marker);
  void markerDeleteAll();
  void setWrapMode(WrapMode mode);
  void setFolding(FoldStyle style);
  void setFoldMarginColors(const QColor &foreground, const QColor &background);
  void setIndentationsUseTabs(bool useTabs);
  void setAutoIndent(bool enabled);
  void setIndentationGuides(bool enabled);
  void setIndentationGuidesBackgroundColor(const QColor &color);
  void setIndentationGuidesForegroundColor(const QColor &color);
  void setBraceMatching(BraceMatch style);
  void setMatchedBraceBackgroundColor(const QColor &color);
  void setUnmatchedBraceBackgroundColor(const QColor &color);
  void setMarkerBackgroundColor(const QColor &color, int marker);
  void setMarkerForegroundColor(const QColor &color, int marker);
  void setMarkerToolTip(const QString &toolTip, int marker);
  int markerAdd(int line, int marker);
  void markerDelete(int line, int marker);
  int markersAtLine(int line) const;
  void setTreeSitterHighlightingEnabled(bool enabled);
  bool treeSitterHighlightingEnabled() const {
    return _treeSitterHighlightingEnabled;
  }
  // Treats the text as a unified diff: the old and new sides of each hunk are
  // highlighted with the lexer's language, the line prefixes and headers are
  // left as plain text.
  void setUnifiedDiffHighlighting(bool enabled);
  bool unifiedDiffHighlighting() const { return _unifiedDiffHighlighting; }

signals:
  void marginClicked(int margin, int line, Qt::KeyboardModifiers state);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void focusInEvent(QFocusEvent *event) override;

private slots:
  void refreshCurrentLineHighlight();

private:
  void applyCurrentPalette();
  void updateViewportMargins();
  int lineFromPoint(const QPoint &point) const;
  int marginLineAt(int y) const;
  int marginPixelWidth(int margin) const;
  int marginAtX(int x) const;
  QRect marginCellRect(int margin, int line) const;
  int clickableMarkerAt(int margin, int line) const;
  bool hasClickableMarginText(int margin, int line) const;
  void setHoveredMarginText(int margin, int line);
  int lineNumberAreaWidth() const;
  void lineNumberAreaPaintEvent(QPaintEvent *event);
  void updateLineNumberAreaWidth(int);
  void updateLineNumberArea(const QRect &rect, int dy);
  void updateTreeSitterHighlighting();
  void scheduleTreeSitterHighlighting();

private:
  QsciLexer *_lexer;
  QHash<int, QColor> _marginBackgroundColors;
  QColor _paper;
  QColor _color;
  QColor _caretLineBackground;
  QColor _caretForeground;
  QColor _selectionBackground;
  QColor _selectionForeground;
  QColor _marginsBackground;
  QColor _marginsForeground;
  QColor _foldMarginForeground;
  QColor _foldMarginBackground;
  QColor _indentGuidesBackground;
  QColor _indentGuidesForeground;
  QColor _matchedBraceBackground;
  QColor _unmatchedBraceBackground;
  bool _caretLineVisible;
  bool _indentationsUseTabs;
  bool _autoIndent;
  bool _indentationGuides;
  bool _marginLineNumbers[4];
  bool _marginSensitivity[4];
  int _marginTypes[4];
  int _marginMarkerMasks[4];
  int _marginWidths[4];
  FoldStyle _foldStyle;
  BraceMatch _braceMatch;
  QHash<int, QColor> _markerBackgroundColors;
  QHash<int, QColor> _markerForegroundColors;
  QHash<int, int> _markerSymbols;
  QHash<int, QString> _markerToolTips;
  QHash<int, QSet<int>> _markers;
  QHash<int, QHash<int, QString>> _marginTexts;
  QHash<int, QHash<int, QString>> _marginToolTips;
  QHash<int, QColor> _marginTextColors;
  int _hoveredMargin = -1;
  int _hoveredMarginLine = -1;
  QWidget *_lineNumberArea;
  const TSLanguage *_treeSitterLanguage;
  bool _updatingTreeSitterHighlighting;
  bool _treeSitterHighlightingEnabled;
  bool _unifiedDiffHighlighting;
  TSParser *_parser;
  QEditorTreeSitterHighlighter *_highlighter;
  QTimer *_highlightTimer;
};
