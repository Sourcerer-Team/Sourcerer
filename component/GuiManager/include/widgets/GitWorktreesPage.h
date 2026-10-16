#pragma once

#include "models/WorktreesModel.h"

#include <GitRepo.h>

#include <QWidget>

namespace Ui {
class GitWorktreesPage;
}

namespace GuiManager::Widgets {

class GitWorktreesPage final : public QWidget {
  Q_OBJECT

public:
  explicit GitWorktreesPage(QWidget *parent = nullptr);
  ~GitWorktreesPage() override;

  void updateModel(const QVector<GitRepo::WorktreeInfo> &worktrees);

signals:
  void rightClicked(const QPoint &pos);
  void worktreeSelected(const GitRepo::WorktreeInfo &worktree);
  void worktreeRightClicked(const GitRepo::WorktreeInfo &worktree,
                            const QPoint &pos);

private slots:
  void onListViewContextMenuRequested(const QPoint &pos);
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::WorktreesModel *_model;
  Ui::GitWorktreesPage *ui;
};

} // namespace GuiManager::Widgets
