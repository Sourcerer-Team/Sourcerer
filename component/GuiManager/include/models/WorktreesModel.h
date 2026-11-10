// MIT License
// Copyright (c) 2026 Sourcerer


#pragma once

#include <GitRepo.h>

#include <QObject>
#include <QStandardItemModel>

namespace GuiManager::Models {

class WorktreesModel : public QStandardItemModel {
  Q_OBJECT
public:
  enum Roles {
    NameRole = Qt::UserRole,
    PathRole,
    IsMainRole,
    IsCurrentRole,
    LockedRole,
    ValidRole,
    BranchRole,
  };

  explicit WorktreesModel(QObject *parent = nullptr);
  ~WorktreesModel() override;

  void updateModel(const QVector<GitRepo::WorktreeInfo> &worktrees);

  [[nodiscard]] GitRepo::WorktreeInfo worktreeForIndex(int row) const;
};

} // namespace GuiManager::Models
