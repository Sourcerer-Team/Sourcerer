#include "widgets/ToastWidget.h"
#include "widgets/ToastCloseButton.h"

#include <Log.h>

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

#define THREAD_ID "ToastWidget"

namespace GuiManager {

namespace Widgets {

ToastWidget::ToastWidget(QWidget *parent) : QWidget{parent} {
  LOG_PERF(0, "Enter");

  QWidget *titleWidget = new QWidget(this);

  ToastCloseButton *closeButton = new ToastCloseButton(this);
  closeButton->setMaximumSize(16, 16);

  QHBoxLayout *titleLayout = new QHBoxLayout(titleWidget);
  titleLayout->setSpacing(0);
  titleLayout->setContentsMargins(0, 0, 0, 0);
  titleLayout->addStretch();
  titleLayout->addWidget(closeButton);

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(titleWidget);

  setLayout(layout);

  setStyleSheet("font-size:14px;color:white;background-color:#55aa7f;");

  connect(closeButton, &ToastCloseButton::clicked, this,
          &ToastWidget::onCloseButton);

  LOG_PERF(0, "Exit");
}

ToastWidget::~ToastWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void ToastWidget::stop() {
  LOG_PERF(0, "Enter");

  emit stopped(this);

  LOG_PERF(0, "Exit");
}

QSize ToastWidget::sizeHint() const {
  QRect boundingRect;
  QList<QWidget *> widgets = findChildren<QWidget *>();

  for (const auto p : widgets) {
    if (!p)
      continue;

    boundingRect = boundingRect.united(p->rect());
  }

  return boundingRect.size();
}

void ToastWidget::onCloseButton() {
  LOG_PERF(0, "Enter");

  stop();

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
