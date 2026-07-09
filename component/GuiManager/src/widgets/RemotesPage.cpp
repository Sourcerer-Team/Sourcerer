#include "widgets/RemotesPage.h"
#include "MousePositionCorrection.h"
#include "widgets/ui_RemotesPage.h"

#include <Log.h>

#include <QMenu>
#include <QTreeView>
#include <QVBoxLayout>

#define THREAD_ID "RemotesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

RemotesPage::RemotesPage(QWidget *parent)
    : QWidget(parent), _model(new RemotesModel(this)), ui(new Ui::RemotesPage),
      _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  // Cast listView to QTreeView (it's now a QTreeView from the .ui file)
  auto *treeView = qobject_cast<QTreeView *>(ui->treeView);
  if (!treeView) {
    LOG_ERROR(Log::ErrorId::Gui, "Failed to cast listView to QTreeView");
    LOG_PERF(0, "Exit");
    return;
  }

  // Set the model once during initialization
  treeView->setModel(_model);

  connect(treeView, &QTreeView::doubleClicked, this,
          &RemotesPage::onListViewDoubleClicked);
  connect(this, &RemotesPage::rightClickOnRemote, this,
          &RemotesPage::onRemoteRightClicked);
  connect(this, &RemotesPage::rightClickOnTarget, this,
          &RemotesPage::onTargetRightClicked);
  connect(this, &RemotesPage::rightClickOnEmpty, this,
          &RemotesPage::onEmptySpaceClicked);

  treeView->viewport()->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

RemotesPage::~RemotesPage() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void RemotesPage::clearCached() {
  LOG_PERF(0, "Enter");

  _previous = -1;
  _pendingExpandedRemote.clear();

  LOG_PERF(0, "Exit");
}

void RemotesPage::updateActiveRemote(const QString &remoteName) {
  LOG_PERF(0, "Enter");

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    return;
  }

  const int index =
      !remoteName.isEmpty() ? _model->indexForRemote(remoteName) : -1;

  _model->setActiveIndex(index);

  if (index >= 0) {
    auto *treeView = qobject_cast<QTreeView *>(ui->treeView);
    if (treeView) {
      treeView->scrollTo(_model->index(index, 0));
    }
  }

  _previous = index;

  LOG_PERF(0, "Exit");
}

void RemotesPage::expandRemote(const QString &remoteName) {
  LOG_PERF(0, "Enter");

  _pendingExpandedRemote = remoteName;

  if (!_model) {
    LOG_ERROR(Log::ErrorId::Gui,
              "Internal error. Model has not been initialized.");
    LOG_PERF(0, "Exit");
    return;
  }

  auto *treeView = qobject_cast<QTreeView *>(ui->treeView);
  if (!treeView) {
    LOG_PERF(0, "Exit");
    return;
  }

  const QModelIndex remoteIndex = _model->getRemoteIndex(remoteName);
  if (remoteIndex.isValid()) {
    treeView->expand(remoteIndex);
    _pendingExpandedRemote.clear();
  }

  LOG_PERF(0, "Exit");
}

void RemotesPage::updateModel(const QVector<GitRepo::RemoteInfo> &remotes) {
  LOG_PERF(0, "Enter");

  _remotes = remotes;

  _model->updateModel(remotes);

  if (!_pendingExpandedRemote.isEmpty()) {
    auto *treeView = qobject_cast<QTreeView *>(ui->treeView);
    if (treeView) {
      const QModelIndex remoteIndex =
          _model->getRemoteIndex(_pendingExpandedRemote);
      if (remoteIndex.isValid()) {
        treeView->expand(remoteIndex);
        _pendingExpandedRemote.clear();
      }
    }
  }

  _previous = -1;

  LOG_PERF(0, "Exit");
}

QString RemotesPage::activeRemote() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  if (_previous < 0 || _previous >= _remotes.size()) {
    return "";
  }

  return _remotes[_previous].name;
}

