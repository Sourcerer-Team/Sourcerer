#include "containers/PleaseWaitContainer.h"
#include "containers/ui_PleaseWaitContainer.h"

namespace GuiManager {

namespace Containers {

PleaseWaitContainer::PleaseWaitContainer(QWidget *parent)
    : ContainerWidget(parent), ui(new Ui::PleaseWaitContainer) {
  ui->setupUi(this);
}

PleaseWaitContainer::~PleaseWaitContainer() { delete ui; }

void PleaseWaitContainer::start() { ui->spinnerWidget->start(); }

} // namespace Containers
} // namespace GuiManager
