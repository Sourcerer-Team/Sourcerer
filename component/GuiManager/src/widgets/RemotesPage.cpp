#include "widgets/RemotesPage.h"
#include "widgets/ui_RemotesPage.h"

#include <Log.h>

#include <QListView>
#include <QMenu>

#define THREAD_ID "RemotesPage"

using namespace GuiManager::Models;

namespace GuiManager::Widgets {

RemotesPage::RemotesPage(QWidget *parent)
    : QWidget(parent), _model(new RemotesModel(this)), ui(new Ui::RemotesPage),
      _previous(-1) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  connect(ui->listView, &QListView::doubleClicked, this,
          &RemotesPage::onListViewDoubleClicked);
  connect(this, &RemotesPage::rightClickOnRemote, this,
          &RemotesPage::onRemoteRightClicked);
  connect(this, &RemotesPage::rightClickOnEmpty, this,
          &RemotesPage::onEmptySpaceClicked);

  ui->listView->viewport()->installEventFilter(this);

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
    ui->listView->scrollTo(_model->index(index, 0));
  }

  _previous = index;

  LOG_PERF(0, "Exit");
}

void RemotesPage::updateModel(const QVector<GitRepo::RemoteInfo> &remotes) {
  LOG_PERF(0, "Enter");

  _remotes = remotes;

  _model->clear();
  _model->setActiveIndex(-1);
  _model->updateModel(remotes);

  ui->listView->setModel(_model);

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
      if (const auto *listView = qobject_cast<QListView *>(obj->parent())) {
        if (const QModelIndex index = listView->indexAt(mouseEvent->pos());
            index.isValid()) {
          emit rightClickOnRemote(index.row(), mouseEvent->globalPos());
        } else {
          // Right-click on empty space
          emit rightClickOnEmpty(mouseEvent->globalPos());
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

  if (index.row() == _previous) {
    LOG_PERF(0, "Exit");
    return;
  }

  emit remoteDoubleClicked(index.row());
  emit setDefaultRemote(index.row());

  _previous = index.row();

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
  QAction actionSetDefault(tr("Set as default"), this);
  QAction actionDelete(tr("Delete"), this);

  contextMenu.addAction(&actionEdit);
  contextMenu.addAction(&actionSetDefault);
  contextMenu.addSeparator();
  contextMenu.addAction(&actionDelete);

  connect(&actionEdit, &QAction::triggered, this,
          [=]() { emit editRemote(remoteIndex); });
  connect(&actionSetDefault, &QAction::triggered, this,
          [=]() { emit setDefaultRemote(remoteIndex); });
  connect(&actionDelete, &QAction::triggered, this,
          [=]() { emit deleteRemote(remoteIndex); });

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
