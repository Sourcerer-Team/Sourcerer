#include "widgets/GitTagsPage.h"
#include "widgets/ui_GitTagsPage.h"

namespace GuiManager {

namespace Widgets {

GitTagsPage::GitTagsPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::GitTagsPage) {
  ui->setupUi(this);
}

GitTagsPage::~GitTagsPage() { delete ui; }

} // namespace Widgets
} // namespace GuiManager
