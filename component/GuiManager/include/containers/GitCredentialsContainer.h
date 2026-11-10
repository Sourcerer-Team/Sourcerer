// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "widgets/ContainerWidget.h"

#include <GitCredentialStorage.h>

#include <QStandardItemModel>
#include <QWidget>

namespace GuiManager::Widgets {
class PinnedCertificatesWidget;
}

namespace Ui {
class GitCredentialsContainer;
}

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
  void onEditClicked();
  void onRefreshClicked();
  void onSelectionChanged();

private:
  void loadCredentials();

private:
  Ui::GitCredentialsContainer *ui;
  QStandardItemModel *_model;
  Widgets::PinnedCertificatesWidget *_pinnedCertificates = nullptr;

  GitRepo::GitCredentialStorage &_credentialStorage;
};

} // namespace Containers
} // namespace GuiManager
