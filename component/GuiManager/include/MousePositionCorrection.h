#pragma once

#include <QObject>
#include <QPoint>
#include <QPointF>

class QApplication;
class QEvent;
class QMouseEvent;

namespace GuiManager {

class MousePositionCorrection final : public QObject {
public:
  static MousePositionCorrection &instance();

  void install(QApplication &app);

  bool enabled() const { return _enabled; }
  QPointF offset() const { return _offset; }

  QPoint correctedGlobalPosition(const QMouseEvent *event) const;
  QPointF correctedGlobalPosition(const QPointF &globalPosition) const;

  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  explicit MousePositionCorrection(QObject *parent = nullptr);

  static bool isRunningUnderWsl2();
  void updateOffset(const QMouseEvent *event);

private:
  bool _enabled;
  bool _initialized;
  QPointF _offset;
};

} // namespace GuiManager
