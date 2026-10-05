#pragma once

#include "models/GitCommitListModel.h"

#include <QApplication>
#include <QDir>
#include <QMap>
#include <QPainter>
#include <QStyledItemDelegate>
#include <qfileinfo.h>

namespace GuiManager::Delegates {

class ElidePartialColorDelegate final : public QStyledItemDelegate {
public:
  explicit ElidePartialColorDelegate(
      const Qt::TextElideMode elideMode = Qt::TextElideMode::ElideRight,
      bool treePainter = false, QObject *parent = nullptr)
      : QStyledItemDelegate(parent), _elideMode(elideMode),
        _treePainter(treePainter) {
    _modificationTypeToIcon = {
        {GuiManager::Models::GitCommitListModificationType::New,
         QPixmap(":/icons/file-add.png")},
        {GuiManager::Models::GitCommitListModificationType::Modified,
         QPixmap(":/icons/file-modify.png")},
        {GuiManager::Models::GitCommitListModificationType::Deleted,
         QPixmap(":/icons/file-delete.png")},
        {GuiManager::Models::GitCommitListModificationType::Renamed,
         QPixmap(":/icons/file-rename.png")},
        {GuiManager::Models::GitCommitListModificationType::TypeChanged,
         QPixmap(":/icons/file-type-change.png")},
        {GuiManager::Models::GitCommitListModificationType::Unreadable,
         QPixmap(":/icons/file-unreadable.png")},
        {GuiManager::Models::GitCommitListModificationType::Conflicted,
         QPixmap(":/icons/file-conflict.png")},
        {GuiManager::Models::GitCommitListModificationType::Ignored,
         QPixmap(":/icons/file-add.png")},
        {GuiManager::Models::GitCommitListModificationType::Untracked,
         QPixmap(":/icons/file-add.png")},
        {GuiManager::Models::GitCommitListModificationType::Other,
         QPixmap(":/icons/file-other.png")},
    };
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    const GuiManager::Models::GitCommitListModificationType modificationType =
        static_cast<GuiManager::Models::GitCommitListModificationType>(
            index.data(Qt::UserRole).toInt());

    painter->save();

    QString fullText = QDir::toNativeSeparators(opt.text);

    const QWidget *widget = opt.widget;
    QStyle *style = widget ? widget->style() : QApplication::style();

    opt.text.clear();

    // Let Qt draw the full item (background, focus, etc.)
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    // Get the correct text rectangle
    QRect textRect =
        style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);

    const int height = opt.rect.height();
    const QPixmap &icon = _modificationTypeToIcon.value(modificationType);

    if (!icon.isNull()) {
      const int left = opt.rect.left();
      const int top = opt.rect.top();

      if (_treePainter) {
        if (index.model()->rowCount(index) == 0) {
          painter->drawPixmap(QRect(left + 4, top + 4, height - 8, height - 8),
                              icon);
          textRect = textRect.adjusted(height, 0, 0, 0); // Adjust for icon
        }
      } else {
        painter->drawPixmap(QRect(left + 4, top + 4, height - 8, height - 8),
                            icon);
        textRect = textRect.adjusted(height, 0, 0, 0); // Adjust for icon
      }
    }

    QFontMetrics fm(opt.font);

    // Elide manually
    const QString elided =
        fm.elidedText(fullText, _elideMode, textRect.width());

    QString prefix;
    int separatorIndex = -1;

    if (!_treePainter) {
      separatorIndex = elided.lastIndexOf("…");
      if (separatorIndex != -1) {
        int i = elided.size() - 1;
        while (i > separatorIndex && elided[i] != QDir::separator()) {
          --i;
        }

        if (i > separatorIndex) {
          separatorIndex = i;
        } else {
          int i = separatorIndex;

          while (i >= 0 && elided[i] != QDir::separator()) {
            --i;
          };

          const QString leftOfElideSymbol = elided.mid(i < 0    ? 0
                                                       : i == 0 ? 0
                                                                : i + 1,
                                                       separatorIndex - i - 1);
          const QString rightOfElideSymbol = elided.mid(separatorIndex + 1);
          const QString fileName = QFileInfo(fullText).fileName();

          if (fileName.startsWith(leftOfElideSymbol) &&
              fileName.endsWith(rightOfElideSymbol)) {
            separatorIndex = i;
          }
        }

        prefix = elided.left(separatorIndex + 1);
      } else {
        prefix = QDir::toNativeSeparators(QFileInfo(elided).path()) +
                 QDir::separator();
      }
    }

    if (prefix.startsWith(".\\") || prefix.startsWith("./")) {
      prefix = prefix.mid(2);
    }

    QString suffix = elided;
    if (!_treePainter) {
      if (separatorIndex != -1) {
        suffix = elided.mid(separatorIndex + 1);
      } else {
        suffix = QFileInfo(elided).fileName();
      }
    }

    // Draw prefix (normal color)
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, prefix);

    // Measure prefix width
    int prefixWidth = fm.horizontalAdvance(prefix);
    QRect suffixRect = textRect.adjusted(prefixWidth, 0, 0, 0);

    // Draw suffix (custom color)
    if (_treePainter) {
      if (index.model()->rowCount(index) == 0) {
        painter->setPen(Qt::darkYellow);
      }
    } else {
      painter->setPen(Qt::darkYellow);
    }

    painter->drawText(suffixRect, Qt::AlignLeft | Qt::AlignVCenter, suffix);

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem &option,
                 const QModelIndex &index) const override {
    QFontMetrics fm(option.font);
    int height = fm.height() + 8; // Add padding
    return QSize(option.rect.width(), height);
  }

private:
  Qt::TextElideMode _elideMode;
  QMap<GuiManager::Models::GitCommitListModificationType, QPixmap>
      _modificationTypeToIcon;
  bool _treePainter;
};

} // namespace GuiManager::Delegates
