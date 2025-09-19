#pragma once

#include "widgets/ContainerWidget.h"

#include <EventManager.h>

#include <QWidget>

namespace Ui {
class CloneGitRepoContainer;
}

namespace GuiManager {

namespace Containers {

using namespace Widgets;

class CloneGitRepoContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit CloneGitRepoContainer(QWidget *parent = nullptr);
  virtual ~CloneGitRepoContainer();

signals:
  void finished();

private slots:
  void onBrowseClicked();
  void onCloneClicked();
  void onTextChanged(const QString &text) const;

private:
  Ui::CloneGitRepoContainer *ui;
  EventManager::EventManager &_eventManager;
  QString _uuid;
};

} // namespace Containers
} // namespace GuiManager
