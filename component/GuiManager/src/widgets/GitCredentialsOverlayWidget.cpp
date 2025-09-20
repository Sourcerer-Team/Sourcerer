#include "Widgets/GitCredentialsOverlayWidget.h"

#include "spdlog/details/backtracer.h"

#include <Log.h>

#define THREAD_ID "GitCredentialsOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

GitCredentialsOverlayWidget::GitCredentialsOverlayWidget(uint id,
                                                         QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _eventManager(::EventManager::EventManager::getInstance()),
      _showing(false), _holder(QSharedPointer<bool>::create(false)), _id(id) {
  LOG_PERF(0, "Enter");

  const auto widget = new QWidget(this);

  const auto titleLabel = new QLabel(tr("Credentials"), widget);
  titleLabel->setAlignment(Qt::AlignLeft);
  titleLabel->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
  titleLabel->setStyleSheet(QString("font-size: 16px; font-weight: bold; "
                                    "background-color: %1; padding: 4")
                                .arg(QColor(Qt::darkYellow).name()));

  const auto titleLayout = new QHBoxLayout();
  titleLayout->addWidget(titleLabel);
  titleLayout->addStretch(1);

  _message = new QLabel(widget);

  _usernameLineEdit = new QLineEdit(widget);
  _usernameLineEdit->setAlignment(Qt::AlignLeft);
  _usernameLineEdit->setFocus();
  _usernameLineEdit->setMinimumWidth(250);
  _usernameLineEdit->setMaximumWidth(250);
  _usernameLineEdit->setPlaceholderText(tr("Enter username"));

  _passwordLineEdit = new QLineEdit(widget);
  _passwordLineEdit->setEchoMode(QLineEdit::Password);
  _passwordLineEdit->setAlignment(Qt::AlignLeft);
  _passwordLineEdit->setMinimumWidth(250);
  _passwordLineEdit->setMaximumWidth(250);
  _passwordLineEdit->setPlaceholderText(tr("Enter password"));

  _submitButton = new QPushButton(tr("Submit"), this);
  _submitButton->setFlat(true);
  _submitButton->setCursor(Qt::PointingHandCursor);
  _submitButton->setDefault(true);

  const auto cancelButton = new QPushButton(tr("Cancel"), this);
  cancelButton->setFlat(true);
  cancelButton->setCursor(Qt::PointingHandCursor);

  const auto idCardLabel = new QLabel(this);
  idCardLabel->setPixmap(QPixmap(":icons/id-card.png"));

  const auto usernameLayout = new QHBoxLayout();
  usernameLayout->addWidget(idCardLabel);
  usernameLayout->addWidget(_usernameLineEdit);

  const auto padlockLabel = new QLabel(this);
  padlockLabel->setPixmap(QPixmap(":icons/padlock.png"));

  const auto passwordLayout = new QHBoxLayout();
  passwordLayout->addWidget(padlockLabel);
  passwordLayout->addWidget(_passwordLineEdit);

  const auto editsLayout = new QVBoxLayout();
  editsLayout->addLayout(usernameLayout);
  editsLayout->addLayout(passwordLayout);

  const auto buttonsLayout = new QHBoxLayout();
  buttonsLayout->addWidget(_submitButton);
  buttonsLayout->addSpacing(16);
  buttonsLayout->addWidget(cancelButton);

  const auto composedLayout4 = new QVBoxLayout();
  composedLayout4->addLayout(buttonsLayout);

  const auto composedLayout3 = new QHBoxLayout();
  composedLayout3->addLayout(editsLayout);

  const auto composedLayout2 = new QVBoxLayout();
  composedLayout2->addLayout(titleLayout);
  composedLayout2->addSpacing(16);
  composedLayout2->addWidget(_message);
  composedLayout2->addSpacing(8);
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

  connect(_submitButton, &QPushButton::clicked, this,
          &GitCredentialsOverlayWidget::onSubmitClicked);

  const auto callbackFunc = std::bind(
      &GitCredentialsOverlayWidget::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCredentialsInvalid,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCredentialsFinished,
      callbackFunc));
  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerCloneRepoError, callbackFunc));

  LOG_PERF(0, "Exit");
}

GitCredentialsOverlayWidget::~GitCredentialsOverlayWidget() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc = std::bind(
      &GitCredentialsOverlayWidget::eventCallback, this, std::placeholders::_1);

  _eventManager.subscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCredentialsInvalid,
      callbackFunc));
  _eventManager.unsubscribe(CAPTURE_CALLBACK(
      EventManager::EventType::GitRepoManagerGetCredentialsFinished,
      callbackFunc));
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

void GitCredentialsOverlayWidget::onCancelClicked() const {
  LOG_PERF(0, "Enter");

  GitRepo::GitCredentialsErrorResponse response;
  response.id = _request.id;

  EventManager::Event cancelEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsCancel);
  cancelEvent.addData(response);

  _eventManager.send(cancelEvent);

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::onSubmitClicked() const {
  LOG_PERF(0, "Enter");

  GitRepo::GitCredentialsResponse response;
  response.id = _request.id;
  response.username = _usernameLineEdit->text();
  response.password = _passwordLineEdit->text();

  EventManager::Event submitEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit);
  submitEvent.addData(response);

  _eventManager.send(submitEvent);

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::eventCallback(EventManager::Event event) {
  LOG_PERF(0, "Enter");

  switch (const EventManager::EventType eventType = event.type()) {
  case EventManager::EventType::GitRepoManagerCloneRepoError: {
    const auto guard = _holder.toWeakRef();
    Q_ASSERT(!guard.isNull());
    Util::Dispatcher::dispatchToMainThread(guard, [=]() { emit error(); });
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCredentialsFinished: {
    if (const uint id = event.data<size_t>(); id != _id) {
      break;
    }
    const auto guard = _holder.toWeakRef();
    Q_ASSERT(!guard.isNull());
    Util::Dispatcher::dispatchToMainThread(guard, [=]() { emit finished(); });
    break;
  }
  case EventManager::EventType::GitRepoManagerGetCredentialsInvalid: {
    if (const uint id = event.data<size_t>(); id != _id) {
      break;
    }
    const auto guard = _holder.toWeakRef();
    Q_ASSERT(!guard.isNull());
    Util::Dispatcher::dispatchToMainThread(guard,
                                           [=]() { invalidCredentials(); });
    break;
  }
  default: {
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void GitCredentialsOverlayWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (_submitButton) {
      _submitButton->click();
      return;
    }
  }

  OverlayWidget::keyPressEvent(event);
}

void GitCredentialsOverlayWidget::invalidCredentials() const {
  LOG_PERF(0, "Enter");

  _message->setText(tr("Invalid credentials."));

  _passwordLineEdit->setFocus();
  _passwordLineEdit->selectAll();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets