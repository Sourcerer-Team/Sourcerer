#pragma once

#include <recyclerviewadapter.h>

#include <QObject>

namespace GuiManager {

namespace Widgets {

class GitHistoryAdapter : public RecyclerViewAdapter
{
    Q_OBJECT
public:
    explicit GitHistoryAdapter(QAbstractItemModel *model, QObject *parent = nullptr);
    ~GitHistoryAdapter() override;

    ViewHolder *createViewHolder() override;
    void onBindViewHolder(ViewHolder *holder, int position) override;
    void onRecycleViewHolder(ViewHolder *vh) const override;

private slots:
    static void onDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight, const QVector<int> &roles);

};

} // namespace Widgets

} // namespace GuiManager
