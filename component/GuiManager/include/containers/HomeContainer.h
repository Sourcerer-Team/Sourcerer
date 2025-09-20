#pragma once

#include "models/RecentlyOpenedReposModel.h"
#include "widgets/ContainerWidget.h"
#include <delegates/HomeRecentItemDelegate.h>

#include <QModelIndex>
#include <QWidget>

namespace Ui {
class HomeContainer;
}

namespace GuiManager::Containers {

using namespace Delegates;
using namespace Models;
using namespace Widgets;

class HomeContainer final : public ContainerWidget {
  Q_OBJECT

public:
  explicit HomeContainer(QWidget *parent = nullptr);
  ~HomeContainer() override;

  void reloadRecentlyOpened() const;

  void selected() override;

signals:
  void clone();
  void init() override;
  void open();
  void recentClicked(const QString &path);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void hoverEnter(const QModelIndex &index);
  void hoverLeave(const QModelIndex &index);
  void onCloneClicked();
  void onInitClicked();
  void onOpenClicked(bool checked = false);

private:
  Ui::HomeContainer *ui;
  RecentlyOpenedReposModel *_recentlyOpenedModel;
  HomeRecentItemDelegate *_itemDelegate;
};

} // namespace GuiManager::Containers