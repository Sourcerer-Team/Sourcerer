#include "widgets/GitHistoryRecyclerView.h"

#include "widgets/GitHistoryListItem.h"

namespace GuiManager {

namespace Widgets {

GitHistoryRecyclerView::GitHistoryRecyclerView(QWidget *parent)
    : RecyclerView(parent) {}

ViewHolder *GitHistoryRecyclerView::populateItem(int dataPos, int y) {

  ViewHolder *vh = RecyclerView::populateItem(dataPos, y);

  auto view = qobject_cast<GitHistoryListItem *>(vh->getItemView());
  if (view) {
    view->restoreSplitterState();
  }

  return vh;
}

} // namespace Widgets
} // namespace GuiManager