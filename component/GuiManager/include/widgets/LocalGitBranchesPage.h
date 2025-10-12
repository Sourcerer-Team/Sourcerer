#pragma once

#include "models/BranchesModel.h"

#include <QWidget>

namespace Ui {
class LocalGitBranchesPage;
}

namespace GuiManager::Widgets {

class LocalGitBranchesPage final : public QWidget {
  Q_OBJECT

public:
  explicit LocalGitBranchesPage(QWidget *parent = nullptr);
  ~LocalGitBranchesPage() override;

  void clearCached();
  void updateActiveBranch(const QString &branchName);
  void updateModel(const QStringList &localBranches);

signals:
  void branchSelected(const QString &branchName);
  void branchRightClicked(const QString &branchName, const QPoint &pos);

private slots:
  void onListViewContextMenuRequested(const QPoint &pos);
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::BranchesModel *_model;
  Ui::LocalGitBranchesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets