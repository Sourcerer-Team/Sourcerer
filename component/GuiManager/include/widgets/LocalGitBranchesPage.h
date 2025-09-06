#pragma once

#include "models/BranchesModel.h"

#include <QWidget>

namespace Ui {
class LocalGitBranchesPage;
}

namespace GuiManager::Widgets {

class LocalGitBranchesPage : public QWidget {
  Q_OBJECT

public:
  explicit LocalGitBranchesPage(QWidget *parent = nullptr);
  ~LocalGitBranchesPage() override;

  void updateActiveBranch(const QString &branchName) const;
  void updateModel(const QStringList &localBranches) const;

signals:
  void branchSelected(const QString &branchName);

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::BranchesModel *_model;
  Ui::LocalGitBranchesPage *ui;
};

} // namespace GuiManager::Widgets
