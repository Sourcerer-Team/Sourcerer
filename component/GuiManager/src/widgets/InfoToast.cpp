
#include "widgets/InfoToast.h"

#include <Log.h>

#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <Uuid.h>

#define THREAD_ID "InfoToast"

namespace GuiManager::Widgets {

InfoToast::InfoToast(const QString &message, QWidget *parent)
    : ToastWidget{parent}, _stopTimer{new QTimer},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  QWidget *widget = new QWidget(this);

  QLabel *messageLabel = new QLabel(message + "\n", widget);
  messageLabel->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
  messageLabel->setWordWrap(true);

  QVBoxLayout *widgetLayout = new QVBoxLayout(widget);
  widgetLayout->setContentsMargins(0, 0, 0, 0);
  widgetLayout->setSizeConstraint(QLayout::SetMinimumSize);
  widgetLayout->addWidget(messageLabel);

  layout()->addWidget(widget);

  const qlonglong displayTime =
      _configManager.getLong(ConfigManager::ConfigKey::InfoToastDisplayTime);

  _stopTimer->singleShot(displayTime, this, [=]() { stop(); });

  LOG_PERF(0, "Exit");
}

InfoToast::~InfoToast() {
  LOG_PERF(0, "Enter");

  delete _stopTimer;

  LOG_PERF(0, "Exit");
}

void InfoToast::stop() {
  LOG_PERF(0, "Enter");

  _stopTimer->stop();

  ToastWidget::stop();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets