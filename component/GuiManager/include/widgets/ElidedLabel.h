#pragma once

#include <QLabel>
#include <QResizeEvent>
#include <QString>

namespace GuiManager {

namespace Widgets {
class ElidedLabel : public QLabel {
  Q_OBJECT
public:
  explicit ElidedLabel(QWidget *parent = nullptr);
  ~ElidedLabel() override = default;
  void setTextElideMode(Qt::TextElideMode mode);
  void setFullText(const QString &text);

  void resizeEvent(QResizeEvent *event) override;

private:
  void updateElidedText();

private:
  QString _fullText;
  Qt::TextElideMode _textElideMode;
};

} // namespace Widgets
} // namespace GuiManager