bool RemotesPage::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::MouseButtonPress) {
    if (const auto *mouseEvent = static_cast<QMouseEvent *>(event);
        mouseEvent->button() == Qt::RightButton) {
      if (const auto *treeView = qobject_cast<QTreeView *>(obj->parent())) {
        const QPoint globalPos =
            MousePositionCorrection::instance().correctedGlobalPosition(
                mouseEvent);
        const QModelIndex index =
            treeView->indexAt(treeView->viewport()->mapFromGlobal(globalPos));
        if (index.isValid()) {
          // Check if it's a remote item (parent) or target item (child)
          if (_model->isRemoteItem(index)) {
            const QString remoteName = _model->getRemoteName(index);
            // Find the remote index
            const int remoteIndex = _model->indexForRemote(remoteName);
            emit rightClickOnRemote(remoteIndex, globalPos);
          } else if (_model->isTargetItem(index)) {
            const QString remoteName = _model->getRemoteName(index);
            const auto targetInfo = _model->getTargetInfo(index);
            const int remoteIndex = _model->indexForRemote(remoteName);

            // Find target index within the remote
            if (remoteIndex >= 0 && remoteIndex < _remotes.size()) {
              const auto &remote = _remotes[remoteIndex];
              int targetIndex = -1;
              for (int i = 0; i < remote.targets.size(); ++i) {
                if (remote.targets[i].name == targetInfo.name) {
                  targetIndex = i;
                  break;
                }
              }
              emit rightClickOnTarget(remoteIndex, targetIndex, globalPos);
            }
          }
        } else {
          // Right-click on empty space
          emit rightClickOnEmpty(globalPos);
        }
      }
    }
  }

  return QObject::eventFilter(obj, event);
}

void RemotesPage::onListViewDoubleClicked(const QModelIndex &index) {
  LOG_PERF(0, "Enter");

  if (!index.isValid()) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Only handle double-click on remote items (parents), not targets
  if (!_model->isRemoteItem(index)) {
    LOG_PERF(0, "Exit");
    return;
  }

  const int remoteRow = index.row();
  if (remoteRow == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  emit remoteDoubleClicked(remoteRow);
  emit setDefaultRemote(remoteRow);

  _previous = remoteRow;

  LOG_PERF(0, "Exit");
}

void RemotesPage::onRemoteRightClicked(int remoteIndex, const QPoint &pos) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 || remoteIndex >= _remotes.size()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = _remotes[remoteIndex];

  QMenu contextMenu;

  QAction actionEdit(tr("Edit"), this);
  QAction actionAddTarget(tr("Add target"), this);
  QAction actionSetDefault(tr("Set as default"), this);
  QAction actionDelete(tr("Delete"), this);

  contextMenu.addAction(&actionEdit);
  contextMenu.addAction(&actionAddTarget);
  contextMenu.addAction(&actionSetDefault);
  contextMenu.addSeparator();
  contextMenu.addAction(&actionDelete);

  connect(&actionEdit, &QAction::triggered, this,
          [=]() { emit editRemote(remoteIndex); });
  connect(&actionAddTarget, &QAction::triggered, this,
          [=]() { emit addTarget(remoteIndex); });
  connect(&actionSetDefault, &QAction::triggered, this,
          [=]() { emit setDefaultRemote(remoteIndex); });
  connect(&actionDelete, &QAction::triggered, this,
          [=]() { emit deleteRemote(remoteIndex); });

  contextMenu.exec(pos);

  LOG_PERF(0, "Exit");
}

void RemotesPage::onTargetRightClicked(int remoteIndex, int targetIndex,
                                       const QPoint &pos) {
  LOG_PERF(0, "Enter");

  if (remoteIndex < 0 || remoteIndex >= _remotes.size()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &remote = _remotes[remoteIndex];
  if (targetIndex < 0 || targetIndex >= remote.targets.size()) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto &target = remote.targets[targetIndex];

  if (target.name == "Default") {
    LOG_PERF(0, "Exit");
    return; // Do not allow actions on the "Default" target
  }

  const bool active =
      (target.fetchUrl == remote.fetchUrl && target.pushUrl == remote.pushUrl);

  QMenu contextMenu;

  QAction actionEdit(tr("Edit"), this);
  QAction actionSetDefault(tr("Set as default"), this);
  QAction actionDelete(tr("Delete"), this);

  contextMenu.addAction(&actionEdit);
  contextMenu.addAction(&actionSetDefault);

  if (!active) {
    contextMenu.addSeparator();
    contextMenu.addAction(&actionDelete);
  }

  connect(&actionEdit, &QAction::triggered, this,
          [=]() { emit editTarget(remoteIndex, targetIndex); });
  connect(&actionSetDefault, &QAction::triggered, this,
          [=]() { emit setDefaultTarget(remoteIndex, targetIndex); });

  if (!active) {
    connect(&actionDelete, &QAction::triggered, this,
            [=]() { emit deleteTarget(remoteIndex, targetIndex); });
  }

  contextMenu.exec(pos);

  LOG_PERF(0, "Exit");
}

void RemotesPage::onEmptySpaceClicked(const QPoint &pos) {
  LOG_PERF(0, "Enter");

  QMenu contextMenu;

  QAction actionAdd(tr("Add remote"), this);

  contextMenu.addAction(&actionAdd);

  connect(&actionAdd, &QAction::triggered, this, [=]() { emit addRemote(); });

  contextMenu.exec(pos);

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
