#pragma once

#include "Dispatcher.h"
#include "EventManager.h"
#include "GitRepo.h"
#include "OpaqueLabel.h"
#include "OpaquePushButton.h"
#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QEnableSharedFromThis>
#include <QKeyEvent>
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
  explicit GitCredentialsOverlayWidget(uint id, QWidget *parent);
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
  PersistentPlaceholderLineEdit *_usernameLineEdit;
  PersistentPlaceholderLineEdit *_passwordLineEdit;
  OpaquePushButton *_submitButton;
  OpaqueLabel *_message;
  GitRepo::GitCredentialsRequest _request;
  bool _showing;
  QSharedPointer<bool> _holder;
  size_t _id;
};

} // namespace GuiManager::Widgets
