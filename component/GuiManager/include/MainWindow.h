#pragma once

#include "TitlebarWidget.h"
#include "ContainerWidget.h"

#include <ConfigManager.h>

#include <QGoodCentralWidget>
#include <QGoodWindow>

#include <QVector>

namespace GuiManager {

class MainWindow : public QGoodWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private:
  int _current;
  TitlebarWidget *_titleBarWidget;
  QGoodCentralWidget *_centralWidget;
  QVector<ContainerWidget*> _containers;
  ConfigManager::ConfigManager& _configManager;
};

} // namespace GuiManager
