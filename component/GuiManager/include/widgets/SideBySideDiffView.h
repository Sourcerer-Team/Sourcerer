#pragma once

#include <ConfigManager.h>
#include <GitRepo.h>
#include <SvnRepo.h>

#include <Qsci/qsciscintilla.h>

#include <QStringList>
#include <QWidget>

class QEvent;

namespace GuiManager::Widgets {

struct SideBySideDiffContent {
  QString left;
  QString right;
  QVector<int> leftMarks0;
  QVector<int> leftMarks1;
  QVector<int> rightMarks0;
  QVector<int> rightMarks1;
};

class ScrollbarMarksWidget : public QWidget {
  Q_OBJECT

public:
  explicit ScrollbarMarksWidget(QScrollBar *scrollBar, QsciScintilla *editor,
                                QWidget *parent = nullptr);

  void setMarks(const QVector<int> &deletedLines,
                const QVector<int> &addedLines);

protected:
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  void updateGeometry();

private:
  QScrollBar *_scrollBar;
  QsciScintilla *_editor;
  QVector<int> _deletedLines;
  QVector<int> _addedLines;
};

class SideBySideDiffView : public QWidget {
  Q_OBJECT

public:
  SideBySideDiffView(QWidget *parent = nullptr);

  void loadAndHighlight(const QString &leftContent,
                        const GitRepo::FilesDiffDelta &rightDelta,
                        const QString &extension);
  void loadAndHighlight(const QString &leftContent,
                        const SvnRepo::FilesDiffDelta &rightDelta,
                        const QString &extension);
  void clearContent();

protected:
  void changeEvent(QEvent *event) override;

private:
  void setupEditors(const QString &extension);
  void refreshScintillaTheme();
  void syncScrollbars();

private:
  QsciScintilla *_leftEditor;
  QsciScintilla *_rightEditor;
  ScrollbarMarksWidget *_leftMarks;
  ScrollbarMarksWidget *_rightMarks;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
