#pragma once

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

private:
  Ui::GitSubmodulesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
