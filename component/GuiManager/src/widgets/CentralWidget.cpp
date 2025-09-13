#include "widgets/CentralWidget.h"

#include <Log.h>

#include <QVBoxLayout>

#define THREAD_ID "CentralWidget"

namespace GuiManager::Widgets {

CentralWidget::CentralWidget(QWidget *parent)
    : QWidget{parent}, _centralWidget{nullptr} {
  LOG_PERF(0, "Enter");

  new QVBoxLayout(this);

  LOG_PERF(0, "Exit");
}

void CentralWidget::setCentralWidget(QWidget *widget) {
  LOG_PERF(0, "Enter");

  if(_centralWidget) {
    layout()->removeWidget(_centralWidget);
  }

  _centralWidget = widget;

  layout()->addWidget(_centralWidget);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
