#include "widgets/GitHistoryViewHolder.h"

namespace GuiManager
{

namespace Widgets
{

GitHistoryViewHolder::GitHistoryViewHolder(QObject *parent, GitHistoryListItem* view)
    : ViewHolder(parent, view)
{

}

GitHistoryViewHolder::~GitHistoryViewHolder() = default;

} // namespace Widgets

} // namespace GuiManager
