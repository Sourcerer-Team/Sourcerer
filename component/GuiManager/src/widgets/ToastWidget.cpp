#include "widgets/ToastWidget.h"
#include "widgets/ToastCloseButton.h"

#include <Log.h>

#include <QApplication>
#include <QColor>
#include <QPalette>
#include <QPushButton>
#include <QVBoxLayout>

#define THREAD_ID "ToastWidget"

namespace GuiManager::Widgets {

ToastWidget::ToastWidget(QWidget *parent)
    : QWidget{parent}, _titleWidget{nullptr} {
  LOG_PERF(0, "Enter");

  // Make this a top-level window so it can receive mouse events independently
  setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
  setAttribute(Qt::WA_TransparentForMouseEvents, false);
  setAttribute(Qt::WA_StyledBackground, true);

  _titleWidget = new QWidget(this);
  _titleWidget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

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
  closeButtonLayout->setContentsMargins(0, 0, 8, 0);
  closeButtonLayout->addWidget(closeButton);

  const auto titleLayout = new QHBoxLayout(_titleWidget);
  titleLayout->setSpacing(0);
  titleLayout->setContentsMargins(0, 4, 0, 4);
  titleLayout->addWidget(_titleLabel);
  titleLayout->addStretch();
  titleLayout->addLayout(closeButtonLayout);

  const auto layout = new QVBoxLayout(this);
  layout->setSpacing(0);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(_titleWidget);

  // Use QPalette for background color
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, QColor("#808a89"));
  setPalette(pal);

  // Stylesheet for text properties only
  setStyleSheet("font-size:14px;color:white;");

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

void ToastWidget::applyToastColors(const QColor &backgroundColor,
                                   const QColor &titleColor,
                                   const QColor &textColor) {
  LOG_PERF(0, "Enter");

  // Use QPalette for background color
  setAutoFillBackground(true);
  QPalette pal = palette();
  pal.setColor(QPalette::Window, backgroundColor);
  pal.setColor(QPalette::WindowText, textColor);
  pal.setColor(QPalette::Text, textColor);
  pal.setColor(QPalette::ButtonText, textColor);
  setPalette(pal);

  // Enable styled background for all children so stylesheet works
  QList<QWidget *> children = findChildren<QWidget *>();
  for (QWidget *child : children) {
    child->setAttribute(Qt::WA_StyledBackground, true);
    child->setAutoFillBackground(true);
    child->setPalette(pal);
  }

  // Force all child widgets to use the background color
  setStyleSheet(QString("* { font-size:14px; color:%2; background-color:%1; }"
                        "QPushButton { background-color: transparent; border: "
                        "none; color: %2; }")
                    .arg(backgroundColor.name(), textColor.name()));

  // Update title label to use title color background
  if (getTitleLabel()) {
    getTitleLabel()->setStyleSheet(
        QString("font-size: 16px; font-weight: bold; "
                "background-color: %1; color: %2; padding: 4")
            .arg(titleColor.name(), textColor.name()));
  }

  // Ensure all QWidget children (including message container) have background
  // color
  for (QWidget *child : children) {
    if (QString(child->metaObject()->className()) == "QWidget") {
      child->setStyleSheet(
          QString("background-color: %1;").arg(backgroundColor.name()));
    }
  }

  // Explicitly handle all QLabel widgets except the title label
  QList<QLabel *> labels = findChildren<QLabel *>();
  for (QLabel *label : labels) {
    if (label != getTitleLabel()) {
      label->setStyleSheet(QString("background-color: %1; color: %2;")
                               .arg(backgroundColor.name(), textColor.name()));
    }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
