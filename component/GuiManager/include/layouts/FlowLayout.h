#pragma once

#include <QLayout>
#include <QStyle>
#include <QWidget>

namespace GuiManager::Layouts {

enum class FlowLayoutDirectionType {
  Horizontal = 0,
  Vertical = 1,
};

class FlowLayout final : public QLayout {
public:
  explicit FlowLayout(
      QWidget *parent,
      FlowLayoutDirectionType direction = FlowLayoutDirectionType::Horizontal,
      int margin = -1, int hSpacing = -1, int vSpacing = -1);
  explicit FlowLayout(int margin = -1, int hSpacing = -1, int vSpacing = -1);
  ~FlowLayout() override;

  void addItem(QLayoutItem *item) override;
  int horizontalSpacing() const;
  int verticalSpacing() const;
  Qt::Orientations expandingDirections() const override;
  bool hasHeightForWidth() const override;
  int heightForWidth(int) const override;
  int count() const override;
  QLayoutItem *itemAt(int index) const override;
  QSize minimumSize() const override;
  void setGeometry(const QRect &rect) override;
  QSize sizeHint() const override;
  QLayoutItem *takeAt(int index) override;

private:
  int doLayout(const QRect &rect, bool testOnly) const;
  int doLayoutHorizontalFirst(const QRect &rect, bool testOnly) const;
  int doLayoutVerticalFirst(const QRect &rect, bool testOnly) const;

  int smartSpacing(QStyle::PixelMetric pm) const;

  QList<QLayoutItem *> _itemList;
  int _hSpace;
  int _vSpace;

  FlowLayoutDirectionType _direction;
};

} // namespace GuiManager::Layouts