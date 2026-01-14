#pragma once

#include <ConfigManager.h>
#include <GitRepo.h>

#include <Qsci/qsciscintilla.h>

#include <QStringList>
#include <QWidget>

namespace GuiManager::Widgets {

struct SideBySideDiffContent {
  QString left;
  QString right;
  QVector<int> leftMarks0;
  QVector<int> leftMarks1;
  QVector<int> rightMarks0;
  QVector<int> rightMarks1;
};

class SideBySideDiffView : public QWidget {
  Q_OBJECT

public:
  SideBySideDiffView(QWidget *parent = nullptr);

  void loadAndHighlight(const QString &leftContent,
                        const GitRepo::FilesDiffDelta &rightDelta,
                        const QString &extension);

private:
  void setupEditors(const QString &extension);
  void syncScrollbars();

private:
  QsciScintilla *_leftEditor;
  QsciScintilla *_rightEditor;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
