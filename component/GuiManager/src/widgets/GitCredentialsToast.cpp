#include "widgets/GitCredentialsToast.h"

#include <Log.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#define THREAD_ID "GitCredentialsToast"

namespace GuiManager {

namespace Widgets {

GitCredentialsToast::GitCredentialsToast(const GitCredentialsRequest &request,
                                         QWidget *parent)
    : ToastWidget(parent), _request(request),
      _eventManager(EventManager::EventManager::getInstance()),
      _usernameLineEdit(nullptr), _passwordLineEdit(nullptr) {
  LOG_PERF(0, "Enter");

  QWidget *widget = new QWidget(this);

  QLabel *urlLabel =
      new QLabel(tr("Enter credentials for the following Git repository:") +
                 " " + request.url);
  urlLabel->setWordWrap(true);

  QLabel *usernameLabel = new QLabel(tr("Username:"), widget);
  _usernameLineEdit = new QLineEdit(widget);
  _usernameLineEdit->setText(request.username);

  QLabel *passwordLabel = new QLabel(tr("Password:"), widget);
  _passwordLineEdit = new QLineEdit(widget);

  QPushButton *submitButton = new QPushButton(tr("Submit"), widget);
  QPushButton *cancelButton = new QPushButton(tr("Cancel"), widget);

  QVBoxLayout *labelsLayout = new QVBoxLayout();
  labelsLayout->setContentsMargins(0, 0, 0, 0);
  labelsLayout->setSizeConstraint(QLayout::SetMinimumSize);
  labelsLayout->addWidget(usernameLabel);
  labelsLayout->addWidget(passwordLabel);

  QHBoxLayout *buttonsLayout = new QHBoxLayout();
  buttonsLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
  buttonsLayout->addWidget(cancelButton);
  buttonsLayout->addWidget(submitButton);
  buttonsLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

  QVBoxLayout *lineEditsLayout = new QVBoxLayout();
  lineEditsLayout->setContentsMargins(0, 0, 0, 0);
  lineEditsLayout->setSizeConstraint(QLayout::SetMinimumSize);
  lineEditsLayout->addWidget(_usernameLineEdit);
  lineEditsLayout->addWidget(_passwordLineEdit);

  QHBoxLayout *horizontalLayout = new QHBoxLayout();
  horizontalLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
  horizontalLayout->addLayout(labelsLayout);
  horizontalLayout->addLayout(lineEditsLayout);
  horizontalLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

  QVBoxLayout *widgetLayout = new QVBoxLayout(widget);
  widgetLayout->setContentsMargins(0, 0, 0, 0);
  widgetLayout->setSizeConstraint(QLayout::SetMinimumSize);
  widgetLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
  widgetLayout->addWidget(urlLabel);
  widgetLayout->addLayout(horizontalLayout);
  widgetLayout->addLayout(buttonsLayout);
  widgetLayout->addItem(
      new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

  connect(cancelButton, &QPushButton::clicked, this,
          &GitCredentialsToast::onCancelClicked);

  connect(submitButton, &QPushButton::clicked, this,
          &GitCredentialsToast::onSubmitClicked);

  layout()->addWidget(widget);

  LOG_PERF(0, "Exit");
}

GitCredentialsToast::~GitCredentialsToast() {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");
}

void GitCredentialsToast::stop() {
  LOG_PERF(0, "Enter");

  ToastWidget::stop();

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

} // namespace Widgets
} // namespace GuiManager
