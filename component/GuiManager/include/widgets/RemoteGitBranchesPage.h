#pragma once

#include "models/BranchesModel.h"

#include <QWidget>

namespace Ui {
class RemoteGitBranchesPage;
}

namespace GuiManager::Widgets {

class RemoteGitBranchesPage : public QWidget {
  Q_OBJECT

public:
  explicit RemoteGitBranchesPage(QWidget *parent = nullptr);
  ~RemoteGitBranchesPage() override;

  void updateActiveBranch(const QString &branchName) const;
  void updateModel(const QStringList &localBranches) const;

signals:
  void branchSelected(const QString &branchName);

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::BranchesModel *_model;
  Ui::RemoteGitBranchesPage *ui;
};

} // namespace GuiManager::Widgets

