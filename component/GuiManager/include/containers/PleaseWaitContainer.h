#pragma once

#include "ContainerWidget.h"

#include <QWidget>

namespace Ui {
class PleaseWaitContainer;
}

namespace GuiManager {

namespace Containers {

class PleaseWaitContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit PleaseWaitContainer(QWidget *parent = nullptr);
  virtual ~PleaseWaitContainer();

  void start();

private:
  Ui::PleaseWaitContainer *ui;
};

} // namespace Containers
} // namespace GuiManager
