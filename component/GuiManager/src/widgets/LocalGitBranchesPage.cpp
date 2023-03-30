#include "widgets/LocalGitBranchesPage.h"
#include "widgets/ui_LocalGitBranchesPage.h"

namespace GuiManager {

namespace Widgets {

LocalGitBranchesPage::LocalGitBranchesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::LocalGitBranchesPage) {
  ui->setupUi(this);
}

LocalGitBranchesPage::~LocalGitBranchesPage() { delete ui; }

} // namespace Widgets
} // namespace GuiManager
