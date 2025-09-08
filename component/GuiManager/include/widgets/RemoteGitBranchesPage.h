#pragma once

#include "models/BranchesModel.h"

#include <QWidget>

namespace Ui {
class RemoteGitBranchesPage;
}

namespace GuiManager::Widgets {

class RemoteGitBranchesPage final : public QWidget {
  Q_OBJECT

public:
  explicit RemoteGitBranchesPage(QWidget *parent = nullptr);
  ~RemoteGitBranchesPage() override;

  void clearCached();
  void updateActiveBranch(const QString &branchName);
  void updateModel(const QStringList &localBranches);

signals:
  void branchSelected(const QString &branchName);

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::BranchesModel *_model;
  Ui::RemoteGitBranchesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets

