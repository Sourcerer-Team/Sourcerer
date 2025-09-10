#pragma once

#include "OverlayWidget.h"
#include "ToastWidget.h"

#include <ConfigManager.h>

#include <QEnableSharedFromThis>
#include <QQueue>
#include <QSharedPointer>
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

class ToastHostWidget final : public OverlayWidget, public QEnableSharedFromThis<ToastHostWidget> {
  Q_OBJECT

  typedef QSharedPointer<ToastHostWidget> SharedPtr;
  typedef QWeakPointer<ToastHostWidget> WeakPtr;

public:
  ToastHostWidget();
  ~ToastHostWidget() override;

  static ToastHostWidget &getInstance();

  SharedPtr getSelf() { return sharedFromThis(); }

  void addToast(ToastWidget *toast);
  void addToast(const QSharedPointer<ToastWidget>& toast);
  [[nodiscard]] ToastPosition position() const { return _position; }
  void setPosition(ToastPosition position);
  void setYOffset(int offset);

private slots:
  void onToastStopped(ToastWidget *toast);

private:
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
  QVector<QSharedPointer<ToastWidget>> _sharedToasts;
  QQueue<ToastWidget *> _waitingToasts;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace GuiManager::Widgets
