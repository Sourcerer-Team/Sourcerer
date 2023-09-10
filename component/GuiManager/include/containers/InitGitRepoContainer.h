#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <EventManager.h>

#include <QWidget>

namespace Ui {
class InitGitRepoContainer;
}

namespace GuiManager {

namespace Containers {

class InitGitRepoContainer : public Widgets::ContainerWidget {
  Q_OBJECT

public:
  explicit InitGitRepoContainer(QWidget *parent = nullptr);
  virtual ~InitGitRepoContainer();

signals:
  void initialized();

private slots:
  void onBrowseClicked();
  void onInitClicked();

private:
  void eventCallback(EventManager::Event event);
  void showToast(const QString& message);

private:
  Ui::InitGitRepoContainer *ui;
  Widgets::ToastHostWidget& _toastHost;
  EventManager::EventManager &_eventManager;
};

} // namespace Containers
} // namespace GuiManager
