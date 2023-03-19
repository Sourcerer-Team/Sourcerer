#pragma once

#include "ContainerWidget.h"

#include <QWidget>

namespace Ui {
class HomeContainer;
}

namespace GuiManager {

class HomeContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit HomeContainer(QWidget *parent = nullptr);
  virtual ~HomeContainer();

private:
  Ui::HomeContainer *ui;
};

} // namespace GuiManager
