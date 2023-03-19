#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QGoodWindow(parent)
    , centralWidget(new QGoodCentralWidget(this))
    //, ui(new Ui::MainWindow)
{
    //ui->setupUi(this);
    setCentralWidget(centralWidget);
}

MainWindow::~MainWindow()
{
    //delete ui;
}

