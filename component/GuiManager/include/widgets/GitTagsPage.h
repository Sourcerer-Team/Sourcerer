#pragma once

#include "models/TagsModel.h"

#include <QWidget>

namespace Ui {
class GitTagsPage;
}

namespace GuiManager {

namespace Widgets {

class GitTagsPage : public QWidget {
  Q_OBJECT

public:
  explicit GitTagsPage(QWidget *parent = nullptr);
  ~GitTagsPage();

  void updateActiveBranch(const QString& tagName);
  void updateModel(const QStringList &tags);

private:
  Models::TagsModel *_model;
  Ui::GitTagsPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
