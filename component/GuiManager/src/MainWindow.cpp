#include "MainWindow.h"

namespace GuiManager {

MainWindow::MainWindow(QWidget *parent)
    : QGoodWindow(parent), centralWidget(new QGoodCentralWidget(this)) {
  setCentralWidget(centralWidget);
}

MainWindow::~MainWindow() {}

} // namespace GuiManager
