#pragma once

#include <ConfigManager.h>
#include <GitRepo.h>

#include <Qsci/qsciscintilla.h>

#include <QStringList>
#include <QWidget>

class QEvent;

namespace GuiManager::Widgets {

class ScrollbarMarksWidget;

enum class MergeDiffResolutionType {
  Unknown = 0,

  Left,
  Right,
  LeftThenRight,
  RightThenLeft,

  last
};

// Line ranges of a conflict. In the top editors the ranges are half-open
// ([start, end)). In the bottom editor the ranges are inclusive and -1 marks
// a side that is not present; while unresolved, leftStart/rightEnd point to
// the conflict markers and leftEnd == rightStart to the separator.
struct MergeDiffRange {
  int leftStart = -1;
  int leftEnd = -1;
  int rightStart = -1;
  int rightEnd = -1;
  int blockStart = -1;
  int blockEnd = -1;
  QString startMarkerLabel;
  QString endMarkerLabel;
  MergeDiffResolutionType resolution = MergeDiffResolutionType::Unknown;
};

// Original text of a conflict, used to rebuild the merged content.
struct MergeDiffConflict {
  QString startMarkerLine;
  QString separatorLine;
  QString endMarkerLine;
  QStringList leftLines;
  QStringList rightLines;
};

struct MergeDiffContent {
  QString left;
  QString right;
  QString bottom;
  QVector<int> leftMarks0;
  QVector<int> leftMarks1;
  QVector<int> rightMarks0;
  QVector<int> rightMarks1;
  QVector<MergeDiffRange> rangesTop;
  QVector<MergeDiffRange> rangesBottom;
  // commonChunks[i] precedes conflicts[i]; the last chunk follows the last
  // conflict, so commonChunks.size() == conflicts.size() + 1.
  QVector<QStringList> commonChunks;
  QVector<MergeDiffConflict> conflicts;
  bool windowsFile = false;
};

class MergeDiffView : public QWidget {
  Q_OBJECT

public:
  MergeDiffView(QWidget *parent = nullptr);

  const QString &filePath() const { return _filePath; }

  void loadAndHighlight(const QString &contentWithMarkers,
                        const QString &filePath);

  const QString &mergedContent() const { return _content.bottom; }

  bool shouldConvertToWindowsFile() const { return _content.windowsFile; }

  bool areConflictsResolved() const;

  void clearContent();

private slots:
  void leftMarginClicked(int margin, int line, Qt::KeyboardModifiers state);
  void rightMarginClicked(int margin, int line, Qt::KeyboardModifiers state);
  void bottomMarginClicked(int margin, int line, Qt::KeyboardModifiers state);

protected:
  void changeEvent(QEvent *event) override;

private:
  int conflictsBeforeIndex(int index) const;
  void highlightConflictsInBottomEditor();
  void updateTopPlusMarkers();
  void applyConflictChoice(int rangeIndex, MergeDiffResolutionType side);
  void setupEditors(const QString &filePath);
  void refreshScintillaTheme();
  void syncScrollbars();

private:
  QString _filePath;
  MergeDiffContent _content;
  QsciScintilla *_leftEditor;
  QsciScintilla *_rightEditor;
  QsciScintilla *_bottomEditor;
  QsciLexer *_lexer = nullptr;
  ScrollbarMarksWidget *_leftMarks;
  ScrollbarMarksWidget *_rightMarks;
  ScrollbarMarksWidget *_bottomMarks;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
