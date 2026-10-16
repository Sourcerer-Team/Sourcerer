#pragma once

#include <QStyledItemDelegate>

namespace GuiManager::Delegates {

// Paints the local branches panel in Graphite mode: stack lines, a dot per
// branch, the branch name and badges for the pull request and branches that
// need a restack. Rows without Graphite data are painted as usual.
class GraphiteBranchDelegate final : public QStyledItemDelegate {
  Q_OBJECT
public:
  explicit GraphiteBranchDelegate(QObject *parent = nullptr);

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override;
  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override;
};

} // namespace GuiManager::Delegates
