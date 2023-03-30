#pragma once

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

private:
  Ui::GitTagsPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
