#include "widgets/GitCredentialsOverlayWidget.h"
#include "widgets/OpagueGroupBox.h"
#include "widgets/OpaqueLabel.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#define THREAD_ID "GitCredentialsOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

GitCredentialsOverlayWidget::GitCredentialsOverlayWidget(uint id,
                                                         QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _eventManager(::EventManager::EventManager::getInstance()),
      _usernameLineEdit(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _passwordLineEdit(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _showing(false), _holder(QSharedPointer<bool>::create(false)), _id(id) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(QColor::fromString("#384D59"), this);
  groupBox->setObjectName("groupBoxGiCredentialsTextOverlayWidgetCredentials");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(640);
  groupBox->setMaximumWidth(640);
  groupBox->setMinimumHeight(100);
  groupBox->setMaximumHeight(100);

  _usernameLineEdit->setParent(groupBox);
  _usernameLineEdit->setObjectName(
      "lineEditTextGitCredentialsOverlayWidgetUsername");
  _usernameLineEdit->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
  _usernameLineEdit->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _usernameLineEdit->setMaximumWidth(250);
  _usernameLineEdit->setMaximumWidth(250);
  _usernameLineEdit->setFocus();
  _usernameLineEdit->setPlaceholderText(tr("Enter username"));

  _passwordLineEdit->setParent(groupBox);
  _passwordLineEdit->setObjectName(
      "lineEditTextGitCredentialsOverlayWidgetPassword");
  _passwordLineEdit->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
  _passwordLineEdit->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _passwordLineEdit->setMaximumWidth(250);
  _passwordLineEdit->setMaximumWidth(250);
  _passwordLineEdit->setPlaceholderText(tr("Enter password"));

  _message = new OpaqueLabel(QColor::fromString("#384D59"), this);

  QPalette palette = _message->palette();
  palette.setColor(QPalette::WindowText, Qt::darkRed);
  _message->setPalette(palette);

  _submitButton = new OpaquePushButton(QColor::fromString("#384D59"), groupBox);
  _submitButton->setObjectName("pushButtonTextOverlayWidgetInputSubmit");
  _submitButton->setMaximumWidth(32);
  _submitButton->setMaximumHeight(32);
  _submitButton->setIcon(QPixmap(":icons/ok.png"));
  _submitButton->setFlat(false);

  const auto pushButtonCancel =
      new OpaquePushButton(QColor::fromString("#384D59"), groupBox);
  pushButtonCancel->setObjectName("pushButtonTextOverlayWidgetInputCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(QPixmap(":icons/cancel.png"));
  pushButtonCancel->setFlat(false);

  const auto horizontalLayout = new QHBoxLayout();
  horizontalLayout->setAlignment(Qt::AlignCenter);
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->setSpacing(6);
  horizontalLayout->addWidget(_usernameLineEdit);
  horizontalLayout->addWidget(_passwordLineEdit);
  horizontalLayout->addWidget(_submitButton);
  horizontalLayout->addWidget(pushButtonCancel);

  const auto horizontalLayoutMessage = new QHBoxLayout();
  horizontalLayoutMessage->setAlignment(Qt::AlignCenter);
  horizontalLayoutMessage->setContentsMargins(0, 0, 0, 0);
  horizontalLayoutMessage->addWidget(_message);

  const auto verticalLayout = new QVBoxLayout();
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  verticalLayout->setSpacing(12);
  verticalLayout->addLayout(horizontalLayoutMessage);
  verticalLayout->addLayout(horizontalLayout);

  const auto horizontalLayoutGroupBox = new QVBoxLayout(groupBox);
  horizontalLayoutGroupBox->setAlignment(Qt::AlignCenter);
  horizontalLayoutGroupBox->setContentsMargins(6, 6, 6, 6);
  horizontalLayoutGroupBox->setSpacing(6);
  horizontalLayoutGroupBox->addLayout(verticalLayout);

  getLayout()->addWidget(groupBox);

  connect(_submitButton, &QPushButton::clicked, this,
          &GitCredentialsOverlayWidget::onSubmitClicked);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &GitCredentialsOverlayWidget::onCancelClicked);

  _usernameLineEdit->installEventFilter(this);
  _passwordLineEdit->installEventFilter(this);

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

  _eventManager.unsubscribe(CAPTURE_CALLBACK(
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

  _message->setText(tr("Invalid credentials"));

  _passwordLineEdit->setFocus();
  _passwordLineEdit->selectAll();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
