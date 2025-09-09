#pragma once

#include "ToastWidget.h"

#include <EventManager.h>
#include <GitRepo.h>

#include <QEnableSharedFromThis>
#include <QLabel>
#include <QProgressBar>

namespace GuiManager::Widgets {

using namespace GitRepo;

class GitProgressToast final : public ToastWidget,
                               public QEnableSharedFromThis<GitProgressToast> {
  Q_OBJECT

  typedef QSharedPointer<GitProgressToast> SharedPtr;
  typedef QWeakPointer<GitProgressToast> WeakPtr;

public:
  explicit GitProgressToast(const CloneProgress &data, const QString &title,
                            QWidget *parent = nullptr);
  ~GitProgressToast() override;

  SharedPtr getSelf() { return sharedFromThis(); }

signals:
  void finished();

private slots:
  void onFinished();

private:
  void eventCallback(EventManager::Event event);

private:
  CloneProgress _data;
  EventManager::EventManager &_eventManager;
  QLabel *_messageLabel;
  QProgressBar *_progressBar;
};

} // namespace GuiManager::Widgets