// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "models/BranchesModel.h"

#include <GraphiteUtils.h>

#include <QWidget>

class QAbstractItemDelegate;

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
  // Shows the branches in titled sections, e.g. Mercurial's named branches
  // and bookmarks. Graphite stacks show the branches without sections.
  void updateModel(const QVector<Models::BranchSection> &sections);
  // Shows the item that selects the history of all branches as the first item.
  void setShowAllBranchesItem(bool show);

  // Shows the branches as Graphite stacks until clearGraphiteStacks().
  void setGraphiteStacks(const GitRepo::GraphiteStacks &stacks);
  void clearGraphiteStacks();

signals:
  void branchSelected(const QString &branchName);
  void branchRightClicked(const QString &branchName, const QPoint &pos);
  void branchesMoved(const QString &sourceBranch, const QString &destBranch);

private slots:
  void onBranchesMoved(const QString &sourceBranch, const QString &destBranch);
  void onListViewContextMenuRequested(const QPoint &pos);
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  void rebuild();

  Models::BranchesModel *_model;
  Ui::LocalGitBranchesPage *ui;

  int _previous;
  QStringList _localBranches;
  // Empty unless the branches were given in sections.
  QVector<Models::BranchSection> _sections;
  QString _activeBranch;
  GitRepo::GraphiteStacks _graphiteStacks;
  bool _showAllBranchesItem{true};
  QAbstractItemDelegate *_defaultDelegate{nullptr};
  QAbstractItemDelegate *_graphiteDelegate{nullptr};
};

} // namespace GuiManager::Widgets
