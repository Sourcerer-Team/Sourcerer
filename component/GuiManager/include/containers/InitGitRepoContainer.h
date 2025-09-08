#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <EventManager.h>

#include <QEnableSharedFromThis>
#include <QWidget>

namespace Ui {
class InitGitRepoContainer;
}

namespace GuiManager::Containers {

class InitGitRepoContainer final
    : public Widgets::ContainerWidget,
      public QEnableSharedFromThis<InitGitRepoContainer> {
  Q_OBJECT

  typedef QSharedPointer<InitGitRepoContainer> SharedPtr;
  typedef QWeakPointer<InitGitRepoContainer> WeakPtr;

public:
  explicit InitGitRepoContainer(QWidget *parent = nullptr);
  ~InitGitRepoContainer() override;

  SharedPtr getSelf() { return sharedFromThis(); }

signals:
  void initialized();

private slots:
  void onBrowseClicked();
  void onInitClicked() const;

private:
  void eventCallback(EventManager::Event event);
  void showToast(const QString &message) const;

private:
  Ui::InitGitRepoContainer *ui;
  Widgets::ToastHostWidget &_toastHost;
  EventManager::EventManager &_eventManager;
  QString _uuid;
};

} // namespace GuiManager::Containers