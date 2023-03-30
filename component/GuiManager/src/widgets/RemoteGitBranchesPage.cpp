#include "widgets/RemoteGitBranchesPage.h"
#include "widgets/ui_RemoteGitBranchesPage.h"

namespace GuiManager {

namespace Widgets {

RemoteGitBranchesPage::RemoteGitBranchesPage(QWidget *parent)
    : QWidget(parent), ui(new Ui::RemoteGitBranchesPage) {
  ui->setupUi(this);
}

RemoteGitBranchesPage::~RemoteGitBranchesPage() { delete ui; }

} // namespace Widgets
} // namespace GuiManager
