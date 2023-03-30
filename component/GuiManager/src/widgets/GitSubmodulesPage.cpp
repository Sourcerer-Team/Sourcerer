#include "widgets/GitSubmodulesPage.h"
#include "widgets/ui_GitSubmodulesPage.h"

namespace GuiManager {

namespace Widgets {

GitSubmodulesPage::GitSubmodulesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GitSubmodulesPage) {
  ui->setupUi(this);
}

GitSubmodulesPage::~GitSubmodulesPage() { delete ui; }

} // namespace Widgets
} // namespace GuiManager
