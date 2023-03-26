#pragma once

#include <QTabBar>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarTabBar : public QTabBar {
  Q_OBJECT
public:
  explicit TitleBarTabBar(QWidget *parent = nullptr);

  void addTab(const QString &text);

signals:
  void resizeTabBar();

  // QWidget interface
protected:
  void paintEvent(QPaintEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

  // QTabBar interface
protected:
  QSize sizeHint() const override;

private slots:
  void onClickedClosed(int index);

private:
  bool _resized;
  int _maximumTabWidth;
};

} // namespace Widgets
} // namespace GuiManager
