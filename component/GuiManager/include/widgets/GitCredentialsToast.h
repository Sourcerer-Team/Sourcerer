#pragma once

#include "ToastWidget.h"

#include <EventManager.h>
#include <GitRepo.h>

#include <QEnableSharedFromThis>
#include <QLineEdit>
#include <QSharedPointer>

namespace GuiManager::Widgets {

using namespace GitRepo;

class GitCredentialsToast final
    : public ToastWidget,
      public QEnableSharedFromThis<GitCredentialsToast> {
  Q_OBJECT

  typedef QSharedPointer<GitCredentialsToast> SharedPtr;
  typedef QWeakPointer<GitCredentialsToast> WeakPtr;

public:
  explicit GitCredentialsToast(const GitCredentialsRequest &request,
                               QWidget *parent = nullptr);
  ~GitCredentialsToast() override;

  SharedPtr getSelf() { return sharedFromThis(); }

private slots:
  void onCancelClicked();
  void onSubmitClicked();

private:
  void eventCallback(EventManager::Event event);

private:
  GitCredentialsRequest _request;
  EventManager::EventManager &_eventManager;
  QLineEdit *_usernameLineEdit;
  QLineEdit *_passwordLineEdit;
};

} // namespace GuiManager::Widgets