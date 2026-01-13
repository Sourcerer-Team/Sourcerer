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

  void loadAndHighlight(const QString &contentWithMarkers,
                        const QString &extension);

private:
  void setupEditors(const QString &extension);
  void syncScrollbars();

private:
  QsciScintilla *_leftEditor;
  QsciScintilla *_rightEditor;
  QsciScintilla *_bottomEditor;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
