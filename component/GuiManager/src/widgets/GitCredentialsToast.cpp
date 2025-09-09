#include "widgets/GitCredentialsToast.h"

#include <Dispatcher.h>
#include <Log.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <functional>

#define THREAD_ID "GitCredentialsToast"

namespace GuiManager {

namespace Widgets {

GitCredentialsToast::GitCredentialsToast(const GitCredentialsRequest &request,
                                         QWidget *parent)
    : ToastWidget(parent), _request(request),
      _eventManager(EventManager::EventManager::getInstance()),
      _usernameLineEdit(nullptr), _passwordLineEdit(nullptr) {
  LOG_PERF(0, "Enter");

  setTitleText(tr("Git credentials"));

  QWidget *widget = new QWidget(this);

  QLabel *messageLabel = new QLabel(tr("Please enter git credentials: "), widget);
  messageLabel->setWordWrap(true);

  QLabel *usernameLabel = new QLabel(tr("Username:"), widget);
  _usernameLineEdit = new QLineEdit(widget);
  _usernameLineEdit->setText(request.username);
  _usernameLineEdit->setReadOnly(!request.username.isEmpty());
  _usernameLineEdit->setEnabled(request.username.isEmpty());

  QLabel *passwordLabel = new QLabel(tr("Password:"), widget);
  _passwordLineEdit = new QLineEdit(widget);
  _passwordLineEdit->setEchoMode(QLineEdit::Password);

  if (request.username.isEmpty())
    _usernameLineEdit->setFocus();
  else
    _passwordLineEdit->setFocus();

  QPushButton *submitButton = new QPushButton(tr("Submit"), widget);
  submitButton->setFlat(true);
  submitButton->setCursor(Qt::PointingHandCursor);

  QPushButton *cancelButton = new QPushButton(tr("Cancel"), widget);
  cancelButton->setFlat(true);
  cancelButton->setCursor(Qt::PointingHandCursor);

  QVBoxLayout *labelsLayout = new QVBoxLayout();
  labelsLayout->setContentsMargins(0, 0, 0, 0);
  labelsLayout->setSizeConstraint(QLayout::SetMinimumSize);
  labelsLayout->addWidget(usernameLabel);
  labelsLayout->addWidget(passwordLabel);

  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  buttonsLayout->setContentsMargins(0, 0, 0, 0);
  buttonsLayout->setSizeConstraint(QLayout::SetMinimumSize);
  buttonsLayout->addWidget(cancelButton);
  buttonsLayout->addWidget(submitButton);

  QVBoxLayout *lineEditsLayout = new QVBoxLayout();
  lineEditsLayout->setContentsMargins(0, 0, 0, 0);
  lineEditsLayout->setSizeConstraint(QLayout::SetMinimumSize);
  lineEditsLayout->addWidget(_usernameLineEdit);
  lineEditsLayout->addWidget(_passwordLineEdit);

  QHBoxLayout *horizontalLayout = new QHBoxLayout();
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->setSizeConstraint(QLayout::SetMinimumSize);
  horizontalLayout->addLayout(labelsLayout);
  horizontalLayout->addLayout(lineEditsLayout);

  QVBoxLayout *widgetLayout = new QVBoxLayout(widget);
  widgetLayout->setSpacing(8);
  widgetLayout->setContentsMargins(8, 8, 8, 8);
  widgetLayout->setSizeConstraint(QLayout::SetMinimumSize);
  widgetLayout->addWidget(messageLabel);
  widgetLayout->addLayout(horizontalLayout);
  widgetLayout->addLayout(buttonsLayout);

  connect(cancelButton, &QPushButton::clicked, this,
          &GitCredentialsToast::onCancelClicked);

  connect(submitButton, &QPushButton::clicked, this,
          &GitCredentialsToast::onSubmitClicked);

  layout()->addWidget(widget);

  setStyleSheet("font-size:14px;color:white;background-color:#8B4000;");

  const auto callbackFunc = std::bind(&GitCredentialsToast::eventCallback, this,
                                      std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitCredentialsToast::~GitCredentialsToast() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc = std::bind(&GitCredentialsToast::eventCallback, this,
                                      std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));

  LOG_PERF(0, "Exit");
}

void GitCredentialsToast::onCancelClicked() {
  LOG_PERF(0, "Enter");

  GitCredentialsErrorResponse response;
  response.uuid = _request.uuid;

  EventManager::Event cancelEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsError);
  cancelEvent.addData(response);

  _eventManager.send(cancelEvent);

  stop();

  LOG_PERF(0, "Exit");
}

void GitCredentialsToast::onSubmitClicked() {
  LOG_PERF(0, "Enter");

  GitCredentialsResponse response;
  response.uuid = _request.uuid;
  response.username = _usernameLineEdit->text();
  response.password = _passwordLineEdit->text();

  EventManager::Event submitEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsResult);
  submitEvent.addData(response);

  _eventManager.send(submitEvent);

  stop();

  LOG_PERF(0, "Exit");
}

void GitCredentialsToast::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  const EventManager::EventType eventType = event.type();
  LOG_DEBUG("Processing event of type {}.", (int)eventType);

  switch (eventType) {
  case EventManager::EventType::GitRepoManagerCloneRepoError: {
    Util::Dispatcher::dispatchToMainThread(getSelf().toWeakRef(),
                                           [=]() { stop(); });
    break;
  }

  default: {
    LOG_WARN("Unhandled event.");
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace Widgets
} // namespace GuiManager
