#pragma once

#include "models/BranchesModel.h"

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

  void updateActiveBranch(const QString& branchName);
  void updateModel(const QStringList &localBranches);

private:
  Models::BranchesModel *_model;
  Ui::RemoteGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
