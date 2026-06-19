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

  last
};

struct MergeDiffRange {
  int leftStart = -1;
  int leftEnd = -1;
  int rightStart = -1;
  int rightEnd = -1;
  int removedCount = -1;
  MergeDiffResolutionType resolution = MergeDiffResolutionType::Unknown;
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

private slots:
  void leftMarginClicked(int margin, int line, Qt::KeyboardModifiers state);
  void rightMarginClicked(int margin, int line, Qt::KeyboardModifiers state);

protected:
  void changeEvent(QEvent *event) override;

private:
  int conflictsBeforeIndex(int index) const;
  void highlightConflictsInBottomEditor();
  int removedLinesBeforeRange(int rangeIndex) const;
  void setupEditors(const QString &extension);
  void refreshScintillaTheme();
  void syncScrollbars();
  void updateBottomRangesFromIndex(int index,
                                   MergeDiffResolutionType resolution,
                                   int newStart, int newEnd);
  ;

private:
  QString _filePath;
  MergeDiffContent _content;
  QsciScintilla *_leftEditor;
  QsciScintilla *_rightEditor;
  QsciScintilla *_bottomEditor;
  ScrollbarMarksWidget *_leftMarks;
  ScrollbarMarksWidget *_rightMarks;
  ScrollbarMarksWidget *_bottomMarks;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
