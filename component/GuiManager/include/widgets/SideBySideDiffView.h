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

  void loadAndHighlight(const QString &leftContent,
                        const QString &rightContent);

private:
  void setupEditors();
  void syncScrollbars();

private:
  QsciScintilla *leftEditor;
  QsciScintilla *rightEditor;
  ::ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
