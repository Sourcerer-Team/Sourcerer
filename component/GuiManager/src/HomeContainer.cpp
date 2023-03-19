#include "HomeContainer.h"
#include "ui_HomeContainer.h"

namespace GuiManager {

HomeContainer::HomeContainer(QWidget *parent)
    : ContainerWidget(parent), ui(new Ui::HomeContainer) {
  ui->setupUi(this);
}

HomeContainer::~HomeContainer() { delete ui; }

} // namespace GuiManager
