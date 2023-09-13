#pragma once

#include "ToastWidget.h"

#include <EventManager.h>
#include <GitRepo.h>

#include <QLineEdit>

namespace GuiManager {

namespace Widgets {

using namespace GitRepo;

class GitCredentialsToast : public ToastWidget {
  Q_OBJECT
public:
  explicit GitCredentialsToast(const GitCredentialsRequest &request,
                               QWidget *parent = nullptr);
  virtual ~GitCredentialsToast();

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

} // namespace Widgets
} // namespace GuiManager
