#pragma once

#include "models/StashesModel.h"

#include <QMouseEvent>
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

  int lastStash() const;

signals:
  void applyStash(int stashIndex);
  void deleteStash(int stashIndex);
  void popStash(int stashIndex);
  void rightClickOnStash(int stashIndex, const QPoint &pos);
  void stashSelected(int stashIndex);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);
  void onStashClicked(int stashIndex, const QPoint &pos);

private:
  Models::StashesModel *_model;
  Ui::StashesPage *ui;

  int _previous;
};

} // namespace GuiManager::Widgets
