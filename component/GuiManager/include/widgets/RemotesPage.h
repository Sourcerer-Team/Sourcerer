#pragma once

#include "models/RemotesModel.h"

#include <GitRepo.h>

#include <QMouseEvent>
#include <QWidget>

namespace Ui {
class RemotesPage;
}

namespace GuiManager::Widgets {

class RemotesPage final : public QWidget {
  Q_OBJECT

public:
  explicit RemotesPage(QWidget *parent = nullptr);
  ~RemotesPage() override;

  void clearCached();
  void updateActiveRemote(const QString &remoteName);
  void expandRemote(const QString &remoteName);
  void updateModel(const QVector<GitRepo::RemoteInfo> &remotes);

  QString activeRemote() const;

  const QVector<GitRepo::RemoteInfo> &remotes() const { return _remotes; }

signals:
  void remoteSelected(int remoteIndex);
  void remoteDoubleClicked(int remoteIndex);
  void rightClickOnEmpty(const QPoint &pos);
  void rightClickOnRemote(int remoteIndex, const QPoint &pos);
  void rightClickOnTarget(int remoteIndex, int targetIndex, const QPoint &pos);
  void addRemote();
  void editRemote(int remoteIndex);
  void fetchRemote(int remoteIndex);
  void deleteRemote(int remoteIndex);
  void setDefaultRemote(int remoteIndex);
  void addTarget(int remoteIndex);
  void editTarget(int remoteIndex, int targetIndex);
  void deleteTarget(int remoteIndex, int targetIndex);
  void setDefaultTarget(int remoteIndex, int targetIndex);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onListViewDoubleClicked(const QModelIndex &index);
  void onRemoteRightClicked(int remoteIndex, const QPoint &pos);
  void onTargetRightClicked(int remoteIndex, int targetIndex,
                            const QPoint &pos);
  void onEmptySpaceClicked(const QPoint &pos);

private:
  Models::RemotesModel *_model;
  Ui::RemotesPage *ui;

  int _previous;
  QVector<GitRepo::RemoteInfo> _remotes;
  QString _pendingExpandedRemote;
};

} // namespace GuiManager::Widgets
