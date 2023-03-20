#pragma once

#include "ContainerWidget.h"
#include "models/RecentlyOpenedReposModel.h"

#include <QWidget>

namespace Ui {
class HomeContainer;
}

namespace GuiManager {

namespace Containers {

using namespace Models;

class HomeContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit HomeContainer(QWidget *parent = nullptr);
  virtual ~HomeContainer();

  void reloadRecentlyOpened();

private:
  Ui::HomeContainer *ui;
  RecentlyOpenedReposModel *_recentlyOpenedModel;
};

} // namespace Containers
} // namespace GuiManager
