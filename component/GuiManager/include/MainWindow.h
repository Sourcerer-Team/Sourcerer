#pragma once

#include <QGoodCentralWidget>
#include <QGoodWindow>

namespace GuiManager {

class MainWindow : public QGoodWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private:
  QGoodCentralWidget *centralWidget;
};

} // namespace GuiManager
