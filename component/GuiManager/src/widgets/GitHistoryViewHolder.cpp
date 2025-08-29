#include "widgets/GitHistoryViewHolder.h"

#include "spdlog/fmt/bundled/core.h"

namespace GuiManager {

namespace Widgets {

GitHistoryViewHolder::GitHistoryViewHolder(QObject *parent,
                                           GitHistoryListItem *view)
    : ViewHolder(parent, view) {
  connect(view, &GitHistoryListItem::splitterMoved, this,
          &GitHistoryViewHolder::splitterMoved);
}

GitHistoryViewHolder::~GitHistoryViewHolder() {
  disconnect(static_cast<GitHistoryListItem *>(getItemView()),
             &GitHistoryListItem::splitterMoved, this,
             &GitHistoryViewHolder::splitterMoved);
}

} // namespace Widgets

} // namespace GuiManager
