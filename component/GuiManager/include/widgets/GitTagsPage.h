#pragma once

#include <QStandardItemModel>
#include <QStringList>
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

  void updateModel(const QStringList &tags);

private:
  QStandardItemModel *_model;
  Ui::GitTagsPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
