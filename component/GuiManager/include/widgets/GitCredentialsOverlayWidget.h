#pragma once

#include "Dispatcher.h"
#include "EventManager.h"
#include "GitRepo.h"
#include "OpaqueLabel.h"
#include "OpaquePushButton.h"
#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QAction>
#include <QCheckBox>
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
  explicit GitCredentialsOverlayWidget(size_t id, QWidget *parent);
  virtual ~GitCredentialsOverlayWidget() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  bool showing() const override { return _showing; }
  void setRequest(const GitRepo::GitCredentialsRequest &request);
  void setRepoPath(const QString &repoPath);
  void setIsSvn(bool isSvn);
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
  void invalidCredentials() const;

private:
  ::EventManager::EventManager &_eventManager;
  PersistentPlaceholderLineEdit *_usernameLineEdit;
  PersistentPlaceholderLineEdit *_passwordLineEdit;
  QCheckBox *_storeCredentialsCheckBox;
  OpaquePushButton *_submitButton;
  OpaquePushButton *_cancelButton;
  OpaqueLabel *_message;
  QAction *_passwordVisibilityAction;
  GitRepo::GitCredentialsRequest _request;
  QString _repoPath;
  bool _showing;
  bool _isSvn;
  QSharedPointer<bool> _holder;
  size_t _id;
};

} // namespace GuiManager::Widgets
