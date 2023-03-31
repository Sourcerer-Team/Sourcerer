#pragma once

#include <QStringList>
#include <QStringListModel>
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

  void updateModel(const QStringList &localBranches);

private:
  QStringListModel *_model;
  Ui::LocalGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
