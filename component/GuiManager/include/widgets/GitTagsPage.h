#pragma once

#include "models/TagsModel.h"

#include <QWidget>

namespace Ui {
class GitTagsPage;
}

namespace GuiManager {

namespace Widgets {

class GitTagsPage final : public QWidget {
  Q_OBJECT

public:
  explicit GitTagsPage(QWidget *parent = nullptr);
  ~GitTagsPage() override;

  void clearCached();
  void updateActiveTag(const QString &tagName);
  void updateModel(const QStringList &tags);

signals:
  void tagSelected(const QString &branchName);

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);

private:
  Models::TagsModel *_model;
  Ui::GitTagsPage *ui;

  int _previous;
};

} // namespace Widgets
} // namespace GuiManager
