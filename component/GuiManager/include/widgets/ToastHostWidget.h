#pragma once

#include "ToastWidget.h"

#include <ConfigManager.h>

#include <QLayout>
#include <QQueue>
#include <QWidget>

namespace GuiManager::Widgets {

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

class ToastHostWidget final : public QWidget {
  Q_OBJECT
public:
  static ToastHostWidget &getInstance();

  void addToast(ToastWidget *toast);
  [[nodiscard]] ToastPosition position() const { return _position; }
  void setParentSize(const QRect& geometry);
  void setPosition(ToastPosition position);
  void setYOffset(int offset);

private slots:
  void onToastStopped(ToastWidget *toast);

private:
  ToastHostWidget();
  ~ToastHostWidget() override;
  ToastHostWidget &operator=(const ToastHostWidget &) = delete;

  [[nodiscard]] QRect calculateGeometry() const;
  void destroyAllToasts();
  void addToLayout(ToastWidget *toast) const;
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

} // namespace GuiManager::Widgets

