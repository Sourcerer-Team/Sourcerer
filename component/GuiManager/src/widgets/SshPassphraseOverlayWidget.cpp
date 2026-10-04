#include "widgets/SshPassphraseOverlayWidget.h"
#include "widgets/OpaqueGroupBox.h"
#include "widgets/OpaqueLabel.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QAction>
#include <QIcon>
#include <QLineEdit>
#include <QSize>
#include <QToolButton>

#define THREAD_ID "SshPassphraseOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

SshPassphraseOverlayWidget::SshPassphraseOverlayWidget(uint id, QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _eventManager(::EventManager::EventManager::getInstance()),
      _passphraseLineEdit(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _passphraseVisibilityAction(nullptr), _showing(false),
      _holder(QSharedPointer<bool>::create(false)), _id(id) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxSshPassphraseOverlayWidgetPassphrase");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(480);
  groupBox->setMaximumWidth(480);
  groupBox->setMinimumHeight(100);
  groupBox->setMaximumHeight(100);

  _passphraseLineEdit->setParent(groupBox);
  _passphraseLineEdit->setObjectName(
      "lineEditTextSshPassphraseOverlayWidgetPassphrase");
  _passphraseLineEdit->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Preferred);
  _passphraseLineEdit->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _passphraseLineEdit->setMaximumWidth(250);
  _passphraseLineEdit->setMaximumWidth(250);
  _passphraseLineEdit->setFocus();
  _passphraseLineEdit->setPlaceholderText(tr("Enter SSH key passphrase"));
  _passphraseLineEdit->setEchoMode(QLineEdit::Password);

  _passphraseVisibilityAction = _passphraseLineEdit->addAction(
      overlayButtonIcon(":/icons/eye.svg"), QLineEdit::TrailingPosition);
  _passphraseVisibilityAction->setCheckable(true);
  _passphraseVisibilityAction->setToolTip(tr("Show passphrase"));
  connect(_passphraseVisibilityAction, &QAction::toggled, this,
          [this](bool visible) {
            _passphraseLineEdit->setEchoMode(visible ? QLineEdit::Normal
                                                     : QLineEdit::Password);
            _passphraseVisibilityAction->setIcon(overlayButtonIcon(
                visible ? ":/icons/eye-off.svg" : ":/icons/eye.svg"));
            _passphraseVisibilityAction->setToolTip(
                visible ? tr("Hide passphrase") : tr("Show passphrase"));
          });

  if (const auto actionButtons =
          _passphraseLineEdit->findChildren<QToolButton *>();
      !actionButtons.isEmpty()) {
    auto *eyeButton = actionButtons.last();
    eyeButton->setObjectName("buttonSshPassphraseOverlayWidgetPassphraseEye");
    eyeButton->setAutoRaise(true);
    eyeButton->setCursor(Qt::PointingHandCursor);
    eyeButton->setIconSize(QSize(18, 18));
    eyeButton->setStyleSheet(
        "QToolButton { background: transparent; border: none; padding: 0px; "
        "margin: 0px; }");
  }

  _message = new OpaqueLabel(overlayBackgroundColor(), this);
  _message->setText(tr("SSH key is encrypted. Please enter the passphrase."));

  QPalette palette = _message->palette();
  palette.setColor(QPalette::WindowText, Qt::gray);
  _message->setPalette(palette);

  _submitButton = new OpaquePushButton(overlayBackgroundColor(), groupBox);
  _submitButton->setObjectName("pushButtonSshPassphraseOverlayWidgetSubmit");
  _submitButton->setMaximumWidth(32);
  _submitButton->setMaximumHeight(32);
  _submitButton->setIcon(overlayButtonIcon(":icons/ok.png"));
  _submitButton->setFlat(false);

  _cancelButton = new OpaquePushButton(overlayBackgroundColor(), groupBox);
  _cancelButton->setObjectName("pushButtonSshPassphraseOverlayWidgetCancel");
  _cancelButton->setMaximumWidth(32);
  _cancelButton->setMaximumHeight(32);
  _cancelButton->setIcon(overlayButtonIcon(":icons/cancel.png"));
  _cancelButton->setFlat(false);

  const auto horizontalLayout = new QHBoxLayout();
  horizontalLayout->setAlignment(Qt::AlignCenter);
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->setSpacing(6);
  horizontalLayout->addWidget(_passphraseLineEdit);
  horizontalLayout->addWidget(_submitButton);
  horizontalLayout->addWidget(_cancelButton);

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
          &SshPassphraseOverlayWidget::onSubmitClicked);
  connect(_cancelButton, &QPushButton::clicked, this,
          &SshPassphraseOverlayWidget::onCancelClicked);

  _passphraseLineEdit->installEventFilter(this);

  const auto callbackFunc = std::bind(
      &SshPassphraseOverlayWidget::eventCallback, this, std::placeholders::_1);

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

