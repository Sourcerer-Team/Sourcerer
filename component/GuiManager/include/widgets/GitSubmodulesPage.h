// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "models/SubmodulesModel.h"

#include <GitRepo.h>

#include <QWidget>

namespace Ui {
class GitSubmodulesPage;
}

namespace GuiManager::Widgets {

class GitSubmodulesPage final : public QWidget {
  Q_OBJECT

public:
  explicit GitSubmodulesPage(QWidget *parent = nullptr);
  ~GitSubmodulesPage() override;

  void updateActiveBranch(const QString &submoduleName);
  void updateModel(const QStringList &submodules);
  void updateModelWithStatus(
      const QVector<GitRepo::SubmoduleInfo> &submodulesWithStatus);

signals:
  void rightClicked(const QPoint &pos);
  void submoduleSelected(const QString &branchName);
  void submoduleRightClicked(const QString &submoduleName, const QPoint &pos);

private slots:
  void onListViewContextMenuRequested(const QPoint &pos);
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::SubmodulesModel *_model;
  Ui::GitSubmodulesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets