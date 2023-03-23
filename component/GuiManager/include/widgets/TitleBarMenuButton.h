#pragma once

#include <ConfigManager.h>

#include <QHoverEvent>
#include <QMenu>
#include <QPushButton>
#include <QWidget>

namespace GuiManager {

namespace Widgets {

class TitleBarMenuButton : public QPushButton {
  Q_OBJECT
public:
  explicit TitleBarMenuButton(QWidget *parent = nullptr);

  // QObject interface
public:
  bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
  void onClicked();

private:
  QMenu* createMenu();
  bool hover(QHoverEvent* event, bool entered);

private:
  ConfigManager::ConfigManager& _configManager;
};

} // namespace Widgets
} // namespace GuiManager
