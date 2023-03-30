#pragma once

#include <QWidget>

namespace Ui {
class RemoteGitBranchesPage;
}

namespace GuiManager {

namespace Widgets {

class RemoteGitBranchesPage : public QWidget {
  Q_OBJECT

public:
  explicit RemoteGitBranchesPage(QWidget *parent = nullptr);
  ~RemoteGitBranchesPage();

private:
  Ui::RemoteGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
