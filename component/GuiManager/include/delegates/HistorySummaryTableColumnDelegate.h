#pragma once

#pragma once

#include "models/GitHistoryModel.h"

#include <QPainter>
#include <QStyledItemDelegate>

namespace GuiManager::Delegates {

class HistorySummaryTableColumnDelegate final : public QStyledItemDelegate {
public:
  explicit HistorySummaryTableColumnDelegate(QObject *parent = nullptr);
  ~HistorySummaryTableColumnDelegate() override = default;

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;

private:
  void
  paintStatus(QPainter *painter, const QStyleOptionViewItem &option,
              const QModelIndex &index,
              const QMap<Models::StatusFields, int> &statusFieldsMap) const;

  void paintDefault(QPainter *painter, const QStyleOptionViewItem &option,
                    const QModelIndex &index) const;
};

} // namespace GuiManager::Delegates
