#pragma once

#include <QLabel>

namespace GuiManager {

namespace Widgets {
class ElidedLabel : public QLabel {
  Q_OBJECT
public:
  explicit ElidedLabel(QWidget *parent = nullptr);
  ~ElidedLabel() override = default;
  void setTextElideMode(Qt::TextElideMode mode);

  void resizeEvent(QResizeEvent *event) override;

private:
  Qt::TextElideMode _textElideMode;
};

} // namespace Widgets
} // namespace GuiManager
