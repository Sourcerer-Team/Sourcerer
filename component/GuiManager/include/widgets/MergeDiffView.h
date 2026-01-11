#pragma once

#include <ConfigManager.h>
#include <GitRepo.h>

#include <Qsci/qsciscintilla.h>

#include <QStringList>
#include <QWidget>

namespace GuiManager::Widgets {

class MergeDiffView : public QWidget {
  Q_OBJECT

public:
  MergeDiffView(QWidget *parent = nullptr);

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
