#pragma once

#include <QStandardItemModel>
#include <QStringList>
#include <QWidget>

namespace Ui {
class RemoteGitBranchesPage;
}

namespace GuiManager {

namespace Widgets {

class RemoteGitBranchesPage : public QWidget {
  Q_OBJECT

public:
  explicit RemoteGitBranchesPage(QWidget *parent = nullptr);
  ~RemoteGitBranchesPage();

  void updateModel(const QStringList &remoteBranches);

private:
  QStandardItemModel *_model;
  Ui::RemoteGitBranchesPage *ui;
};

} // namespace Widgets
} // namespace GuiManager
