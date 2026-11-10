// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "models/BranchesModel.h"

#include <GitRepo.h>

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
  void
  updateModelWithStatus(const QVector<GitRepo::BranchInfo> &branchesWithStatus);

signals:
  void branchSelected(const QString &branchName);
  void branchRightClicked(const QString &branchName, const QPoint &pos);

private slots:
  void onListViewContextMenuRequested(const QPoint &pos);
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::BranchesModel *_model;
  Ui::RemoteGitBranchesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets
