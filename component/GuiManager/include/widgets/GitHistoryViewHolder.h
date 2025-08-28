#pragma once

#include "widgets/GitHistoryListItem.h"

#include <recyclerviewadapter.h>

#include <QObject>

namespace GuiManager
{

namespace Widgets
{

class GitHistoryViewHolder : public ViewHolder
{
    Q_OBJECT
public:
    explicit GitHistoryViewHolder(QObject *parent, GitHistoryListItem* view);
    ~GitHistoryViewHolder() override;
};


} // namespace Widgets

} // namespace GuiManager
