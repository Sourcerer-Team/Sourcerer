#include "PleaseWaitContainer.h"
#include "ui_PleaseWaitContainer.h"

namespace GuiManager {

PleaseWaitContainer::PleaseWaitContainer(QWidget *parent)
    : ContainerWidget(parent), ui(new Ui::PleaseWaitContainer) {
  ui->setupUi(this);
}

PleaseWaitContainer::~PleaseWaitContainer() { delete ui; }

void PleaseWaitContainer::start() {
  ui->spinnerWidget->start();
}

} // namespace GuiManager
