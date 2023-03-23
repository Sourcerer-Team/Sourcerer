#pragma once

#include "widgets/ContainerWidget.h"
#include <delegates/HomeRecentItemDelegate.h>
#include "models/RecentlyOpenedReposModel.h"

#include <QModelIndex>
#include <QWidget>

namespace Ui {
class HomeContainer;
}

namespace GuiManager {

namespace Containers {

using namespace Delegates;
using namespace Models;
using namespace Widgets;

class HomeContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit HomeContainer(QWidget *parent = nullptr);
  virtual ~HomeContainer();

  void reloadRecentlyOpened();

private slots:
  void hoverEnter(const QModelIndex& index);
  void hoverLeave(const QModelIndex& index);

private:
  Ui::HomeContainer *ui;
  RecentlyOpenedReposModel *_recentlyOpenedModel;
  HomeRecentItemDelegate *_itemDelegate;
};

} // namespace Containers
} // namespace GuiManager
