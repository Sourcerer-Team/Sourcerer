#pragma once

#include <QStandardItemModel>
#include <QStringList>
#include <QWidget>

namespace Ui {
class GitSubmodulesPage;
}

namespace GuiManager {

namespace Widgets {

class GitSubmodulesPage : public QWidget {
  Q_OBJECT

public:
  explicit GitSubmodulesPage(QWidget *parent = nullptr);
  ~GitSubmodulesPage();

  void updateModel(const QStringList &submodules);

private:
  QStandardItemModel *_model;
  Ui::GitSubmodulesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
