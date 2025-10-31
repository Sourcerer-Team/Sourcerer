#pragma once

#include <ConfigManager.h>

#include <Qsci/qsciscintilla.h>

#include <QStringList>
#include <QWidget>

namespace GuiManager::Widgets {

class SideBySideDiffView : public QWidget {
  Q_OBJECT

public:
  SideBySideDiffView(QWidget *parent = nullptr);

  void loadAndHighlight(const QString &leftContent, const QString &rightContent,
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
