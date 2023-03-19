#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QGoodWindow(parent), centralWidget(new QGoodCentralWidget(this)) {
  setCentralWidget(centralWidget);
}

MainWindow::~MainWindow() {}
