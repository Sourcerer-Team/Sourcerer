#pragma once

#include <QApplication>
#include <QDir>
#include <QPainter>
#include <QStyledItemDelegate>
#include <qfileinfo.h>

namespace GuiManager::Delegates {

class ElidePartialColorDelegate final : public QStyledItemDelegate {
public:
  explicit ElidePartialColorDelegate(const Qt::TextElideMode elideMode, QObject* parent = nullptr)
      : QStyledItemDelegate(parent), _elideMode(elideMode) {}

  void paint(QPainter* painter, const QStyleOptionViewItem& option,
             const QModelIndex& index) const override {
    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);

    painter->save();

    QString fullText = opt.text;

    const QWidget* widget = opt.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();

    opt.text.clear();

    // Let Qt draw the full item (background, focus, etc.)
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);

    // Get the correct text rectangle
    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, widget);
    QFontMetrics fm(opt.font);

    // Elide manually
    const QString elided = fm.elidedText(fullText, _elideMode, textRect.width());
    QString prefix = QDir::toNativeSeparators(QFileInfo(elided).path()) + QDir::separator();
    if(prefix.startsWith(".\\") || prefix.startsWith("./")) {
      prefix = prefix.mid(2);
    }

    const QString suffix = QFileInfo(elided).fileName();

    // Draw prefix (normal color)
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, prefix);

    // Measure prefix width
    int prefixWidth = fm.horizontalAdvance(prefix);
    QRect suffixRect = textRect.adjusted(prefixWidth, 0, 0, 0);

    // Draw suffix (custom color)
    painter->setPen(Qt::darkYellow); // Or any color you want
    painter->drawText(suffixRect, Qt::AlignLeft | Qt::AlignVCenter, suffix);

    painter->restore();
  }

  QSize sizeHint(const QStyleOptionViewItem& option,
                 const QModelIndex& index) const override {
    QFontMetrics fm(option.font);
    int height = fm.height() + 8; // Add padding
    return QSize(option.rect.width(), height);
  }


private:
  Qt::TextElideMode _elideMode;
};

}
