#include "widgets/GitProgressToast.h"

#include "GitRepoManagerStructs.h"

#include <Dispatcher.h>
#include <Log.h>

#include <QVBoxLayout>

#include <functional>

#define THREAD_ID "GitProgressToast"

namespace GuiManager::Widgets {

auto FETCHING_STR = "Fetching...";
auto CHECKOUT_STR = "Checking out...";

GitProgressToast::GitProgressToast(const CloneProgress &data,
                                   const QString &title, QWidget *parent)
    : ToastWidget(parent), _data(data),
      _eventManager(EventManager::EventManager::getInstance()),
      _messageLabel(nullptr), _progressBar(nullptr) {
  LOG_PERF(0, "Enter");

  setTitleText(title);

  QWidget *widget = new QWidget(this);

  _messageLabel = new QLabel(tr(FETCHING_STR), widget);
  _messageLabel->setWordWrap(true);

  _progressBar = new QProgressBar(widget);

  QVBoxLayout *widgetLayout = new QVBoxLayout(widget);
  widgetLayout->setContentsMargins(8, 8, 8, 8);
  widgetLayout->setSizeConstraint(QLayout::SetMinimumSize);
  widgetLayout->addWidget(_messageLabel);
  widgetLayout->addWidget(_progressBar);

  layout()->addWidget(widget);

  setStyleSheet("font-size:14px;color:white;background-color:#384846;");

  connect(this, &GitProgressToast::finished, this,
          &GitProgressToast::onFinished);

  const auto callbackFunc =
      std::bind(&GitProgressToast::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoProgress, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitProgressToast::~GitProgressToast() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc =
      std::bind(&GitProgressToast::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoProgress, callbackFunc));

  LOG_PERF(0, "Exit");
}

void GitProgressToast::onFinished() {
  LOG_PERF(0, "Enter");

  stop();

  LOG_PERF(0, "Exit");
}

void GitProgressToast::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerCloneRepoProgress: {
    const GitRepoManager::Structs::GitRepoResult commandResult =
        event.data<GitRepoManager::Structs::GitRepoResult>();
    if (commandResult.uuid != _data.uuid) {
      break;
    }

    const CloneProgress data = commandResult.value.value<CloneProgress>();
    if (data.uuid != _data.uuid)
      break;

    if (data.finished)
      emit finished();
    else
      Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(), [=]() {
        _progressBar->setRange(0, data.totalSteps);
        _progressBar->setValue(data.completedSteps);
        _messageLabel->setText(data.stage == CloneProgressStageType::Checkout
                                   ? tr(CHECKOUT_STR)
                                   : tr(FETCHING_STR));
      });
    break;
  }
  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets