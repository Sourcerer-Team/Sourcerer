#include "viewholder.h"

#include <QDebug>

static int numViewHolderAlloc = 0;

ViewHolder::ViewHolder(QObject *parent, QWidget *view)
    : QObject(parent)
{
    Q_ASSERT(view != nullptr);
    _itemView = view;
    _dataPos = -1;
}

ViewHolder::~ViewHolder()
{
    if (_itemView != nullptr) {
        _itemView->deleteLater();
    }
}
