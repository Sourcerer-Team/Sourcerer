
#pragma once

#include <QLabel>
#include <QWidget>

namespace GuiManager::Widgets {

class ToastWidget : public QWidget {
  Q_OBJECT
public:
  explicit ToastWidget(QWidget *parent = nullptr);
  ~ToastWidget() override;

  virtual void stop();

  void setTitleText(const QString &text) const;

  void hideTitle() const;

  // QWidget interface
public:
  QSize sizeHint() const override;

public slots:
  void onCloseButton();

signals:
  void stopped(ToastWidget *toast);

protected:
  QLabel *getTitleLabel() const { return _titleLabel; }
  void applyToastColors(const QColor &backgroundColor, const QColor &titleColor,
                        const QColor &textColor);

private:
  QWidget *_titleWidget;
  QLabel *_titleLabel;
};

} // namespace GuiManager::Widgets