#pragma once

#include <ConfigManager.h>

#include <QObject>
#include <QPoint>
#include <QRect>

namespace GuiManager {

class MainWindow;

class HitDetector : public QObject {
  Q_OBJECT
public:
  explicit HitDetector(MainWindow *window);
  virtual ~HitDetector() {}

  // WindowFrame interface
public:
  void init();
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void edgeHit(Qt::Edges edges);

private:
  enum class Margin : int {
    Unknown = 0,
    HTNOWHERE,
    HTBOTTOMRIGHT,
    HTBOTTOM,
    HTBOTTOMLEFT,
    HTLEFT,
    HTTOPLEFT,
    HTTOP,
    HTTOPRIGHT,
    HTRIGHT
  };

  Margin ncHitTest(const QRect &frameGeometry, int x, int y);

private:
  MainWindow *_window;
  qreal _pixelRatio;
  Qt::Edges _lastEdges;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager
