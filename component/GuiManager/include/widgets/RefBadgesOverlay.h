// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <QHash>
#include <QPersistentModelIndex>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QWidget>

#include <functional>

class QTableView;

namespace GuiManager::Widgets {

/// Transparent overlay over the history view's viewport. It always draws the
/// branches and tags pointing at a commit as badges next to the commit's
/// graph lanes, on every visible row that has any. While the graph column is
/// hovered, the hovered row also shows the local branches containing its
/// commit, wrapped into up to two lines.
///
/// The badges may extend over the neighbour columns, which a delegate cannot
/// do, and never take mouse events.
///
/// The refs pointing exactly at a commit come from the model. The branches
/// containing a commit are expensive to compute, so they are requested
/// through the refs provider once the cursor rests on a row and are cached
/// until the model is reset.
class RefBadgesOverlay final : public QWidget {
public:
  /// Asked (on the GUI thread) for the branches containing a commit. The
  /// answer is given back with setExtraRefs().
  using RefsProvider = std::function<void(const QString &commit)>;

  explicit RefBadgesOverlay(QTableView *view);
  ~RefBadgesOverlay() override = default;

  /// Connects the overlay to the model and the header of the view. Call it
  /// once both are set up.
  void attach();

  /// Pass an invalid index to stop showing the extra badges.
  void setHoveredIndex(const QModelIndex &index);

  /// Stops showing the extra badges unless the cursor is still over the
  /// viewport. Used for Leave events, which can also be sent when a tooltip
  /// pops up.
  void clearIfCursorOutside();

  void setRefsProvider(RefsProvider provider);

  /// Full ref names (refs/heads/...) of the branches containing a commit.
  void setExtraRefs(const QString &commit, const QStringList &refs);

protected:
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  void refresh();
  void refreshHoveredRow();
  void clearExtraRefs();
  QString hoveredCommit() const;
  void requestExtraRefs();

  QTableView *_view;
  QPersistentModelIndex _index;
  RefsProvider _refsProvider;
  QHash<QString, QStringList> _extraRefs;
  QSet<QString> _requested;
  QTimer _requestTimer;
};

} // namespace GuiManager::Widgets
