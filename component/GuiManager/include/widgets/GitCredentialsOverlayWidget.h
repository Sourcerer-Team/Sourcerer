#pragma once

#include "Dispatcher.h"
#include "EventManager.h"
#include "GitRepo.h"
#include "OverlayWidget.h"

#include <QEnableSharedFromThis>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>

namespace GuiManager::Widgets {

class GitCredentialsOverlayWidget final
    : public OverlayWidget,
      public QEnableSharedFromThis<GitCredentialsOverlayWidget> {
  Q_OBJECT

public:
  typedef QSharedPointer<GitCredentialsOverlayWidget> SharedPtr;
  typedef QWeakPointer<GitCredentialsOverlayWidget> WeakPtr;

public:
  explicit GitCredentialsOverlayWidget(QWidget *parent);
  ~GitCredentialsOverlayWidget() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  bool showing() const override { return _showing; }
  void setRequest(const GitRepo::GitCredentialsRequest &request);
  void start();
  void stop();

  void onCancelClicked() const;
  void onSubmitClicked() const;

  void eventCallback(EventManager::Event event);

protected:
  void keyPressEvent(QKeyEvent *event) override;

signals:
  void error();
  void finished();

private:
  void invalidCredentials() const;

private:
  ::EventManager::EventManager &_eventManager;
  QLineEdit *_usernameLineEdit;
  QLineEdit *_passwordLineEdit;
  QPushButton *_submitButton;
  QLabel *_message;
  GitRepo::GitCredentialsRequest _request;
  bool _showing;
  QSharedPointer<bool> _holder;
};

} // namespace GuiManager::Widgets