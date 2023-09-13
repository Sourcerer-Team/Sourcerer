#pragma once

#include "ToastWidget.h"

#include <EventManager.h>
#include <GitRepo.h>

#include <QLabel>
#include <QProgressBar>

namespace GuiManager {

namespace Widgets {

using namespace GitRepo;

class GitProgressToast : public ToastWidget {
  Q_OBJECT
public:
  explicit GitProgressToast(const CloneProgress &data,
                            QWidget *parent = nullptr);
  virtual ~GitProgressToast();

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

} // namespace Widgets
} // namespace GuiManager
