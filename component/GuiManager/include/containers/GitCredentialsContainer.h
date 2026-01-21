#pragma once

#include "widgets/ContainerWidget.h"

#include <GitCredentialStorage.h>

#include <QStandardItemModel>
#include <QWidget>

class QTableView;
class QPushButton;

namespace GuiManager {

namespace Containers {

using namespace Widgets;

class GitCredentialsContainer : public ContainerWidget {
  Q_OBJECT

public:
  explicit GitCredentialsContainer(QWidget *parent = nullptr);
  virtual ~GitCredentialsContainer();

  virtual void selected() override;
  virtual void deselected() override;

private slots:
  void onDeleteClicked();
  void onRefreshClicked();
  void onSelectionChanged();

private:
  void loadCredentials();
  void setupUi();

private:
  QTableView *_tableView;
  QStandardItemModel *_model;
  QPushButton *_deleteButton;
  QPushButton *_refreshButton;

  GitRepo::GitCredentialStorage &_credentialStorage;
};

} // namespace Containers
} // namespace GuiManager
