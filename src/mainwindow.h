#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QGoodCentralWidget>
#include <QGoodWindow>

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QGoodWindow {
  Q_OBJECT

public:
  MainWindow(QWidget *parent = nullptr);
  ~MainWindow();

private:
  Ui::MainWindow *ui;
  QGoodCentralWidget *centralWidget;
};
#endif // MAINWINDOW_H
