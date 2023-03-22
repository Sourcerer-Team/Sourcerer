#include "containers/HomeContainer.h"
#include "containers/ui_HomeContainer.h"

namespace GuiManager {

namespace Containers {

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(ContainerType::Home, parent), ui(new Ui::HomeContainer),
      _recentlyOpenedModel(new RecentlyOpenedReposModel(this)) {
  ui->setupUi(this);
  ui->recentTableView->setModel(_recentlyOpenedModel);
  ui->recentTableView->horizontalHeader()->setSectionResizeMode(
      QHeaderView::Stretch);
}

HomeContainer::~HomeContainer() {
  delete ui;
  delete _recentlyOpenedModel;
}

void HomeContainer::reloadRecentlyOpened() { _recentlyOpenedModel->reload(); }

} // namespace Containers
} // namespace GuiManager
