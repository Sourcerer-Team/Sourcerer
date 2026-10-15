#pragma once

#include "widgets/ContainerWidget.h"

#include <EventManager.h>

#include <QWidget>

namespace Ui {
class CloneRepoContainer;
}

namespace GuiManager {

namespace Containers {

enum class CloneType { Git, Subversion };

using namespace Widgets;

class CloneRepoContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit CloneRepoContainer(QWidget *parent = nullptr,
                              CloneType cloneType = CloneType::Git);
  virtual ~CloneRepoContainer();

signals:
  void finished();

private slots:
  void onBrowseClicked();
  void onCloneClicked();
  void onTextChanged(const QString &text) const;

private:
  Ui::CloneRepoContainer *ui;
  EventManager::EventManager &_eventManager;
  CloneType _cloneType;
  QString _uuid;
};

} // namespace Containers
} // namespace GuiManager
