#pragma once

#include "ToastWidget.h"

#include <ConfigManager.h>

#include <QLayout>
#include <QQueue>
#include <QVector>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

enum class ToastPosition {
  Unknown = 0,
  Top,
  Bottom,
  TopLeft,
  TopRight,
  BottomLeft,
  BottomRight,
  Last
};

class ToastHostWidget : public QWidget {
  Q_OBJECT
public:
  static ToastHostWidget &getInstance();

  void addToast(ToastWidget *toast);
  ToastPosition position() { return _position; }
  void setParentSize(const QRect& geometry);
  void setPosition(ToastPosition position);
  void setYOffset(int offset);

private slots:
  void onToastStopped(ToastWidget *toast);

private:
  ToastHostWidget();
  virtual ~ToastHostWidget();
  ToastHostWidget &operator=(const ToastHostWidget &) = delete;

  QRect calculateGeometry() const;
  void destroyAllToasts();
  void addToLayout(ToastWidget *toast);
  bool toastWillFit(ToastWidget *toast) const;

private:
  int _yOffset;
  ToastPosition _position;
  QRect _parentGeometry;
  QRect _cachedGeometry;
  QVector<ToastWidget *> _toasts;
  QQueue<ToastWidget *> _waitingToasts;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Widgets
} // namespace GuiManager
