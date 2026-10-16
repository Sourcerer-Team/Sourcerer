#pragma once

#include <QWidget>

namespace GuiManager::Widgets {

class CentralWidget final : public QWidget {
  Q_OBJECT
public:
  explicit CentralWidget(QWidget *parent = nullptr);

  void setCentralWidget(QWidget *widget);

private:
  QWidget *_centralWidget;
};

} // namespace GuiManager::Widgets