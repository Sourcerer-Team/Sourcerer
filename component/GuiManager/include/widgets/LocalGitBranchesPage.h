#pragma once

#include <QStandardItemModel>
#include <QStringList>
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
  QStandardItemModel *_model;
  Ui::LocalGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
