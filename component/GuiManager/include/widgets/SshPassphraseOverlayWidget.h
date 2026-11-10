// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "Dispatcher.h"
#include "EventManager.h"
#include "GitRepo.h"
#include "OpaqueLabel.h"
#include "OpaquePushButton.h"
#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QAction>
#include <QEnableSharedFromThis>
#include <QKeyEvent>
#include <QWidget>

namespace GuiManager::Widgets {

class SshPassphraseOverlayWidget final
    : public OverlayWidget,
      public QEnableSharedFromThis<SshPassphraseOverlayWidget> {
  Q_OBJECT

public:
  typedef QSharedPointer<SshPassphraseOverlayWidget> SharedPtr;
  typedef QWeakPointer<SshPassphraseOverlayWidget> WeakPtr;

public:
  explicit SshPassphraseOverlayWidget(size_t id, QWidget *parent);
  ~SshPassphraseOverlayWidget() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  bool showing() const override { return _showing; }
  void setRequest(const GitRepo::GitCredentialsRequest &request);
  void start();
  void stop();

  void onCancelClicked();
  void onSubmitClicked() const;

  void eventCallback(EventManager::Event event);

protected:
  void keyPressEvent(QKeyEvent *event) override;

signals:
  void error();
  void finished();

private:
  void invalidPassphrase() const;

private:
  ::EventManager::EventManager &_eventManager;
  PersistentPlaceholderLineEdit *_passphraseLineEdit;
  OpaquePushButton *_submitButton;
  OpaquePushButton *_cancelButton;
  OpaqueLabel *_message;
  QAction *_passphraseVisibilityAction;
  GitRepo::GitCredentialsRequest _request;
  bool _showing;
  QSharedPointer<bool> _holder;
  size_t _id;
};

} // namespace GuiManager::Widgets
