#pragma once

#include "models/SubmodulesModel.h"

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

  void updateActiveBranch(const QString& submoduleName);
  void updateModel(const QStringList &submodules);

private:
  Models::SubmodulesModel *_model;
  Ui::GitSubmodulesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
