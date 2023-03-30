#pragma once

#include "widgets/ContainerWidget.h"

#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager {

namespace Containers {

class GitRepoContainer : public Widgets::ContainerWidget {
  Q_OBJECT

public:
  explicit GitRepoContainer(const QString& path, QWidget *parent = nullptr);
  ~GitRepoContainer();

private:
  Ui::GitRepoContainer *ui;
  QString _path;
};

} // namespace Containers
} // namespace GuiManager
