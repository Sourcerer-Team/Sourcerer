#include "widgets/ToastWidget.h"
#include "widgets/ToastCloseButton.h"

#include <Log.h>

#include <QApplication>
#include <QVBoxLayout>

#define THREAD_ID "ToastWidget"

namespace GuiManager::Widgets {

ToastWidget::ToastWidget(QWidget *parent)
    : QWidget{parent}, _titleWidget{nullptr} {
  LOG_PERF(0, "Enter");

  // Make this a top-level window so it can receive mouse events independently
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  setAttribute(Qt::WA_TransparentForMouseEvents, false);

  _titleWidget = new QWidget(this);

  _titleLabel = new QLabel(_titleWidget);
  _titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; "
                                     "background-color: %1; padding: 4")
                                 .arg(QColor(Qt::darkYellow).name()));

  const auto closeButton = new ToastCloseButton(this);
  closeButton->setMaximumSize(16, 16);
  closeButton->setAttribute(Qt::WA_TransparentForMouseEvents, false);

  const auto closeButtonLayout = new QVBoxLayout();
  closeButtonLayout->setAlignment(Qt::AlignTop | Qt::AlignRight);
  closeButtonLayout->setSpacing(0);
  closeButtonLayout->setContentsMargins(0, 0, 0, 0);
  closeButtonLayout->addWidget(closeButton);

  const auto titleLayout = new QHBoxLayout(_titleWidget);
  titleLayout->setSpacing(0);
  titleLayout->setContentsMargins(0, 8, 0, 8);
  titleLayout->addWidget(_titleLabel);
  titleLayout->addStretch();
  titleLayout->addLayout(closeButtonLayout);

  const auto layout = new QVBoxLayout(this);
  layout->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 8);
  layout->addWidget(_titleWidget);

  setStyleSheet("font-size:14px;color:white;background-color:#808a89;");

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

void ToastWidget::setTitleText(const QString &text) const {
  LOG_PERF(0, "Enter");

  _titleLabel->setText(text);

  LOG_PERF(0, "Exit");
}

void ToastWidget::hideTitle() const {
  LOG_PERF(0, "Enter");

  _titleWidget->setVisible(false);

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

} // namespace GuiManager::Widgets
