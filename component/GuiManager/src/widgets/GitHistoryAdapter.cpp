#include "widgets/GitHistoryAdapter.h"

#include "models/GitHistoryModel.h"
#include "widgets/GitHistoryListItem.h"
#include "widgets/GitHistoryViewHolder.h"

#include <Log.h>

#define THREAD_ID "GitHistoryAdapter"

namespace GuiManager {

namespace Widgets {

GitHistoryAdapter::GitHistoryAdapter(QAbstractItemModel *model, QObject *parent)
    : RecyclerViewAdapter(model, parent) {
  LOG_PERF(0, "Enter");

  connect(_dataModel, &QAbstractItemModel::dataChanged, this,
          &GitHistoryAdapter::onDataChanged);

  LOG_PERF(0, "Exit");
}

GitHistoryAdapter::~GitHistoryAdapter() = default;

ViewHolder *GitHistoryAdapter::createViewHolder() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return new GitHistoryViewHolder(this, new GitHistoryListItem());
}

void GitHistoryAdapter::onBindViewHolder(ViewHolder *vh, int position) {
  LOG_PERF(0, "Enter");

  if (!_dataModel->hasIndex(position, 0)) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto *historyViewHolder = qobject_cast<GitHistoryViewHolder *>(vh);
  if (!historyViewHolder) {
    LOG_PERF(0, "Exit");
    return;
  }

  const auto *view = qobject_cast<GitHistoryListItem *>(vh->getItemView());
  if (!view) {
    LOG_PERF(0, "Exit");
    return;
  }

  // connect(channelViewHolder, &ChannelSliderViewHolder::valueChanged, this,
  // &ChannelSliderAdapter::newUserChannelValue);

  QModelIndex index = _dataModel->index(position, 0);
  const auto &summary = _dataModel->data(index, ROLE_SUMMARY);
  view->setCommitSummary(summary.toString());

  const auto &authorName = _dataModel->data(index, ROLE_AUTHOR_NAME);
  view->setAuthor(authorName.toString());

  const auto &dateTime = _dataModel->data(index, ROLE_COMMIT_DATETIME);
  view->setCommitDateTime(dateTime.toString());

  const auto &sha = _dataModel->data(index, ROLE_COMMIT_SHA);
  view->setSHA(sha.toString());

  vh->dataPos = position;

  LOG_PERF(0, "Exit");
}

void GitHistoryAdapter::onRecycleViewHolder(ViewHolder *vh) const {
  LOG_PERF(0, "Enter");

  const auto *customViewHolder = qobject_cast<GitHistoryViewHolder *>(vh);
  if (!customViewHolder) {
    LOG_PERF(0, "Exit");
    return;
  }

  // Disconnect everything from valueChanged signal
  // disconnect(customViewHolder, &GitHistoryViewHolder::valueChanged, nullptr,
  // nullptr);

  LOG_PERF(0, "Exit");
}

void GitHistoryAdapter::onDataChanged(const QModelIndex &topLeft,
                                      const QModelIndex &bottomRight,
                                      const QVector<int> &roles) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

} // namespace Widgets

} // namespace GuiManager
