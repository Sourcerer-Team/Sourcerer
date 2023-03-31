#pragma once

#include "widgets/ContainerWidget.h"

#include <EventManager.h>

#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager {

namespace Containers {

class GitRepoContainer : public Widgets::ContainerWidget {
  Q_OBJECT

public:
  explicit GitRepoContainer(const QString &path, QWidget *parent = nullptr);
  virtual ~GitRepoContainer();

private:
  void closeRepo() const;
  void eventCallback(EventManager::Event event);
  void getLocalBranches() const;
  void openRepo() const;
  void refreshViews();
  void updateLocalBranchesModel(const QStringList& localBranches);

private:
  Ui::GitRepoContainer *ui;
  QString _uuid;
  QString _path;
  EventManager::EventManager &_eventManager;
};

} // namespace Containers
} // namespace GuiManager
