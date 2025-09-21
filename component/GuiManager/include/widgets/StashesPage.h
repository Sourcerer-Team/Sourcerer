#pragma once

#include "models/StashesModel.h"

#include <QWidget>

namespace Ui {
class StashesPage;
}

namespace GuiManager::Widgets {

class StashesPage final : public QWidget {
  Q_OBJECT

public:
  explicit StashesPage(QWidget *parent = nullptr);
  ~StashesPage() override;

  void clearCached();
  void updateActiveBranch(const QString &stashName);
  void updateModel(const QStringList &stashes);

signals:
  void stashSelected(const QString &branchName);

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::StashesModel *_model;
  Ui::StashesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets
