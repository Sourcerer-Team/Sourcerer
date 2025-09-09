#pragma once

#include "OverlayWidget.h"
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

class ToastHostWidget final : public OverlayWidget {
  Q_OBJECT
public:
  static ToastHostWidget &getInstance();

  void addToast(ToastWidget *toast);
  [[nodiscard]] ToastPosition position() const { return _position; }
  void setPosition(ToastPosition position);
  void setYOffset(int offset);

private slots:
  void onToastStopped(ToastWidget *toast);

private:
  ToastHostWidget();
  ~ToastHostWidget() override;
  ToastHostWidget &operator=(const ToastHostWidget &) = delete;

  void destroyAllToasts();
  void addToLayout(ToastWidget *toast) const;
  void animateWidget(QWidget *widget) const;
  bool toastWillFit(const ToastWidget *toast) const;

public:
  bool showing() const override { return true; };

private:
  int _yOffset;
  ToastPosition _position;
  QVector<ToastWidget *> _toasts;
  QQueue<ToastWidget *> _waitingToasts;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
