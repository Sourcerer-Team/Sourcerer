#include "containers/HomeContainer.h"
#include "containers/ui_HomeContainer.h"
#include "delegates/HomeRecentItemDelegate.h"
#include <widgets/RecentTableView.h>

namespace GuiManager {

using namespace Delegates;

namespace Containers {

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Home, parent), ui(new Ui::HomeContainer),
      _recentlyOpenedModel(new RecentlyOpenedReposModel(this)),
      _itemDelegate(new HomeRecentItemDelegate(this)) {
  ui->setupUi(this);
  ui->recentTableView->setModel(_recentlyOpenedModel);
  ui->recentTableView->setItemDelegate(_itemDelegate);

  connect(ui->recentTableView, &RecentTableView::hoverEnter, this, &HomeContainer::hoverEnter);
  connect(ui->recentTableView, &RecentTableView::hoverLeave, this, &HomeContainer::hoverLeave);
}

HomeContainer::~HomeContainer() {
  delete ui;
  delete _recentlyOpenedModel;
}

void HomeContainer::reloadRecentlyOpened() { _recentlyOpenedModel->reload(); }

void HomeContainer::hoverEnter(const QModelIndex &index)
{
  if(index.column() != 0) {
    setCursor(Qt::ArrowCursor);
    return;
  }

  _itemDelegate->setHover(index);
  ui->recentTableView->viewport()->update();

  setCursor(Qt::PointingHandCursor);
}

void HomeContainer::hoverLeave(const QModelIndex &index)
{
  _itemDelegate->setHover(QModelIndex());
  ui->recentTableView->viewport()->update();

  setCursor(Qt::ArrowCursor);
}

} // namespace Containers
} // namespace GuiManager
