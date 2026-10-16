#pragma once

#include "widgets/ContainerWidget.h"

#include <GitKeyStorage.h>

#include <QStandardItemModel>
#include <QWidget>

namespace Ui {
class GitKeysContainer;
}

namespace GuiManager {

namespace Containers {

using namespace Widgets;

class GitKeysContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit GitKeysContainer(QWidget *parent = nullptr);
  virtual ~GitKeysContainer();

  virtual void selected() override;
  virtual void deselected() override;

private slots:
  void onGenerateClicked();
  void onAddClicked();
  void onEditClicked();
  void onDeleteClicked();
  void onRefreshClicked();
  void onSelectionChanged();

private:
  void loadKeyPairs();

private:
  Ui::GitKeysContainer *ui;
  QStandardItemModel *_model;

  GitRepo::GitKeyStorage &_keyStorage;
};

} // namespace Containers
} // namespace GuiManager
