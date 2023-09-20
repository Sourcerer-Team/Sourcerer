#pragma once

#include <QWidget>

namespace GuiManager {
namespace Widgets {

class CentralWidget : public QWidget {
  Q_OBJECT
public:
  explicit CentralWidget(QWidget *parent = nullptr);

  void setCentralWidget(QWidget *widget);

signals:

private:
  QWidget *_centralWidget;
};

} // namespace Widgets
} // namespace GuiManager
