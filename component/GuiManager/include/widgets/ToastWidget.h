
#pragma once

#include <QWidget>

namespace GuiManager::Widgets {

class ToastWidget : public QWidget {
  Q_OBJECT
public:
  explicit ToastWidget(QWidget *parent = nullptr);
  ~ToastWidget() override;

  virtual void stop();

  void hideTitle();

  // QWidget interface
public:
  QSize sizeHint() const override;

public slots:
  void onCloseButton();

signals:
  void stopped(ToastWidget *toast);

private:
  QWidget *_titleWidget;
};

} // namespace GuiManager::Widgets