SshPassphraseOverlayWidget::~SshPassphraseOverlayWidget() {
  LOG_PERF(0, "Enter");

  const auto callbackFunc = std::bind(
      &SshPassphraseOverlayWidget::eventCallback, this, std::placeholders::_1);

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

void SshPassphraseOverlayWidget::setRequest(
    const GitRepo::GitCredentialsRequest &request) {
  LOG_PERF(0, "Enter");

  _request = request;

  _passphraseLineEdit->clear();
  if (_passphraseVisibilityAction) {
    _passphraseVisibilityAction->setChecked(false);
  }
  _passphraseLineEdit->setFocus();

  LOG_PERF(0, "Exit");
}

void SshPassphraseOverlayWidget::start() {
  LOG_PERF(0, "Enter");

  _showing = true;
  adjustToParent();

  LOG_PERF(0, "Exit");
}

void SshPassphraseOverlayWidget::stop() {
  LOG_PERF(0, "Enter");

  _showing = false;

  LOG_PERF(0, "Exit");
}

void SshPassphraseOverlayWidget::onCancelClicked() {
  LOG_PERF(0, "Enter");

  GitRepo::GitCredentialsErrorResponse response;
  response.id = _request.id;

  EventManager::Event cancelEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsCancel);
  cancelEvent.addData(response);

  _eventManager.send(cancelEvent);

  // Always close the overlay locally; the backend may no longer be waiting.
  // Queued because the owner deletes this widget in response to the signal.
  QMetaObject::invokeMethod(
      this, [this]() { emit finished(); }, Qt::QueuedConnection);

  LOG_PERF(0, "Exit");
}

void SshPassphraseOverlayWidget::onSubmitClicked() const {
  LOG_PERF(0, "Enter");

  GitRepo::GitCredentialsResponse response;
  response.id = _request.id;
  response.username = _request.username;
  response.passphrase = _passphraseLineEdit->text();
  response.rememberCredentials = false; // Don't remember passphrases
  response.needPassphrase = true;

  EventManager::Event submitEvent(
      EventManager::EventType::GitRepoManagerGetCredentialsSubmit);
  submitEvent.addData(response);

  _eventManager.send(submitEvent);

  LOG_PERF(0, "Exit");
}

void SshPassphraseOverlayWidget::eventCallback(EventManager::Event event) {
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
                                           [=]() { invalidPassphrase(); });
    break;
  }
  default: {
    break;
  }
  }

  LOG_PERF(0, "Exit");
}

void SshPassphraseOverlayWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
    if (_submitButton) {
      _submitButton->click();
      return;
    }
  } else if (event->key() == Qt::Key_Escape) {
    if (_cancelButton) {
      _cancelButton->click();
    }
    return;
  }

  OverlayWidget::keyPressEvent(event);
}

void SshPassphraseOverlayWidget::invalidPassphrase() const {
  LOG_PERF(0, "Enter");

  _message->setText(tr("Invalid passphrase. Please try again."));

  QPalette palette = _message->palette();
  palette.setColor(QPalette::WindowText, Qt::darkRed);
  _message->setPalette(palette);

  _passphraseLineEdit->setFocus();
  _passphraseLineEdit->selectAll();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
