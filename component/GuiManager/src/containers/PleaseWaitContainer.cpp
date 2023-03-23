#include "containers/PleaseWaitContainer.h"
#include "containers/ui_PleaseWaitContainer.h"

#include <Log.h>

#define THREAD_ID "PleaseWaitContainer"

namespace GuiManager {

namespace Containers {

PleaseWaitContainer::PleaseWaitContainer(QWidget *parent)
    : ContainerWidget(ContainerType::PleaseWait, parent),
      ui(new Ui::PleaseWaitContainer) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

PleaseWaitContainer::~PleaseWaitContainer() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void PleaseWaitContainer::start() {
  LOG_PERF(0, "Enter");

  ui->spinnerWidget->start();

  LOG_PERF(0, "Exit");
}

} // namespace Containers
} // namespace GuiManager
