#pragma once

#include "models/BranchesModel.h"

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

  void updateActiveBranch(const QString& branchName);
  void updateModel(const QStringList &localBranches);

private:
  Models::BranchesModel *_model;
  Ui::LocalGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
