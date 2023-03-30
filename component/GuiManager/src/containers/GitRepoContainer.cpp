#include "containers/GitRepoContainer.h"
#include "containers/ui_GitRepoContainer.h"

namespace GuiManager {

namespace Containers {

GitRepoContainer::GitRepoContainer(const QString &path, QWidget *parent)
    : ContainerWidget(Widgets::ContainerType::Unknown, parent),
      ui(new Ui::GitRepoContainer), _path(path) {
  ui->setupUi(this);
}

GitRepoContainer::~GitRepoContainer() { delete ui; }

} // namespace Containers
} // namespace GuiManager
