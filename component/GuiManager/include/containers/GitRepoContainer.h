#pragma once

#include "models/GitHistoryModel.h"
#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>
#include <Dispatcher.h>
#include <EventManager.h>
#include <LockGuard.h>
#include <GitRepo.h>

#include <QMovie>
#include <QSet>
#include <QString>
#include <QWidget>

namespace Ui {
class GitRepoContainer;
}

namespace GuiManager {

namespace Containers {

class GitRepoContainer : public Widgets::ContainerWidget {
  Q_OBJECT

  public:
  explicit GitRepoContainer(const QString &path, QWidget *parent = nullptr);
  ~GitRepoContainer() override;

  // QObject interface
  public:
  bool eventFilter(QObject *watched, QEvent *event) override;

  protected:
  void resizeEvent(QResizeEvent *event) override;

  private slots:
  void onSplitterMoved(int pos, int index) const;
  void onHistoryViewItemEntered(const QModelIndex& index);

  private:
  void closeRepo() const;
  void eventCallback(EventManager::Event event);
  void getActiveBranch() const;
  void getBranchHistory() const;
  QStringList getCommitBranches(const QString& commit) const;
  void getLocalBranches() const;
  void getRemoteBranches() const;
  void getSubmodules() const;
  void getTags() const;
  void openRepo() const;
  void refreshViews() const;
  void stretchBranchHistoryTableColumns() const;
  void showErrorPage(bool error = true) const;
  void showSpinnerPage(bool spinnerEnabled = true) const;
  void showToast(const QString &message) const;
  void updateBranchHistory(const GitRepo::GitHistory& history) const;
  void updateLocalBranchesModel(const QStringList &localBranches) const;
  void updateLocalBranchesModelActiveBranch(const QString &activeBranch);
  void updateRemoteBranchesModel(const QStringList &remoteBranches) const;
  void updateSubmodulesModel(const QStringList &tags) const;
  void updateTagsModel(const QStringList &tags) const;

  private:
  Ui::GitRepoContainer *ui;
  QString _uuid;
  QString _path;
  QString _activeBranch;
  ConfigManager::ConfigManager &_configManager;
  EventManager::EventManager &_eventManager;
  Widgets::ToastHostWidget &_toastHost;
  Models::GitHistoryModel *_historyModel;
  QMovie *_spinnerMovie;
  QPixmap *_errorPixmap;
  Util::Dispatcher _dispatcher;

  Util::Mutex _tooltipsMutex;
  QPoint _mousePos;
  QSet<QString> _tooltipsToShow;
};

} // namespace Containers
} // namespace GuiManager
