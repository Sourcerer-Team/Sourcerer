#pragma once

#include <QWidget>

namespace Ui {
class LocalGitBranchesPage;
}

namespace GuiManager {

namespace Widgets {

class LocalGitBranchesPage : public QWidget {
  Q_OBJECT

public:
  explicit LocalGitBranchesPage(QWidget *parent = nullptr);
  ~LocalGitBranchesPage();

private:
  Ui::LocalGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
