#pragma once

#include <QTabWidget>

namespace GuiManager::Widgets {

class ExtendedTabWidget final : public QTabWidget {
  Q_OBJECT
public:

  using QTabWidget::setTabBar;

  explicit ExtendedTabWidget(QWidget *parent = nullptr) : QTabWidget(parent) {}
};

} // namespace GuiManager::Widgets