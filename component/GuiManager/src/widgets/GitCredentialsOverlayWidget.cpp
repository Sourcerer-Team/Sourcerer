#include "Widgets/GitCredentialsOverlayWidget.h"

#include <Log.h>

#define THREAD_ID "GitCredentialsOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

GitCredentialsOverlayWidget::GitCredentialsOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _eventManager(::EventManager::EventManager::getInstance()),
      _showing(false) {
  LOG_PERF(0, "Enter");

  const auto titleLabel = new QLabel(tr("Git Credentials"), this);
  titleLabel->setAlignment(Qt::AlignLeft);
  titleLabel->setMinimumWidth(400);
  titleLabel->setMaximumWidth(400);
  titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; "
                                    "background-color: %1; padding: 4")
                                .arg(QColor(Qt::darkYellow).name()));

  const auto widget = new QWidget(this);

  const auto messageLabel =
      new QLabel(tr("Please enter git credentials:"), this);
  messageLabel->setAlignment(Qt::AlignHCenter);
  messageLabel->setWordWrap(true);

  _usernameLineEdit = new QLineEdit(widget);
  _usernameLineEdit->setAlignment(Qt::AlignLeft);
  _usernameLineEdit->setFocus();
  _usernameLineEdit->setMinimumWidth(400);
  _usernameLineEdit->setMaximumWidth(400);
  _usernameLineEdit->setPlaceholderText(tr("Enter username"));

  _passwordLineEdit = new QLineEdit(widget);
  _passwordLineEdit->setEchoMode(QLineEdit::Password);
  _passwordLineEdit->setAlignment(Qt::AlignLeft);
  _passwordLineEdit->setMinimumWidth(400);
  _passwordLineEdit->setMaximumWidth(400);
  _passwordLineEdit->setPlaceholderText(tr("Enter password"));

  const auto submitButton = new QPushButton(tr("Submit"), this);
  submitButton->setFlat(true);
  submitButton->setCursor(Qt::PointingHandCursor);

  const auto cancelButton = new QPushButton(tr("Cancel"), this);
  cancelButton->setFlat(true);
  cancelButton->setCursor(Qt::PointingHandCursor);

  const auto editsLayout = new QVBoxLayout();
  editsLayout->addWidget(_usernameLineEdit);
  editsLayout->addWidget(_passwordLineEdit);

  const auto buttonsLayout = new QHBoxLayout();
  buttonsLayout->addWidget(submitButton);
  buttonsLayout->addSpacing(16);
  buttonsLayout->addWidget(cancelButton);

  const auto composedLayout4 = new QVBoxLayout();
  composedLayout4->addLayout(buttonsLayout);

  const auto composedLayout3 = new QHBoxLayout();
  composedLayout3->addLayout(editsLayout);

  const auto composedLayout2 = new QVBoxLayout();
  composedLayout2->addWidget(titleLabel);
  composedLayout2->addSpacing(16);
  composedLayout2->addWidget(messageLabel);
  composedLayout2->addSpacing(16);
  composedLayout2->addLayout(composedLayout3);
  composedLayout2->addLayout(composedLayout4);

  const auto composedLayout1 = new QHBoxLayout();
  composedLayout1->addStretch(1);
  composedLayout1->addLayout(composedLayout2);
  composedLayout1->addStretch(1);

  const auto composedLayout0 = new QVBoxLayout();
  composedLayout0->addStretch(1);
  composedLayout0->addLayout(composedLayout1);
  composedLayout0->addStretch(2);

  const auto widgetLayout = new QHBoxLayout(widget);
  widgetLayout->addLayout(composedLayout0);

  const auto layout = getLayout();
  Q_ASSERT(layout != nullptr);

  if (layout)
    layout->addWidget(widget);

  connect(cancelButton, &QPushButton::clicked, this,
          &GitCredentialsOverlayWidget::onCancelClicked);

  connect(submitButton, &QPushButton::clicked, this,
          &GitCredentialsOverlayWidget::onSubmitClicked);

  const auto callbackFunc = std::bind(
      &GitCredentialsOverlayWidget::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitCredentialsOverlayWidget::~GitCredentialsOverlayWidget() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc = std::bind(
      &GitCredentialsOverlayWidget::eventCallback, this, std::placeholders::_1);

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::setRequest(
    const GitRepo::GitCredentialsRequest &request) {
  LOG_PERF(0, "Enter");

  _request = request;

  _usernameLineEdit->setText(request.username);
  _usernameLineEdit->setReadOnly(!request.username.isEmpty());
  _usernameLineEdit->setEnabled(request.username.isEmpty());

  if (request.username.isEmpty())
    _usernameLineEdit->setFocus();
  else
    _passwordLineEdit->setFocus();

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::start() {
  LOG_PERF(0, "Enter");

  _showing = true;
  adjustToParent();

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::stop() {
  LOG_PERF(0, "Enter");

  _showing = false;

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::onCancelClicked() {
  LOG_PERF(0, "Enter");

  GitRepo::GitCredentialsErrorResponse response;
  response.uuid = _request.uuid;

  EventManager::Event cancelEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsError);
  cancelEvent.addData(response);

  _eventManager.send(cancelEvent);

  emit finished();

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::onSubmitClicked() {
  LOG_PERF(0, "Enter");

  GitRepo::GitCredentialsResponse response;
  response.uuid = _request.uuid;
  response.username = _usernameLineEdit->text();
  response.password = _passwordLineEdit->text();

  EventManager::Event submitEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsResult);
  submitEvent.addData(response);

  _eventManager.send(submitEvent);

  emit finished();

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  switch (const EventManager::EventType eventType = event.type()) {
  case EventManager::EventType::GitRepoManagerCloneRepoError: {
    const auto guard = getSelf().toWeakRef();
    Q_ASSERT(!guard.isNull());
    Util::Dispatcher::dispatchToMainThread(guard, [=]() { emit error(); });
    break;
  }

  default: {
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets