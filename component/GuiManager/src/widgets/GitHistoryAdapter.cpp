#include "widgets/GitHistoryAdapter.h"

#include "models/GitHistoryModel.h"
#include "widgets/GitHistoryListItem.h"
#include "widgets/GitHistoryViewHolder.h"

#include <recyclerview.h>

#include <Log.h>

#define THREAD_ID "GitHistoryAdapter"

namespace GuiManager {

namespace Widgets {

GitHistoryAdapter::GitHistoryAdapter(QAbstractItemModel *model, QObject *parent)
    : RecyclerViewAdapter(model, parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

GitHistoryAdapter::~GitHistoryAdapter() = default;

ViewHolder *GitHistoryAdapter::createViewHolder() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return new GitHistoryViewHolder(this, new GitHistoryListItem());
}

void GitHistoryAdapter::onBindViewHolder(ViewHolder *holder,
                                         const int position) {
  LOG_PERF(0, "Enter");

  if (!_dataModel->hasIndex(position, 0)) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto *historyViewHolder = qobject_cast<GitHistoryViewHolder *>(holder);
  if (!historyViewHolder) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto *view = qobject_cast<GitHistoryListItem *>(holder->getItemView());
  if (!view) {
    LOG_PERF(0, "Exit");
    return;
  }

  QModelIndex index = _dataModel->index(position, 0);
  const auto &summary = _dataModel->data(index, ROLE_SUMMARY);
  view->setCommitSummary(summary.toString());

  const auto &authorName = _dataModel->data(index, ROLE_AUTHOR_NAME);
  view->setAuthor(authorName.toString());

  const auto &dateTime = _dataModel->data(index, ROLE_COMMIT_DATETIME);
  view->setCommitDateTime(dateTime.toString());

  const auto &sha = _dataModel->data(index, ROLE_COMMIT_SHA);
  view->setSHA(sha.toString());

  holder->_dataPos = position;

  connect(historyViewHolder, &GitHistoryViewHolder::splitterMoved, this,
          &GitHistoryAdapter::onSplitterMoved);

  LOG_PERF(0, "Exit");
}

void GitHistoryAdapter::onRecycleViewHolder(ViewHolder *vh) const {
  LOG_PERF(0, "Enter");

  const auto *historyViewHolder = qobject_cast<GitHistoryViewHolder *>(vh);
  if (!historyViewHolder) {
    LOG_PERF(0, "Exit");
    return;
  }

  disconnect(historyViewHolder, &GitHistoryViewHolder::splitterMoved, this,
             &GitHistoryAdapter::onSplitterMoved);

  LOG_PERF(0, "Exit");
}

void GitHistoryAdapter::onSplitterMoved() const {
  LOG_PERF(0, "Enter");

  if (const auto rv = qobject_cast<RecyclerView *>(parent())) {
    rv->transformItems([](const ViewHolder *vh) {
      if (const auto view =
              qobject_cast<GitHistoryListItem *>(vh->getItemView())) {
        view->restoreSplitterState();
      }
    });
  }

  LOG_PERF(0, "Exit");
}

} // namespace Widgets

} // namespace GuiManager
