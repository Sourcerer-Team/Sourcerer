#pragma once

#include "models/SubmodulesModel.h"

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

signals:
  void submoduleSelected(const QString &branchName);

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::SubmodulesModel *_model;
  Ui::GitSubmodulesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets