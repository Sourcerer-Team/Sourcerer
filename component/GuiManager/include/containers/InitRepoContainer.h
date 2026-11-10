// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <EventManager.h>

#include <QEnableSharedFromThis>
#include <QWidget>

namespace Ui {
class InitRepoContainer;
}

namespace GuiManager::Containers {

enum class InitType { Git, Subversion, Mercurial };

class InitRepoContainer final
    : public Widgets::ContainerWidget,
      public QEnableSharedFromThis<InitRepoContainer> {
  Q_OBJECT

  typedef QSharedPointer<InitRepoContainer> SharedPtr;
  typedef QWeakPointer<InitRepoContainer> WeakPtr;

public:
  explicit InitRepoContainer(QWidget *parent = nullptr,
                             InitType initType = InitType::Git);
  ~InitRepoContainer() override;

  SharedPtr getSelf() { return sharedFromThis(); }

signals:
  void initialized();

private slots:
  void onBrowseClicked();
  void onInitClicked();

private:
  void eventCallback(EventManager::Event event);
  void showToast(const QString &message, const QString &title);
  void showErrorToast(const QString &message, const QString &title);

private:
  Ui::InitRepoContainer *ui;
  Widgets::ToastHostWidget &_toastHost;
  EventManager::EventManager &_eventManager;
  InitType _initType;
  QString _uuid;
};

} // namespace GuiManager::Containers
